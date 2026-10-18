/*
 * hostapd / IEEE 802.11ac VHT
 * Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of BSD license
 *
 * See README and COPYING for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/hw_features_common.h"
#include "hostapd.h"
#include "ap_config.h"
#include "sta_info.h"
#include "beacon.h"
#include "ieee802_11.h"
#include "dfs.h"

#define VHT_MCS_NOT_SUPPORTED  3

static le16 intersect_vht_mcs_set(le16 hw_vht_mcs_set, u16 usr_vht_mcs_set)
{
	le16 mcs_nss_set = 0xffff;
	u8 nss = 0, hw_max_mcs, usr_max_mcs, out;

	while (hw_vht_mcs_set) {
		hw_max_mcs = hw_vht_mcs_set & 0x3;
		usr_max_mcs = usr_vht_mcs_set & 0x3;

		if (hw_max_mcs == VHT_MCS_NOT_SUPPORTED)
			break;

		if (usr_max_mcs == VHT_MCS_NOT_SUPPORTED)
			out = VHT_MCS_NOT_SUPPORTED;
		else
			out = MIN(hw_max_mcs, usr_max_mcs);

		mcs_nss_set &= ~(0x3 << (nss * 2));
		mcs_nss_set |= (out & 0x3) << (nss * 2);

		nss++;
		hw_vht_mcs_set >>= 2;
		usr_vht_mcs_set >>= 2;
	}

	return mcs_nss_set;
}


static struct hostapd_hw_modes *
mode_for_vht_capab(struct hostapd_data *hapd, struct hostapd_hw_modes *mode)
{
	if (mode->mode == HOSTAPD_MODE_IEEE80211G && hapd->conf->vendor_vht &&
	    mode->vht_capab == 0 && hapd->iface->hw_features) {
		int i;

		for (i = 0; i < hapd->iface->num_hw_features; i++) {
			if (hapd->iface->hw_features[i].mode ==
			    HOSTAPD_MODE_IEEE80211A)
			    	return &hapd->iface->hw_features[i];
		}
	}

	return mode;
}


u8 * hostapd_eid_vht_capabilities(struct hostapd_data *hapd, u8 *eid, u32 nsts)
{
	struct ieee80211_vht_capabilities *cap;
	struct hostapd_hw_modes *mode = hapd->iface->current_mode;
	struct hostapd_data *tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
	u8 *pos = eid;
	u8 chwidth;
	u32 vht_capab;

	if (!mode || is_6ghz_op_class(hapd->iconf->op_class))
		return eid;

	mode = mode_for_vht_capab(hapd, mode);

	*pos++ = WLAN_EID_VHT_CAP;
	*pos++ = sizeof(*cap);

	cap = (struct ieee80211_vht_capabilities *) pos;
	os_memset(cap, 0, sizeof(*cap));

	vht_capab = hapd->iface->conf->vht_capab;

	/* For non-transmitting BSSs in MBSSID, inherit BSS-level overrides
	 * from the transmitting BSS */
	if (tx_hapd != hapd && tx_hapd->conf->vht_capab_mask) {
		hapd->conf->vht_capab = tx_hapd->conf->vht_capab;
		hapd->conf->vht_capab_mask = tx_hapd->conf->vht_capab_mask;
	}

	if (hapd->conf->vht_capab_mask) {
		u32 bss_capab = hapd->conf->vht_capab;
		u32 mask = hapd->conf->vht_capab_mask;

		if (mask & VHT_CAP_BSS_OVR_SU_BEAMFORMER) {
			vht_capab &= ~VHT_CAP_SU_BEAMFORMER_CAPABLE;
			vht_capab |= (bss_capab & VHT_CAP_SU_BEAMFORMER_CAPABLE);
		}

		if (mask & VHT_CAP_BSS_OVR_SU_BEAMFORMEE) {
			vht_capab &= ~VHT_CAP_SU_BEAMFORMEE_CAPABLE;
			vht_capab |= (bss_capab & VHT_CAP_SU_BEAMFORMEE_CAPABLE);
		}

		if (mask & VHT_CAP_BSS_OVR_MU_BEAMFORMER) {
			vht_capab &= ~VHT_CAP_MU_BEAMFORMER_CAPABLE;
			vht_capab |= (bss_capab & VHT_CAP_MU_BEAMFORMER_CAPABLE);
		}

		if (mask & VHT_CAP_BSS_OVR_SOUNDING_DIMENSION) {
			vht_capab &= ~VHT_CAP_SOUNDING_DIMENSION_MAX;
			vht_capab |= (bss_capab & VHT_CAP_SOUNDING_DIMENSION_MAX);
		}

		if (mask & VHT_CAP_BSS_OVR_STS_CAPABILITY) {
			vht_capab &= ~VHT_CAP_BEAMFORMEE_STS_MAX;
			vht_capab |= (bss_capab & VHT_CAP_BEAMFORMEE_STS_MAX);
		}
	}

	cap->vht_capabilities_info = host_to_le32(vht_capab);

	if (nsts != 0) {
		u32 hapd_nsts;

		hapd_nsts = le_to_host32(cap->vht_capabilities_info);
		hapd_nsts = (hapd_nsts >> VHT_CAP_BEAMFORMEE_STS_OFFSET) & 7;
		cap->vht_capabilities_info &=
			~(host_to_le32(hapd_nsts <<
				       VHT_CAP_BEAMFORMEE_STS_OFFSET));
		cap->vht_capabilities_info |=
			host_to_le32(nsts << VHT_CAP_BEAMFORMEE_STS_OFFSET);
	}

	chwidth = hapd->iconf->vht_oper_chwidth;
#ifdef CONFIG_QCN_EXTN
	/* If the BSS is repurposed BSS, then its operating channel width
	 * advertised in VHT operation element can be lesser than the interface
	 * level setting. In that case, rederive the repurposed BSS's operating
	 * chan width and update the VHT Capability Info accordingly.
	 */
	hostapd_repurpose_update_vht_capabilities_extn(hapd, &chwidth, cap);
#endif /* CONFIG_QCN_EXTN */

	if (!(mode->vht_capab & VHT_CAP_EXTENDED_NSS_BW_SUPPORT)) {
		u32 cur_caps = le_to_host32(cap->vht_capabilities_info);

		if (!(cur_caps & VHT_CAP_SUPP_CHAN_WIDTH_MASK)) {
			if (chwidth == CHANWIDTH_160MHZ)
				cap->vht_capabilities_info |=
					host_to_le32(VHT_CAP_SUPP_CHAN_WIDTH_160MHZ);
			else if (chwidth == CHANWIDTH_80P80MHZ)
				cap->vht_capabilities_info |=
					host_to_le32(VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ);
		}
	}

	/* Supported MCS set comes from hw */
	os_memcpy(&cap->vht_supported_mcs_set, mode->vht_mcs_set, 8);

	if (tx_hapd != hapd)
		hapd->conf->vht_mcs_nss_set = tx_hapd->conf->vht_mcs_nss_set;

	if (hapd->conf->vht_mcs_nss_set) {
		cap->vht_supported_mcs_set.rx_map =
			intersect_vht_mcs_set(cap->vht_supported_mcs_set.rx_map,
					      hapd->conf->vht_mcs_nss_set);
		cap->vht_supported_mcs_set.tx_map =
			intersect_vht_mcs_set(cap->vht_supported_mcs_set.tx_map,
					      hapd->conf->vht_mcs_nss_set);
	}

	pos += sizeof(*cap);

	return pos;
}


u8 * hostapd_eid_vht_operation(struct hostapd_data *hapd, u8 *eid)
{
	struct ieee80211_vht_operation *oper;
	le32 vht_capabilities_info = 0;
	u8 *pos = eid;
	enum oper_chan_width oper_chwidth =

#ifdef CONFIG_QCN_EXTN
		hapd->iconf->vht_oper_chwidth;
#else

		hostapd_get_oper_chwidth(hapd->iconf);
#endif
	u8 seg0 = hapd->iconf->vht_oper_centr_freq_seg0_idx;
	u8 seg1 = hapd->iconf->vht_oper_centr_freq_seg1_idx;
#ifdef CONFIG_IEEE80211BE
	u16 punct_bitmap = hostapd_get_punct_bitmap(hapd);
#endif /* CONFIG_IEEE80211BE */

	if (is_6ghz_op_class(hapd->iconf->op_class))
		return eid;

	*pos++ = WLAN_EID_VHT_OPERATION;
	*pos++ = sizeof(*oper);

	oper = (struct ieee80211_vht_operation *) pos;
	os_memset(oper, 0, sizeof(*oper));

#ifdef CONFIG_IEEE80211BE
	if (punct_bitmap) {
#ifdef CONFIG_QCN_EXTN
		hostapd_get_oper_center_freq_seg_extn(hapd->iconf, &seg0, &seg1,
						      &oper_chwidth);
#endif /* CONFIG_QCN_EXTN */
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

	/*
	 * center freq = 5 GHz + (5 * index)
	 * So index 42 gives center freq 5.210 GHz
	 * which is channel 42 in 5G band
	 */
	oper->vht_op_info_chan_center_freq_seg0_idx = seg0;
	oper->vht_op_info_chan_center_freq_seg1_idx = seg1;

	oper->vht_op_info_chwidth = oper_chwidth;
	if (hapd->iface->current_mode && hapd->iface->current_mode->vht_capab)
		vht_capabilities_info = host_to_le32(hapd->iface->current_mode->vht_capab);
	if (oper_chwidth == CONF_OPER_CHWIDTH_160MHZ) {
		/*
		 * Convert 160 MHz channel width to new style as interop
		 * workaround.
		 */
		oper->vht_op_info_chwidth = CHANWIDTH_80MHZ;
		oper->vht_op_info_chan_center_freq_seg1_idx =
			oper->vht_op_info_chan_center_freq_seg0_idx;
		if (hapd->iconf->channel <
		    hapd->iconf->vht_oper_centr_freq_seg0_idx)
			oper->vht_op_info_chan_center_freq_seg0_idx -= 8;
		else
			oper->vht_op_info_chan_center_freq_seg0_idx += 8;

		if (vht_capabilities_info & VHT_CAP_EXTENDED_NSS_BW_SUPPORT)
			oper->vht_op_info_chan_center_freq_seg1_idx = 0;
	} else if (oper_chwidth == CONF_OPER_CHWIDTH_80P80MHZ) {
		/*
		 * Convert 80+80 MHz channel width to new style as interop
		 * workaround.
		 */
		oper->vht_op_info_chwidth = CHANWIDTH_80MHZ;
	}

	/* VHT Basic MCS set comes from hw */
	/* Hard code 1 stream, MCS0-7 is a min Basic VHT MCS rates */
	oper->vht_basic_mcs_set = host_to_le16(0xfffc);
	pos += sizeof(*oper);

	return pos;
}


static int check_valid_vht_mcs(struct hostapd_data *hapd,
			       const u8 *sta_vht_capab)
{
	struct hostapd_hw_modes *mode = hapd->iface->current_mode;
	const struct ieee80211_vht_capabilities *vht_cap;
	struct ieee80211_vht_capabilities ap_vht_cap;
	u16 sta_rx_mcs_set, ap_tx_mcs_set;
	int i;

	if (!mode)
		return 1;
	mode = mode_for_vht_capab(hapd, mode);

	/*
	 * Disable VHT caps for STAs for which there is not even a single
	 * allowed MCS in any supported number of streams, i.e., STA is
	 * advertising 3 (not supported) as VHT MCS rates for all supported
	 * stream cases.
	 */
	os_memcpy(&ap_vht_cap.vht_supported_mcs_set, mode->vht_mcs_set,
		  sizeof(ap_vht_cap.vht_supported_mcs_set));
	vht_cap = (const struct ieee80211_vht_capabilities *) sta_vht_capab;

	/* AP Tx MCS map vs. STA Rx MCS map */
	sta_rx_mcs_set = le_to_host16(vht_cap->vht_supported_mcs_set.rx_map);
	ap_tx_mcs_set = le_to_host16(ap_vht_cap.vht_supported_mcs_set.tx_map);

	for (i = 0; i < VHT_RX_NSS_MAX_STREAMS; i++) {
		if (((ap_tx_mcs_set >> (i * 2)) & 0x3) == 3)
			continue;

		if (((sta_rx_mcs_set >> (i * 2)) & 0x3) == 3)
			continue;

		return 1;
	}

	wpa_printf(MSG_DEBUG,
		   "No matching VHT MCS found between AP TX and STA RX");
	return 0;
}


u16 copy_sta_vht_capab(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *vht_capab)
{
	/* Disable VHT caps for STAs associated to no-VHT BSSes. */
	if (!vht_capab || !(sta->flags & WLAN_STA_WMM) ||
	    !hostapd_is_vht_enabled(hapd) ||
	    !check_valid_vht_mcs(hapd, vht_capab) ||
	    !(sta->flags & WLAN_STA_HT)) {
		sta->flags &= ~WLAN_STA_VHT;
		os_free(sta->vht_capabilities);
		sta->vht_capabilities = NULL;
		return WLAN_STATUS_SUCCESS;
	}

	if (sta->vht_capabilities == NULL) {
		sta->vht_capabilities =
			os_zalloc(sizeof(struct ieee80211_vht_capabilities));
		if (sta->vht_capabilities == NULL)
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	sta->flags |= WLAN_STA_VHT;
	os_memcpy(sta->vht_capabilities, vht_capab,
		  sizeof(struct ieee80211_vht_capabilities));

	return WLAN_STATUS_SUCCESS;
}


u16 copy_sta_vht_oper(struct hostapd_data *hapd, struct sta_info *sta,
		      const u8 *vht_oper)
{
	if (!vht_oper || !(sta->flags & WLAN_STA_VHT)) {
		os_free(sta->vht_operation);
		sta->vht_operation = NULL;
		return WLAN_STATUS_SUCCESS;
	}

	if (!sta->vht_operation) {
		sta->vht_operation =
			os_zalloc(sizeof(struct ieee80211_vht_operation));
		if (!sta->vht_operation)
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	os_memcpy(sta->vht_operation, vht_oper,
		  sizeof(struct ieee80211_vht_operation));

	return WLAN_STATUS_SUCCESS;
}


u16 copy_sta_vendor_vht(struct hostapd_data *hapd, struct sta_info *sta,
			const u8 *ie, size_t len)
{
	const u8 *vht_capab;
	unsigned int vht_capab_len;

	if (!ie || len < 5 + 2 + sizeof(struct ieee80211_vht_capabilities) ||
	    hapd->conf->disable_11ac)
		goto no_capab;

	/* The VHT Capabilities element embedded in vendor VHT */
	vht_capab = ie + 5;
	if (vht_capab[0] != WLAN_EID_VHT_CAP)
		goto no_capab;
	vht_capab_len = vht_capab[1];
	if (vht_capab_len < sizeof(struct ieee80211_vht_capabilities) ||
	    (int) vht_capab_len > ie + len - vht_capab - 2)
		goto no_capab;
	vht_capab += 2;

	if (sta->vht_capabilities == NULL) {
		sta->vht_capabilities =
			os_zalloc(sizeof(struct ieee80211_vht_capabilities));
		if (sta->vht_capabilities == NULL)
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	sta->flags |= WLAN_STA_VHT | WLAN_STA_VENDOR_VHT;
	os_memcpy(sta->vht_capabilities, vht_capab,
		  sizeof(struct ieee80211_vht_capabilities));
	return WLAN_STATUS_SUCCESS;

no_capab:
	sta->flags &= ~WLAN_STA_VENDOR_VHT;
	return WLAN_STATUS_SUCCESS;
}


u8 * hostapd_eid_vendor_vht(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;

	/* Vendor VHT is applicable only to 2.4 GHz */
	if (!hapd->iface->current_mode ||
	    hapd->iface->current_mode->mode != HOSTAPD_MODE_IEEE80211G)
		return eid;

	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = (5 +		/* The Vendor OUI, type and subtype */
		  2 + sizeof(struct ieee80211_vht_capabilities) +
		  2 + sizeof(struct ieee80211_vht_operation));

	WPA_PUT_BE32(pos, (OUI_BROADCOM << 8) | VENDOR_VHT_TYPE);
	pos += 4;
	*pos++ = VENDOR_VHT_SUBTYPE;
	pos = hostapd_eid_vht_capabilities(hapd, pos, 0);
	pos = hostapd_eid_vht_operation(hapd, pos);

	return pos;
}


u16 set_sta_vht_opmode(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *vht_oper_notif)
{
	if (!vht_oper_notif) {
		sta->flags &= ~WLAN_STA_VHT_OPMODE_ENABLED;
		return WLAN_STATUS_SUCCESS;
	}

	sta->flags |= WLAN_STA_VHT_OPMODE_ENABLED;
	sta->vht_opmode = *vht_oper_notif;
	return WLAN_STATUS_SUCCESS;
}


void hostapd_get_vht_capab(struct hostapd_data *hapd,
			   struct ieee80211_vht_capabilities *vht_cap,
			   struct ieee80211_vht_capabilities *neg_vht_cap)
{
	u32 cap, own_cap, sym_caps;

	if (vht_cap == NULL)
		return;
	os_memcpy(neg_vht_cap, vht_cap, sizeof(*neg_vht_cap));

	cap = le_to_host32(neg_vht_cap->vht_capabilities_info);
	own_cap = hapd->iconf->vht_capab;

	/* mask out symmetric VHT capabilities we don't support */
	sym_caps = VHT_CAP_SHORT_GI_80 | VHT_CAP_SHORT_GI_160;
	cap &= ~sym_caps | (own_cap & sym_caps);

	/* mask out beamformer/beamformee caps if not supported */
	if (!(own_cap & VHT_CAP_SU_BEAMFORMER_CAPABLE))
		cap &= ~(VHT_CAP_SU_BEAMFORMEE_CAPABLE |
			 VHT_CAP_BEAMFORMEE_STS_MAX);

	if (!(own_cap & VHT_CAP_SU_BEAMFORMEE_CAPABLE))
		cap &= ~(VHT_CAP_SU_BEAMFORMER_CAPABLE |
			 VHT_CAP_SOUNDING_DIMENSION_MAX);

	if (!(own_cap & VHT_CAP_MU_BEAMFORMER_CAPABLE))
		cap &= ~VHT_CAP_MU_BEAMFORMEE_CAPABLE;

	if (!(own_cap & VHT_CAP_MU_BEAMFORMEE_CAPABLE))
		cap &= ~VHT_CAP_MU_BEAMFORMER_CAPABLE;

#ifdef CONFIG_QCN_EXTN
	/* If the BSS is repurposed and bandwidth is not 160,
	 * clear the supported channel width mask from self cap
	 */
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
		enum oper_chan_width chwidth;
		u8 seg0, seg1;

		hostapd_repurpose_get_vht_legacy_chan_info_extn(hapd, &chwidth,
								&seg0, &seg1);
		hostapd_get_oper_info_of_repurposed_bss_extn(hapd, &chwidth,
							     &seg0, &seg1);
		if (chwidth != CHANWIDTH_160MHZ &&
		    chwidth != CHANWIDTH_80P80MHZ)
			own_cap &= ~VHT_CAP_SUPP_CHAN_WIDTH_MASK;
	}
#endif /* CONFIG_QCN_EXTN */

	/* mask channel widths we don't support */
	switch (own_cap & VHT_CAP_SUPP_CHAN_WIDTH_MASK) {
	case VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ:
		break;
	case VHT_CAP_SUPP_CHAN_WIDTH_160MHZ:
		if (cap & VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ) {
			cap &= ~VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ;
			cap |= VHT_CAP_SUPP_CHAN_WIDTH_160MHZ;
		}
		break;
	default:
		cap &= ~VHT_CAP_SUPP_CHAN_WIDTH_MASK;
		break;
	}

	if (!(cap & VHT_CAP_SUPP_CHAN_WIDTH_MASK))
		cap &= ~VHT_CAP_SHORT_GI_160;

	/*
	 * if we don't support RX STBC, mask out TX STBC in the STA's HT caps
	 * if we don't support TX STBC, mask out RX STBC in the STA's HT caps
	 */
	if (!(own_cap & VHT_CAP_RXSTBC_MASK))
		cap &= ~VHT_CAP_TXSTBC;
	if (!(own_cap & VHT_CAP_TXSTBC))
		cap &= ~VHT_CAP_RXSTBC_MASK;

	neg_vht_cap->vht_capabilities_info = host_to_le32(cap);
}
