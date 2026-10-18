/*
 * hostapd / IEEE 802.11be EHT
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "common/ocv.h"
#include "common/wpa_ctrl.h"
#include "crypto/crypto.h"
#include "crypto/dh_groups.h"
#include "hostapd.h"
#include "sta_info.h"
#include "ap_drv_ops.h"
#include "wpa_auth.h"
#include "ieee802_11.h"
#include "common/defs.h"
#include "common/ieee802_11_defs.h"
#include "ap_config.h"
#include "ap_drv_ops.h"
#include "utils/eloop.h"
#include "wmm.h"
#include "ap/ctrl_iface_ap.h"
#include <linux/netfilter.h>

static u16 ieee80211_eht_ppet_size(u16 ppe_thres_hdr, const u8 *phy_cap_info)
{
	u8 ru;
	u16 sz = 0;

	if ((phy_cap_info[EHT_PHYCAP_PPE_THRESHOLD_PRESENT_IDX] &
	     EHT_PHYCAP_PPE_THRESHOLD_PRESENT) == 0)
		return 0;

	ru = (ppe_thres_hdr &
	      EHT_PPE_THRES_RU_INDEX_MASK) >> EHT_PPE_THRES_RU_INDEX_SHIFT;
	while (ru) {
		if (ru & 0x1)
			sz++;
		ru >>= 1;
	}

	sz = sz * (1 + ((ppe_thres_hdr & EHT_PPE_THRES_NSS_MASK) >>
			EHT_PPE_THRES_NSS_SHIFT));
	sz = (sz * 6) + 9;
	if (sz % 8)
		sz += 8;
	sz /= 8;

	return sz;
}


static u8 ieee80211_eht_mcs_set_size(enum hostapd_hw_mode mode, u8 opclass,
				     const u8 *he_phy_cap,
				     const u8 *eht_phy_cap)
{
	u8 sz = EHT_PHYCAP_MCS_NSS_LEN_20MHZ_PLUS;
	bool band24, band5, band6;
	u8 cap_chwidth;

	cap_chwidth = he_phy_cap[HE_PHYCAP_CHANNEL_WIDTH_SET_IDX];

	band24 = mode == HOSTAPD_MODE_IEEE80211B ||
		mode == HOSTAPD_MODE_IEEE80211G ||
		mode == NUM_HOSTAPD_MODES;
	band5 = mode == HOSTAPD_MODE_IEEE80211A ||
		mode == NUM_HOSTAPD_MODES;
	band6 = is_6ghz_op_class(opclass);

	if (band24 &&
	    (cap_chwidth & HE_PHYCAP_CHANNEL_WIDTH_SET_40MHZ_IN_2G) == 0)
		return EHT_PHYCAP_MCS_NSS_LEN_20MHZ_ONLY;

	if (band5 &&
	    (cap_chwidth &
	     (HE_PHYCAP_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G |
	      HE_PHYCAP_CHANNEL_WIDTH_SET_160MHZ_IN_5G |
	      HE_PHYCAP_CHANNEL_WIDTH_SET_80PLUS80MHZ_IN_5G)) == 0)
		return EHT_PHYCAP_MCS_NSS_LEN_20MHZ_ONLY;

	if (band5 &&
	    (cap_chwidth &
	     (HE_PHYCAP_CHANNEL_WIDTH_SET_160MHZ_IN_5G |
	      HE_PHYCAP_CHANNEL_WIDTH_SET_80PLUS80MHZ_IN_5G)))
	    sz += EHT_PHYCAP_MCS_NSS_LEN_20MHZ_PLUS;

	if (band6 &&
	    (eht_phy_cap[EHT_PHYCAP_320MHZ_IN_6GHZ_SUPPORT_IDX] &
	     EHT_PHYCAP_320MHZ_IN_6GHZ_SUPPORT_MASK))
		sz += EHT_PHYCAP_MCS_NSS_LEN_20MHZ_PLUS;

	return sz;
}



static void intersect_eht_mcs_map(u8 *pos, const u8 *hw_mcs_set, size_t mcs_set_len,
				  const u16 *usr_tx_mcs_set, const u16 *usr_rx_mcs_set)
{
	u8 i, idx, hw_val, hw_rx, hw_tx, usr_rx, usr_tx;

	for (i = 0; i < (u8)(mcs_set_len / EHT_PHYCAP_MCS_NSS_LEN_20MHZ_PLUS); i++) {
		for (idx = 0; idx < EHT_PHYCAP_MCS_NSS_LEN_20MHZ_PLUS; idx++) {
			hw_val = *hw_mcs_set++;

			hw_rx = hw_val & 0xF;
			hw_tx = hw_val >> 4;

			usr_rx = (usr_rx_mcs_set[i] >> (idx * 4)) & 0xF;
			usr_tx = (usr_tx_mcs_set[i] >> (idx * 4)) & 0xF;

			*pos++ = MIN(hw_rx, usr_rx) | (MIN(hw_tx, usr_tx) << 4);
		}
	}
}

bool eht_mu_mask_valid(u8 mask)
{
	if (mask & BIT(2)) {
		if (!(mask & BIT(1)) || !(mask & BIT(0)))
			return false;
	}
	if (mask & BIT(1)) {
		if (!(mask & BIT(0)))
			return false;
	}
	return true;
}

size_t hostapd_eid_eht_capab_len(struct hostapd_data *hapd,
				 enum ieee80211_op_mode opmode)
{
	struct hostapd_hw_modes *mode;
	struct eht_capabilities *eht_cap;
	size_t len = 3 + 2 + EHT_PHY_CAPAB_LEN;

	mode = hapd->iface->current_mode;
	if (!mode)
		return 0;

	eht_cap = &mode->eht_capab[opmode];
	if (!eht_cap->eht_supported)
		return 0;

	len += ieee80211_eht_mcs_set_size(mode->mode, hapd->iconf->op_class,
					  mode->he_capab[opmode].phy_cap,
					  eht_cap->phy_cap);
	len += ieee80211_eht_ppet_size(WPA_GET_LE16(&eht_cap->ppet[0]),
				       eht_cap->phy_cap);

	return len;
}

static bool hostapd_conf_eht_rtwt_enabled(struct hostapd_data *hapd)
{
	return (hapd->conf->twt_responder_caps == TWT_ITWT_BTWT_RTWT_ENABLED);
}

u8 * hostapd_eid_eht_capab(struct hostapd_data *hapd, u8 *eid,
			   enum ieee80211_op_mode opmode)
{
	struct hostapd_hw_modes *mode;
	struct eht_capabilities *eht_cap;
	struct hostapd_data *tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
	struct ieee80211_eht_capabilities *cap;
	size_t mcs_nss_len, ppe_thresh_len;
	u8 *pos = eid, *length_pos;

	mode = hapd->iface->current_mode;
	if (!mode)
		return eid;

	eht_cap = &mode->eht_capab[opmode];
	if (!eht_cap->eht_supported)
		return eid;

	*pos++ = WLAN_EID_EXTENSION;
	length_pos = pos++;
	*pos++ = WLAN_EID_EXT_EHT_CAPABILITIES;

	cap = (struct ieee80211_eht_capabilities *) pos;
	os_memset(cap, 0, sizeof(*cap));
	cap->mac_cap = host_to_le16(eht_cap->mac_cap);

	if (hapd->conf->is_epcs_enabled)
		cap->mac_cap |= EHT_MACCAP_EPCS_PRIO;
	else
		cap->mac_cap &= ~EHT_MACCAP_EPCS_PRIO;

	os_memcpy(cap->phy_cap, eht_cap->phy_cap, EHT_PHY_CAPAB_LEN);

	if (!hostapd_conf_eht_rtwt_enabled(hapd))
		cap->mac_cap &= ~EHT_MACCAP_TWT_RESTRICTED;

	if (!is_6ghz_op_class(hapd->iconf->op_class))
		cap->phy_cap[EHT_PHYCAP_320MHZ_IN_6GHZ_SUPPORT_IDX] &=
			~EHT_PHYCAP_320MHZ_IN_6GHZ_SUPPORT_MASK;


	/* For non-transmitting BSSs in MBSSID, inherit BSS-level overrides
	 * from the transmitting BSS */
	if (tx_hapd != hapd) {
		if (tx_hapd->conf->eht_phy_capab_mask) {
			hapd->conf->eht_phy_capab = tx_hapd->conf->eht_phy_capab;
			hapd->conf->eht_phy_capab_mask = tx_hapd->conf->eht_phy_capab_mask;
		}
		os_memcpy(hapd->conf->eht_tx_mcs_nss_set,
			  tx_hapd->conf->eht_tx_mcs_nss_set,
			  sizeof(hapd->conf->eht_tx_mcs_nss_set));
		os_memcpy(hapd->conf->eht_rx_mcs_nss_set,
			  tx_hapd->conf->eht_rx_mcs_nss_set,
			  sizeof(hapd->conf->eht_rx_mcs_nss_set));
	}
	if (!((hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_SU_BEAMFORMER) ?
	      hapd->conf->eht_phy_capab.su_beamformer :
	      hapd->iface->conf->eht_phy_capab.su_beamformer))
		cap->phy_cap[EHT_PHYCAP_SU_BEAMFORMER_IDX] &=
			~EHT_PHYCAP_SU_BEAMFORMER;

	if (!((hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_SU_BEAMFORMEE) ?
	      hapd->conf->eht_phy_capab.su_beamformee :
	      hapd->iface->conf->eht_phy_capab.su_beamformee))
		cap->phy_cap[EHT_PHYCAP_SU_BEAMFORMEE_IDX] &=
			~EHT_PHYCAP_SU_BEAMFORMEE;

	if (hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_MU_BFMR_MASK) {
		u8 mask = hapd->conf->eht_phy_capab.eht_mu_bfmr_mask;

		if (!(mask & BIT(0))) {
			cap->phy_cap[EHT_PHYCAP_MU_CAPABILITY_IDX] &=
				~EHT_PHYCAP_MU_BEAMFORMER_80MHZ;
		}
		if (!(mask & BIT(1))) {
			cap->phy_cap[EHT_PHYCAP_MU_CAPABILITY_IDX] &=
				~EHT_PHYCAP_MU_BEAMFORMER_160MHZ;
		}
		if (!(mask & BIT(2))) {
			cap->phy_cap[EHT_PHYCAP_MU_CAPABILITY_IDX] &=
				~EHT_PHYCAP_MU_BEAMFORMER_320MHZ;
		}
	}

	if (!(((hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_MU_BEAMFORMER) ?
	       hapd->conf->eht_phy_capab.mu_beamformer :
	       hapd->iface->conf->eht_phy_capab.mu_beamformer))) {
		cap->phy_cap[EHT_PHYCAP_MU_CAPABILITY_IDX] &=
			~EHT_PHYCAP_MU_BEAMFORMER_MASK;
	}

	{
		int val80, val160, val320;

		if (hapd->conf->eht_phy_capab_mask &
		    EHT_PHY_BSS_OVR_NON_OFDMA_UL_MUMIMO) {
			u8 m = hapd->conf->eht_phy_capab.eht_mu_mimo_mask;

			val80  = (m & BIT(0)) ? 1 : 0;
			val160 = (m & BIT(1)) ? 1 : 0;
			val320 = (m & BIT(2)) ? 1 : 0;
		} else {
			val80  = hapd->iface->conf->eht_phy_capab.non_ofdma_ulmumimo_80mhz;
			val160 = hapd->iface->conf->eht_phy_capab.non_ofdma_ulmumimo_160mhz;
			val320 = hapd->iface->conf->eht_phy_capab.non_ofdma_ulmumimo_320mhz;
		}

		if (val80 == 0)
			cap->phy_cap[EHT_PHYCAP_MU_CAPABILITY_IDX] &=
				~EHT_PHYCAP_NON_OFDMA_UL_MU_MIMO_80MHZ;
		if (val160 == 0)
			cap->phy_cap[EHT_PHYCAP_MU_CAPABILITY_IDX] &=
				~EHT_PHYCAP_NON_OFDMA_UL_MU_MIMO_160MHZ;
		if (val320 == 0)
			cap->phy_cap[EHT_PHYCAP_MU_CAPABILITY_IDX] &=
				~EHT_PHYCAP_NON_OFDMA_UL_MU_MIMO_320MHZ;
	}

       /*
        * Enforce band-specific Non-OFDMA UL MU-MIMO constraints
        * unconditionally, regardless of config or BSS-level overrides:
        * - 2.4 GHz: EHT max BW is 40 MHz, so 160 MHz and 320 MHz
        *   Non-OFDMA UL MU-MIMO bits are not applicable and must be cleared.
        * - 5 GHz: 320 MHz is a 6 GHz-only feature and must be cleared.
        */
       if (mode->mode == HOSTAPD_MODE_IEEE80211B ||
           mode->mode == HOSTAPD_MODE_IEEE80211G) {
               cap->phy_cap[EHT_PHYCAP_MU_CAPABILITY_IDX] &=
                       ~(EHT_PHYCAP_NON_OFDMA_UL_MU_MIMO_160MHZ |
                         EHT_PHYCAP_NON_OFDMA_UL_MU_MIMO_320MHZ);
       } else if (!is_6ghz_op_class(hapd->iconf->op_class)) {
               /* 5 GHz: 320 MHz Non-OFDMA UL MU-MIMO not applicable */
               cap->phy_cap[EHT_PHYCAP_MU_CAPABILITY_IDX] &=
                       ~EHT_PHYCAP_NON_OFDMA_UL_MU_MIMO_320MHZ;
       }

	/* Apply BSS-level beamformee spatial streams overrides */
	if (hapd->conf->eht_phy_capab_mask &
	    (EHT_PHY_BSS_OVR_BFME_SS_80 | EHT_PHY_BSS_OVR_BFME_SS_160 |
	     EHT_PHY_BSS_OVR_BFME_SS_320)) {
		u16 phy = WPA_GET_LE16(&cap->phy_cap[EHT_PHY_BFMEE_SS_80MHZ_IDX]);

		if (hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_BFME_SS_80) {
			u8 ss_80 = hapd->conf->eht_phy_capab.eht_bfme_ss_80;

			phy &= ~EHT_PHY_BFMEE_SS_80MHZ_MASK;
			phy |= ((u16) ss_80 << EHT_PHY_BFMEE_SS_80MHZ_SHIFT) &
				EHT_PHY_BFMEE_SS_80MHZ_MASK;
		}

		if (hapd->conf->eht_phy_capab_mask &
		    EHT_PHY_BSS_OVR_BFME_SS_160) {
			u8 ss_160 = hapd->conf->eht_phy_capab.eht_bfme_ss_160;

			phy &= ~EHT_PHY_BFMEE_SS_160MHZ_MASK;
			phy |= ((u16) ss_160 << EHT_PHY_BFMEE_SS_160MHZ_SHIFT) &
				EHT_PHY_BFMEE_SS_160MHZ_MASK;
		}

		if (hapd->conf->eht_phy_capab_mask &
		    EHT_PHY_BSS_OVR_BFME_SS_320) {
			u8 ss_320 = hapd->conf->eht_phy_capab.eht_bfme_ss_320;

			phy &= ~EHT_PHY_BFMEE_SS_320MHZ_MASK;
			phy |= ((u16) ss_320 << EHT_PHY_BFMEE_SS_320MHZ_SHIFT) &
				EHT_PHY_BFMEE_SS_320MHZ_MASK;
		}

		WPA_PUT_LE16(&cap->phy_cap[EHT_PHY_BFMEE_SS_80MHZ_IDX], phy);
	}

	if (hapd->conf->eht_phy_capab_mask &
	    EHT_PHY_BSS_OVR_NDP_4X_EHT_LTF_AND_320NSGI) {
		if (hapd->conf->eht_phy_capab.eht_ndp_4x_eht_ltf_and_320nsgi)
			cap->phy_cap[EHT_PHYCAP_NDP_4X_EHT_LTF_AND_320NSGI_IDX] |=
				EHT_PHYCAP_NDP_4X_EHT_LTF_AND_320NSGI;
		else
			cap->phy_cap[EHT_PHYCAP_NDP_4X_EHT_LTF_AND_320NSGI_IDX] &=
				~EHT_PHYCAP_NDP_4X_EHT_LTF_AND_320NSGI;
	}

	if (hapd->conf->eht_phy_capab_mask &
	    (EHT_PHY_BSS_OVR_NUM_SD_LT80 | EHT_PHY_BSS_OVR_NUM_SD_160 |
	     EHT_PHY_BSS_OVR_NUM_SD_320)) {
		u8 num_sd_lt80;
		u8 num_sd_160;
		u8 num_sd_320;
		bool su_bfmr =
			((hapd->conf->eht_phy_capab_mask &
			  EHT_PHY_BSS_OVR_SU_BEAMFORMER) ?
			 hapd->conf->eht_phy_capab.su_beamformer :
			 hapd->iface->conf->eht_phy_capab.su_beamformer);

		num_sd_lt80 =
			(cap->phy_cap[EHT_PHYCAP_NUM_SD_LT80_IDX] &
			 EHT_PHYCAP_NUM_SD_LT80_MASK) >>
			EHT_PHYCAP_NUM_SD_LT80_SHIFT;
		num_sd_160 =
			(cap->phy_cap[EHT_PHYCAP_NUM_SD_160_IDX] &
			 EHT_PHYCAP_NUM_SD_160_MASK) >>
			EHT_PHYCAP_NUM_SD_160_SHIFT;
		num_sd_320 =
			((cap->phy_cap[EHT_PHYCAP_NUM_SD_320_LOW_IDX] &
			  EHT_PHYCAP_NUM_SD_320_LOW_MASK) >>
			 EHT_PHYCAP_NUM_SD_320_LOW_SHIFT) |
			(((cap->phy_cap[EHT_PHYCAP_NUM_SD_320_HIGH_IDX] &
			   EHT_PHYCAP_NUM_SD_320_HIGH_MASK) >>
			  EHT_PHYCAP_NUM_SD_320_HIGH_SHIFT) << 2);

		if (hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_NUM_SD_LT80)
			num_sd_lt80 = hapd->conf->eht_phy_capab.eht_num_sd_lt80;
		if (hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_NUM_SD_160)
			num_sd_160 = hapd->conf->eht_phy_capab.eht_num_sd_160;
		if (hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_NUM_SD_320)
			num_sd_320 = hapd->conf->eht_phy_capab.eht_num_sd_320;

		if (!su_bfmr) {
			num_sd_lt80 = 0;
			num_sd_160 = 0;
			num_sd_320 = 0;
		}

		cap->phy_cap[EHT_PHYCAP_NUM_SD_LT80_IDX] &=
			~EHT_PHYCAP_NUM_SD_LT80_MASK;
		cap->phy_cap[EHT_PHYCAP_NUM_SD_LT80_IDX] |=
			(num_sd_lt80 << EHT_PHYCAP_NUM_SD_LT80_SHIFT) &
			EHT_PHYCAP_NUM_SD_LT80_MASK;

		cap->phy_cap[EHT_PHYCAP_NUM_SD_160_IDX] &=
			~EHT_PHYCAP_NUM_SD_160_MASK;
		cap->phy_cap[EHT_PHYCAP_NUM_SD_160_IDX] |=
			(num_sd_160 << EHT_PHYCAP_NUM_SD_160_SHIFT) &
			EHT_PHYCAP_NUM_SD_160_MASK;

		cap->phy_cap[EHT_PHYCAP_NUM_SD_320_LOW_IDX] &=
			~EHT_PHYCAP_NUM_SD_320_LOW_MASK;
		cap->phy_cap[EHT_PHYCAP_NUM_SD_320_LOW_IDX] |=
			(num_sd_320 << EHT_PHYCAP_NUM_SD_320_LOW_SHIFT) &
			EHT_PHYCAP_NUM_SD_320_LOW_MASK;

		cap->phy_cap[EHT_PHYCAP_NUM_SD_320_HIGH_IDX] &=
			~EHT_PHYCAP_NUM_SD_320_HIGH_MASK;
		cap->phy_cap[EHT_PHYCAP_NUM_SD_320_HIGH_IDX] |=
			((num_sd_320 >> 2) << EHT_PHYCAP_NUM_SD_320_HIGH_SHIFT) &
			EHT_PHYCAP_NUM_SD_320_HIGH_MASK;
	}

	if (hapd->conf->eht_phy_capab_mask &
	    EHT_PHY_BSS_OVR_4X_EHT_LTF_AND_800NS_GI) {
		if (hapd->conf->eht_phy_capab.eht_4x_eht_ltf_and_800ns_gi)
			cap->phy_cap[EHT_PHYCAP_4X_EHT_LTF_AND_800NS_GI_IDX] |=
				EHT_PHYCAP_4X_EHT_LTF_AND_800NS_GI;
		else
			cap->phy_cap[EHT_PHYCAP_4X_EHT_LTF_AND_800NS_GI_IDX] &=
				~EHT_PHYCAP_4X_EHT_LTF_AND_800NS_GI;
	}

	if (hapd->conf->eht_phy_capab_mask &
	    EHT_PHY_BSS_OVR_RX_1024_AND_4096_QAM_LS_242_TONE_RU) {
		if (hapd->conf->eht_phy_capab
			    .eht_rx_1024_and_4096_qam_ls_242_tone_ru)
			cap->phy_cap
				[EHT_PHYCAP_RX_1024_AND_4096_QAM_LS_242_TONE_RU_IDX] |=
				EHT_PHYCAP_RX_1024_AND_4096_QAM_LS_242_TONE_RU;
		else
			cap->phy_cap
				[EHT_PHYCAP_RX_1024_AND_4096_QAM_LS_242_TONE_RU_IDX] &=
				~EHT_PHYCAP_RX_1024_AND_4096_QAM_LS_242_TONE_RU;
	}

	if (hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_NON_TRIG_CQI_FB) {
		if (!hapd->conf->eht_phy_capab.eht_non_trig_cqi_fb)
			cap->phy_cap[EHT_PHYCAP_NON_TRIG_CQI_FB_IDX] &=
				~EHT_PHYCAP_NON_TRIG_CQI_FB;
	}

	if (hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_TRIG_SU_BF_FB) {
		if (!hapd->conf->eht_phy_capab.eht_trig_su_bf_fb)
			cap->phy_cap[EHT_PHYCAP_TRIG_SU_BF_FB_IDX] &=
				~EHT_PHYCAP_TRIG_SU_BF_FB;
	}

	if (hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_TRIG_CQI_FB) {
		if (!hapd->conf->eht_phy_capab.eht_trig_cqi_fb)
			cap->phy_cap[EHT_PHYCAP_TRIG_CQI_FB_IDX] &=
				~EHT_PHYCAP_TRIG_CQI_FB;
	}

	if (hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_DL_OFDMA_TXBF) {
		if (hapd->conf->eht_phy_capab.eht_dl_ofdma_txbf)
			cap->phy_cap[EHT_PHYCAP_TRIG_MU_BF_PART_BW_FB_IDX] |=
				EHT_PHYCAP_TRIG_MU_BF_PART_BW_FB;
		else
			cap->phy_cap[EHT_PHYCAP_TRIG_MU_BF_PART_BW_FB_IDX] &=
				~EHT_PHYCAP_TRIG_MU_BF_PART_BW_FB;
	}

	if (hapd->conf->eht_phy_capab_mask & EHT_PHY_BSS_OVR_SUP_MCS15_IN_MRU) {
		cap->phy_cap[EHT_PHYCAP_SUP_MCS15_IN_MRU_IDX] &=
			~EHT_PHYCAP_SUP_MCS15_IN_MRU_MASK;
		cap->phy_cap[EHT_PHYCAP_SUP_MCS15_IN_MRU_IDX] |=
			(hapd->conf->eht_phy_capab.eht_sup_mcs15_in_mru <<
			 EHT_PHYCAP_SUP_MCS15_IN_MRU_SHIFT) &
			EHT_PHYCAP_SUP_MCS15_IN_MRU_MASK;
	}

	if (hapd->conf->eht_phy_capab_mask &
	    EHT_PHY_BSS_OVR_MCS14_DUP_IN_6GHZ) {
		if (hapd->conf->eht_phy_capab.eht_mcs14_dup_in_6ghz)
			cap->phy_cap[EHT_PHYCAP_MCS14_DUP_IN_6GHZ_IDX] |=
				EHT_PHYCAP_MCS14_DUP_IN_6GHZ;
		else
			cap->phy_cap[EHT_PHYCAP_MCS14_DUP_IN_6GHZ_IDX] &=
				~EHT_PHYCAP_MCS14_DUP_IN_6GHZ;
	}
	pos = cap->optional;

	mcs_nss_len = ieee80211_eht_mcs_set_size(mode->mode,
						 hapd->iconf->op_class,
						 mode->he_capab[opmode].phy_cap,
						 eht_cap->phy_cap);
	if (mcs_nss_len) {
		intersect_eht_mcs_map(pos, eht_cap->mcs, mcs_nss_len,
				      hapd->conf->eht_tx_mcs_nss_set,
				      hapd->conf->eht_rx_mcs_nss_set);
		pos += mcs_nss_len;
	}

	ppe_thresh_len = ieee80211_eht_ppet_size(
				WPA_GET_LE16(&eht_cap->ppet[0]),
				eht_cap->phy_cap);
	if (ppe_thresh_len) {
		os_memcpy(pos, eht_cap->ppet, ppe_thresh_len);
		pos += ppe_thresh_len;
	}

	*length_pos = pos - (eid + 2);
	return pos;
}


u8 * hostapd_eid_eht_operation(struct hostapd_data *hapd, u8 *eid)
{
	struct hostapd_config *conf = hapd->iconf;
	struct ieee80211_eht_operation *oper;
	u8 *pos = eid, seg0 = 0, seg1 = 0;
	enum oper_chan_width chwidth;
	size_t elen = 1 + 4;
	bool eht_oper_info_present;
	u16 punct_bitmap = hostapd_get_punct_bitmap(hapd);

	if (!hapd->iface->current_mode)
		return eid;

	if (is_6ghz_op_class(conf->op_class))
		chwidth = op_class_to_ch_width(conf->op_class);
	else
		chwidth = conf->eht_oper_chwidth;

	seg0 = hostapd_get_oper_centr_freq_seg0_idx(conf);
	if (!seg0)
		seg0 = hapd->iconf->channel;

	if (is_5ghz_freq(hapd->iface->freq) && (chwidth == CONF_OPER_CHWIDTH_320MHZ)
	    && (hapd->iconf->punct_bitmap)) {
		chwidth = CONF_OPER_CHWIDTH_160MHZ;
		punct_bitmap &= 0xFF;
		seg0 -= 16;
	}

	eht_oper_info_present = chwidth == CONF_OPER_CHWIDTH_320MHZ ||
		punct_bitmap;

	if (eht_oper_info_present)
		elen += 3;

	if (punct_bitmap)
		elen += EHT_OPER_DISABLED_SUBCHAN_BITMAP_SIZE;

	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 1 + elen;
	*pos++ = WLAN_EID_EXT_EHT_OPERATION;

	oper = (struct ieee80211_eht_operation *) pos;
	oper->oper_params = 0;

	if (hapd->iconf->eht_default_pe_duration)
		oper->oper_params |= EHT_OPER_DEFAULT_PE_DURATION;
	if (!hapd->iconf->enable_mcs15)
		oper->oper_params |= EHT_OPER_MCS15_DISABLE;

	/* TODO: Fill in appropriate EHT-MCS max Nss information */
	oper->basic_eht_mcs_nss_set[0] = 0x11;
	oper->basic_eht_mcs_nss_set[1] = 0x00;
	oper->basic_eht_mcs_nss_set[2] = 0x00;
	oper->basic_eht_mcs_nss_set[3] = 0x00;

	if (!eht_oper_info_present)
		return pos + elen;

	oper->oper_params |= EHT_OPER_INFO_PRESENT;

	switch (chwidth) {
	case CONF_OPER_CHWIDTH_320MHZ:
		oper->oper_info.control |= EHT_OPER_CHANNEL_WIDTH_320MHZ;
		seg1 = seg0;
		if (hapd->iconf->channel < seg0)
			seg0 -= 16;
		else
			seg0 += 16;
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		oper->oper_info.control |= EHT_OPER_CHANNEL_WIDTH_160MHZ;
		seg1 = seg0;
		if (hapd->iconf->channel < seg0)
			seg0 -= 8;
		else
			seg0 += 8;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		oper->oper_info.control |= EHT_OPER_CHANNEL_WIDTH_80MHZ;
		break;
	case CONF_OPER_CHWIDTH_USE_HT:
		if ((is_6ghz_op_class(hapd->iconf->op_class) &&
		    op_class_to_bandwidth(hapd->iconf->op_class) == 40) ||
		    hapd->iconf->secondary_channel)
			oper->oper_info.control |= EHT_OPER_CHANNEL_WIDTH_40MHZ;
		else
			oper->oper_info.control |= EHT_OPER_CHANNEL_WIDTH_20MHZ;

		break;
	default:
		punct_bitmap = 0;
		oper->oper_info.control |= EHT_OPER_CHANNEL_WIDTH_20MHZ;
		break;
	}

#ifdef CONFIG_QCN_EXTN
	if (hapd->iconf->conf_extn.eht_config_ccfs0)
		seg0 = 0;
#endif
	oper->oper_info.ccfs0 = seg0;
	oper->oper_info.ccfs1 = seg1;

	if (punct_bitmap) {
		oper->oper_params |= EHT_OPER_DISABLED_SUBCHAN_BITMAP_PRESENT;
		oper->oper_info.disabled_chan_bitmap =
			host_to_le16(punct_bitmap);
	}

	return pos + elen;
}


static bool check_valid_eht_mcs_nss(struct hostapd_data *hapd, const u8 *ap_mcs,
				    const u8 *sta_mcs, u8 mcs_count, u8 map_len)
{
	unsigned int i, j;

	for (i = 0; i < mcs_count; i++) {
		ap_mcs += i * 3;
		sta_mcs += i * 3;

		for (j = 0; j < map_len; j++) {
			if (((ap_mcs[j] >> 4) & 0xFF) == 0)
				continue;

			if ((sta_mcs[j] & 0xFF) == 0)
				continue;

			return true;
		}
	}

	wpa_printf(MSG_DEBUG,
		   "No matching EHT MCS found between AP TX and STA RX");
	return false;
}


static bool check_valid_eht_mcs(struct hostapd_data *hapd,
				const u8 *sta_eht_capab,
				enum ieee80211_op_mode opmode)
{
	struct hostapd_hw_modes *mode;
	const struct ieee80211_eht_capabilities *capab;
	const u8 *ap_mcs, *sta_mcs;
	u8 mcs_count = 1;

	mode = hapd->iface->current_mode;
	if (!mode)
		return true;

	ap_mcs = mode->eht_capab[opmode].mcs;
	capab = (const struct ieee80211_eht_capabilities *) sta_eht_capab;
	sta_mcs = capab->optional;

	if (ieee80211_eht_mcs_set_size(mode->mode, hapd->iconf->op_class,
				       mode->he_capab[opmode].phy_cap,
				       mode->eht_capab[opmode].phy_cap) ==
	    EHT_PHYCAP_MCS_NSS_LEN_20MHZ_ONLY)
		return check_valid_eht_mcs_nss(
			hapd, ap_mcs, sta_mcs, 1,
			EHT_PHYCAP_MCS_NSS_LEN_20MHZ_ONLY);

	switch (hapd->iface->conf->eht_oper_chwidth) {
	case CONF_OPER_CHWIDTH_320MHZ:
		mcs_count++;
		/* fall through */
	case CONF_OPER_CHWIDTH_80P80MHZ:
	case CONF_OPER_CHWIDTH_160MHZ:
		mcs_count++;
		break;
	default:
		break;
	}

	return check_valid_eht_mcs_nss(hapd, ap_mcs, sta_mcs, mcs_count,
				       EHT_PHYCAP_MCS_NSS_LEN_20MHZ_PLUS);
}


static bool ieee80211_invalid_eht_cap_size(enum hostapd_hw_mode mode,
					   u8 opclass, const u8 *he_cap,
					   const u8 *eht_cap, size_t len)
{
	const struct ieee80211_he_capabilities *he_capab;
	struct ieee80211_eht_capabilities *cap;
	const u8 *he_phy_cap;
	size_t cap_len;
	u16 ppe_thres_hdr;

	he_capab = (const struct ieee80211_he_capabilities *) he_cap;
	he_phy_cap = he_capab->he_phy_capab_info;
	cap = (struct ieee80211_eht_capabilities *) eht_cap;
	cap_len = sizeof(*cap) - sizeof(cap->optional);
	if (len < cap_len)
		return true;

	cap_len += ieee80211_eht_mcs_set_size(mode, opclass, he_phy_cap,
					      cap->phy_cap);
	if (len < cap_len)
		return true;

	ppe_thres_hdr = len > cap_len + 1 ?
		WPA_GET_LE16(&eht_cap[cap_len]) : 0x01ff;
	cap_len += ieee80211_eht_ppet_size(ppe_thres_hdr, cap->phy_cap);

	return len < cap_len;
}


u16 copy_sta_eht_capab(struct hostapd_data *hapd, struct sta_info *sta,
		       enum ieee80211_op_mode opmode,
		       const u8 *he_capab, size_t he_capab_len,
		       const u8 *eht_capab, size_t eht_capab_len)
{
	struct hostapd_hw_modes *c_mode = hapd->iface->current_mode;
	enum hostapd_hw_mode mode = c_mode ? c_mode->mode : NUM_HOSTAPD_MODES;

	if (!hostapd_is_eht_enabled(hapd) ||
	    !he_capab || he_capab_len < IEEE80211_HE_CAPAB_MIN_LEN ||
	    !eht_capab ||
	    ieee80211_invalid_eht_cap_size(mode, hapd->iconf->op_class,
					   he_capab, eht_capab,
					   eht_capab_len) ||
	    !check_valid_eht_mcs(hapd, eht_capab, opmode) ||
	    !(sta->flags & WLAN_STA_HE)) {
		sta->flags &= ~WLAN_STA_EHT;
		os_free(sta->eht_capab);
		sta->eht_capab = NULL;
		return WLAN_STATUS_SUCCESS;
	}

	os_free(sta->eht_capab);
	sta->eht_capab = os_memdup(eht_capab, eht_capab_len);
	if (!sta->eht_capab) {
		sta->eht_capab_len = 0;
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	sta->flags |= WLAN_STA_EHT;
	sta->eht_capab_len = eht_capab_len;

	return WLAN_STATUS_SUCCESS;
}


void hostapd_get_eht_capab(struct hostapd_data *hapd,
			   const struct ieee80211_eht_capabilities *src,
			   struct ieee80211_eht_capabilities *dest,
			   size_t len)
{
	if (!src || !dest)
		return;

	if (len > sizeof(*dest))
		len = sizeof(*dest);
	/* TODO: mask out unsupported features */

	os_memset(dest, 0, sizeof(*dest));
	os_memcpy(dest, src, len);
}


/* Common info (1) + MLD Mac address (6) + STA Info
 * length(1) + STA Mac address(6) + AP removal
 * timer(2) + Operation parameters(3)
 */
#define ML_RECONFIG_FIXED_IE_LEN 19
size_t hostapd_eid_eht_ml_reconfig_len(struct hostapd_data *hapd)
{
	size_t len, sta_prof_len = 0;
	struct hostapd_data *link_bss;

	/* Include WLAN_EID_EXT_MULTI_LINK (1) */
	len = 1;
	/* Control Field */
	len += 2;
	/* Common Info - Doesn't include any information */
	len += 1;

	for_each_mld_link(link_bss, hapd) {

		/* Currently we support removing only one link
		 * at a time from a MLD
		 */
		if (!link_bss->eht_mld_link_removal_count ||
		    !link_bss->eht_mld_link_removal_inprogress)
			continue;

		/* Sub-Element field */
		sta_prof_len += 2;

		/* Per-STA profile */

		/*STA-Control(2) field - only AP removal timer */
		sta_prof_len += 2;

		/* STA-Info(3) */

		/* STA-Info length */
		sta_prof_len += 1;

		/* STA-Info AP removal timer */
		sta_prof_len += 2;
	}

	/* If no Sta profile is present, element itself is not needed */
	if (!sta_prof_len)
		return 0;

	/*WLAN_EID_EXTENSION (1) + length (1) */
	return len + sta_prof_len + 2;
}


u8 *hostapd_eid_eht_reconf_ml(struct hostapd_data *hapd,
			      u8 *eid)
{
	struct hostapd_data *link_bss;
	u16 control;
	u8 *pos = eid;
	bool sta_profile_present = false;

	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 0;
	*pos++ = WLAN_EID_EXT_MULTI_LINK;

	/* set the multi-link control field */
	control = MULTI_LINK_CONTROL_TYPE_RECONF;
	WPA_PUT_LE16(pos, control);
	pos += 2;

	/* common info doesn't include any information */
	*pos++ = 1;

	/* Need to have this in other API and make this generic or align with
	 * the upstream
	 */
	for_each_mld_link(link_bss, hapd) {
		/* Currently we support removing only one link
		 * at a time from a MLD
		 */
		if (!link_bss->eht_mld_link_removal_count ||
		    !link_bss->eht_mld_link_removal_inprogress)
			continue;

		sta_profile_present = true;
		/* sub element ID is 0 */
		*pos++ = 0;
		*pos++ = 5;

		control = link_bss->mld_link_id |
			  EHT_PER_STA_RECONF_CTRL_AP_REMOVAL_TIMER;
		WPA_PUT_LE16(pos, control);
		pos += 2;

		/* STA Profile length */
		*pos++ = 3;

		WPA_PUT_LE16(pos, link_bss->eht_mld_link_removal_count);
		pos += 2;
	}

	if (!sta_profile_present)
		return eid;

	eid[1] = pos - eid - 2;

	wpa_hexdump_ascii(MSG_INFO, "MLD: reconfiguration MLE dump: ",
			  eid, eid[1]);
	return pos;
}


/* Beacon or a non ML Probe Response frame should include
 * Common Info Length(1) + MLD MAC Address(6) +
 * Link ID Info(1) + BSS Parameters Change count(1) +
 * EML Capabilities (2) + MLD Capabilities (2)
 */
#define EHT_ML_COMMON_INFO_LEN 13
/*
 * control (2) + station info length (1) + MAC address (6) +
 * beacon interval (2) + TSF offset (8) + DTIM info (2)
 */
#define EHT_ML_STA_INFO_LEN 21

#ifdef CONFIG_IEEE80211BN
static u8 hostapd_bss_load_age(struct os_reltime *ch_util_update_time)
{
	static const u32 threshold[] = {
		5000, 10000, 20000, 30000, 60000
	};
	struct os_reltime age;
	u64 tu;
	u8 i;

	/* If channel_utilization was never updated return 0 */
	if (!os_reltime_initialized(ch_util_update_time))
		return 0;

	os_reltime_age(ch_util_update_time, &age);
	tu = USEC_TO_TU((u64) age.sec * 1000000ULL + age.usec);

	/* IEEE 802.11bn D1.5 Table 9-468a: Age of BSS Load encoding */
	for (i = 0; i < ARRAY_SIZE(threshold); i++) {
		if (tu < threshold[i])
			return i;
	}

	return ARRAY_SIZE(threshold);
}
#endif /* CONFIG_IEEE80211BN */

u8 * hostapd_eid_eht_basic_ml_common(struct hostapd_data *hapd,
				     u8 *eid, struct mld_info *mld_info,
				     bool include_mld_id, bool include_bpcc,
				     u8 include_ext_cap, bool is_smd,
				     bool is_uhr_sta, bool include_bss_load_age,
				     bool skip_uhr_extn)
{
	struct wpabuf *buf;
	u16 control;
	u8 *pos = eid;
	const u8 *ptr;
	size_t len, slice_len;
	u8 link_id;
	u8 common_info_len;
	u16 mld_cap;
	u8 max_simul_links, active_links, max_rec_links;
	u16 ext_mld_cap;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return pos;
#endif /* CONFIG_QCN_EXTN */

	/*
	 * As the Multi-Link element can exceed the size of 255 bytes need to
	 * first build it and then handle fragmentation.
	 */
	buf = wpabuf_alloc(1024);
	if (!buf)
		return pos;

	/* Multi-Link Control field */
	control = MULTI_LINK_CONTROL_TYPE_BASIC |
		BASIC_MULTI_LINK_CTRL_PRES_LINK_ID |
		BASIC_MULTI_LINK_CTRL_PRES_BSS_PARAM_CH_COUNT |
		BASIC_MULTI_LINK_CTRL_PRES_EML_CAPA |
		BASIC_MULTI_LINK_CTRL_PRES_MLD_CAPA;

	/*
	 * Set the basic Multi-Link common information. Hard code the common
	 * info length to 13 based on the length of the present fields:
	 * Length (1) + MLD address (6) + Link ID (1) +
	 * BSS Parameters Change Count (1) + EML Capabilities (2) +
	 * MLD Capabilities and Operations (2)
	 */
	common_info_len = EHT_ML_COMMON_INFO_LEN;

	if (include_mld_id) {
		/* AP MLD ID */
		control |= BASIC_MULTI_LINK_CTRL_PRES_AP_MLD_ID;
		common_info_len++;
	}

	if (include_ext_cap) {
		control |= BASIC_MULTI_LINK_CTRL_PRES_EXT_MLD_CAP;
		common_info_len += 2;
	}

	if (hostapd_is_uhr_enabled(hapd) && is_uhr_sta && !skip_uhr_extn) {
		/* Enhanced Critical Updates Information */
		control |= BASIC_MULTI_LINK_CTRL_PRES_ENH_CRIT_UPD;
		common_info_len++;

		if (include_bss_load_age && hapd->conf->bss_load_update_period) {
			/* Age of BSS Load Present */
			control |= BASIC_MULTI_LINK_CTRL_PRES_AGE_OF_BSS_LOAD;
			common_info_len++;
		}
	}

	wpabuf_put_le16(buf, control);

	wpabuf_put_u8(buf, common_info_len);

	/* Own MLD MAC Address */
	wpabuf_put_data(buf, hapd->mld->mld_addr, ETH_ALEN);

	/* Own Link ID */
	wpabuf_put_u8(buf, hapd->mld_link_id);

	/* BSS Parameters Change Count */
	if (include_bpcc)
		wpabuf_put_u8(buf, hapd->rx_cu_param.bpcc);
	else
		wpabuf_put_u8(buf, 0);

	/* Reset the EMLSR Transision and Padding delay to zero for
	 * MLD AP as per IEEE802.11 be draft 5.0
	 */
	hapd->iface->mld_eml_capa &= ~EHT_ML_EML_CAPA_EMLSR_TRANS_DELAY_MASK;
	hapd->iface->mld_eml_capa &= ~EHT_ML_EML_CAPA_EMLSR_PADDING_DELAY_MASK;

	/* Disable all EML capabilities (EMLSR + EMLMR) if configured */
	if (hapd->conf->disable_eml)
		hapd->iface->mld_eml_capa = 0;

	wpa_printf(MSG_DEBUG, "MLD: EML Capabilities=0x%x",
		   hapd->iface->mld_eml_capa);
	wpabuf_put_le16(buf, hapd->iface->mld_eml_capa);

	mld_cap = hapd->iface->mld_mld_capa;
	max_simul_links = mld_cap & EHT_ML_MLD_CAPA_MAX_NUM_SIM_LINKS_MASK;
	active_links = hostapd_get_active_links(hapd);

	if (active_links > max_simul_links) {
		wpa_printf(MSG_ERROR,
			   "MLD: Error in max simultaneous links, advertised: 0x%x current: 0x%x",
			   max_simul_links, active_links);
		active_links = max_simul_links;
	}

	mld_cap &= ~EHT_ML_MLD_CAPA_MAX_NUM_SIM_LINKS_MASK;
	mld_cap |= active_links & EHT_ML_MLD_CAPA_MAX_NUM_SIM_LINKS_MASK;

	if (!hapd->conf->ttlm_enable)
		mld_cap &= ~EHT_ML_MLD_CAPA_TID_TO_LINK_MAP_NEG_SUPP_MSK;

	mld_cap |= EHT_ML_MLD_CAPA_LINK_RECONF_OP_SUPPORT;

	wpa_printf(MSG_DEBUG, "MLD: MLD Capabilities and Operations=0x%x",
		   mld_cap);
	wpabuf_put_le16(buf, mld_cap);

	if (include_mld_id) {
		wpa_printf(MSG_DEBUG, "MLD: AP MLD ID=0x%x",
			   hostapd_get_mld_id(hapd));
		wpabuf_put_u8(buf, hostapd_get_mld_id(hapd));
	}

	if (include_ext_cap) {
		ext_mld_cap = 0;
		if (include_ext_cap & BIT(BASIC_MULTI_LINK_CTRL_EXT_RMSL_INFO_EN)) {
			max_rec_links = hapd->conf->ml_max_rec_links;
			if (max_rec_links > (active_links + 1)) {
				wpa_printf(MSG_ERROR,
					   "MLD: Error in max recommended links, advertised: 0x%x current: 0x%x",
					   active_links + 1, max_rec_links);
				max_rec_links = active_links + 1;
			}

			if (max_rec_links == ML_IE_RSVD_MAX_REC_LINKS)
				max_rec_links = ML_IE_NO_MAX_REC_LINKS;

			ext_mld_cap &= ~EHT_ML_MLD_EXT_CAPA_MAX_NUM_REC_LINKS_MASK;
			ext_mld_cap |= ((max_rec_links <<
					EHT_ML_MLD_EXT_CAPA_MAX_NUM_REC_LINKS_OFFSET) &
					EHT_ML_MLD_EXT_CAPA_MAX_NUM_REC_LINKS_MASK);
			wpa_printf(MSG_DEBUG, "MLD: rec_links %u", max_rec_links);
		}

		if (include_ext_cap & BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK))
			ext_mld_cap |= BIT(EHT_ML_MLD_EXT_CAPA_EMLSR_ONE_LINK);

		wpa_printf(MSG_DEBUG,
			   "MLD: Ext MLD Capabilities and Operations=0x%x", ext_mld_cap);
		wpabuf_put_le16(buf, ext_mld_cap);
	}

#ifdef CONFIG_IEEE80211BN
	if (hostapd_is_uhr_enabled(hapd) && is_uhr_sta && !skip_uhr_extn) {
		u8 ecu_info = include_bpcc ?
			      hostapd_get_ecu_info(&hapd->rx_ecu_param) : 0;
		wpabuf_put_u8(buf, ecu_info);
		if (include_bss_load_age && hapd->conf->bss_load_update_period)
			wpabuf_put_u8(buf, hostapd_bss_load_age(
					&hapd->iface->ch_util_update_time));
	}
#endif /* CONFIG_IEEE80211BN */

	if (!mld_info)
		goto out;

	/* Add link info for the other links */
	for (link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		struct mld_link_info *link = &mld_info->links[link_id];
		size_t sta_info_len = EHT_ML_STA_INFO_LEN;
		struct hostapd_data *link_bss;
		size_t total_len;

		/* Skip the local one */
		if (!link->valid)
			continue;

		if (!is_smd) {
			if (link_id == hapd->mld_link_id)
				continue;
		}

		link_bss = hostapd_mld_get_link_bss(hapd, link_id);
		if (!link_bss) {
			wpa_printf(MSG_ERROR,
				   "MLD: Couldn't find link BSS - skip it");
			continue;
		}

#ifdef CONFIG_QCN_EXTN
		if (hostapd_is_repurpose_disabled_11be_extn(link_bss->conf)) {
			wpa_printf(MSG_DEBUG,
				   "Skip the repurposed link from link info");
			continue;
		}
#endif /* CONFIG_QCN_EXTN */

		/* BSS Parameters Change Count (1) for (Re)Association Response
		 * frames */
		if (include_bpcc)
			sta_info_len++;
		/* Enhanced Critical Updates Information */
		if (include_bpcc && hostapd_is_uhr_enabled(hapd) &&
		    !skip_uhr_extn && is_uhr_sta)
			sta_info_len++;

		total_len = sta_info_len + link->resp_sta_profile_len;

		/* Per-STA Profile subelement */
		wpabuf_put_u8(buf, MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE);

		if (total_len <= 255)
			wpabuf_put_u8(buf, total_len);
		else
			wpabuf_put_u8(buf, 255);

		/* STA Control */
		control = (link_id & 0xf) |
			BASIC_MLE_STA_CTRL_PRES_STA_MAC |
			BASIC_MLE_STA_CTRL_COMPLETE_PROFILE |
			BASIC_MLE_STA_CTRL_PRES_TSF_OFFSET |
			BASIC_MLE_STA_CTRL_PRES_BEACON_INT |
			BASIC_MLE_STA_CTRL_PRES_DTIM_INFO;

		if (include_bpcc)
			control |= BASIC_MLE_STA_CTRL_PRES_BSS_PARAM_COUNT;
		if (include_bpcc && hostapd_is_uhr_enabled(hapd) &&
		    !skip_uhr_extn && is_uhr_sta)
			control |= BASIC_MLE_STA_CTRL_PRES_ENH_CRIT_UPD;

		wpabuf_put_le16(buf, control);

		/* STA Info */

		/* STA Info Length */
		wpabuf_put_u8(buf, sta_info_len - 2);
		wpabuf_put_data(buf, link->local_addr, ETH_ALEN);
		wpabuf_put_le16(buf, link_bss->iconf->beacon_int);

		/* TSF Offset */
		/*
		 * TODO: Currently setting TSF offset to zero. However, this
		 * information needs to come from the driver.
		 */
		wpabuf_put_le64(buf, 0);

		/* DTIM Info */
		wpabuf_put_u8(buf, 0); /* DTIM Count */
		wpabuf_put_u8(buf, link_bss->conf->dtim_period);

		/* BSS Parameters Change Count */
		if (include_bpcc)
			wpabuf_put_u8(buf, link_bss->rx_cu_param.bpcc);
		/* Enhanced Critical Updates Information */
		if (include_bpcc && hostapd_is_uhr_enabled(hapd) &&
		    !skip_uhr_extn && is_uhr_sta)
			wpabuf_put_u8(buf, hostapd_get_ecu_info(&link_bss->rx_ecu_param));

		if (!link->resp_sta_profile)
			continue;

		/* Fragment the sub element if needed */
		if (total_len <= 255) {
			wpabuf_put_data(buf, link->resp_sta_profile,
					link->resp_sta_profile_len);
		} else {
			ptr = link->resp_sta_profile;
			len = link->resp_sta_profile_len;

			slice_len = 255 - sta_info_len;

			wpabuf_put_data(buf, ptr, slice_len);
			len -= slice_len;
			ptr += slice_len;

			while (len) {
				if (len <= 255)
					slice_len = len;
				else
					slice_len = 255;

				wpabuf_put_u8(buf,
					      MULTI_LINK_SUB_ELEM_ID_FRAGMENT);
				wpabuf_put_u8(buf, slice_len);
				wpabuf_put_data(buf, ptr, slice_len);

				len -= slice_len;
				ptr += slice_len;
			}
		}
	}

out:
	/* Fragment the Multi-Link element, if needed */
	pos = hostapd_fragment_multi_link_element(buf, pos);
	wpabuf_free(buf);
	return pos;
}


size_t hostapd_eid_eht_basic_ml_len(struct hostapd_data *hapd,
				    struct sta_info *info,
				    bool include_mld_id, bool include_bpcc,
				    u8 include_ext_cap, bool is_uhr_sta,
				    bool include_bss_load_age, bool skip_uhr_extn)
{
	int link_id;
	size_t len, num_frags;

	if (!hapd->conf->mld_ap)
		return 0;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return 0;
#endif /* CONFIG_QCN_EXTN */

	/* Include WLAN_EID_EXT_MULTI_LINK (1) */
	len = 1;
	/* control field */
	len += 2;
	/* Common info len for Basic MLE */
	len += EHT_ML_COMMON_INFO_LEN;
	if (include_mld_id)
		len++;

	if (include_ext_cap)
		len += 2;

	if (hostapd_is_uhr_enabled(hapd) && is_uhr_sta && !skip_uhr_extn) {
		/* Enhanced Critical Updates Information */
		len++;
		if (include_bss_load_age && hapd->conf->bss_load_update_period)
			len++; /* Age of BSS Load */
	}

	if (!info)
		goto out;

	/* Add link info for the other links */
	for (link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		struct mld_link_info *link = &info->mld_info.links[link_id];
		struct hostapd_data *link_bss;
		size_t sta_prof_len = EHT_ML_STA_INFO_LEN +
			link->resp_sta_profile_len;

		/* Skip the local one */
		if (link_id == hapd->mld_link_id || !link->valid)
			continue;

		link_bss = hostapd_mld_get_link_bss(hapd, link_id);
		if (!link_bss) {
			wpa_printf(MSG_ERROR,
				   "MLD: Couldn't find link BSS - skip it");
			continue;
		}

#ifdef CONFIG_QCN_EXTN
		if (hostapd_is_repurpose_disabled_11be_extn(link_bss->conf)) {
			wpa_printf(MSG_ERROR,
				   "MLD: repurposed link can't have ML elem");
			continue;
		}
#endif /* CONFIG_QCN_EXTN */

		/* BSS Parameters Change Count (1) for (Re)Association Response
		 * frames */
		if (include_bpcc)
			sta_prof_len++;

		/* Enhanced Critical Updates Information */
		if (include_bpcc && hostapd_is_uhr_enabled(hapd) &&
		    !skip_uhr_extn && is_uhr_sta)
			sta_prof_len++;

		/* Per-STA Profile Subelement(1), Length (1) */
		len += 2;
		len += sta_prof_len;
		/* Consider Fragment EID(1) and Length (1) for each subelement
		 * fragment. */
		if (sta_prof_len > 255) {
			num_frags = (sta_prof_len / 255 - 1) +
				!!(sta_prof_len % 255);
			len += num_frags * 2;
		}

	}

out:
	if (len > 255) {
		num_frags = (len / 255 - 1) + !!(len % 255);
		len += num_frags * 2;
	}

	/* WLAN_EID_EXTENSION (1) + length (1) */
	return len + 2;
}


size_t hostapd_eid_eht_ml_len(struct hostapd_data *hapd, struct mld_info *info,
			      bool include_mld_id, bool include_bpcc,
			      u8 include_ext_cap, bool is_uhr_sta,
			      bool include_bss_load_age, bool skip_uhr_extn)
{
	size_t len = 0;
	size_t eht_ml_len = 2 + EHT_ML_COMMON_INFO_LEN;
	u8 link_id;

	if (include_mld_id)
		eht_ml_len++;

	if (include_ext_cap)
		eht_ml_len += 2;

	if (hostapd_is_uhr_enabled(hapd) && is_uhr_sta && !skip_uhr_extn) {
		/* Enhanced Critical Updates Information (1) in common info */
		eht_ml_len++;
		if (include_bss_load_age && hapd->conf->bss_load_update_period)
			eht_ml_len++; /* Age of BSS Load */
	}

	for (link_id = 0; info && link_id < ARRAY_SIZE(info->links);
	     link_id++) {
		struct mld_link_info *link;
		size_t sta_len = EHT_ML_STA_INFO_LEN;

		link = &info->links[link_id];
		if (!link->valid)
			continue;

		sta_len += link->resp_sta_profile_len;

		/* BSS Parameters Change Count (1) for (Re)Association Response
		 * frames */
		if (include_bpcc)
			sta_len++;

		/* Enhanced Critical Updates Information (1) in per-STA profile */
		if (include_bpcc && hostapd_is_uhr_enabled(hapd) &&
		    !skip_uhr_extn && is_uhr_sta)
			sta_len++;

		/* Element data and (fragmentation) headers */
		eht_ml_len += sta_len;
		eht_ml_len += 2 + (sta_len > 255 ? ((sta_len - 1) / 255) * 2 : 0);
	}

	/* Element data */
	len += eht_ml_len;

	/* First header (254 bytes of data) */
	len += 3;

	if (eht_ml_len > 254)
		len += (eht_ml_len / 255) * 2;

	return len;
}
#undef EHT_ML_COMMON_INFO_LEN
#undef EHT_ML_STA_INFO_LEN


u8 * hostapd_eid_eht_ml_beacon(struct hostapd_data *hapd,
			       struct mld_info *info,
			       u8 *eid, bool include_mld_id,
			       u8 include_ext_cap, bool is_uhr_sta,
			       bool include_bss_load_age, bool skip_uhr_extn)
{
	eid = hostapd_eid_eht_basic_ml_common(hapd, eid, info, include_mld_id,
					      false, include_ext_cap, false,
					      is_uhr_sta, include_bss_load_age,
					      skip_uhr_extn);

	if (hapd->iface->drv_flags2 & WPA_DRIVER_FLAG2_MLD_LINK_REMOVAL_OFFLOAD)
		return eid;
	else
		return hostapd_eid_eht_reconf_ml(hapd, eid);
}


u8 * hostapd_eid_eht_ml_assoc(struct hostapd_data *hapd, struct sta_info *info,
			      u8 *eid, u8 include_ext_cap)
{
	bool is_uhr_sta;

	if (!ap_sta_is_mld(hapd, info))
		return eid;

	is_uhr_sta = !!(info->flags & WLAN_STA_UHR);

	eid = hostapd_eid_eht_basic_ml_common(hapd, eid, &info->mld_info,
					      false, true, include_ext_cap, false,
					      is_uhr_sta, false, false);
	ap_sta_free_sta_profile(&info->mld_info);
	return eid;
}


size_t hostapd_eid_eht_ml_beacon_len(struct hostapd_data *hapd,
				     struct mld_info *info,
				     bool include_mld_id,
				     u8 include_ext_cap, bool is_uhr_sta,
				     bool include_bss_load_age, bool skip_uhr_extn)
{
	return hostapd_eid_eht_ml_len(hapd, info, include_mld_id, false,
				       include_ext_cap, is_uhr_sta,
				       include_bss_load_age, skip_uhr_extn);
}


struct wpabuf * hostapd_ml_auth_resp(struct hostapd_data *hapd)
{
	struct wpabuf *buf = wpabuf_alloc(12);

	if (!buf)
		return NULL;

	wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
	wpabuf_put_u8(buf, 10);
	wpabuf_put_u8(buf, WLAN_EID_EXT_MULTI_LINK);
	wpabuf_put_le16(buf, MULTI_LINK_CONTROL_TYPE_BASIC);
	wpabuf_put_u8(buf, ETH_ALEN + 1);
	wpabuf_put_data(buf, hapd->mld->mld_addr, ETH_ALEN);

	return buf;
}


#ifdef CONFIG_SAE

static const u8 *
sae_commit_skip_fixed_fields(const struct ieee80211_mgmt *mgmt, size_t len,
			     const u8 *pos, u16 status_code)
{
	u16 group;
	size_t prime_len;
	struct crypto_ec *ec;

	if (status_code != WLAN_STATUS_SAE_HASH_TO_ELEMENT)
		return pos;

	/* SAE H2E commit message (group, scalar, FFE) */
	if (len < 2) {
		wpa_printf(MSG_DEBUG,
			   "EHT: SAE Group is not present");
		return NULL;
	}

	group = WPA_GET_LE16(pos);
	pos += 2;

	/* TODO: How to parse when the group is unknown? */
	ec = crypto_ec_init(group);
	if (!ec) {
		const struct dh_group *dh = dh_groups_get(group);

		if (!dh) {
			wpa_printf(MSG_DEBUG, "EHT: Unknown SAE group %u",
				   group);
			return NULL;
		}

		prime_len = dh->prime_len;
	} else {
		prime_len = crypto_ec_prime_len(ec);
	}

	wpa_printf(MSG_DEBUG, "EHT: SAE scalar length is %zu", prime_len);

	if (len - 2 < prime_len * (ec ? 3 : 2))
		goto truncated;
	/* scalar */
	pos += prime_len;

	if (ec) {
		pos += prime_len * 2;
		crypto_ec_deinit(ec);
	} else {
		pos += prime_len;
	}

	if (pos - mgmt->u.auth.variable > (int) len) {
	truncated:
		wpa_printf(MSG_DEBUG,
			   "EHT: Too short SAE commit Authentication frame");
		return NULL;
	}

	wpa_hexdump(MSG_DEBUG, "EHT: SAE: Authentication frame elements",
		    pos, (int) len - (pos - mgmt->u.auth.variable));

	return pos;
}

#define GROUP_19_CONFIRM_FIXED_FIELDS_LEN  32
#define GROUP_20_CONFIRM_FIXED_FIELDS_LEN  48
#define GROUP_21_CONFIRM_FIXED_FIELDS_LEN  64

static bool sae_confirm_ies_valid(const u8 *pos, size_t len, bool *has_mle)
{
	struct ieee802_11_elems elems;

	if (has_mle)
		*has_mle = false;

	if (!len)
		return true;

	if (ieee802_11_parse_elems(pos, (int) len, &elems, 0) == ParseFailed)
		return false;

	if (has_mle && elems.basic_mle && elems.basic_mle_len)
		*has_mle = true;

	return true;
}

static const u8 *
sae_confirm_skip_fixed_fields(struct hostapd_data *hapd,
			      const struct ieee80211_mgmt *mgmt, size_t len,
			      const u8 *pos, u16 status_code)
{
	static const size_t kck_lens[] = {
		GROUP_19_CONFIRM_FIXED_FIELDS_LEN,
		GROUP_20_CONFIRM_FIXED_FIELDS_LEN,
		GROUP_21_CONFIRM_FIXED_FIELDS_LEN
	};
	const u8 *best = NULL;
	size_t rem;
	size_t i;

	if (status_code == WLAN_STATUS_REJECTED_WITH_SUGGESTED_BSS_TRANSITION)
		return pos;

	/* send confirm integer */
	if (len < 2)
		goto truncated;
	pos += 2;
	len -= 2;
	rem = len;

	for (i = 0; i < ARRAY_SIZE(kck_lens); i++) {
		const u8 *ie_pos;
		size_t ie_len;
		bool has_mle = false;

		if (rem < kck_lens[i])
			continue;

		ie_pos = pos + kck_lens[i];
		ie_len = rem - kck_lens[i];

		if (!sae_confirm_ies_valid(ie_pos, ie_len, &has_mle))
			continue;

		if (hapd && hapd->conf && hapd->conf->mld_ap && has_mle)
			return ie_pos;

		if (!best)
			best = ie_pos;
	}

	if (!best)
		return NULL;

	pos = best;

	if (pos - mgmt->u.auth.variable > (int) len) {
	truncated:
		wpa_printf(MSG_DEBUG,
			   "EHT: Too short SAE confirm Authentication frame");
		return NULL;
	}

	return pos;
}

#endif /* CONFIG_SAE */


const u8 * auth_skip_fixed_fields(struct hostapd_data *hapd,
					 const struct ieee80211_mgmt *mgmt,
					 size_t len)
{
	u16 auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
#if defined(CONFIG_SAE) || defined(CONFIG_IEEE8021X_AUTH)
	u16 auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);
	u16 status_code = le_to_host16(mgmt->u.auth.status_code);
#endif /* CONFIG_SAE or CONFIG_IEEE8021X_AUTH */
	const u8 *pos = mgmt->u.auth.variable;

	/* Skip fixed fields as based on IEEE Std 802.11-2024, Table 9-71
	 * (Presence of fields and elements in Authentications frames) */
	switch (auth_alg) {
	case WLAN_AUTH_OPEN:
	case WLAN_AUTH_EPPKE:
	case WLAN_AUTH_FT:
		return pos;
#ifdef CONFIG_IEEE8021X_AUTH
	case WLAN_AUTH_802_1X: {
		u16 encap_len;

		/* Extract 2-octet Encapsulation Length from variable field */
		if (len < 2)
			return NULL;

		encap_len = WPA_GET_LE16(pos);
		pos += 2;
		len -= 2;

		/* Skip Encapsulation field if present */
		if (encap_len > 0) {
			if (len < encap_len)
				return NULL;
			pos += encap_len;
		}

		return pos;
	}
#endif /* CONFIG_IEEE8021X_AUTH */
#ifdef CONFIG_SAE
	case WLAN_AUTH_SAE:
		if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT) {
			if (status_code == WLAN_STATUS_SUCCESS) {
				wpa_printf(MSG_DEBUG,
					   "EHT: SAE H2E is mandatory for MLD");
				goto out;
			}

			return sae_commit_skip_fixed_fields(mgmt, len, pos,
							    status_code);
		} else if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_CONFIRM) {
			return sae_confirm_skip_fixed_fields(hapd, mgmt, len,
							     pos, status_code);
		}

		return pos;
#endif /* CONFIG_SAE */
	/* TODO: Support additional algorithms that can be used for MLO */
	case WLAN_AUTH_FILS_SK:
	case WLAN_AUTH_FILS_SK_PFS:
	case WLAN_AUTH_FILS_PK:
	case WLAN_AUTH_PASN:
	default:
		break;
	}

#ifdef CONFIG_SAE
out:
#endif /* CONFIG_SAE */
	wpa_printf(MSG_DEBUG,
		   "TODO: Authentication algorithm %u not supported with MLD",
		   auth_alg);
	return NULL;
}


const u8 * hostapd_process_ml_auth(struct hostapd_data *hapd,
				   const struct ieee80211_mgmt *mgmt,
				   size_t len)
{
	struct ieee802_11_elems elems;
	const u8 *pos;

	if (!hapd->conf->mld_ap)
		return NULL;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return NULL;
#endif /* CONFIG_QCN_EXTN */

	len -= offsetof(struct ieee80211_mgmt, u.auth.variable);

	pos = auth_skip_fixed_fields(hapd, mgmt, len);
	if (!pos)
		return NULL;

	if (ieee802_11_parse_elems(pos,
				   (int)len - (pos - mgmt->u.auth.variable),
				   &elems, 1) == ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Failed parsing Authentication frame");
	}

	if (!elems.basic_mle || !elems.basic_mle_len)
		return NULL;

	return get_basic_mle_mld_addr(elems.basic_mle, elems.basic_mle_len);
}

const u8 * skip_ml_auth_fixed_fields(struct hostapd_data *hapd,
                                   const struct ieee80211_mgmt *mgmt,
                                   size_t len)
{
	const u8 *pos;

	len -= offsetof(struct ieee80211_mgmt, u.auth.variable);

	pos = auth_skip_fixed_fields(hapd, mgmt, len);

	if (!pos)
		return NULL;

	return pos;
}



static int hostapd_mld_validate_assoc_info(struct hostapd_data *hapd,
					   struct sta_info *sta)
{
	u8 link_id;
	struct mld_info *info = &sta->mld_info;

	if (!ap_sta_is_mld(hapd, sta)) {
		wpa_printf(MSG_DEBUG, "MLD: Not a non-AP MLD");
		return 0;
	}

	/*
	 * Iterate over the links negotiated in the (Re)Association Request
	 * frame and validate that they are indeed valid links in the local AP
	 * MLD.
	 *
	 * While at it, also update the local address for the links in the
	 * mld_info, so it could be easily available for later flows, e.g., for
	 * the RSN Authenticator, etc.
	 */
	for (link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		struct hostapd_data *other_hapd;

		if (!info->links[link_id].valid || link_id == hapd->mld_link_id)
			continue;

		other_hapd = hostapd_mld_get_link_bss(hapd, link_id);
		if (!other_hapd) {
			/**
			 * STA requested a link ID that does not exist in this AP MLD.
			 * Per IEEE 802.11be, do not fail the entire association; instead
			 * allow association to proceed and reject this link explicitly in
			 * per-link response handling.
			 */
			wpa_printf(MSG_DEBUG, "MLD: Unsupported link ID=%u; will reject per-link",
				   link_id);
			continue;
		}

#ifdef CONFIG_QCN_EXTN
		if (hostapd_is_repurpose_disabled_11be_extn(other_hapd->conf)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: repurposed link=%d not allowed in ml assoc",
				   link_id);
			return -1;
		}
#endif /* CONFIG_QCN_EXTN */

		os_memcpy(info->links[link_id].local_addr, other_hapd->own_addr,
			  ETH_ALEN);
	}

	return 0;
}


int hostapd_process_ml_assoc_req_addr(struct hostapd_data *hapd,
				      const u8 *basic_mle, size_t basic_mle_len,
				      u8 *mld_addr)
{
	struct wpabuf *mlbuf = ieee802_11_defrag(basic_mle, basic_mle_len,
						 true);
	struct ieee80211_eht_ml *ml;
	struct eht_ml_basic_common_info *common_info;
	size_t ml_len, common_info_len;
	int ret = -1;
	u16 ml_control;

	if (!mlbuf)
		return ret;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
		wpa_printf(MSG_DEBUG, "MLD: ml assoc req on non-ml BSS");
		goto out;
	}
#endif /* CONFIG_QCN_EXTN */

	ml = (struct ieee80211_eht_ml *) wpabuf_head(mlbuf);
	ml_len = wpabuf_len(mlbuf);

	if (ml_len < sizeof(*ml))
		goto out;

	ml_control = le_to_host16(ml->ml_control);
	if ((ml_control & MULTI_LINK_CONTROL_TYPE_MASK) !=
	    MULTI_LINK_CONTROL_TYPE_BASIC) {
		wpa_printf(MSG_DEBUG, "MLD: Invalid ML type=%u",
			   ml_control & MULTI_LINK_CONTROL_TYPE_MASK);
		goto out;
	}

	/* Common Info Length and MLD MAC Address must always be present */
	common_info_len = 1 + ETH_ALEN;
	/* Ignore optional fields */

	if (sizeof(*ml) + common_info_len > ml_len) {
		wpa_printf(MSG_DEBUG, "MLD: Not enough bytes for common info");
		goto out;
	}

	common_info = (struct eht_ml_basic_common_info *) ml->variable;

	/* Common information length includes the length octet */
	if (common_info->len < common_info_len) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Invalid common info len=%u", common_info->len);
		goto out;
	}

	/* Get the MLD MAC Address */
	os_memcpy(mld_addr, common_info->mld_addr, ETH_ALEN);
	ret = 0;

out:
	wpabuf_free(mlbuf);
	return ret;
}


u16 hostapd_process_ml_assoc_req(struct hostapd_data *hapd,
				 struct ieee802_11_elems *elems,
				 struct sta_info *sta)
{
	struct wpabuf *mlbuf;
	const struct ieee80211_eht_ml *ml;
	const struct eht_ml_basic_common_info *common_info;
	size_t ml_len, common_info_len;
	struct mld_link_info *link_info;
	struct mld_info *info = &sta->mld_info;
	const u8 *pos, *end;
	int ret = -1;
	u16 ml_control;
	const u8 *ml_end;
	u8 saved_peer_addr[ETH_ALEN];

	mlbuf = ieee802_11_defrag(elems->basic_mle, elems->basic_mle_len, true);
	if (!mlbuf)
		return WLAN_STATUS_SUCCESS;


	ml = wpabuf_head(mlbuf);
	ml_len = wpabuf_len(mlbuf);
	ml_end = ((const u8 *) ml) + ml_len;

	wpa_hexdump(MSG_DEBUG, "UHR: BMLE", (const u8 *) ml, ml_len);

	ml_control = le_to_host16(ml->ml_control);

	if ((ml_control & MULTI_LINK_CONTROL_TYPE_MASK) !=
	    MULTI_LINK_CONTROL_TYPE_BASIC) {
		wpa_printf(MSG_DEBUG, "MLD: Invalid ML type=%u",
			   ml_control & MULTI_LINK_CONTROL_TYPE_MASK);
		goto out;
	}

	/* Common Info length and MLD MAC address must always be present */
	common_info_len = 1 + ETH_ALEN;

	if (ml_control & BASIC_MULTI_LINK_CTRL_PRES_LINK_ID) {
		wpa_printf(MSG_DEBUG, "MLD: Link ID info not expected");
		goto out;
	}

	if (ml_control & BASIC_MULTI_LINK_CTRL_PRES_BSS_PARAM_CH_COUNT) {
		wpa_printf(MSG_DEBUG, "MLD: BSS params change not expected");
		goto out;
	}

	if (ml_control & BASIC_MULTI_LINK_CTRL_PRES_MSD_INFO) {
		wpa_printf(MSG_DEBUG, "MLD: Sync delay not expected");
		goto out;
	}

	if (ml_control & BASIC_MULTI_LINK_CTRL_PRES_EML_CAPA) {
		common_info_len += 2;
	} else {
		wpa_printf(MSG_DEBUG, "MLD: EML capabilities not present");
	}

	if (ml_control & BASIC_MULTI_LINK_CTRL_PRES_MLD_CAPA) {
		common_info_len += 2;

	} else {
		wpa_printf(MSG_DEBUG, "MLD: MLD capabilities not present");
		goto out;
	}

	if (ml_control & BASIC_MULTI_LINK_CTRL_PRES_EXT_MLD_CAP) {
		common_info_len += 2;
	} else {
		wpa_printf(MSG_DEBUG, "MLD: EXT ML capabilities not present");
	}

	wpa_printf(MSG_DEBUG, "MLD: expected_common_info_len=%zu",
		   common_info_len);

	if (sizeof(*ml) + common_info_len > ml_len) {
		wpa_printf(MSG_DEBUG, "MLD: Not enough bytes for common info");
		goto out;
	}

	common_info = (const struct eht_ml_basic_common_info *) ml->variable;

	/* Common information length includes the length octet */
	if (common_info->len < common_info_len) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Invalid common info len=%u (expected %zu)",
			   common_info->len, common_info_len);
		goto out;
	}

	pos = common_info->variable;
	end = ((const u8 *) common_info) + common_info->len;

	if (ml_control & BASIC_MULTI_LINK_CTRL_PRES_EML_CAPA) {
		info->common_info.eml_capa = WPA_GET_LE16(pos);
		pos += 2;
	} else {
		info->common_info.eml_capa = 0;
	}

	info->common_info.mld_capa = WPA_GET_LE16(pos);
	pos += 2;

	if (ml_control & BASIC_MULTI_LINK_CTRL_PRES_EXT_MLD_CAP) {
		pos += 2;
	}

	wpa_printf(MSG_DEBUG, "MLD: addr=" MACSTR ", eml=0x%x, mld=0x%x",
		   MAC2STR(info->common_info.mld_addr),
		   info->common_info.eml_capa, info->common_info.mld_capa);
	/* Check the MLD MAC Address */
	if (!ether_addr_equal(info->common_info.mld_addr,
			      common_info->mld_addr)) {
		wpa_printf(MSG_DEBUG,
			   "MLD: MLD address mismatch between authentication ("
			   MACSTR ") and association (" MACSTR ")",
			   MAC2STR(info->common_info.mld_addr),
			   MAC2STR(common_info->mld_addr));
		goto out;
	}
	/*
	 * Reset all per-link state before parsing the new ML IE. A STA may
	 * send back-to-back reassoc-reqs with different ML IEs (e.g., fewer
	 * links or without NSTR). The valid-flag-only reset left stale fields
	 * (nstr_bitmap_len, peer_addr, nstr_bitmap, capability) in non-assoc
	 * link slots and leaked resp_sta_profile heap allocations. Free those
	 * allocations and zero all link slots. The assoc link's peer_addr
	 * holds the link-specific MAC (set from mgmt->sa at auth time, before
	 * driver address translation) and must be saved and restored.
	 */
	os_memcpy(saved_peer_addr,
		  info->links[hapd->mld_link_id].peer_addr, ETH_ALEN);
	ap_sta_free_sta_profile(info);
	os_memset(info->links, 0, sizeof(info->links));
	info->links[hapd->mld_link_id].valid = 1;
	os_memcpy(info->links[hapd->mld_link_id].local_addr,
		  hapd->own_addr, ETH_ALEN);
	os_memcpy(info->links[hapd->mld_link_id].peer_addr,
		  saved_peer_addr, ETH_ALEN);

	/* Parse the Link Info field that starts after the end of the variable
	 * length Common Info field. */
	pos = end;
	while (ml_end - pos > 2) {
		size_t sub_elem_len, sta_info_len;
		u16 control;
		const u8 *sub_elem_end;
		int num_frag_subelems;
		u8 link_id;

		num_frag_subelems =
			ieee802_11_defrag_mle_subelem(mlbuf, pos,
						      &sub_elem_len);
		if (num_frag_subelems < 0) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Failed to parse MLE subelem");
			goto out;
		}

		ml_len -= num_frag_subelems * 2;
		ml_end = ((const u8 *) ml) + ml_len;

		wpa_printf(MSG_DEBUG,
			   "MLD: sub element len=%zu, Fragment subelems=%u",
			   sub_elem_len, num_frag_subelems);

		if (2 + sub_elem_len > (size_t) (ml_end - pos)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Invalid link info len: %zu %zu",
				   2 + sub_elem_len, ml_end - pos);
			goto out;
		}

		if (*pos == MULTI_LINK_SUB_ELEM_ID_VENDOR) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Skip vendor specific subelement");

			pos += 2 + sub_elem_len;
			continue;
		}

		if (*pos != MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Skip unknown Multi-Link element subelement ID=%u",
				   *pos);
			pos += 2 + sub_elem_len;
			continue;
		}

		/* Skip the subelement ID and the length */
		pos += 2;
		sub_elem_end = pos + sub_elem_len;

		/* Get the station control field */
		if (sub_elem_end - pos < 2) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Too short Per-STA Profile subelement");
			goto out;
		}
		control = WPA_GET_LE16(pos);
		link_id = control & BASIC_MLE_STA_CTRL_LINK_ID_MASK;
		if (link_id >= MAX_NUM_MLD_LINKS) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Invalid Link ID in Per-STA Profile subelement");
			goto out;
		}
		link_info = &info->links[link_id];
		pos += 2;

		if (!(control & BASIC_MLE_STA_CTRL_COMPLETE_PROFILE)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Per-STA complete profile expected");
			goto out;
		}

		if (!(control & BASIC_MLE_STA_CTRL_PRES_STA_MAC)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Per-STA MAC address not present");
			goto out;
		}

		if ((control & (BASIC_MLE_STA_CTRL_PRES_BEACON_INT |
				BASIC_MLE_STA_CTRL_PRES_DTIM_INFO))) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Beacon/DTIM interval not expected");
			goto out;
		}

		/* The length octet and the MAC address must be present */
		sta_info_len = 1 + ETH_ALEN;

		if (control & BASIC_MLE_STA_CTRL_PRES_NSTR_LINK_PAIR) {
			if (control & BASIC_MLE_STA_CTRL_NSTR_BITMAP)
				link_info->nstr_bitmap_len = 2;
			else
				link_info->nstr_bitmap_len = 1;
		}

		sta_info_len += link_info->nstr_bitmap_len;

		if (sta_info_len > (size_t) (sub_elem_end - pos) ||
		    sta_info_len > *pos ||
		    *pos > sub_elem_end - pos ||
		    sta_info_len > (size_t) (sub_elem_end - pos)) {
			wpa_printf(MSG_DEBUG, "MLD: Invalid STA Info length");
			goto out;
		}

		sta_info_len = *pos;
		end = pos + sta_info_len;

		/* skip the length */
		pos++;

		/* get the link address */
		os_memcpy(link_info->peer_addr, pos, ETH_ALEN);
		wpa_printf(MSG_DEBUG,
			   "MLD: assoc: link id=%u, addr=" MACSTR,
			   control & BASIC_MLE_STA_CTRL_LINK_ID_MASK,
			   MAC2STR(link_info->peer_addr));

		pos += ETH_ALEN;

		/* Get the NSTR bitmap */
		if (link_info->nstr_bitmap_len) {
			os_memcpy(link_info->nstr_bitmap, pos,
				  link_info->nstr_bitmap_len);
			pos += link_info->nstr_bitmap_len;
		}

		pos = end;

		if (sub_elem_end - pos >= 2)
			link_info->capability = WPA_GET_LE16(pos);

		pos = sub_elem_end;

		wpa_printf(MSG_DEBUG, "MLD: link ctrl=0x%x, " MACSTR
			   ", nstr bitmap len=%u",
			   control, MAC2STR(link_info->peer_addr),
			   link_info->nstr_bitmap_len);

		link_info->valid = true;
	}

	ret = hostapd_mld_validate_assoc_info(hapd, sta);
out:
	wpabuf_free(mlbuf);
	if (ret) {
		ap_sta_free_sta_profile(info);
		os_memset(info, 0, sizeof(*info));
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	return WLAN_STATUS_SUCCESS;
}

#ifdef CONFIG_IEEE80211BE
void
hostapd_free_reconf_sta_add_params(struct hostapd_sta_add_params *params)
{
	if (!params)
		return;

	os_free((void *)params->eht_capab);
	os_free((void *)params->he_6ghz_capab);
	os_free((void *)params->he_capab);
	os_free((void *)params->vht_capabilities);
	os_free((void *)params->ht_capabilities);
	os_free(params);
}
#endif

void ml_deinit_link_reconf_req(struct link_reconf_req_list **req_list_ptr)
{
	struct link_reconf_req_list *req_list;
	struct link_reconf_req_info *info, *tmp;

	if (!(*req_list_ptr))
		return;

	wpa_printf(MSG_DEBUG, "MLD: Deinit Link Reconf Request context");

	req_list = *req_list_ptr;

	if (dl_list_empty(&req_list->add_req)) {
		wpa_printf(MSG_DEBUG, "MLD: No Link Reconf additions");
		return;
	}

	dl_list_for_each_safe(info, tmp, &req_list->add_req,
			      struct link_reconf_req_info, list) {
		dl_list_del(&info->list);
		os_free(info);
	}

	dl_list_for_each_safe(info, tmp, &req_list->del_req,
			      struct link_reconf_req_info, list) {
		dl_list_del(&info->list);
		os_free(info);
	}

	os_free(req_list);
	*req_list_ptr = NULL;
}


void hostapd_link_reconf_resp_tx_status(struct hostapd_data *hapd,
					struct sta_info *sta,
					const struct ieee80211_mgmt *mgmt,
					size_t len, int ok)
{
	u8 dialog_token = mgmt->u.action.u.link_reconf_resp.dialog_token;
	struct hostapd_data *assoc_hapd, *lhapd, *other_hapd;
	struct sta_info *assoc_sta, *lsta, *other_sta;
	struct link_reconf_req_list *req_list;
	struct link_reconf_req_info *info;
	uint8_t link_id;

	wpa_printf(MSG_DEBUG,
		   "MLD: Link Reconf Response TX status - dialog token=%u ok=%d",
		   dialog_token, ok);

	assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
	if (!assoc_sta) {
		wpa_printf(MSG_INFO, "MLD: Assoc STA not found for " MACSTR,
			   MAC2STR(mgmt->da));
		return;
	}

	if (!assoc_sta->reconf_req) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Unexpected Link Reconf Request TX status");
		return;
	}

	req_list = assoc_sta->reconf_req;

	if (!ether_addr_equal(mgmt->da, req_list->sta_mld_addr)) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Link Reconfiguration Response TX status from wrong STA");
		return;
	}

	if (dialog_token != req_list->dialog_token) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Link Reconfiguration session expired for %u",
			   dialog_token);
		return;
	}

	if (!ok) {
		wpa_printf(MSG_INFO,
			   "MLD: Link Reconf Response ack failed for " MACSTR
			   "; revert link additions",
			   MAC2STR(mgmt->da));

		dl_list_for_each(info, &req_list->add_req,
				 struct link_reconf_req_info, list) {
			if (info->status != WLAN_STATUS_SUCCESS)
				continue;

			lhapd = NULL;
			lsta = NULL;
			lhapd = hostapd_mld_get_link_bss(hapd, info->link_id);
			if (lhapd)
				lsta = ap_get_sta(lhapd,
						  req_list->sta_mld_addr);

#ifdef CONFIG_QCN_EXTN
			if (lhapd &&
			    hostapd_is_repurpose_disabled_11be_extn(lhapd->conf))
				lsta = NULL;
#endif /* CONFIG_QCN_EXTN */

			if (lsta)
				ap_free_sta_link(lhapd, lsta);
		}
		goto exit;
	}

	if (dl_list_empty(&req_list->del_req))
		goto exit;

	dl_list_for_each(info, &req_list->del_req, struct link_reconf_req_info,
			 list) {
		if (info->status != WLAN_STATUS_SUCCESS)
			continue;

		link_id = info->link_id;
		lhapd = hostapd_mld_get_link_bss(hapd, link_id);
		if (!lhapd) {
			wpa_printf(MSG_INFO,
				   "MLD: Link (%u) hapd cannot be NULL",
				   link_id);
			continue;
		}

#ifdef CONFIG_QCN_EXTN
		if (hostapd_is_repurpose_disabled_11be_extn(lhapd->conf)) {
			wpa_printf(MSG_INFO,
				   "MLD: Link (%u) hapd repurposed", link_id);
			continue;
		}
#endif /* CONFIG_QCN_EXTN */

		lsta = ap_get_sta(lhapd, mgmt->da);
		if (!lsta) {
			wpa_printf(MSG_INFO,
				   "MLD: Link (%u) STA cannot be NULL",
				   link_id);
			continue;
		}

		/* Reassign assoc_sta to the link with lowest link ID */
		if (!hostapd_sta_is_link_sta(lhapd, lsta) &&
		    lsta == assoc_sta) {
			struct mld_info *mld_info = &assoc_sta->mld_info;
			int i;

			for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
				if (i == assoc_sta->mld_assoc_link_id ||
				    !mld_info->links[i].valid ||
				    req_list->links_del_ok & BIT(i)) {
					continue;
				}
				break;
			}

			if (i == MAX_NUM_MLD_LINKS) {
				wpa_printf(MSG_INFO,
					   "MLD: No new assoc STA could be found; disconnect STA");
				hostapd_notif_disassoc_mld(assoc_hapd, sta,
							   sta->addr);
				goto exit;
			}
			wpa_printf(MSG_DEBUG, "MLD: New assoc link=%d", i);

			/* Reset wpa_auth and assoc link ID */
			for_each_mld_link(other_hapd, lhapd) {
				other_sta = ap_get_sta(other_hapd, mgmt->da);
				if (other_sta)
					other_sta->mld_assoc_link_id = i;
			}

			/* Reset reconfig request queue which will be freed
			 * at the end */
			assoc_sta->reconf_req = NULL;

			/* assoc_sta switched */
			assoc_sta = hostapd_ml_get_assoc_sta(lhapd, lsta,
							     &assoc_hapd);

			/* assoc_sta cannot be NULL since both AP and STA are
			 * MLD and new valid assoc_sta is already found */
			if (!assoc_sta)
				goto exit;

			if (assoc_hapd == lhapd) {
				wpa_printf(MSG_ERROR,
					   "MLD: assoc_hapd is not updated; please check");
				goto exit;
			}

			assoc_sta->reconf_req = req_list;
			wpa_reset_assoc_sm_info(assoc_sta->wpa_sm,
						assoc_hapd->wpa_auth, i);
		}

		/* Free as a link STA */
		wpa_msg(hapd->msg_ctx, MSG_INFO,
			WPA_EVENT_LINK_STA_REMOVED "sta=" MACSTR " link_id=%u",
			MAC2STR(lsta->addr), link_id);
		ap_free_sta_link(lhapd, lsta);

		for_each_mld_link(other_hapd, lhapd) {
			struct mld_link_info *link;

			other_sta = ap_get_sta(other_hapd, mgmt->da);
			if (!other_sta)
				continue;

			link = &other_sta->mld_info.links[link_id];
			os_free(link->resp_sta_profile);
			link->resp_sta_profile = NULL;
			link->resp_sta_profile_len = 0;
			link->valid = false;
		}
		wpa_auth_set_ml_info(assoc_sta->wpa_sm,
				     assoc_sta->mld_assoc_link_id,
				     &assoc_sta->mld_info);
	}

exit:
	ml_deinit_link_reconf_req(&req_list);
	if (assoc_sta && assoc_sta->reconf_req)
		assoc_sta->reconf_req = NULL;
}


static bool recover_from_zero_links(u16 *links_del_ok, u8 *recovery_link)
{
	u8 pos = 0;
	u16 del_links;

	del_links = *links_del_ok;

	while (del_links) {
		if (del_links & 1)
			break;
		del_links >>= 1;
		pos++;
	}

	/* No link found */
	if (!del_links) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Total valid links is 0 and no del-link found to reject for recovery");
		return false;
	}

	*recovery_link = pos;
	*links_del_ok &= ~BIT(*recovery_link);
	wpa_printf(MSG_INFO,
		   "MLD: Del-link request for link (%u) rejected to recover from no remaining links",
		   *recovery_link);
	return true;
}


u16
hostapd_ml_process_reconf_link(struct hostapd_data *hapd,
			       struct sta_info *assoc_sta, const u8 *ies,
			       size_t ies_len, u8 link_id, const u8 *link_addr, u8 type)
{
	struct hostapd_data *lhapd, *other_hapd;
	struct mld_link_info link;
	struct sta_info *lsta, *other_sta;
	bool *set_beacon = false;

	lhapd = hostapd_mld_get_link_bss(hapd, link_id);
	if (!lhapd) /* This cannot be NULL */
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(lhapd->conf)) {
		wpa_printf(MSG_ERROR,
			   "link %u is repurposed, hence cant process ml reconf",
			   link_id);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
#endif /* CONFIG_QCN_EXTN */

	os_memset(&link, 0, sizeof(link));

	link.valid = 1;
	os_memcpy(link.local_addr, lhapd->own_addr, ETH_ALEN);
	os_memcpy(link.peer_addr, link_addr, ETH_ALEN);

	/* Parse STA profile, check the IEs, and send ADD_LINK_STA */
	ieee80211_ml_process_link(lhapd, NULL, assoc_sta, &link, ies, ies_len,
				  type, false, set_beacon);

	if (link.status != WLAN_STATUS_SUCCESS)
		return link.status;

	lsta = ap_get_sta(lhapd, assoc_sta->addr);
	if (!lsta)
		return WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;

	for_each_mld_link(other_hapd, lhapd) {
		struct mld_link_info *_link;

		other_sta = ap_get_sta(other_hapd, lsta->addr);
		if (!other_sta)
			continue;

		_link = &other_sta->mld_info.links[link_id];
		_link->valid = true;
		_link->status = WLAN_STATUS_SUCCESS;
		os_memcpy(_link->local_addr, other_hapd->own_addr, ETH_ALEN);
		os_memcpy(_link->peer_addr, link_addr, ETH_ALEN);
	}
	wpa_auth_set_ml_info(lsta->wpa_sm, lsta->mld_assoc_link_id,
			     &lsta->mld_info);

	return WLAN_STATUS_SUCCESS;
}


static int
hostapd_reject_all_reconf_req(struct hostapd_data *hapd, u8 *pos,
			      struct link_reconf_req_list *req_list)
{
	struct link_reconf_req_info *info;
	struct hostapd_data *lhapd;
	struct sta_info *lsta;
	u16 status;
	u8 *buf = pos;

	dl_list_for_each(info, &req_list->add_req, struct link_reconf_req_info,
			 list) {
		lhapd = NULL;
		lsta = NULL;
		*pos++ = info->link_id;
		status = info->status != WLAN_STATUS_SUCCESS ? info->status :
			WLAN_STATUS_UNSPECIFIED_FAILURE;
		WPA_PUT_LE16(pos, status);

		if (info->status == WLAN_STATUS_SUCCESS) {
			lhapd = hostapd_mld_get_link_bss(hapd, info->link_id);
#ifdef CONFIG_QCN_EXTN
			if (lhapd &&
			    !hostapd_is_repurpose_disabled_11be_extn(lhapd->conf))
#else /* CONFIG_QCN_EXTN */
			if (lhapd)
#endif /* CONFIG_QCN_EXTN */
				lsta = ap_get_sta(lhapd,
						  req_list->sta_mld_addr);

			if (lsta)
				ap_free_sta_link(lhapd, lsta);

			info->status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		}
		wpa_printf(MSG_DEBUG, "MLD: Reject add-link=%u with status=%u",
			   info->link_id, status);
		pos += 2;
	}

	dl_list_for_each(info, &req_list->del_req, struct link_reconf_req_info,
			 list) {
		*pos++ = info->link_id;
		status = info->status != WLAN_STATUS_SUCCESS ? info->status :
			WLAN_STATUS_UNSPECIFIED_FAILURE;
		WPA_PUT_LE16(pos, status);

		if (info->status == WLAN_STATUS_SUCCESS)
			info->status = WLAN_STATUS_UNSPECIFIED_FAILURE;

		wpa_printf(MSG_DEBUG, "MLD: Reject del-link=%u with status=%u",
			   info->link_id, status);
		pos += 2;
	}

	return pos - buf;
}


static int
hostapd_send_link_reconf_resp(struct hostapd_data *hapd,
			      struct sta_info *assoc_sta,
			      struct link_reconf_req_list *req_list)
{
	u8 *buf, *orig_pos, *pos;
	struct ieee80211_mgmt *mgmt;
	struct link_reconf_req_info *info;
	struct mld_info mld;
	int ret;
	unsigned int count;
	u8 dialog_token;
	bool reject_all = false;
	size_t len, pos_len, kde_len, mle_len;

	count = dl_list_len(&req_list->add_req) +
		dl_list_len(&req_list->del_req);
	if (!count)
		return 0;

	os_memset(&mld, 0, sizeof(mld));

	dialog_token = req_list->dialog_token;

	/*
	 * Link Reconfiguration Response:
	 *
	 * IEEE80211 Header (24B) +
	 * Category (1B) + Action code (1B) + Dialog Token (1B) +
	 * Count (1B) + Status list (count * 3B) +
	 * Optional: Group Key Data field (variable) +
	 * Optional: OCI element (6B) +
	 * Optional: Basic Multi-Link element (variable)
	 */
	len = IEEE80211_HDRLEN + 3 + 1 + count * 3;
	kde_len = mle_len = 0;
#ifdef CONFIG_IEEE80211BN
	if (hostapd_is_uhr_enabled(hapd))
		len += hostapd_eid_uhr_params_update_len(hapd, true, false);
#endif /* CONFIG_IEEE80211BN */

	if (req_list->links_add_ok) {
		kde_len = wpa_auth_ml_group_kdes_len(
			assoc_sta->wpa_sm, req_list->links_add_ok) + 1;
		len += kde_len;

#ifdef CONFIG_OCV
		if (wpa_auth_uses_ocv(assoc_sta->wpa_sm))
			len += OCV_OCI_EXTENDED_LEN;
#endif /* CONFIG_OCV */

		mld.mld_sta = true;
		dl_list_for_each(info, &req_list->add_req,
				 struct link_reconf_req_info, list) {
			struct mld_link_info *link = &mld.links[info->link_id];
			struct hostapd_data *lhapd = NULL;

			if (info->status != WLAN_STATUS_SUCCESS)
				continue;

			link->status = info->status;

			lhapd = hostapd_mld_get_link_bss(hapd, info->link_id);
			if (!lhapd)
				continue;

#ifdef CONFIG_QCN_EXTN
			if (hostapd_is_repurpose_disabled_11be_extn(lhapd->conf))
				continue;
#endif /* CONFIG_QCN_EXTN */

			link->valid = true;

			os_memcpy(link->local_addr, lhapd->own_addr, ETH_ALEN);
			ieee80211_ml_build_assoc_resp(lhapd, NULL, assoc_sta, link);
		}
		/* TODO: Basic MLE is not supposed to include BPCC in Link
		 * Reconfiguration Response, but mac80211 implementation for
		 * processing this frame requires that to be present. For now,
		 * include that subfield as a workaround. This should be removed
		 * once mac80211 is fixed to match the standard (or this comment
		 * be removed if the standard is modified to match
		 * implementation). */
		mle_len = hostapd_eid_eht_ml_len(hapd, &mld, false, true, 0, false,
						  false, false);
		len += mle_len;
	}

	buf = os_zalloc(len);
	if (!buf) {
		wpa_printf(MSG_INFO,
			   "MLD: Failed to allocate Link Reconf Response buffer (%zu bytes)",
			   len);
		return -1;
	}

	mgmt = (struct ieee80211_mgmt *) buf;
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	os_memcpy(mgmt->da, assoc_sta->addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->mld->mld_addr, ETH_ALEN);

	mgmt->u.action.category = WLAN_ACTION_PROTECTED_EHT;
	mgmt->u.action.u.link_reconf_resp.action =
		WLAN_PROT_EHT_LINK_RECONFIG_RESPONSE;
	mgmt->u.action.u.link_reconf_resp.dialog_token = dialog_token;
	mgmt->u.action.u.link_reconf_resp.count = count;

	orig_pos = pos = mgmt->u.action.u.link_reconf_resp.variable;
	pos_len = 28; /* IEEE80211 Header, category, code, token, count */

	dl_list_for_each(info, &req_list->add_req, struct link_reconf_req_info,
			 list) {
		*pos++ = info->link_id;
		WPA_PUT_LE16(pos, info->status);
		pos += 2;
		pos_len += 3;
	}

	dl_list_for_each(info, &req_list->del_req, struct link_reconf_req_info,
			 list) {
		/* Mark the status as INVALID for rejected link to recover */
		if (!(req_list->links_del_ok & BIT(info->link_id)) &&
		    info->status == WLAN_STATUS_SUCCESS)
			info->status = WLAN_STATUS_UNSPECIFIED_FAILURE;

		*pos++ = info->link_id;
		WPA_PUT_LE16(pos, info->status);
		pos += 2;
		pos_len += 3;
	}

	if (!req_list->links_add_ok)
		goto send_resp;

	/* Key Data for add links */
	if (kde_len) {
		u8 *kde_pos = pos;

		kde_pos = wpa_auth_ml_group_kdes(assoc_sta->wpa_sm, ++kde_pos,
						 req_list->links_add_ok);
		*pos = kde_pos - pos - 1;
		if (kde_len - 1 != *pos) {
			reject_all = true;
			goto reject_all_req;
		}

		wpa_hexdump_key(MSG_DEBUG, "MLD: Group KDE", pos + 1, *pos);

		pos += kde_len;
		pos_len += kde_len;
	}

#ifdef CONFIG_OCV
	/* OCI element for add links */
	if (wpa_auth_uses_ocv(assoc_sta->wpa_sm)) {
		struct wpa_channel_info ci;

		if (hostapd_drv_channel_info(hapd, &ci)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Failed to fetch OCI; reject all requests");
			reject_all = true;
			goto reject_all_req;
		}

		if (ocv_insert_extended_oci(&ci, pos)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Failed to add OCI element; reject all requests");
			reject_all = true;
			goto reject_all_req;
		}

		pos += OCV_OCI_EXTENDED_LEN;
		pos_len += OCV_OCI_EXTENDED_LEN;
	}
#endif /* CONFIG_OCV */

	/* Basic Multi-Link element for add links */
	if (mle_len) {
		u8 *mle_pos = pos;

		/* TODO: Basic MLE is not supposed to include BPCC in Link
		 * Reconfiguration Response, but mac80211 implementation for
		 * processing this frame requires that to be present. For now,
		 * include that subfield as a workaround. This should be removed
		 * once mac80211 is fixed to match the standard (or this comment
		 * be removed if the standard is modified to match
		 * implementation). */
		mle_pos = hostapd_eid_eht_basic_ml_common(hapd, mle_pos, &mld,
							  false, true, 0, false,
							  false, false, false);
		if ((size_t) (mle_pos - pos) != mle_len) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Unexpected MLE length: %ld != %zu",
				   (long) (mle_pos - pos), mle_len);
			reject_all = true;
			goto reject_all_req;
		}

		pos += mle_len;
		pos_len += mle_len;
	}

#ifdef CONFIG_IEEE80211BN
	if (hostapd_is_uhr_enabled(hapd))
		pos = hostapd_eid_uhr_params_update(hapd, pos, true, false);
#endif /* CONFIG_IEEE80211BN */

reject_all_req:
	if (reject_all) {
		pos = orig_pos;
		pos_len = 28; /* reset pos_len */
		pos += hostapd_reject_all_reconf_req(hapd, orig_pos, req_list);
		pos_len += pos - orig_pos;

		req_list->links_add_ok = req_list->links_del_ok = 0;
		req_list->new_valid_links = 0;
	}

send_resp:
	ret = hostapd_drv_send_mlme(hapd, mgmt, pos_len, 0, NULL, 0, 0, 0, 0);
	os_free(buf);

	if (mld.mld_sta)
		ap_sta_free_sta_profile(&mld);

	return ret;
}


static int
hostapd_ml_check_sta_entry_by_link_addr_iter(struct hostapd_data *hapd,
					     struct sta_info *sta, void *ctx)
{
	const u8 *link_addr = ctx;
	struct mld_link_info li;

	if (!link_addr)
		return 0;

	if (sta->mld_info.mld_sta) {
		li = sta->mld_info.links[hapd->mld_link_id];
		if (!li.valid || !ether_addr_equal(li.peer_addr, link_addr))
			return 0;

		wpa_printf(MSG_DEBUG, "MLD: STA with address " MACSTR
			   " exists for AP (link_id=%u) as a non-AP STA affiliated with non-AP MLD "
			   MACSTR, MAC2STR(link_addr),
			   hapd->mld_link_id, MAC2STR(sta->addr));
		return 1;
	}

	if (ether_addr_equal(sta->addr, link_addr)) {
		wpa_printf(MSG_DEBUG, "MLD: STA with address " MACSTR
			   " exists for AP (link_id=%u) as a legacy STA",
			   MAC2STR(link_addr), hapd->mld_link_id);
		return 1;
	}

	return 0;
}


/* Returns:
 * 0 = successful parsing
 * 1 = per-STA profile (subelement) skipped or rejected
 * -1 = fail due to fatal errors
 */
static int
hostapd_parse_link_reconf_req_sta_profile(struct hostapd_data *hapd,
					    struct link_reconf_req_list *req,
					    const u8 *buf, size_t len)
{
	struct link_reconf_req_info *info = NULL;
	const struct ieee80211_eht_per_sta_profile *per_sta_prof;
	const struct element *elem;
	struct hostapd_data *lhapd = NULL;
	struct sta_info *lsta;
	size_t sta_info_len, sta_prof_len = 0;
	u16 sta_control, reconf_type_mask;
	u8 link_id, reconf_type;
	const u8 *sta_info = NULL, *end;
	u8 sta_addr[ETH_ALEN];
	size_t nstr_bitmap_size = 0;
	int ret = -1;
	u16 status;

	if (len < sizeof(*elem) + 2UL)
		goto out;

	elem = (const struct element *) buf;
	end = buf + len;

	os_memset(sta_addr, 0, ETH_ALEN);

	if (elem->id != MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE) {
		wpa_printf(MSG_DEBUG, "MLD: Unexpected subelement (%u) found",
			   elem->id);
		ret = 1; /* skip this subelement */
		goto out;
	}

	status = WLAN_STATUS_UNSPECIFIED_FAILURE;

	per_sta_prof = (const struct ieee80211_eht_per_sta_profile *)
		elem->data;
	sta_info = per_sta_prof->variable;
	if (end < sta_info + *sta_info) {
		wpa_printf(MSG_DEBUG, "MLD: STA Info with excess length");
		goto out;
	}

	sta_control = le_to_host16(per_sta_prof->sta_control);
	sta_info_len = 1;

	link_id = sta_control & EHT_PER_STA_RECONF_CTRL_LINK_ID_MSK;
	wpa_printf(MSG_DEBUG, "MLD: Per-STA profile for link=%u", link_id);

	reconf_type_mask =
		sta_control & EHT_PER_STA_RECONF_CTRL_OP_UPDATE_TYPE_MSK;
	reconf_type =
		EHT_PER_STA_RECONF_CTRL_OP_UPDATE_TYPE_VAL(reconf_type_mask);

	switch (reconf_type) {
	case EHT_RECONF_TYPE_ADD_LINK:
	case EHT_RECONF_TYPE_DELETE_LINK:
		break;
	default:
		wpa_printf(MSG_ERROR,
			   "MLD: Unsupported Reconfiguration type %u",
			   reconf_type);
		ret = 1; /* skip this per-STA profile */
		goto out;
	}

	if (!(sta_control & EHT_PER_STA_RECONF_CTRL_MAC_ADDR)) {
		wpa_printf(MSG_DEBUG,
			   "MLD: STA MAC address not set in STA control");
		ret = 1; /* reject this per-STA profile */
		goto add_to_list;
	}
	sta_info_len += ETH_ALEN;

	if (sta_control & EHT_PER_STA_RECONF_CTRL_AP_REMOVAL_TIMER) {
		wpa_printf(MSG_DEBUG,
			   "MLD: AP removal timer set in STA control");
		sta_info_len += 2;
	}

	if (sta_control & EHT_PER_STA_RECONF_CTRL_OP_PARAMS) {
		wpa_printf(MSG_DEBUG, "MLD: Op params set in STA control");
		sta_info_len += 3;
	}

	if (!(sta_control & EHT_PER_STA_RECONF_CTRL_COMPLETE_PROFILE)) {
		if (reconf_type == EHT_RECONF_TYPE_ADD_LINK) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Complete profile not set in STA control");
			ret = 1; /* reject this per-STA profile */
			goto add_to_list;
		}
	} else {
		if (reconf_type == EHT_RECONF_TYPE_DELETE_LINK)
			wpa_printf(MSG_DEBUG,
				   "MLD: Complete profile set in STA control");
	}

	if (sta_control & EHT_PER_STA_RECONF_CTRL_NSTR_INDICATION) {
		nstr_bitmap_size = 1;
		if (sta_control &
		    EHT_PER_STA_RECONF_CTRL_NSTR_BITMAP_SIZE)
			nstr_bitmap_size = 2;

		if (reconf_type == EHT_RECONF_TYPE_DELETE_LINK)
			wpa_printf(MSG_DEBUG,
				   "MLD: NSTR Indication set in STA control");
	}
	sta_info_len += nstr_bitmap_size;


	if (*sta_info < sta_info_len) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Invalid Reconf STA Info len (%u); min expected=%zu",
			   *sta_info, sta_info_len);
		goto out;
	}

	sta_info_len = *sta_info;

	os_memcpy(sta_addr, sta_info + 1, ETH_ALEN);
	wpa_printf(MSG_DEBUG, "MLD: Link STA addr=" MACSTR, MAC2STR(sta_addr));

	sta_info += sta_info_len;

	lhapd = hostapd_mld_get_link_bss(hapd, link_id);
	if (!lhapd) {
		wpa_printf(MSG_DEBUG, "MLD: No AP link found for link id=%u",
			   link_id);
		ret = 1; /* reject this per-STA profile */
		goto add_to_list;
	}

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(lhapd->conf)) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Link %d can't be used for reconfig", link_id);
		lhapd = NULL;
		ret = 1;
		goto add_to_list;
	}
#endif /* CONFIG_QCN_EXTN */

	lsta = ap_get_sta(lhapd, req->sta_mld_addr);

	if (reconf_type == EHT_RECONF_TYPE_DELETE_LINK) {
		/* DELETE_LINK request shall not have STA profile */
		if (len != sizeof(sta_control) + sta_info_len)
			wpa_printf(MSG_DEBUG,
				   "MLD: Delete link request has STA profile");

		if (!lsta || !ap_sta_is_mld(lhapd, lsta) ||
		    !lsta->mld_info.links[link_id].valid) {
			wpa_printf(MSG_DEBUG,
				   "MLD: STA invalid for link id=%u peer addr="
				   MACSTR, link_id, MAC2STR(sta_addr));
			ret = 1; /* reject this per-STA profile */
			goto add_to_list;
		}

		if (!ether_addr_equal(lsta->mld_info.links[link_id].peer_addr,
				      sta_addr)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: STA invalid for addr=" MACSTR,
				   MAC2STR(sta_addr));
			ret = 1; /* reject this per-STA profile */
			goto add_to_list;
		}

		status = WLAN_STATUS_SUCCESS;
		ret = 0;
		goto add_to_list;
	}

	/* EHT_RECONF_TYPE_ADD_LINK */
	if (len < sizeof(sta_control) + sta_info_len + 2)
		goto out;
	sta_prof_len = len - sizeof(sta_control) - sta_info_len - 2;
	if (sta_prof_len > (size_t) (end - sta_info)) {
		wpa_printf(MSG_DEBUG, "MLD: STA Profile with excess length");
		goto out;
	}

	if (lsta) {
		wpa_printf(MSG_DEBUG,
			   "MLD: STA exists for link id=%u MLD addr=" MACSTR,
			   link_id, MAC2STR(req->sta_mld_addr));
		ret = 1; /* reject this per-STA profile */
		goto add_to_list;
	}

	/* Check if link address is already used by any connected legacy
	 * non-AP STA or non-AP STA affiliated with a non-AP MLD.
	 */
	if (ap_for_each_sta(lhapd, hostapd_ml_check_sta_entry_by_link_addr_iter,
			    sta_addr)) {
		ret = 1; /* Reject this per-STA profile */
		goto add_to_list;
	}

	status = WLAN_STATUS_SUCCESS; /* IE validations done later */
	ret = 0;

add_to_list:
	info = os_zalloc(sizeof(struct link_reconf_req_info) + sta_prof_len);
	if (!info) {
		wpa_printf(MSG_DEBUG, "MLD: Failed to allocate request info");
		ret = 1; /* skip this per-STA profile */
		goto out;
	}

	info->link_id = link_id;
	info->status = status;
	os_memcpy(info->peer_addr, sta_addr, ETH_ALEN);
	if (lhapd)
		os_memcpy(info->local_addr, lhapd->own_addr, ETH_ALEN);

	if (reconf_type == EHT_RECONF_TYPE_DELETE_LINK) {
		dl_list_add_tail(&req->del_req, &info->list);
	} else if (sta_info) {
		os_memcpy(info->sta_prof, sta_info, sta_prof_len);
		info->sta_prof_len = sta_prof_len;

		dl_list_add_tail(&req->add_req, &info->list);
	} else {
		os_free(info);
	}
	wpa_printf(MSG_INFO, "MLD: Link (%d) parsed to %s request; status=%u",
		   link_id,
		   reconf_type == EHT_RECONF_TYPE_DELETE_LINK ? "del" : "add",
		   status);

out:
	if (ret < 0)
		wpa_printf(MSG_DEBUG,
			   "MLD: Failed to parse reconf req STA profile");
	return ret;
}


static int
hostapd_parse_link_reconf_req_reconf_mle(
		struct hostapd_data *hapd, const u8 *mle, size_t mle_len,
		struct link_reconf_req_list **req_list_ptr)
{
	struct link_reconf_req_list *req_list;
	struct wpabuf *mlbuf = NULL;
	struct sta_info *sta;
	const struct ieee80211_eht_ml *ml;
	const struct eht_ml_reconf_common_info *ml_common_info;
	size_t len, ml_common_len;
	u16 ml_control;
	const u8 *pos, *end;
	int ret = -1;

	mlbuf = ieee802_11_defrag(mle, mle_len, true);
	if (!mlbuf) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Failed to defrag Reconfiguration MLE");
		goto fail;
	}

	ml = (const struct ieee80211_eht_ml *) wpabuf_head(mlbuf);
	len = wpabuf_len(mlbuf);
	end = ((const u8 *) ml) + len;

	wpa_hexdump(MSG_DEBUG, "MLD: Defragged Reconfiguration MLE",
		    (const void *) ml, len);

	if (len < sizeof(*ml) + ETH_ALEN + 1UL)
		goto fail;

	ml_control = WPA_GET_LE16((const u8 *) ml) >> 4;
	ml_common_len = 1;
	if (!(ml_control & RECONF_MULTI_LINK_CTRL_PRES_MLD_MAC_ADDR))
		goto fail;
	ml_common_len += ETH_ALEN;

	if (ml_control & RECONF_MULTI_LINK_CTRL_PRES_EML_CAPA)
		ml_common_len += 2;

	if (ml_control & RECONF_MULTI_LINK_CTRL_PRES_MLD_CAPA)
		ml_common_len += 2;

	if (ml_control & RECONF_MULTI_LINK_CTRL_PRES_EXT_MLD_CAP)
		ml_common_len += 2;

	ml_common_info =
		(const struct eht_ml_reconf_common_info *) ml->variable;
	if (len < sizeof(*ml) + ml_common_info->len) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Unexpected Reconfiguration ML element length (%zu < %zu)",
			   len, sizeof(*ml) + ml_common_info->len);
		goto fail;
	}

	if (ml_common_info->len < ml_common_len) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Invalid Reconf common info len (%u); min expected=%zu",
			   ml_common_info->len, ml_common_len);
		goto fail;
	}

	pos = (const u8 *) ml_common_info->variable;

	sta = ap_get_sta(hapd, pos);
	if (!sta || !ap_sta_is_mld(hapd, sta)) {
		wpa_printf(MSG_DEBUG, "MLD: STA invalid%s for " MACSTR,
			   sta ? "" : " (NULL)", MAC2STR(pos));
		goto fail;
	}

	*req_list_ptr = os_zalloc(sizeof(struct link_reconf_req_list));
	if (!(*req_list_ptr)) {
		wpa_printf(MSG_ERROR, "MLD: Failed to allocate request list");
		goto fail;
	}
	req_list = *req_list_ptr;

	os_memcpy(req_list->sta_mld_addr, pos, ETH_ALEN);
	dl_list_init(&req_list->del_req);
	dl_list_init(&req_list->add_req);

	pos = ml->variable + ml_common_info->len;

	while (end - pos > 2) {
		size_t sub_elem_len;
		int num_frag_subelems;

		num_frag_subelems =
			ieee802_11_defrag_mle_subelem(mlbuf, pos,
						      &sub_elem_len);
		if (num_frag_subelems < 0) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Failed to parse Reconfiguration MLE subelem");
			goto fail;
		}

		len -= num_frag_subelems * 2;
		end = ((const u8 *) ml) + len;

		if (sub_elem_len + 2 > (size_t) (end - pos))
			goto fail;

		if (hostapd_parse_link_reconf_req_sta_profile(
			    hapd, req_list, pos, sub_elem_len + 2) < 0)
			goto fail;

		pos += sub_elem_len + 2;
	}

	ret = 0;

fail:
	if (ret)
		ml_deinit_link_reconf_req(req_list_ptr);

	wpabuf_free(mlbuf);
	return ret;
}


static bool
hostapd_validate_link_reconf_req(struct hostapd_data *hapd,
				 struct sta_info *sta,
				 struct link_reconf_req_list *req_list)
{
	struct hostapd_data *assoc_hapd, *lhapd;
	struct link_reconf_req_info *info;
	struct sta_info *assoc_sta, *lsta;
	struct mld_info *mld_info;
	u8 recovery_link;
	u16 valid_links = 0, links_add_ok = 0, links_del_ok = 0, status;
	size_t link_kde_len, total_kde_len = 0;
	int i;

	assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
	if (!assoc_sta)
		return false;

	if (dl_list_empty(&req_list->add_req) &&
	    dl_list_empty(&req_list->del_req)) {
		wpa_printf(MSG_DEBUG, "MLD: No add or delete request found");
		return false;
	}

	mld_info = &assoc_sta->mld_info;
	for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
		if (mld_info->links[i].valid &&
		    mld_info->links[i].status == WLAN_STATUS_SUCCESS)
			valid_links |= BIT(i);
	}

	/* Check IEs for add-link STA profiles */
	dl_list_for_each(info, &req_list->add_req, struct link_reconf_req_info,
			 list) {
		lhapd = NULL;
		lsta = NULL;

		wpa_printf(MSG_DEBUG,
			   "MLD: Add Link Reconf STA for link id=%u status=%u",
			   info->link_id, info->status);
		if (info->status != WLAN_STATUS_SUCCESS ||
		    info->sta_prof_len < 2)
			continue;

		/* Offset 2 bytes for Capabilities in STA Profile */
		status = hostapd_ml_process_reconf_link(hapd, assoc_sta,
							info->sta_prof + 2,
							info->sta_prof_len - 2,
							info->link_id,
							info->peer_addr,
							LINK_PARSE_RECONF);
		if (status != WLAN_STATUS_SUCCESS) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Add link IE validation failed for link=%u",
				   info->link_id);
			info->status = status;
			continue;
		}

		link_kde_len = wpa_auth_ml_group_kdes_len(assoc_sta->wpa_sm,
							  BIT(info->link_id));

		/* Since Group KDE element Length subfield is one byte,
		 * accept as many add-link requests as can be fit.
		 */
		if (total_kde_len + link_kde_len >
		    LINK_RECONF_GROUP_KDE_MAX_LEN) {
			wpa_printf(MSG_INFO,
				   "MLD: Group KDEs cannot fit (%zu > %u) for link=%u",
				   total_kde_len + link_kde_len,
				   LINK_RECONF_GROUP_KDE_MAX_LEN,
				   info->link_id);
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;

			lhapd = hostapd_mld_get_link_bss(hapd, info->link_id);
			if (lhapd)
				lsta = ap_get_sta(lhapd,
						  req_list->sta_mld_addr);

#ifdef CONFIG_QCN_EXTN
			if (lhapd && hostapd_is_repurpose_disabled_11be_extn(lhapd->conf))
				lsta = NULL;
#endif /* CONFIG_QCN_EXTN */

			if (lsta)
				ap_free_sta_link(lhapd, lsta);
		} else {
			total_kde_len += link_kde_len;
			links_add_ok |= BIT(info->link_id);
			wpa_msg(hapd->msg_ctx, MSG_INFO,
				WPA_EVENT_LINK_STA_ADDED "sta=" MACSTR
				" link_id=%u", MAC2STR(req_list->sta_mld_addr),
				info->link_id);
		}

		info->status = status;
	}

	dl_list_for_each(info, &req_list->del_req, struct link_reconf_req_info,
			list) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Del Link Reconf STA for link id=%u status=%u",
			   info->link_id, info->status);
		if (info->status == WLAN_STATUS_SUCCESS)
			links_del_ok |= BIT(info->link_id);
	}

	wpa_printf(MSG_INFO, "MLD: valid_links=0x%x add_ok=0x%x del_ok=0x%x",
		   valid_links, links_add_ok, links_del_ok);

	if ((links_add_ok && (valid_links & links_add_ok)) ||
	    (links_del_ok && !(valid_links & links_del_ok))) {
		wpa_printf(MSG_INFO,
			   "MLD: Links requested failed to satisfy valid links");
		return false;
	}

	if (links_add_ok & links_del_ok) {
		wpa_printf(MSG_INFO,
			   "MLD: Links (0x%x) present in both valid add and delete requests",
			   links_add_ok & links_del_ok);
		return false;
	}

	valid_links |= links_add_ok;
	valid_links &= ~links_del_ok;
	if (!valid_links) {
		if (!recover_from_zero_links(&links_del_ok, &recovery_link)) {
			wpa_printf(MSG_INFO,
				   "MLD: Total-links validation failed");
			return false;
		}
		/* Add the recovery link back to valid_links */
		valid_links |= BIT(recovery_link);
	}

	req_list->new_valid_links = valid_links;
	req_list->links_add_ok = links_add_ok;
	req_list->links_del_ok = links_del_ok;

	/* TODO: Add support to handle multiple requests from the non-AP MLD */
	assoc_sta->reconf_req = req_list;

	return true;
}


static int
hostapd_handle_link_reconf_req(struct hostapd_data *hapd, const u8 *buf,
			       size_t len)
{
	struct ieee802_11_elems elems;
	struct hostapd_data *assoc_hapd;
	struct sta_info *sta, *assoc_sta = NULL;
	u8 dialog_token;
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *) buf;
	struct link_reconf_req_list *req_list = NULL;
	struct ml_reconf_req ml_reconf_req = {};
	const u8 *pos = NULL;
	int ret = -1, i;

	wpa_printf(MSG_DEBUG,
		   "MLD: Link Reconfiguration Request frame from " MACSTR,
		   MAC2STR(mgmt->sa));

	/* Min length: IEEE80211 Header (24B) + Category (1B) + Action (1B) +
	 *	       Dialog token (1B) +
	 *	       Reconfiguration MLE header and extension ID (3B)
	 */
	if (len < IEEE80211_HDRLEN + 3 + 3) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Invalid minimum length (%zu) for Link Reconfiguration Request",
			   len);
		goto out;
	}

	dialog_token = mgmt->u.action.u.link_reconf_req.dialog_token;
	pos = mgmt->u.action.u.link_reconf_req.variable;

	sta = ap_get_sta(hapd, mgmt->sa);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "MLD: No STA found for " MACSTR
			   "; drop Link Reconfiguration Request",
			   MAC2STR(mgmt->sa));
		goto out;
	}

	if (!ap_sta_is_mld(hapd, sta)) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Not an MLD connection; drop Link Reconfiguration Request");
		goto out;
	}

	assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
	if (!assoc_sta) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Not able to get assoc link STA; drop Link Reconfiguration Request");
		goto out;
	}

	if (assoc_sta->reconf_req) {
		wpa_printf(MSG_INFO,
			   "MLD: Link Reconfiguration Request from this STA with token=%u is already in progress",
			   assoc_sta->reconf_req->dialog_token);
		goto out;
	}

	/* Parse Reconfiguration Multi-Link element and OCI elements */
	if (ieee802_11_parse_elems(pos, len - (pos - buf), &elems, 1) ==
	    ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Could not parse Link Reconfiguration Request");
		goto out;
	}

	if (!elems.reconf_mle || !elems.reconf_mle_len) {
		wpa_printf(MSG_DEBUG, "MLD: No Reconfiguration ML element");
		goto out;
	}

	/* Process Reconfiguration MLE */
	if (hostapd_parse_link_reconf_req_reconf_mle(hapd, elems.reconf_mle,
						     elems.reconf_mle_len,
						     &req_list)) {
		wpa_printf(MSG_INFO,
			   "MLD: Reconfiguration MLE parsing failed; drop Link Reconfiguration Request");
		goto out;
	}

	/* Do OCI element validation */
	if (dl_list_empty(&req_list->add_req))
		goto skip_oci_validation;

#ifdef CONFIG_OCV
	if (!elems.oci || !elems.oci_len) {
		if (wpa_auth_uses_ocv(assoc_sta->wpa_sm) == 1) {
			wpa_printf(MSG_INFO,
				   "MLD: No OCI element present; drop Link Reconfiguration Request");
			goto out;
		}
	} else {
		struct wpa_channel_info ci;

		if (!wpa_auth_uses_ocv(assoc_sta->wpa_sm)) {
			wpa_printf(MSG_INFO,
				   "MLD: Unexpected OCI element found; drop Link Reconfiguration Request");
			goto out;
		}

		if (hostapd_drv_channel_info(hapd, &ci)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Failed to get channel info to verify OCI element");
			goto out;
		}

		if (!ocv_verify_tx_params(elems.oci, elems.oci_len, &ci,
					  channel_width_to_int(ci.chanwidth),
					  ci.seg1_idx)) {
			wpa_printf(MSG_INFO,
				   "MLD: OCI verification failed; drop Link Reconfiguration Request");
			goto out;
		}
	}
#endif /* CONFIG_OCV */

skip_oci_validation:
	/* Do STA profile validation */
	if (!hostapd_validate_link_reconf_req(hapd, assoc_sta, req_list))
		goto out;

	os_memcpy(ml_reconf_req.addr, assoc_sta->addr, ETH_ALEN);
	for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
		ml_reconf_req.sta_add_params[i] =
			assoc_sta->recfg_sta_add_params[i];
	}
	ml_reconf_req.add_links = req_list->links_add_ok;
	ml_reconf_req.del_links = req_list->links_del_ok;
	ml_reconf_req.valid_links = req_list->new_valid_links;

	if (ml_reconf_req.add_links || ml_reconf_req.del_links) {
		ret = hostapd_drv_ml_reconf(hapd, &ml_reconf_req);
		if (ret) {
			wpa_printf(MSG_ERROR, "MLD: Failed to send Link reconfiguration "
				   "request to driver (%d)", ret);
			goto out;
		}
	}

	req_list->dialog_token = dialog_token;
	ret = hostapd_send_link_reconf_resp(hapd, assoc_sta, req_list);
	if (ret)
		wpa_printf(MSG_INFO,
			   "MLD: Failed to send Link Reconfiguration Response (%d)",
			   ret);

out:
	if (assoc_sta) {
		for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
			hostapd_free_reconf_sta_add_params(
					assoc_sta->recfg_sta_add_params[i]);
			assoc_sta->recfg_sta_add_params[i] = NULL;
		}
	}

	if (ret) {
		ml_deinit_link_reconf_req(&req_list);
		if (assoc_sta && assoc_sta->reconf_req)
			assoc_sta->reconf_req = NULL;
	}
	return ret;
}


void ieee802_11_rx_protected_eht_action(struct hostapd_data *hapd,
					struct sta_info *sta,
					const struct ieee80211_mgmt *mgmt,
					size_t len)
{
	const u8 *payload;
	u8 action;

	if (!hapd->conf->mld_ap)
		return;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return;
#endif /* CONFIG_QCN_EXTN */

	payload = ((const u8 *) mgmt) + IEEE80211_HDRLEN + 1;
	action = *payload++;

	switch (action) {
	case WLAN_PROT_EHT_LINK_RECONFIG_REQUEST:
		if (hostapd_handle_link_reconf_req(hapd, (const u8 *) mgmt,
						   len))
			wpa_printf(MSG_INFO,
				   "MLD: Link Reconf Request processing failed");
		break;
	case WLAN_PROT_EHT_EPCS_ENABLE_REQUEST:
	case WLAN_PROT_EHT_EPCS_ENABLE_RESPONSE:
	case WLAN_PROT_EHT_EPCS_ENABLE_TEARDOWN:
		hostapd_handle_epcs_action(hapd, (const u8 *)mgmt, len);
		break;
	case WLAN_PROT_EHT_T2L_MAPPING_REQUEST:
		hostapd_handle_ttlm_req(hapd, sta, (const u8 *) mgmt, len);
		break;
	case WLAN_PROT_EHT_T2L_MAPPING_RESPONSE:
		hostapd_handle_ttlm_resp(hapd, sta, (const u8 *) mgmt, len);
		break;
	case WLAN_PROT_EHT_T2L_MAPPING_TEARDOWN:
		hostapd_handle_ttlm_teardown(hapd, sta, (const u8 *) mgmt, len);
		break;
	default:
		wpa_printf(MSG_DEBUG,
			  "MLD: Unsupported Protected EHT Action %u from " MACSTR
			   " discarded", action, MAC2STR(mgmt->sa));
		break;
	}
}

#ifdef CONFIG_IEEE80211BE
int hostapd_wnm_add_multi_link_sub_elem(struct hostapd_data *hapd,
					u8 *links, u8 num_links,
					u8 *pos, size_t len)
{
	u16 control;
	u8 *len_pos = &pos[1];
	u8 link_id;
	int i;

	if (len < 12)
		return -1;

	*pos++ = WNM_NEIGHBOR_MULTI_LINK;
	*pos++ = MULTI_LINK_CONTROL_LEN + 1 + ETH_ALEN + 1;

	control = MULTI_LINK_CONTROL_TYPE_BASIC |
		BASIC_MULTI_LINK_CTRL_PRES_LINK_ID;
	WPA_PUT_LE16(pos, control);
	pos += 2;

	/* common info */
	*pos++ = 1 + ETH_ALEN + 1;

	/* MLD address */
	os_memcpy(pos, hapd->mld->mld_addr, ETH_ALEN);
	pos += ETH_ALEN;

	/* own link-id */
	*pos++ = hapd->mld_link_id;

	len -= 12;

	/* per-STA profile subelements */
	for (i = 0; i < num_links; i++) {
		link_id = links[i];

		if (hapd->mld_link_id == link_id)
			continue;
		if (!hapd->partner_links[link_id].valid)
			return -1;
		if (len < 4)
			return -1;

		*pos++ = MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE;
		*pos++ = 2;

		/* per-STA link-id */
		control = link_id & 0xf;
		WPA_PUT_LE16(pos, control);
		pos += 2;

		*len_pos += 4;
		len -= 4;
	}

	return *len_pos + 2;
}
#endif /* CONFIG_IEEE80211BE */


u8 * hostapd_fragment_multi_link_element(struct wpabuf *buf, u8 *pos)
{
	size_t len, slice_len;
	const u8 *ptr;

	len = wpabuf_len(buf);
	ptr = wpabuf_head(buf);

	if (len <= 254)
		slice_len = len;
	else
		slice_len = 254;

	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = slice_len + 1;
	*pos++ = WLAN_EID_EXT_MULTI_LINK;
	os_memcpy(pos, ptr, slice_len);

	ptr += slice_len;
	pos += slice_len;
	len -= slice_len;

	while (len) {
		if (len <= 255)
			slice_len = len;
		else
			slice_len = 255;

		*pos++ = WLAN_EID_FRAGMENT;
		*pos++ = slice_len;
		os_memcpy(pos, ptr, slice_len);

		ptr += slice_len;
		pos += slice_len;
		len -= slice_len;
	}
	return pos;
}


#define EHT_PRIO_ACCESS_ML_COMMON_INFO_LEN 7 /* common info length: length field (1) + AP MLD MAC ADDR (6) */

static size_t hostapd_eid_eht_ml_priority_access(struct hostapd_data *hapd,
						 u8 *eid,
						 struct mld_info *mld_info)
{
	u8 *pos = eid;
	u32 i;
	struct wpabuf *buf;
	struct hostapd_data *other_hapd;
	size_t sta_profile_len;
	u8 mu_edca_start[3 + sizeof(struct ieee80211_he_mu_edca_parameter_set)];
	u8 wmm_start[2 + sizeof(struct wmm_parameter_element)];
	struct mld_link_info *link;

	/*
	 * As the Multi-Link element can exceed the size of 255 bytes need to
	 * first build it and then handle fragmentation.
	 */
	buf = wpabuf_alloc(1024);
	if (!buf)
		return 0;

	/* Set the Multi-Link Control field */
	wpabuf_put_le16(buf, MULTI_LINK_CONTROL_TYPE_PRIOR_ACCESS);

	/* Set the Priority Access Multi-Link ML element common info len */
	wpabuf_put_u8(buf, EHT_PRIO_ACCESS_ML_COMMON_INFO_LEN);

	/* Own MLD MAC Address */
	wpabuf_put_data(buf, hapd->mld->mld_addr, ETH_ALEN);

	/*sta_profile_len (44): sta control (2) +
	 *                      ext id (1) + length field (1) + sub id (1) +
	 *                      size of mu_edca_params (13) +
	 *                      vendor id (1) + length field (1) +
	 *                      size of wmm_params (24)
	 */
	sta_profile_len = 2 + 3 + sizeof(struct ieee80211_he_mu_edca_parameter_set) +
			  2 + sizeof(struct wmm_parameter_element);

	/* EPCS Priority Access Multi-Link element Link Info
	 * Add link info of links to which connected non-AP MLD is connected
	 */
	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		if (!hapd->iface->interfaces->iface[i])
			continue;

		other_hapd = hapd->iface->interfaces->iface[i]->bss[0];

		if (!other_hapd->conf->mld_ap)
			continue;

#ifdef CONFIG_QCN_EXTN
		if (hostapd_is_repurpose_disabled_11be_extn(other_hapd->conf))
			continue;
#endif /* CONFIG_QCN_EXTN */

		link = &mld_info->links[other_hapd->mld_link_id];

		if (!link || !link->valid)
			continue;

		/* Set the Per-STA Profile subelement */
		wpabuf_put_u8(buf, MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE);

		/* Set the Per-STA Profile length */
		wpabuf_put_u8(buf, sta_profile_len);

		/* Set the STA Control */
		wpabuf_put_le16(buf, (other_hapd->mld_link_id & 0xf));

		/* Set the EPCS MU EDCA params */
		hostapd_eid_he_mu_edca_parameter_set(other_hapd, mu_edca_start,
						     true);
		wpabuf_put_data(buf, mu_edca_start,
				3 + sizeof(struct ieee80211_he_mu_edca_parameter_set));

		/* Set the EPCS WMM params */
		hostapd_eid_wmm(other_hapd, wmm_start, true);
		wpabuf_put_data(buf, wmm_start,
				2 + sizeof(struct wmm_parameter_element));
	}

	/* Fragment the EPCS Priority Access Multi-Link element, if needed */
	pos = hostapd_fragment_multi_link_element(buf, pos);
	wpabuf_free(buf);

	return (size_t)(pos - (u8 *) eid);
}


static int hapd_epcs_drv_send_action_frame(struct hostapd_data *hapd,
					   struct wlan_epcs_info *epcs_info,
					   struct sta_info *sta)
{
	size_t prio_access_ml_elem_len;
	u8 *prio_access_ml_elem;
	struct wpabuf *buf;

	buf = wpabuf_alloc(1024 + 5); /*len 5 for the EPCS hdr without PA ML element*/
	if (!buf)
		return -1;

	wpabuf_put_u8(buf, WLAN_ACTION_PROTECTED_EHT);

	switch (epcs_info->action_code) {
	case WLAN_PROT_EHT_EPCS_ENABLE_REQUEST:
	case WLAN_PROT_EHT_EPCS_ENABLE_RESPONSE:
		/* EPCS Request and Response */
		wpabuf_put_u8(buf, epcs_info->action_code);
		wpabuf_put_u8(buf, epcs_info->dialog_token);

		if (epcs_info->action_code == WLAN_PROT_EHT_EPCS_ENABLE_RESPONSE)
			wpabuf_put_le16(buf, epcs_info->status_code);

		prio_access_ml_elem = os_zalloc(1024);
		if (!prio_access_ml_elem) {
			wpa_printf(MSG_DEBUG, "Failed to allocate buffer for EPCS Action frame");
			wpabuf_free(buf);
			return -1;
		}

		prio_access_ml_elem_len =
			hostapd_eid_eht_ml_priority_access(hapd,
							   prio_access_ml_elem,
							   &sta->mld_info);
		wpabuf_put_data(buf, prio_access_ml_elem, prio_access_ml_elem_len);
		os_free(prio_access_ml_elem);
		break;

	case WLAN_PROT_EHT_EPCS_ENABLE_TEARDOWN:
		/* EPCS Tear Down */
		wpabuf_put_u8(buf, WLAN_PROT_EHT_EPCS_ENABLE_TEARDOWN);
		break;

	default:
		wpa_printf(MSG_DEBUG, "unknown protected_eht action frame of code:%d",
			   epcs_info->action_code);
		wpabuf_free(buf);
		return -1;
	}

	if (hostapd_drv_send_action(hapd, hapd->iface->freq, 0, sta->addr,
				    wpabuf_head(buf), wpabuf_len(buf)) < 0) {
		wpa_printf(MSG_ERROR, "Sending the EPCS action frame failed for the STA mld addr: " MACSTR,
			   MAC2STR(sta->mld_info.common_info.mld_addr));
		wpabuf_free(buf);
		return -1;
	}

	wpabuf_free(buf);
	return 0;
}


static bool hostapd_epcs_is_sta_authorized(struct hostapd_data *hapd,
					   struct sta_info *sta)
{
	return hostapd_maclist_found(hapd->mld->epcs_authorized_mac,
				     hapd->mld->num_epcs_authorized_mac,
				     sta->mld_info.common_info.mld_addr,
				     NULL);
}


void hostapd_epcs_timeout_handler(void *eloop_ctx, void *timeout_ctx)
{
	struct wlan_epcs_info epcs_info = {0};
	struct sta_info *sta = timeout_ctx;
	struct hostapd_data *hapd = eloop_ctx;
	struct mld_info *mld_info = &sta->mld_info;

	if (!mld_info->mld_sta)
		return;

	epcs_info.action_code = WLAN_PROT_EHT_EPCS_ENABLE_TEARDOWN;
	hostapd_epcs_handle_and_send_action_frame(hapd, &epcs_info, sta, false);
	mld_info->epcs.timer_started = false;
}

int hostapd_configure_epcs(struct hostapd_data *hapd,
				  struct sta_info *sta,
				  enum qos_mgmt_req_type req_type)
{
	struct qm_req_data qm_req = {0};
	struct qm_resp_data qm_resp = {0};
	struct hostapd_nft_rule_params rule = {0};

	if (hapd->driver == NULL)
		return -1;

	/* Fill QM request data */
	os_memcpy(qm_req.peer_mac, sta->addr, ETH_ALEN);
	qm_req.qm_type = HOSTAPD_QM_TYPE_SCS;
	/* EPCS carries only one descriptor */
	qm_req.num_qm_desc = EPCS_NUM_QM_DESC;

	/* Fill QM request descriptor data */
	qm_req.qm_req_desc[0].qm_id = EPCS_QM_ID;
	if (req_type == QM_ADD_REQ) {
		qm_req.qm_req_desc[0].request_type = QM_ADD_REQ;
		qm_req.qm_req_desc[0].priority = EPCS_TID_VALUE;
		qm_req.qm_req_desc[0].is_qos_present = true;

		/* Fill QM request descriptor QoS attributes */
		qm_req.qm_req_desc[0].qos_attr.direction =
			QM_DIRECTION_DOWNLINK;
		qm_req.qm_req_desc[0].qos_attr.tid = EPCS_TID_VALUE;
		qm_req.qm_req_desc[0].qos_attr.up = EPCS_TID_VALUE;

	} else if (req_type == QM_REMOVE_REQ)
		qm_req.qm_req_desc[0].request_type = QM_REMOVE_REQ;
	else {
		wpa_printf(MSG_ERROR, "Invalid req type for EPCS: %d", req_type);
		return -1;
	}

	/* Program NFT rule delete */
	if (req_type == QM_REMOVE_REQ) {
		os_snprintf(rule.chain, sizeof(rule.chain), "%s_%s", CHAIN_NAME,
			    hapd->conf->iface);
		os_snprintf(rule.table, sizeof(rule.table), "%s", TABLE_NAME);
		rule.mark = (EPCS_QM_ID << 8) | HOSTAPD_QOS_SCS_TAG;
		rule.nf_family = NFPROTO_NETDEV;
		memcpy(rule.dmac, sta->addr, ETH_ALEN);
		rule.handle = sta->mld_info.epcs.rule_handle;
		sta->mld_info.epcs.rule_handle = 0;
		hostapd_config_nft_rule(&rule, false);
		hostapd_drv_rule_config_notify(hapd, sta->addr, NULL,
					       HOSTAPD_QM_TYPE_MSCS);
	}

	/* Send the QoS request */
	if (hapd->drv_priv && hapd->driver->set_qos &&
	    hapd->driver->set_qos(hapd->drv_priv, &qm_req, &qm_resp)) {
		wpa_printf(MSG_ERROR, "Set QoS failed");
		return -1;
	}

	if (qm_resp.qm_resp_desc[0].qm_id != qm_req.qm_req_desc[0].qm_id) {
		wpa_printf(MSG_ERROR, "EPCS QM ID mismatch in response:%u",
			   qm_resp.qm_resp_desc[0].qm_id);
		return -1;
	}

	if (qm_resp.qm_resp_desc[0].status) {
		wpa_printf(MSG_ERROR, "EPCS QM response failure:%u",
			   qm_resp.qm_resp_desc[0].status);
		return -1;
	}

	/* Program NFT rule add */
	if (req_type == QM_ADD_REQ) {
		os_snprintf(rule.chain, sizeof(rule.chain), "%s_%s", CHAIN_NAME,
			    hapd->conf->iface);
		os_snprintf(rule.table, sizeof(rule.table), "%s", TABLE_NAME);
		memcpy(rule.dmac, sta->addr, ETH_ALEN);
		rule.valid_flags |= NFT_RULE_PARAM_DMAC;
		rule.mark = (EPCS_QM_ID << 8) | HOSTAPD_QOS_SCS_TAG;
		rule.nf_family = NFPROTO_NETDEV;
		hostapd_config_nft_rule(&rule, true);
		hostapd_drv_rule_config_notify(hapd, sta->addr, NULL,
					       HOSTAPD_QM_TYPE_MSCS);
		sta->mld_info.epcs.rule_handle = rule.handle;
	}

	return 0;
}

int hostapd_epcs_handle_and_send_action_frame(struct hostapd_data *hapd,
					      struct wlan_epcs_info *epcs_info,
					      struct sta_info *sta,
					      bool is_rx_frame)
{
	struct mld_peer_epcs_info *sta_epcs;
	enum peer_epcs_state prev_epcs_state;
	int ret = 0;

	if (!sta || !sta->mld_info.mld_sta)
		return -1;

	if (!hapd->conf->is_epcs_enabled && !is_rx_frame) {
		wpa_printf(MSG_ERROR, "EPCS feature is disabled");
		return -1;
	}

	if (!hostapd_epcs_is_sta_authorized(hapd, sta) && !is_rx_frame) {
		wpa_printf(MSG_ERROR, "sta is not authorized for EPCS");
		return -1;
	}

	sta_epcs = &sta->mld_info.epcs;

	if (!sta_epcs->is_epcs_capable && !is_rx_frame) {
		wpa_printf(MSG_ERROR, "sta with mld addr: " MACSTR  " is not EPCS Capable",
			   MAC2STR(sta->mld_info.common_info.mld_addr));
		return -1;
	}

	prev_epcs_state = sta_epcs->state;

	switch (prev_epcs_state) {
	case EPCS_STATE_DISABLED:
		if (epcs_info->action_code != WLAN_PROT_EHT_EPCS_ENABLE_REQUEST)
			goto failed;

		if (is_rx_frame) {
			epcs_info->action_code = WLAN_PROT_EHT_EPCS_ENABLE_RESPONSE;
			epcs_info->status_code = (hapd->conf->is_epcs_enabled && sta_epcs->is_epcs_capable) ?
						 (hostapd_epcs_is_sta_authorized(hapd, sta) ?
						 WLAN_STATUS_SUCCESS : WLAN_STATUS_EPCS_DENIED_UNAUTHORIZED) :
						 WLAN_STATUS_EPCS_DENIED;
		} else
			epcs_info->dialog_token = ++sta_epcs->self_gen_dialog_token;

		if (hapd_epcs_drv_send_action_frame(hapd, epcs_info, sta))
			break;

		if (is_rx_frame && epcs_info->status_code == WLAN_STATUS_SUCCESS) {
			sta_epcs->state = EPCS_STATE_ENABLED;
			break;
		}

		sta_epcs->state = EPCS_STATE_ENABLE_REQ_SENT;
		if (sta_epcs->timer_started) {
			wpa_printf(MSG_DEBUG, "Timer is already started for EPCS Request sent. Restarting the timer");
			eloop_cancel_timeout(hostapd_epcs_timeout_handler, hapd, sta);
			sta_epcs->timer_started = false;
		}

		if (!eloop_register_timeout(150, 0, hostapd_epcs_timeout_handler, hapd, sta))
			sta_epcs->timer_started = true;

		break;

	case EPCS_STATE_ENABLE_REQ_SENT:
		if (epcs_info->action_code == WLAN_PROT_EHT_EPCS_ENABLE_TEARDOWN) {
			if (is_rx_frame)
				sta_epcs->state = EPCS_STATE_DISABLED;
			else if (!is_rx_frame && !hapd_epcs_drv_send_action_frame(hapd, epcs_info, sta))
				sta_epcs->state = EPCS_STATE_DISABLED;

		} else if (epcs_info->action_code == WLAN_PROT_EHT_EPCS_ENABLE_RESPONSE && is_rx_frame) {
			if (epcs_info->dialog_token == sta_epcs->self_gen_dialog_token)
				sta_epcs->state = (epcs_info->status_code == WLAN_STATUS_SUCCESS) ?
						  EPCS_STATE_ENABLED : EPCS_STATE_DISABLED;
			else {
				sta_epcs->state = EPCS_STATE_DISABLED;
				wpa_printf(MSG_ERROR, "dialog token mis-match, "
					   "received token: %d, peer token: %d",
					   epcs_info->dialog_token, sta_epcs->self_gen_dialog_token);
			}
		}

			eloop_cancel_timeout(hostapd_epcs_timeout_handler, hapd, sta);
			sta_epcs->timer_started = false;
		break;

	case EPCS_STATE_ENABLED:
		if (epcs_info->action_code != WLAN_PROT_EHT_EPCS_ENABLE_TEARDOWN)
			goto failed;

		if (is_rx_frame)
			sta_epcs->state = EPCS_STATE_DISABLED;
		else if (!is_rx_frame && !hapd_epcs_drv_send_action_frame(hapd, epcs_info, sta))
			sta_epcs->state = EPCS_STATE_DISABLED;
		break;
	}

	if (epcs_info->status_code)
		wpa_printf(MSG_DEBUG, "EPCS status_code is set to %d for the sta with mld addr" MACSTR,
			   epcs_info->status_code, MAC2STR(sta->mld_info.common_info.mld_addr));

failed:
	if (prev_epcs_state == sta_epcs->state) {
		wpa_printf(MSG_ERROR, "Invalid EPCS State: %d, for the received action code: %d. EPCS state is moved to: %d"
			   "for sta with mld addr" MACSTR, prev_epcs_state, epcs_info->action_code, sta_epcs->state,
			   MAC2STR(sta->mld_info.common_info.mld_addr));
		return -1;
	} else {
		if (sta_epcs->state == EPCS_STATE_DISABLED)
			ret = hostapd_configure_epcs(hapd, sta, QM_REMOVE_REQ);
		else if (sta_epcs->state == EPCS_STATE_ENABLED)
			ret = hostapd_configure_epcs(hapd, sta, QM_ADD_REQ);

		if (ret) {
			wpa_printf(MSG_ERROR, "EPCS configure failed:%d", ret);
			return ret;
		}
	}

	return 0;
}


void hostapd_handle_epcs_action(struct hostapd_data *hapd,
				const u8 *buf,
				size_t len)
{
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *) buf;
	const u8 action_type = mgmt->u.action.u.epcs_req.action;
	struct wlan_epcs_info epcs = {0};
	struct sta_info *sta;

	sta = ap_get_sta(hapd, mgmt->sa);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "sta is NULL");
		return;
	}

	if (len == IEEE80211_HDRLEN + 4 || len > IEEE80211_HDRLEN + 5)
		return;

	epcs.action_code = action_type;
	wpa_msg(hapd->msg_ctx, MSG_INFO, "EPCS frame received: %d", action_type);

	if (action_type == WLAN_PROT_EHT_EPCS_ENABLE_REQUEST &&
	    len == IEEE80211_HDRLEN + 3) /* length: Header len +
					  * action category (1)
					  * action code (1) +
					  * dialog token (1)
					  */
		epcs.dialog_token = mgmt->u.action.u.epcs_req.dialog_token;

	if (action_type == WLAN_PROT_EHT_EPCS_ENABLE_RESPONSE &&
	    len == IEEE80211_HDRLEN + 5) /* length: Header len +
					  * action category (1)
					  * action code (1) +
					  * dialog token (1)
					  * status code (2)
					  */
	{
		epcs.dialog_token = mgmt->u.action.u.epcs_resp.dialog_token;
		epcs.status_code = mgmt->u.action.u.epcs_resp.status;
	}

	hostapd_epcs_handle_and_send_action_frame(hapd, &epcs, sta, true);
}


void hostapd_get_epcs_capab(struct hostapd_data *hapd, struct sta_info *sta)
{
	if (!sta) {
		wpa_printf(MSG_DEBUG, "sta is NULL, not able to get EPCS CAPABILITY");
		return;
	}

	if (sta->mld_info.mld_sta && sta->eht_capab &&
	    (sta->eht_capab->mac_cap & EHT_MACCAP_EPCS_PRIO)) {
		sta->mld_info.epcs.is_epcs_capable = true;
		sta->mld_info.epcs.state = EPCS_STATE_DISABLED;
	} else
		sta->mld_info.epcs.is_epcs_capable = false;
}


static int hostapd_print_epcs_mu_edca_params(struct hostapd_data *hapd,
					     char *buf, size_t buflen)
{
	int i, len = 0, ret;
	u8 *epcs_he_muedca;
	char *ac_name[4] = {"be", "bk", "vi", "vo"};

	for (i = 0; i < 4; i++) {
		switch (i) {
		case 0:
			epcs_he_muedca = hapd->conf->epcs_he_mu_edca.he_mu_ac_be_param;
			break;
		case 1:
			epcs_he_muedca = hapd->conf->epcs_he_mu_edca.he_mu_ac_bk_param;
			break;
		case 2:
			epcs_he_muedca = hapd->conf->epcs_he_mu_edca.he_mu_ac_vi_param;
			break;
		case 3:
			epcs_he_muedca = hapd->conf->epcs_he_mu_edca.he_mu_ac_vo_param;
			break;
		}

		ret = os_snprintf(buf + len, buflen - len,
				  "epcs_he_mu_edca_%s_aifsn: %d\n", ac_name[i],
				  get_bits_using_bitmask(epcs_he_muedca[HE_MU_AC_PARAM_ACI_IDX],
				  HE_MU_AC_PARAM_AIFSN));
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
		ret = os_snprintf(buf + len, buflen - len,
				  "epcs_he_mu_edca_%s_acm: %d\n", ac_name[i],
				  get_bits_using_bitmask(epcs_he_muedca[HE_MU_AC_PARAM_ACI_IDX],
				  HE_MU_AC_PARAM_ACM));
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
		ret = os_snprintf(buf + len, buflen - len,
				  "epcs_he_mu_edca_%s_aci: %d\n", ac_name[i],
				  get_bits_using_bitmask(epcs_he_muedca[HE_MU_AC_PARAM_ACI_IDX],
				  HE_MU_AC_PARAM_ACI));
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
		ret = os_snprintf(buf + len, buflen - len,
				  "epcs_he_mu_edca_%s_ecwmin: %d\n", ac_name[i],
				  get_bits_using_bitmask(epcs_he_muedca[HE_MU_AC_PARAM_ECW_IDX],
				  HE_MU_AC_PARAM_ECWMIN));
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
		ret = os_snprintf(buf + len, buflen - len,
				  "epcs_he_mu_edca_%s_ecwmax: %d\n", ac_name[i],
				  get_bits_using_bitmask(epcs_he_muedca[HE_MU_AC_PARAM_ECW_IDX],
				  HE_MU_AC_PARAM_ECWMAX));
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
		ret = os_snprintf(buf + len, buflen - len,
				  "epcs_he_mu_edca_%s_timer: %d\n", ac_name[i],
				  epcs_he_muedca[HE_MU_AC_PARAM_TIMER_IDX]);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	return len;
}


static int hostapd_print_wmm_params(struct hostapd_data *hapd,
				    char *buf, size_t buflen)
{
	int i, len = 0, ret;
	char *ac_name[4] = {"be", "bk", "vi", "vo"};

	for (i = 0; i < 4; i++) {
		ret = os_snprintf(buf + len, buflen - len,
				  "epcs_wmm_ac_%s_cwmin: %d\n", ac_name[i],
				  hapd->conf->epcs_wmm_ac_params[i].cwmin);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
		ret = os_snprintf(buf + len, buflen - len,
				  "epcs_wmm_ac_%s_cwmax: %d\n", ac_name[i],
				  hapd->conf->epcs_wmm_ac_params[i].cwmax);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
		ret = os_snprintf(buf + len, buflen - len,
				  "epcs_wmm_ac_%s_aifs: %d\n", ac_name[i],
				  hapd->conf->epcs_wmm_ac_params[i].aifs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
		ret = os_snprintf(buf + len, buflen - len,
				  "epcs_wmm_ac_%s_txop_limit: %d\n",
				  ac_name[i],
				  hapd->conf->epcs_wmm_ac_params[i].txop_limit);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	return len;
}

struct sta_info * hostapd_get_sta_info_from_mld_addr(struct hostapd_data *hapd,
						     const u8 *peer_mld_addr)
{
	struct sta_info *sta;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (sta->mld_info.mld_sta &&
		    (os_memcmp(sta->mld_info.common_info.mld_addr,
			       peer_mld_addr, ETH_ALEN) == 0)) {
			return sta;
		}
	}

	wpa_printf(MSG_DEBUG, "sta with requested mld_addr:" MACSTR ", is not present",
		   MAC2STR(peer_mld_addr));
	return NULL;
}

static int hostapd_epcs_authorize_mac_using_cli(struct hostapd_data *hapd,
						char *pos)
{
	u8 peer_mld_addr[ETH_ALEN];

	if (hwaddr_aton(pos, peer_mld_addr))
		return -1;

	if (hostapd_maclist_found(hapd->mld->epcs_authorized_mac,
				  hapd->mld->num_epcs_authorized_mac,
				  peer_mld_addr, NULL)) {
		wpa_printf(MSG_ERROR, "sta with mld_addr " MACSTR " already authorized for EPCS",
			   MAC2STR(peer_mld_addr));
		return -1;
	}

	return hostapd_add_acl_maclist(&hapd->mld->epcs_authorized_mac,
				       &hapd->mld->num_epcs_authorized_mac,
				       0, peer_mld_addr);
}

static int hostapd_epcs_deauthorize_mac_using_cli(struct hostapd_data *hapd,
						  char *pos)
{
	u8 peer_mld_addr[ETH_ALEN];
	struct sta_info *sta;
	struct wlan_epcs_info epcs_info;

	if (hwaddr_aton(pos, peer_mld_addr))
		return -1;

	if (!hostapd_maclist_found(hapd->mld->epcs_authorized_mac,
				   hapd->mld->num_epcs_authorized_mac,
				   peer_mld_addr, NULL)) {
		wpa_printf(MSG_ERROR, "sta with mld_addr " MACSTR " is not authorized for EPCS",
			   MAC2STR(peer_mld_addr));
		return -1;
	}

	sta = hostapd_get_sta_info_from_mld_addr(hapd, peer_mld_addr);
	if (sta) {
		epcs_info.action_code = WLAN_PROT_EHT_EPCS_ENABLE_TEARDOWN;
		hostapd_epcs_handle_and_send_action_frame(hapd, &epcs_info,
							  sta, false);
	}

	hostapd_remove_acl_mac(&hapd->mld->epcs_authorized_mac,
			       &hapd->mld->num_epcs_authorized_mac,
			       peer_mld_addr);

	return 0;
}

static int hostapd_epcs_show_authorized_clients(struct hostapd_data *hapd,
						char *buf, size_t buflen)
{
	int len = 0, i = 0, ret;

	/* Display the clients authorized through CLI*/
	while (i < hapd->mld->num_epcs_authorized_mac) {
		ret = os_snprintf(buf + len, buflen - len,
				  MACSTR "\n",
				  MAC2STR(
				  hapd->mld->epcs_authorized_mac[i].addr));
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
		i++;
	}

	return len;
}


int hostapd_epcs_handle_cli(struct hostapd_data *hapd, char *pos,
			    char *buf, size_t buflen)
{
	u8 peer_mld_addr[ETH_ALEN];
	struct wlan_epcs_info epcs_info = {0};

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
		wpa_printf(MSG_DEBUG, "EPCS: command on repurposed link");
		return -1;
	}
#endif /* CONFIG_QCN_EXTN */

	if (os_strncmp(pos, "session_initiate ", 17) == 0) {
		epcs_info.action_code = WLAN_PROT_EHT_EPCS_ENABLE_REQUEST;
		if (!hwaddr_aton((pos + 17), peer_mld_addr))
			return hostapd_epcs_handle_and_send_action_frame(hapd,
									 &epcs_info,
									 hostapd_get_sta_info_from_mld_addr(hapd, (const u8*)peer_mld_addr),
									 false);

	} else if (os_strncmp(pos, "session_terminate ", 18) == 0) {
		epcs_info.action_code = WLAN_PROT_EHT_EPCS_ENABLE_TEARDOWN;
		if (!hwaddr_aton((pos + 18), peer_mld_addr))
			return hostapd_epcs_handle_and_send_action_frame(hapd,
									 &epcs_info,
									 hostapd_get_sta_info_from_mld_addr(hapd, (const u8*)peer_mld_addr),
									 false);

	} else if (os_strncmp(pos, "deauthorize_mac ", 16) == 0) {
		return hostapd_epcs_deauthorize_mac_using_cli(hapd, pos + 16);

	} else if (os_strncmp(pos, "authorize_mac ", 14) == 0) {
		return hostapd_epcs_authorize_mac_using_cli(hapd, pos + 14);

	} else if (os_strncmp(pos, "show ", 5) == 0) {
		if (os_strncmp(pos + 5, "mu_edca_params", 15) == 0) {
			return hostapd_print_epcs_mu_edca_params(hapd, buf, buflen);
		} else if (os_strncmp(pos + 5, "wmm_params", 10) == 0) {
			return hostapd_print_wmm_params(hapd, buf, buflen);

		} else if (os_strncmp(pos + 5, "authorized_clients", 18) == 0) {
			return hostapd_epcs_show_authorized_clients(hapd,
								    buf,
								    buflen);

		} else {
			wpa_printf(MSG_ERROR, "specify the params to be displayed");
			return -1;
		}

	} else {
		wpa_printf(MSG_ERROR, "invalid epcs command");
		return -1;
	}

	return 0;
}


int hostapd_ctrl_iface_negotiated_ttlm_capabilities(struct hostapd_data *hapd,
							   char *buf, size_t buflen)
{
	int len = 0, ttlm_cap, ret;
	u16 mld_cap;

	mld_cap = hapd->iface->mld_mld_capa;
	ttlm_cap = (mld_cap & EHT_ML_MLD_CAPA_TID_TO_LINK_MAP_NEG_SUPP_MSK) >> 5;

	ret = os_snprintf(buf + len, buflen - len, "ttlm_capability:%d\n", ttlm_cap);

	if (os_snprintf_error(buflen - len, ret))
		return len;

	len += ret;

	return len;
}


int hostapd_ctrl_iface_negotiated_ttlm_config(struct hostapd_data *hapd, const char *cmd,
					      char *buf, size_t buflen)
{
	struct ttlm_prev_negotiated_info *negotiated_ttlm;
	int dir = -1, len = 0, tid = 0, ret;
	struct ttlm_info *ttlm_info;
	struct sta_info *sta;
	u8 addr[ETH_ALEN];

	if (!hapd->conf->ttlm_enable) {
		wpa_printf(MSG_ERROR, "TTLM negotiation support is disabled");
		return -1;
	}

	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_ERROR, "Invalid STA MAC address");
		return -1;
	}

	sta = ap_get_sta(hapd, addr);
	if (!sta) {
		wpa_printf(MSG_ERROR, "Station " MACSTR
			   " not found for Negotiated TTLM config",
			   MAC2STR(addr));
		return -1;
	}

	negotiated_ttlm = &sta->mld_info.tid_map_info.ttlm_prev_negotiated_info;

	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++) {
		ttlm_info = negotiated_ttlm->ttlm_info;

		if (ttlm_info[dir].direction == TTLM_DIRECTION_INVALID)
			continue;

		ret = os_snprintf(buf + len, buflen - len, "direction:%d\n",
				  ttlm_info[dir].direction);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;

		ret = os_snprintf(buf + len, buflen - len, "default_link_mapping:%d\n",
				  ttlm_info[dir].default_link_mapping);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;

		for (tid = 0; tid < NUM_MAX_TIDS; tid++) {
			ret = os_snprintf(buf + len, buflen - len, "ieee_link_map_tid[%d]:0x%x\n",
					  tid, ttlm_info[dir].ieee_link_map_tid[tid]);
			if (!os_snprintf_error(buflen - len, ret))
				len += ret;
		}

		ret = os_snprintf(buf + len, buflen - len, "link_mapping_size:%d\n",
				  ttlm_info[dir].link_mapping_size);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;

		ret = os_snprintf(buf + len, buflen - len, "mapping_switch_time_present:%d\n",
				  ttlm_info[dir].mapping_switch_time_present);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;

		ret = os_snprintf(buf + len, buflen - len, "expected_duration_present:%d\n",
				  ttlm_info[dir].expected_duration_present);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;

		ret = os_snprintf(buf + len, buflen - len, "mapping_switch_time:%u\n",
				  ttlm_info[dir].mapping_switch_time);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;

		ret = os_snprintf(buf + len, buflen - len, "expected_duration:%u\n",
				  ttlm_info[dir].expected_duration);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

	return len;
}

u32 wnm_neighbor_report_get_pref_link_mask(const u8 *neigh_rep,
					   size_t neigh_rep_len)
{
	const u8 *ie, *ie_end, *val, *val_end, *ml, *ml_end, *ml_val, *ml_val_end;
	u8 eid, elen, id, len, common_len, ml_id, ml_len, link_id, own_link_id;
	const u8 *end = neigh_rep + neigh_rep_len;
	const u8 *pos = neigh_rep;
	u32 mask = 0;
	u16 sta_ctrl;
	size_t off;

#define EHT_ML_COMMON_INFO_LEN 13
	while (pos + 2 <= end) {
		eid = pos[0];
		elen = pos[1];
		ie = pos + 2;
		ie_end = ie + elen;

		pos += 2 + elen;

		if (ie_end > end)
			break;

		if (eid != WLAN_EID_NEIGHBOR_REPORT ||
		    elen < EHT_ML_COMMON_INFO_LEN)
			continue;

		/* Neighbor Report subelements */
		off = EHT_ML_COMMON_INFO_LEN;
		while (ie + off + 2 <= ie_end) {
			id = ie[off];
			len = ie[off + 1];
			val = ie + off + 2;
			val_end = val + len;

			if (val_end > ie_end)
				break;

			off += 2 + len;

			if (id != WNM_NEIGHBOR_MULTI_LINK || len < 3)
				continue;

			/* BMLE: [ml_ctrl(2)][common_len(1)]
			 * [Common Info][ML subelems...]
			 */
			common_len = val[2];

			/* Ensure Common Info is fully present */
			if (val + 2 + common_len > val_end)
				continue;

			/* If Common Info has [Length] [MLD addr(6)] + [Link-ID(1)],
			 * consume it
			 */
			if (common_len >= 1 + ETH_ALEN + 1) {
				own_link_id = val[2 + 1 + ETH_ALEN];

				if (own_link_id < MAX_NUM_MLD_LINKS)
					mask |= BIT(own_link_id);
			}

			/* Iterate ML subelements after Common Info */
			ml = val + 2 + common_len;
			ml_end = val_end;

			while (ml + 2 <= ml_end) {
				ml_id = ml[0];
				ml_len = ml[1];
				ml_val = ml + 2;
				ml_val_end = ml_val + ml_len;

				if (ml_val_end > ml_end)
					break;
				ml += 2 + ml_len;

				if (ml_id != MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE ||
				    ml_len < 2)
					continue;

				/* Link-ID is lower 4 bits of STA control (LE16) */
				sta_ctrl = WPA_GET_LE16(ml_val);
				link_id = sta_ctrl & 0x0f;

				if (link_id < MAX_NUM_MLD_LINKS)
					mask |= BIT(link_id);
			}
		}
	}
	return mask;
}
