/*
 * hostapd / IEEE 802.11ax HE
 * Copyright (c) 2016-2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2019 John Crispin <john@phrozen.org>
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/hw_features_common.h"
#include "hostapd.h"
#include "ap_config.h"
#include "beacon.h"
#include "sta_info.h"
#include "ieee802_11.h"
#include "dfs.h"
#include "wmm.h"

static u8 ieee80211_he_ppet_size(u8 ppe_thres_hdr, const u8 *phy_cap_info)
{
	u8 sz = 0, ru;

	if ((phy_cap_info[HE_PHYCAP_PPE_THRESHOLD_PRESENT_IDX] &
	     HE_PHYCAP_PPE_THRESHOLD_PRESENT) == 0)
		return 0;

	ru = (ppe_thres_hdr >> HE_PPE_THRES_RU_INDEX_BITMASK_SHIFT) &
		HE_PPE_THRES_RU_INDEX_BITMASK_MASK;
	/* Count the number of 1 bits in RU Index Bitmask */
	while (ru) {
		if (ru & 0x1)
			sz++;
		ru >>= 1;
	}

	/* fixed header of 3 (NSTS) + 4 (RU Index Bitmask) = 7 bits */
	/* 6 * (NSTS + 1) bits for bit 1 in RU Index Bitmask */
	sz *= 1 + (ppe_thres_hdr & HE_PPE_THRES_NSS_MASK);
	sz = (sz * 6) + 7;
	/* PPE Pad to count the number of needed full octets */
	sz = (sz + 7) / 8;

	return sz;
}


static u8 ieee80211_he_mcs_set_size(const u8 *phy_cap_info)
{
	u8 sz = 4;

	if (phy_cap_info[HE_PHYCAP_CHANNEL_WIDTH_SET_IDX] &
	    HE_PHYCAP_CHANNEL_WIDTH_SET_80PLUS80MHZ_IN_5G)
		sz += 4;
	if (phy_cap_info[HE_PHYCAP_CHANNEL_WIDTH_SET_IDX] &
	    HE_PHYCAP_CHANNEL_WIDTH_SET_160MHZ_IN_5G)
		sz += 4;

	return sz;
}


static int ieee80211_invalid_he_cap_size(const u8 *buf, size_t len)
{
	struct ieee80211_he_capabilities *cap;
	size_t cap_len;
	u8 ppe_thres_hdr;

	cap = (struct ieee80211_he_capabilities *) buf;
	cap_len = sizeof(cap->he_mac_capab_info) +
		  sizeof(cap->he_phy_capab_info);
	if (len < cap_len)
		return 1;

	cap_len += ieee80211_he_mcs_set_size(cap->he_phy_capab_info);
	if (len < cap_len)
		return 1;

	ppe_thres_hdr = len > cap_len ? buf[cap_len] : 0xff;
	cap_len += ieee80211_he_ppet_size(ppe_thres_hdr,
					  cap->he_phy_capab_info);

	return len < cap_len;
}

static bool hostapd_conf_he_twt_enabled(struct hostapd_data *hapd)
{
	return (hapd->conf->twt_responder_caps > TWT_DISABLED);
}

static bool hostapd_conf_he_btwt_enabled(struct hostapd_data *hapd)
{
	return (hapd->conf->twt_responder_caps >= TWT_ITWT_BTWT_ENABLED);
}

static void intersect_he_mcs_map(u8 *pos, const u8 *hw_mcs, size_t mcs_len,
				 const u32 *usr_tx_mcs, const u32 *usr_rx_mcs)
{
	size_t bw, bw_idx, byte_off;
	u16 hw_rx, hw_tx, usr_rx, usr_tx, out_rx, out_tx;
	int nss;

	for (bw = 0; bw < mcs_len / 4; bw++) {
		byte_off = bw * 4;
		bw_idx = bw < HE_MCS_NSS_SET ? bw : HE_MCS_NSS_SET - 1;

		hw_rx  = WPA_GET_LE16(hw_mcs + byte_off);
		hw_tx  = WPA_GET_LE16(hw_mcs + byte_off + 2);
		/* HE_MCS_NSS_MAP_UNSET is both the default (no config) and the
		 * explicit user value to request hardware defaults for a slot. */
		usr_rx = (u16)(usr_rx_mcs[bw_idx] == HE_MCS_NSS_MAP_UNSET ?
			       hw_rx : usr_rx_mcs[bw_idx]);
		usr_tx = (u16)(usr_tx_mcs[bw_idx] == HE_MCS_NSS_MAP_UNSET ?
			       hw_tx : usr_tx_mcs[bw_idx]);

		out_rx = 0;
		out_tx = 0;
		for (nss = 0; nss < 8; nss++) {
			u16 hw_rx_nss  = (hw_rx >> (nss * 2)) & 0x3;
			u16 hw_tx_nss  = (hw_tx >> (nss * 2)) & 0x3;
			u16 usr_rx_nss = (usr_rx >> (nss * 2)) & 0x3;
			u16 usr_tx_nss = (usr_tx >> (nss * 2)) & 0x3;

			if (hw_rx_nss == 0x3 || usr_rx_nss == 0x3)
				out_rx |= (u16)0x3 << (nss * 2);
			else
				out_rx |= (u16)MIN(hw_rx_nss, usr_rx_nss) <<
					  (nss * 2);

			if (hw_tx_nss == 0x3 || usr_tx_nss == 0x3)
				out_tx |= (u16)0x3 << (nss * 2);
			else
				out_tx |= (u16)MIN(hw_tx_nss, usr_tx_nss) <<
					  (nss * 2);
		}
		WPA_PUT_LE16(pos + byte_off, out_rx);
		WPA_PUT_LE16(pos + byte_off + 2, out_tx);
	}
}

u8 * hostapd_eid_he_capab(struct hostapd_data *hapd, u8 *eid,
			  enum ieee80211_op_mode opmode)
{
	struct ieee80211_he_capabilities *cap;
	struct hostapd_hw_modes *mode = hapd->iface->current_mode;
	struct hostapd_data *tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
	const struct he_capabilities *he_capab;
	u8 *pos = eid;
	u8 ie_size = 0, mcs_nss_size, ppet_size;
	u8 *epos;
	bool su_beamformee;
	bool chanwidth_gt80;
	u8 bfee_sts_lteq80;
	u8 bfee_sts_gt80;
	u8 multi_tid_aggr;
	u8 multi_tid_aggr_tx;
	u8 max_ampdu_len_exp;
	u8 fragmentation;
	u8 max_frag_msdu;
	u8 min_frag_size;
	u8 max_nc;

	if (!mode)
		return eid;

	he_capab = &mode->he_capab[opmode];

	mcs_nss_size = ieee80211_he_mcs_set_size(he_capab->phy_cap);
	ppet_size = ieee80211_he_ppet_size(he_capab->ppet[0],
					   he_capab->phy_cap);

	ie_size = IEEE80211_HE_CAPAB_MIN_LEN + mcs_nss_size + ppet_size;

	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 1 + ie_size;
	*pos++ = WLAN_EID_EXT_HE_CAPABILITIES;

	cap = (struct ieee80211_he_capabilities *) pos;
	os_memset(cap, 0, sizeof(*cap));

	os_memcpy(cap->he_mac_capab_info, he_capab->mac_cap,
		  HE_MAX_MAC_CAPAB_SIZE);
	os_memcpy(cap->he_phy_capab_info, he_capab->phy_cap,
		  HE_MAX_PHY_CAPAB_SIZE);
	epos = (u8 *) &cap->he_basic_supported_mcs_set;
	if (hostapd_he_mcs_nss_set_is_set(hapd->conf->he_tx_mcs_nss_set) ||
	    hostapd_he_mcs_nss_set_is_set(hapd->conf->he_rx_mcs_nss_set))
		intersect_he_mcs_map(epos, he_capab->mcs, mcs_nss_size,
				     hapd->conf->he_tx_mcs_nss_set,
				     hapd->conf->he_rx_mcs_nss_set);
	else
		os_memcpy(epos, he_capab->mcs, mcs_nss_size);
	epos += mcs_nss_size;

	if (ppet_size)
		os_memcpy(epos, he_capab->ppet, ppet_size);

	if (!hostapd_conf_he_btwt_enabled(hapd))
		cap->he_mac_capab_info[HE_MAC_CAPAB_2] &= ~HE_MACCAP_TWT_BROADCAST;

	if (!hostapd_conf_he_twt_enabled(hapd)) {
		cap->he_mac_capab_info[HE_MAC_CAPAB_0] &= ~HE_MACCAP_TWT_RESPONDER;
		cap->he_mac_capab_info[HE_MAC_CAPAB_3] &= ~HE_MACCAP_FLEXI_TWT;
	}

	/* For non-transmitting BSSs in MBSSID, inherit BSS-level overrides
	 * from the transmitting BSS */
	if (tx_hapd != hapd && tx_hapd->conf->he_phy_capab_mask) {
		hapd->conf->he_phy_capab = tx_hapd->conf->he_phy_capab;
		hapd->conf->he_phy_capab_mask = tx_hapd->conf->he_phy_capab_mask;
	}

	if (hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_MULTI_TID_AGGR) {
		multi_tid_aggr = hapd->conf->he_phy_capab.he_multi_tid_aggr;

		cap->he_mac_capab_info[HE_MACCAP_MULTI_TID_AGGR_RX_IDX] &=
			~HE_MACCAP_MULTI_TID_AGGR_RX_MASK;
		cap->he_mac_capab_info[HE_MACCAP_MULTI_TID_AGGR_RX_IDX] |=
			(multi_tid_aggr << HE_MACCAP_MULTI_TID_AGGR_RX_SHIFT) &
			HE_MACCAP_MULTI_TID_AGGR_RX_MASK;
	}

	if (hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_MAX_AMPDU_LEN_EXP) {
		max_ampdu_len_exp = hapd->conf->he_phy_capab.he_max_ampdu_len_exp;

		cap->he_mac_capab_info[HE_MACCAP_MAX_AMPDU_LEN_EXP_IDX] &=
			~HE_MACCAP_MAX_AMPDU_LEN_EXP_MASK;
		cap->he_mac_capab_info[HE_MACCAP_MAX_AMPDU_LEN_EXP_IDX] |=
			(max_ampdu_len_exp << HE_MACCAP_MAX_AMPDU_LEN_EXP_SHIFT) &
			HE_MACCAP_MAX_AMPDU_LEN_EXP_MASK;
	}

	if (hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_FRAGMENTATION) {
		fragmentation = hapd->conf->he_phy_capab.he_fragmentation;

		cap->he_mac_capab_info[HE_MACCAP_FRAGMENTATION_IDX] &=
			~HE_MACCAP_FRAGMENTATION_MASK;
		cap->he_mac_capab_info[HE_MACCAP_FRAGMENTATION_IDX] |=
			(fragmentation << HE_MACCAP_FRAGMENTATION_SHIFT) &
			HE_MACCAP_FRAGMENTATION_MASK;
	}

	if (hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_MAX_FRAG_MSDU) {
		max_frag_msdu = hapd->conf->he_phy_capab.he_max_frag_msdu;

		cap->he_mac_capab_info[HE_MACCAP_MAX_FRAG_MSDU_IDX] &=
			~HE_MACCAP_MAX_FRAG_MSDU_MASK;
		cap->he_mac_capab_info[HE_MACCAP_MAX_FRAG_MSDU_IDX] |=
			(max_frag_msdu << HE_MACCAP_MAX_FRAG_MSDU_SHIFT) &
			HE_MACCAP_MAX_FRAG_MSDU_MASK;
	}

	if (hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_MIN_FRAG_SIZE) {
		min_frag_size = hapd->conf->he_phy_capab.he_min_frag_size;

		cap->he_mac_capab_info[HE_MACCAP_MIN_FRAG_SIZE_IDX] &=
			~HE_MACCAP_MIN_FRAG_SIZE_MASK;
		cap->he_mac_capab_info[HE_MACCAP_MIN_FRAG_SIZE_IDX] |=
			(min_frag_size << HE_MACCAP_MIN_FRAG_SIZE_SHIFT) &
			HE_MACCAP_MIN_FRAG_SIZE_MASK;
	}

	if (hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_OMI) {
		if (hapd->conf->he_phy_capab.he_omi)
			cap->he_mac_capab_info[HE_MACCAP_OMI_IDX] |=
				HE_MACCAP_OMI;
		else
			cap->he_mac_capab_info[HE_MACCAP_OMI_IDX] &=
				~HE_MACCAP_OMI;
	}

	if (hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_BSR_SUPPORT) {
		if (hapd->conf->he_phy_capab.he_bsr_support)
			cap->he_mac_capab_info[HE_MACCAP_BSR_IDX] |=
				HE_MACCAP_BSR;
		else
			cap->he_mac_capab_info[HE_MACCAP_BSR_IDX] &=
				~HE_MACCAP_BSR;
	}

	if (hapd->conf->he_phy_capab_mask &
	    HE_PHY_BSS_OVR_AMSDU_IN_AMPDU_SUPRT) {
		if (hapd->conf->he_phy_capab.he_amsdu_in_ampdu_suprt)
			cap->he_mac_capab_info[HE_MACCAP_AMSDU_IN_AMPDU_IDX] |=
				HE_MACCAP_AMSDU_IN_AMPDU;
		else
			cap->he_mac_capab_info[HE_MACCAP_AMSDU_IN_AMPDU_IDX] &=
				~HE_MACCAP_AMSDU_IN_AMPDU;
	}

	if (hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_MULTI_TID_AGGR_TX) {
		multi_tid_aggr_tx = hapd->conf->he_phy_capab.he_multi_tid_aggr_tx;

		cap->he_mac_capab_info[HE_MACCAP_MULTI_TID_AGGR_TX_LO_IDX] &=
			~HE_MACCAP_MULTI_TID_AGGR_TX_LO_MASK;
		cap->he_mac_capab_info[HE_MACCAP_MULTI_TID_AGGR_TX_HI_IDX] &=
			~HE_MACCAP_MULTI_TID_AGGR_TX_HI_MASK;

		if (multi_tid_aggr_tx & 0x1)
			cap->he_mac_capab_info[HE_MACCAP_MULTI_TID_AGGR_TX_LO_IDX] |=
				HE_MACCAP_MULTI_TID_AGGR_TX_LO_MASK;

		cap->he_mac_capab_info[HE_MACCAP_MULTI_TID_AGGR_TX_HI_IDX] |=
			(multi_tid_aggr_tx >> 1) &
			HE_MACCAP_MULTI_TID_AGGR_TX_HI_MASK;
	}

	if (hapd->conf->he_phy_capab_mask &
	    HE_PHY_BSS_OVR_SU_PPDU_1X_LTF_800NS_GI) {
		if (hapd->conf->he_phy_capab.he_su_ppdu_1x_ltf_800ns_gi)
			cap->he_phy_capab_info
				[HE_PHYCAP_SU_PPDU_1X_LTF_800NS_GI_IDX] |=
				HE_PHYCAP_SU_PPDU_1X_LTF_800NS_GI;
		else
			cap->he_phy_capab_info
				[HE_PHYCAP_SU_PPDU_1X_LTF_800NS_GI_IDX] &=
				~HE_PHYCAP_SU_PPDU_1X_LTF_800NS_GI;
	}

	if (hapd->conf->he_phy_capab_mask &
	    HE_PHY_BSS_OVR_NDP_4X_LTF_3200NS_GI) {
		if (hapd->conf->he_phy_capab.he_ndp_4x_ltf_3200ns_gi)
			cap->he_phy_capab_info[HE_PHYCAP_NDP_4X_LTF_3200NS_GI_IDX] |=
				HE_PHYCAP_NDP_4X_LTF_3200NS_GI;
		else
			cap->he_phy_capab_info[HE_PHYCAP_NDP_4X_LTF_3200NS_GI_IDX] &=
				~HE_PHYCAP_NDP_4X_LTF_3200NS_GI;
	}

	if (hapd->conf->he_phy_capab_mask &
	    HE_PHY_BSS_OVR_SU_MU_PPDU_4X_LTF_800NS_GI) {
		if (hapd->conf->he_phy_capab.he_su_mu_ppdu_4x_ltf_800ns_gi)
			cap->he_phy_capab_info
				[HE_PHYCAP_SU_MU_PPDU_4X_LTF_800NS_GI_IDX] |=
				HE_PHYCAP_SU_MU_PPDU_4X_LTF_800NS_GI;
		else
			cap->he_phy_capab_info
				[HE_PHYCAP_SU_MU_PPDU_4X_LTF_800NS_GI_IDX] &=
				~HE_PHYCAP_SU_MU_PPDU_4X_LTF_800NS_GI;
	}

	if (hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_MAX_NC_SUPRT) {
		max_nc = hapd->conf->he_phy_capab.he_max_nc;

		cap->he_phy_capab_info[HE_PHYCAP_MAX_NC_IDX] &=
			~HE_PHYCAP_MAX_NC_MASK;
		cap->he_phy_capab_info[HE_PHYCAP_MAX_NC_IDX] |=
			(max_nc << HE_PHYCAP_MAX_NC_SHIFT) &
			HE_PHYCAP_MAX_NC_MASK;
	}

	if (hapd->conf->he_phy_capab_mask &
	    HE_PHY_BSS_OVR_ER_SU_PPDU_1X_LTF_800NS_GI) {
		if (hapd->conf->he_phy_capab.he_er_su_ppdu_1x_ltf_800ns_gi)
			cap->he_phy_capab_info
				[HE_PHYCAP_ER_SU_PPDU_1X_LTF_800NS_GI_IDX] |=
				HE_PHYCAP_ER_SU_PPDU_1X_LTF_800NS_GI;
		else
			cap->he_phy_capab_info
				[HE_PHYCAP_ER_SU_PPDU_1X_LTF_800NS_GI_IDX] &=
				~HE_PHYCAP_ER_SU_PPDU_1X_LTF_800NS_GI;
	}

	if (hapd->conf->he_phy_capab_mask &
	    HE_PHY_BSS_OVR_ER_SU_PPDU_4X_LTF_800NS_GI) {
		if (hapd->conf->he_phy_capab.he_er_su_ppdu_4x_ltf_800ns_gi)
			cap->he_phy_capab_info
				[HE_PHYCAP_ER_SU_PPDU_4X_LTF_800NS_GI_IDX] |=
				HE_PHYCAP_ER_SU_PPDU_4X_LTF_800NS_GI;
		else
			cap->he_phy_capab_info
				[HE_PHYCAP_ER_SU_PPDU_4X_LTF_800NS_GI_IDX] &=
				~HE_PHYCAP_ER_SU_PPDU_4X_LTF_800NS_GI;
	}

	if (hapd->conf->he_phy_capab_mask &
	    HE_PHY_BSS_OVR_1024QAM_LT242RU_RX_ENABLE) {
		if (hapd->conf->he_phy_capab.he_1024qam_lt242ru_rx_enable)
			cap->he_phy_capab_info[HE_PHYCAP_RX_1024QAM_LT242RU_IDX] |=
				HE_PHYCAP_RX_1024QAM_LT242RU;
		else
			cap->he_phy_capab_info[HE_PHYCAP_RX_1024QAM_LT242RU_IDX] &=
				~HE_PHYCAP_RX_1024QAM_LT242RU;
	}

	if (((hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_SU_BEAMFORMER) ?
	     hapd->conf->he_phy_capab.he_su_beamformer :
	     hapd->iface->conf->he_phy_capab.he_su_beamformer))
		cap->he_phy_capab_info[HE_PHYCAP_SU_BEAMFORMER_CAPAB_IDX] |=
			HE_PHYCAP_SU_BEAMFORMER_CAPAB;
	else
		cap->he_phy_capab_info[HE_PHYCAP_SU_BEAMFORMER_CAPAB_IDX] &=
			~HE_PHYCAP_SU_BEAMFORMER_CAPAB;

	su_beamformee =
		((hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_SU_BEAMFORMEE) ?
		 hapd->conf->he_phy_capab.he_su_beamformee :
		 hapd->iface->conf->he_phy_capab.he_su_beamformee);

	if (su_beamformee)
		cap->he_phy_capab_info[HE_PHYCAP_SU_BEAMFORMEE_CAPAB_IDX] |=
			HE_PHYCAP_SU_BEAMFORMEE_CAPAB;
	else
		cap->he_phy_capab_info[HE_PHYCAP_SU_BEAMFORMEE_CAPAB_IDX] &=
			~HE_PHYCAP_SU_BEAMFORMEE_CAPAB;

	/*
	 * HE BFEE STS fields are only valid when SU BFEE is enabled.
	 * For >80 MHz, advertise non-zero only if the width set supports it.
	 */
	if ((hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_BFEE_STS_LTEQ80) ||
	    (hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_BFEE_STS_GT80)) {
		bfee_sts_lteq80 =
			(hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_BFEE_STS_LTEQ80) ?
			hapd->conf->he_phy_capab.he_bfee_sts_lteq80 :
			((cap->he_phy_capab_info[HE_PHYCAP_BFEE_STS_LTEQ80_IDX] &
			  HE_PHYCAP_BFEE_STS_LTEQ80_MASK) >>
			 HE_PHYCAP_BFEE_STS_LTEQ80_SHIFT);

		bfee_sts_gt80 =
			(hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_BFEE_STS_GT80) ?
			hapd->conf->he_phy_capab.he_bfee_sts_gt80 :
			((cap->he_phy_capab_info[HE_PHYCAP_BFEE_STS_GT80_IDX] &
			  HE_PHYCAP_BFEE_STS_GT80_MASK) >>
			 HE_PHYCAP_BFEE_STS_GT80_SHIFT);

		cap->he_phy_capab_info[HE_PHYCAP_BFEE_STS_LTEQ80_IDX] &=
			~HE_PHYCAP_BFEE_STS_LTEQ80_MASK;
		cap->he_phy_capab_info[HE_PHYCAP_BFEE_STS_GT80_IDX] &=
			~HE_PHYCAP_BFEE_STS_GT80_MASK;

		chanwidth_gt80 =
			!!(cap->he_phy_capab_info[HE_PHYCAP_CHANNEL_WIDTH_SET_IDX] &
			   (HE_PHYCAP_CHANNEL_WIDTH_SET_160MHZ_IN_5G |
			    HE_PHYCAP_CHANNEL_WIDTH_SET_80PLUS80MHZ_IN_5G));

		if (su_beamformee) {
			cap->he_phy_capab_info[HE_PHYCAP_BFEE_STS_LTEQ80_IDX] |=
				(bfee_sts_lteq80 << HE_PHYCAP_BFEE_STS_LTEQ80_SHIFT) &
				HE_PHYCAP_BFEE_STS_LTEQ80_MASK;
			if (chanwidth_gt80) {
				cap->he_phy_capab_info[HE_PHYCAP_BFEE_STS_GT80_IDX] |=
					(bfee_sts_gt80 << HE_PHYCAP_BFEE_STS_GT80_SHIFT) &
					HE_PHYCAP_BFEE_STS_GT80_MASK;
			}
		}
	}

	if (((hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_MU_BEAMFORMER) ?
	 hapd->conf->he_phy_capab.he_mu_beamformer :
	 hapd->iface->conf->he_phy_capab.he_mu_beamformer)) {
	cap->he_phy_capab_info[HE_PHYCAP_MU_BEAMFORMER_CAPAB_IDX] |=
		HE_PHYCAP_MU_BEAMFORMER_CAPAB;
	} else {
	cap->he_phy_capab_info[HE_PHYCAP_MU_BEAMFORMER_CAPAB_IDX] &=
		~HE_PHYCAP_MU_BEAMFORMER_CAPAB;
	}

	if (((hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_UL_MUMIMO) ?
	 hapd->conf->he_phy_capab.he_ul_mumimo :
	 hapd->iface->conf->he_phy_capab.he_ul_mumimo) == 1) {
	cap->he_phy_capab_info[HE_PHYCAP_UL_MUMIMO_CAPB_IDX] |=
		HE_PHYCAP_UL_MUMIMO_CAPB;
	} else if (((hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_UL_MUMIMO) ?
	      hapd->conf->he_phy_capab.he_ul_mumimo :
	      hapd->iface->conf->he_phy_capab.he_ul_mumimo) == 0) {
		cap->he_phy_capab_info[HE_PHYCAP_UL_MUMIMO_CAPB_IDX] &=
			~HE_PHYCAP_UL_MUMIMO_CAPB;
	}

	pos += ie_size;

	return pos;
}


u8 * hostapd_eid_he_operation(struct hostapd_data *hapd, u8 *eid)
{
	struct ieee80211_he_operation *oper;
	u8 *pos = eid;
	int oper_size = 6;
	u32 params = 0;

	if (!hapd->iface->current_mode)
		return eid;

	if (is_6ghz_op_class(hapd->iconf->op_class))
		oper_size += 5;

	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 1 + oper_size;
	*pos++ = WLAN_EID_EXT_HE_OPERATION;

	oper = (struct ieee80211_he_operation *) pos;
	os_memset(oper, 0, sizeof(*oper));

	if (hapd->iface->conf->he_op.he_default_pe_duration)
		params |= (hapd->iface->conf->he_op.he_default_pe_duration <<
			   HE_OPERATION_DFLT_PE_DURATION_OFFSET);

	if (hapd->iface->conf->he_op.he_twt_required)
		params |= HE_OPERATION_TWT_REQUIRED;

	if (hapd->iface->conf->he_op.he_rts_threshold)
		params |= (hapd->iface->conf->he_op.he_rts_threshold <<
			   HE_OPERATION_RTS_THRESHOLD_OFFSET);

	if ((hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_ER_SU_DISABLE) ?
	    hapd->conf->he_phy_capab.he_er_su_disable :
	    hapd->iface->conf->he_op.he_er_su_disable)
		params |= HE_OPERATION_ER_SU_DISABLE;

	if (hapd->iface->conf->he_op.he_bss_color_disabled ||
	    hapd->cca_in_progress)
		params |= HE_OPERATION_BSS_COLOR_DISABLED;
	if (hapd->iface->conf->he_op.he_bss_color_partial)
		params |= HE_OPERATION_BSS_COLOR_PARTIAL;
	params |= hapd->iface->conf->he_op.he_bss_color <<
		HE_OPERATION_BSS_COLOR_OFFSET;

	/* HE minimum required basic MCS and NSS for STAs */
	oper->he_mcs_nss_set =
		host_to_le16(hapd->iface->conf->he_op.he_basic_mcs_nss_set);

	/* TODO: conditional MaxBSSID Indicator subfield */

	pos += 6; /* skip the fixed part */

	if (is_6ghz_op_class(hapd->iconf->op_class)) {
		enum oper_chan_width oper_chwidth =
			hapd->iconf->he_oper_chwidth;
		u8 seg0 = hapd->iconf->he_oper_centr_freq_seg0_idx;
		u8 seg1 = hostapd_get_oper_centr_freq_seg1_idx(hapd->iconf);
		u8 control;
#ifdef CONFIG_IEEE80211BE
		u16 punct_bitmap = hostapd_get_punct_bitmap(hapd);

		if (punct_bitmap) {
			oper_chwidth = hostapd_get_oper_chwidth(hapd->iconf);
			seg0 = hostapd_get_oper_centr_freq_seg0_idx(
				hapd->iconf);
			punct_update_legacy_bw(punct_bitmap,
					       hapd->iconf->channel,
					       &oper_chwidth, &seg0, &seg1);
		}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_QCN_EXTN
		if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
			hostapd_get_oper_info_of_repurposed_bss_extn(
					hapd, &oper_chwidth,
					&seg0, &seg1);
#endif /* CONFIG_QCN_EXTN */

		if (!seg0)
			seg0 = hapd->iconf->channel;

		params |= HE_OPERATION_6GHZ_OPER_INFO;

		/* 6 GHz Operation Information field
		 * IEEE Std 802.11ax-2021, 9.4.2.249 HE Operation element,
		 * Figure 9-788k
		 */
		*pos++ = hapd->iconf->channel; /* Primary Channel */

		/* Control:
		 *	bits 0-1: Channel Width
		 *	bit 2: Duplicate Beacon
		 *	bits 3-5: Regulatory Info
		 */
		/* Channel Width */
		if (seg1)
			control = 3;
		else
			control = center_idx_to_bw_6ghz(seg0);

		if (hostapd_is_indoor_sp_pwr_mode(hapd)) {
			control |= HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP <<
				HE_6GHZ_OPER_INFO_CTRL_REG_INFO_SHIFT;
		} else {
			control |= hapd->iconf->he_6ghz_reg_pwr_type <<
				HE_6GHZ_OPER_INFO_CTRL_REG_INFO_SHIFT;
		}

		if (center_idx_to_bw_6ghz(seg0) &&
		    !is_6ghz_psc_frequency(ieee80211_chan_to_freq(NULL,
				    hapd->iconf->op_class, hapd->iconf->channel)) &&
				    hapd->conf->rate_type == BEACON_RATE_LEGACY)
			control |= HE_6GHZ_OPER_INFO_CTRL_DUP_BEACON;

		*pos++ = control;

		/* Channel Center Freq Seg0/Seg1 */
		if (oper_chwidth == CONF_OPER_CHWIDTH_160MHZ ||
		    oper_chwidth == CONF_OPER_CHWIDTH_320MHZ) {
			/*
			 * Seg 0 indicates the channel center frequency index of
			 * the 160 MHz channel.
			 */
			seg1 = seg0;
			if (hapd->iconf->channel < seg0)
				seg0 -= 8;
			else
				seg0 += 8;
		}

		*pos++ = seg0;
		*pos++ = seg1;
		*pos++ = hapd->iconf->he_6ghz_min_rate;
	}

	oper->he_oper_params = host_to_le32(params);

	return pos;
}


u8 * hostapd_eid_he_mu_edca_parameter_set(struct hostapd_data *hapd, u8 *eid, bool is_epcs)
{
	struct ieee80211_he_mu_edca_parameter_set *edca;
	struct hostapd_wmm_ac_params wmmp[WMM_AC_NUM];
	u8 *pos, updated_count;
	size_t i;
	struct ieee80211_he_mu_edca_parameter_set *he_mu_edca = NULL;

	 /* Updating WME Parameter Set Count to avoid mismatch */
	 os_memset(wmmp, 0, sizeof(wmmp));

	 if (hapd->conf->wmm_enabled)
		wmm_calc_regulatory_limit(hapd, wmmp, is_epcs);

	if (!is_epcs)
		he_mu_edca = &hapd->iface->conf->he_mu_edca;
#ifdef CONFIG_IEEE80211BE
	else
		he_mu_edca = &hapd->conf->epcs_he_mu_edca;
#endif /* CONFIG_IEEE80211BE */

	if (!he_mu_edca) {
		wpa_printf(MSG_ERROR,
			   "he_mu_edca is NULL, is_epcs flag is set to: %d",
			   is_epcs);
		return eid;
	}

	pos = (u8 *) he_mu_edca;
	for (i = 0; i < sizeof(*edca); i++) {
		if (pos[i])
			break;
	}
	if (i == sizeof(*edca))
		return eid; /* no MU EDCA Parameters configured */

	pos = eid;
	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 1 + sizeof(*edca);
	*pos++ = WLAN_EID_EXT_HE_MU_EDCA_PARAMS;

	edca = (struct ieee80211_he_mu_edca_parameter_set *) pos;
	os_memcpy(edca, he_mu_edca, sizeof(*edca));

	updated_count = edca->he_qos_info & 0xf;
	if (updated_count != (hapd->parameter_set_count & 0xf)) {
		updated_count = hapd->parameter_set_count & 0xf;
		edca->he_qos_info &= 0xf0;
		edca->he_qos_info |= updated_count;
	}

	wpa_hexdump(MSG_MSGDUMP, "HE: MU EDCA Parameter Set element",
		    pos, sizeof(*edca));

	pos += sizeof(*edca);

	return pos;
}


u8 * hostapd_eid_spatial_reuse(struct hostapd_data *hapd, u8 *eid)
{
	struct ieee80211_spatial_reuse *spr;
	u8 *pos = eid, *spr_param;
	u8 sz = 1;

	if (!hapd->iface->conf->spr.sr_control)
		return eid;

	if (hapd->iface->conf->spr.sr_control &
	    SPATIAL_REUSE_NON_SRG_OFFSET_PRESENT)
		sz++;

	if (hapd->iface->conf->spr.sr_control &
	    SPATIAL_REUSE_SRG_INFORMATION_PRESENT)
		sz += 18;

	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 1 + sz;
	*pos++ = WLAN_EID_EXT_SPATIAL_REUSE;

	spr = (struct ieee80211_spatial_reuse *) pos;
	os_memset(spr, 0, sizeof(*spr));

	spr->sr_ctrl = hapd->iface->conf->spr.sr_control;
	pos++;
	spr_param = spr->params;
	if (spr->sr_ctrl & SPATIAL_REUSE_NON_SRG_OFFSET_PRESENT) {
		*spr_param++ =
			hapd->iface->conf->spr.non_srg_obss_pd_max_offset;
		pos++;
	}
	if (spr->sr_ctrl & SPATIAL_REUSE_SRG_INFORMATION_PRESENT) {
		*spr_param++ = hapd->iface->conf->spr.srg_obss_pd_min_offset;
		*spr_param++ = hapd->iface->conf->spr.srg_obss_pd_max_offset;
		os_memcpy(spr_param,
			  hapd->iface->conf->spr.srg_bss_color_bitmap, 8);
		spr_param += 8;
		os_memcpy(spr_param,
			  hapd->iface->conf->spr.srg_partial_bssid_bitmap, 8);
		pos += 18;
	}

	return pos;
}


u8 * hostapd_eid_he_6ghz_band_cap(struct hostapd_data *hapd, u8 *eid)
{
	struct hostapd_config *conf = hapd->iface->conf;
	struct hostapd_hw_modes *mode = hapd->iface->current_mode;
	struct he_capabilities *he_cap;
	struct ieee80211_he_6ghz_band_cap *cap;
	u16 capab;
	u8 *pos;

	if (!mode || !is_6ghz_op_class(hapd->iconf->op_class) ||
	    !is_6ghz_freq(hapd->iface->freq))
		return eid;

	he_cap = &mode->he_capab[IEEE80211_MODE_AP];
	capab = he_cap->he_6ghz_capa & HE_6GHZ_BAND_CAP_MIN_MPDU_START;
	capab |= (conf->he_6ghz_max_ampdu_len_exp <<
		  HE_6GHZ_BAND_CAP_MAX_AMPDU_LEN_EXP_SHIFT) &
		HE_6GHZ_BAND_CAP_MAX_AMPDU_LEN_EXP_MASK;
	capab |= (conf->he_6ghz_max_mpdu <<
		  HE_6GHZ_BAND_CAP_MAX_MPDU_LEN_SHIFT) &
		HE_6GHZ_BAND_CAP_MAX_MPDU_LEN_MASK;
	capab |= HE_6GHZ_BAND_CAP_SMPS_DISABLED;
	if (conf->he_6ghz_rx_ant_pat)
		capab |= HE_6GHZ_BAND_CAP_RX_ANTPAT_CONS;
	if (conf->he_6ghz_tx_ant_pat)
		capab |= HE_6GHZ_BAND_CAP_TX_ANTPAT_CONS;

	pos = eid;
	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 1 + sizeof(*cap);
	*pos++ = WLAN_EID_EXT_HE_6GHZ_BAND_CAP;

	cap = (struct ieee80211_he_6ghz_band_cap *) pos;
	cap->capab = host_to_le16(capab);
	pos += sizeof(*cap);

	return pos;
}


void hostapd_get_he_capab(struct hostapd_data *hapd,
			  const struct ieee80211_he_capabilities *he_cap,
			  struct ieee80211_he_capabilities *neg_he_cap,
			  size_t he_capab_len)
{
	if (!he_cap)
		return;

	if (he_capab_len > sizeof(*neg_he_cap))
		he_capab_len = sizeof(*neg_he_cap);
	/* TODO: mask out unsupported features */

	os_memcpy(neg_he_cap, he_cap, he_capab_len);
}


static int check_valid_he_mcs(struct hostapd_data *hapd, const u8 *sta_he_capab,
			      enum ieee80211_op_mode opmode)
{
	u16 sta_rx_mcs_set, ap_tx_mcs_set;
	u8 mcs_count = 0;
	const u16 *ap_mcs_set;
	const u8 *sta_mcs_set;
	int i;

	if (!hapd->iface->current_mode)
		return 1;
	ap_mcs_set = (u16 *) hapd->iface->current_mode->he_capab[opmode].mcs;
	sta_mcs_set = (const u8 *) &((const struct ieee80211_he_capabilities *)
			sta_he_capab)->he_basic_supported_mcs_set;

	/*
	 * Disable HE capabilities for STAs for which there is not even a single
	 * allowed MCS in any supported number of streams, i.e., STA is
	 * advertising 3 (not supported) as HE MCS rates for all supported
	 * band/stream cases.
	 */
	switch (hapd->iface->conf->he_oper_chwidth) {
	case CONF_OPER_CHWIDTH_80P80MHZ:
		mcs_count = 3;
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		mcs_count = 2;
		break;
	default:
		mcs_count = 1;
		break;
	}

	for (i = 0; i < mcs_count; i++) {
		int j;

		/* AP Tx MCS map vs. STA Rx MCS map */
		sta_rx_mcs_set = WPA_GET_LE16(&sta_mcs_set[i * 4]);
		ap_tx_mcs_set = WPA_GET_LE16((const u8 *)
					     &ap_mcs_set[(i * 2) + 1]);

		for (j = 0; j < HE_NSS_MAX_STREAMS; j++) {
			if (((ap_tx_mcs_set >> (j * 2)) & 0x3) == 3)
				continue;

			if (((sta_rx_mcs_set >> (j * 2)) & 0x3) == 3)
				continue;

			return 1;
		}
	}

	wpa_printf(MSG_DEBUG,
		   "No matching HE MCS found between AP TX and STA RX");

	return 0;
}


u16 copy_sta_he_capab(struct hostapd_data *hapd, struct sta_info *sta,
		      enum ieee80211_op_mode opmode, const u8 *he_capab,
		      size_t he_capab_len)
{
	if (!he_capab || !(sta->flags & WLAN_STA_WMM) ||
	    !hostapd_is_he_enabled(hapd) ||
	    !check_valid_he_mcs(hapd, he_capab, opmode) ||
	    ieee80211_invalid_he_cap_size(he_capab, he_capab_len) ||
	    he_capab_len > sizeof(struct ieee80211_he_capabilities)) {
		sta->flags &= ~WLAN_STA_HE;
		os_free(sta->he_capab);
		sta->he_capab = NULL;
		return WLAN_STATUS_SUCCESS;
	}

	if (!sta->he_capab) {
		sta->he_capab =
			os_zalloc(sizeof(struct ieee80211_he_capabilities));
		if (!sta->he_capab)
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	sta->flags |= WLAN_STA_HE;
	os_memset(sta->he_capab, 0, sizeof(struct ieee80211_he_capabilities));
	os_memcpy(sta->he_capab, he_capab, he_capab_len);
	sta->he_capab_len = he_capab_len;

	return WLAN_STATUS_SUCCESS;
}


u16 copy_sta_he_6ghz_capab(struct hostapd_data *hapd, struct sta_info *sta,
			   const u8 *he_6ghz_capab)
{
	if (!he_6ghz_capab || !hostapd_is_he_enabled(hapd) ||
	    !is_6ghz_op_class(hapd->iconf->op_class) ||
	    !(sta->flags & WLAN_STA_HE)) {
		sta->flags &= ~WLAN_STA_6GHZ;
		os_free(sta->he_6ghz_capab);
		sta->he_6ghz_capab = NULL;
		return WLAN_STATUS_SUCCESS;
	}

	if (!sta->he_6ghz_capab) {
		sta->he_6ghz_capab =
			os_zalloc(sizeof(struct ieee80211_he_6ghz_band_cap));
		if (!sta->he_6ghz_capab)
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	sta->flags |= WLAN_STA_6GHZ;
	os_memcpy(sta->he_6ghz_capab, he_6ghz_capab,
		  sizeof(struct ieee80211_he_6ghz_band_cap));

	return WLAN_STATUS_SUCCESS;
}


int hostapd_get_he_twt_responder(struct hostapd_data *hapd,
				 enum ieee80211_op_mode mode)
{
	u8 *mac_cap;

	if (!hapd->iface->current_mode ||
	    !hapd->iface->current_mode->he_capab[mode].he_supported ||
	    !hostapd_is_he_enabled(hapd))
		return 0;

	mac_cap = hapd->iface->current_mode->he_capab[mode].mac_cap;

	return !!(mac_cap[HE_MAC_CAPAB_0] & HE_MACCAP_TWT_RESPONDER) &&
		hapd->iface->conf->he_op.he_twt_responder;
}


u8 * hostapd_eid_cca(struct hostapd_data *hapd, u8 *eid)
{
	if (!hapd->cca_in_progress)
		return eid;

	/* BSS Color Change Announcement element */
	*eid++ = WLAN_EID_EXTENSION;
	*eid++ = 3;
	*eid++ = WLAN_EID_EXT_COLOR_CHANGE_ANNOUNCEMENT;
	*eid++ = hapd->cca_count; /* Color Switch Countdown */
	*eid++ = hapd->cca_color; /* New BSS Color Information */

	return eid;
}
