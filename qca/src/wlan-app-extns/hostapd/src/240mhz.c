/*
 * Copyright (c) 2002-2013, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2013-2017, Qualcomm Atheros, Inc.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "utils/includes.h"
#include <math.h>

#include "utils/common.h"
#include "utils/list.h"
#include "common/ieee802_11_defs.h"
#include "common/hw_features_common.h"
#include "common/wpa_ctrl.h"
#include "drivers/driver.h"
#include "ap/hostapd.h"
#include "ap/ap_drv_ops.h"
#include "ap/ap_config.h"
#include "ap/hw_features.h"
#include "ap/acs.h"
#include "ap/sta_info.h"
#include "cmn.h"
#include "240mhz.h"
#include "qcn_ie_extn.h"
#include "ap/ieee802_11.h"


int hostapd_handle_5ghz_320mhz_bw_indication_extn(struct hostapd_data *hapd,
							  u8 *chan1, u8 *chan2,
							  u16 *punct_bitmap,
							  int *bandwidth)
{
	u8 pri_chan = hapd->cs_freq_params.channel;
	u8 center_chan;
	u8 start_chan;
	u16 pri_freq;
	u16 center_freq_320;
	u16 original_punct_bitmap = *punct_bitmap;
	u16 truncated_punct_bitmap;
	u16 pri_chan_bit_pos;
	u16 target_bw = CHWIDTH_160;

	center_freq_320 = ieee80211_chan_to_freq(NULL, hapd->iconf->op_class,
						 *chan1);
	if (center_freq_320 <= 0)
		return -1;

	/*
	 * Map QCN 5 GHz 240 MHz CSA to a standards-compatible BWI:
	 *   ch 100-128 -> 160 MHz, center ch 114
	 *   ch 132-144 ->  80 MHz, center ch 138
	 * Primaries outside this 240 MHz span do not need QCN remapping.
	 */
	if (pri_chan >= 100 && pri_chan <= 128) {
		center_chan = 114;
	} else if (pri_chan >= 132 && pri_chan <= 144) {
		center_chan = 138;
		target_bw = CHWIDTH_80;
	} else {
		return -1;
	}

	pri_freq = ieee80211_chan_to_freq(NULL, hapd->iconf->op_class, pri_chan);
	if (pri_freq <= 0)
		return -1;

	truncated_punct_bitmap =
		get_lower_bandwidth_puncture_pattern(pri_freq,
						     original_punct_bitmap,
						     center_freq_320,
						     CHWIDTH_320, target_bw);
	*punct_bitmap = truncated_punct_bitmap &
		((1 << (target_bw / CHWIDTH_20)) - 1);

	start_chan = center_chan - (target_bw == CHWIDTH_160 ? 14 : 6);
	pri_chan_bit_pos = (pri_chan - start_chan) / 4;
	if (!is_punct_bitmap_valid(target_bw, pri_chan_bit_pos, *punct_bitmap))
		return -1;

	/*
	 * Return a legal lower-width BWI representation:
	 * - for 160 MHz, chan1 is the 160 MHz center; BWI derives ccfs0/ccfs1.
	 * - for 80 MHz, chan1 is the 80 MHz center used directly as ccfs0.
	 * - chan2 cleared; hostapd_eid_bw_indication() will derive CCFS0/CCFS1.
	 */
	*bandwidth = target_bw;
	*chan1 = center_chan;
	*chan2 = 0;

	return 0;
}


void hostapd_get_oper_center_freq_seg_extn(struct hostapd_config *conf,
					   u8 *oper_centr_freq_seg0_idx,
					   u8 *oper_centr_freq_seg1_idx,
					   enum oper_chan_width *oper_chwidth)
{
	if (oper_centr_freq_seg0_idx)
		*oper_centr_freq_seg0_idx =
			hostapd_get_oper_centr_freq_seg0_idx(conf);

	if (oper_centr_freq_seg1_idx)
		*oper_centr_freq_seg1_idx =
			hostapd_get_oper_centr_freq_seg1_idx(conf);

	if (oper_chwidth)
		*oper_chwidth = hostapd_get_oper_chwidth(conf);
}

u8 hostapd_set_legacy_oper_centr_freq_seg0_extn(struct hostapd_config *conf,
						u8 oper_centr_freq_seg0_idx)
{
	if (conf->eht_oper_chwidth == CONF_OPER_CHWIDTH_320MHZ &&
	    oper_centr_freq_seg0_idx == 130) {
		if (conf->channel < oper_centr_freq_seg0_idx)
			oper_centr_freq_seg0_idx = oper_centr_freq_seg0_idx
						   - 16;

		else
			oper_centr_freq_seg0_idx = oper_centr_freq_seg0_idx + 8;
	}

	return oper_centr_freq_seg0_idx;
}

int hostapd_acs_update_puncturing_bitmap_extn(struct hostapd_config *conf,
					      u16 bw,
					      struct hostapd_channel_data *chan)
{
	if (!is_6ghz_op_class(conf->op_class) && bw == 320) {
		if ((conf->punct_bitmap & PUNCTURING_PATTERN_5G_320MHZ) ==
		    PUNCTURING_PATTERN_5G_320MHZ) {
			chan->punct_bitmap = conf->punct_bitmap;
			return 0;

		} else {
			wpa_printf(MSG_DEBUG, "ACS : Invalid punct_bitmap for 5G %X",
				   conf->punct_bitmap);
			return -1;
		}
	}

	return 1;
}

int hostapd_modify_n_chans_for_240mhz_extn(struct hostapd_iface *iface,
					   int n_chans)
{
	/* 5GHz is supported for 240Mhz and so reducing number of channels*/
	if(!is_6ghz_op_class(iface->conf->op_class) &&
	   hostapd_get_oper_chwidth(iface->conf) == CONF_OPER_CHWIDTH_320MHZ)
		return (n_chans - 4);

	return n_chans;
}

int hostapd_modify_supported_op_class_for_240mhz_extn(int freq,
						      enum oper_chan_width
						      ch_width,
						      u8 *op_class)
{
	switch (ch_width) {
	case CONF_OPER_CHWIDTH_320MHZ:
		if (is_5ghz_freq(freq)) {
			*op_class = 129;
			return 0;
		}
		break;

	default:
		return -1;
		break;
	}

	return -1;
}

void hostapd_modify_supported_op_class_for_320mhz_extn(int freq, u8 *op_class)
{
	*op_class = is_5ghz_freq(freq) ? 129 : 134;
}

static bool hostapd_is_240mhz_attr_required(struct hostapd_data *hapd)
{
	if (!is_5ghz_freq(hapd->iface->freq) ||
	    is_6ghz_op_class(hapd->iconf->op_class) ||
	    hapd->iconf->eht_oper_chwidth != CONF_OPER_CHWIDTH_320MHZ ||
	    !hostapd_is_eht_enabled(hapd))
		return false;
	return true;
}

size_t hostapd_qcn_buflen_add_240mhz_attr(struct hostapd_data *hapd)
{
	if (!hostapd_is_240mhz_attr_required(hapd))
		return 0;

	return QCN_ATTRIB_HDR_LEN +
	       sizeof(struct ieee80211_240mhz_vendor_oper_extn);
}

u8 * hostapd_qcn_eid_add_240mhz_attr(struct hostapd_data *hapd, u8 *pos,
				     enum ieee80211_op_mode opmode)
{
	struct hostapd_hw_modes *mode;
	struct eht_capabilities *eht_cap;
	struct ieee80211_240mhz_vendor_oper_extn *eht_240_cap;
	u8 ccfs0, ccfs1;

	if (!pos)
		return pos;

	if (!hostapd_is_240mhz_attr_required(hapd))
		return pos;

	mode = hapd->iface->current_mode;
	eht_cap = mode ? &mode->eht_capab[opmode] : NULL;
	if (!eht_cap || !eht_cap->eht_supported)
		return pos;

	ccfs0 = hostapd_get_oper_centr_freq_seg0_idx(hapd->iconf);
	ccfs1 = ccfs0 - 16;

	*pos++ = QCN_ATTRIB_HE_240_MHZ_SUPP;
	*pos++ = sizeof(struct ieee80211_240mhz_vendor_oper_extn);

	eht_240_cap = (struct ieee80211_240mhz_vendor_oper_extn *)pos;
	os_memset(eht_240_cap, 0, sizeof(struct ieee80211_240mhz_vendor_oper_extn));

	eht_240_cap->ccfs1 = ccfs1;
	eht_240_cap->ccfs0 = ccfs0;
	eht_240_cap->punct_bitmap = hapd->iconf->punct_bitmap;
	eht_240_cap->is5ghz240mhz =
		(eht_cap->phy_cap[EHT_PHYCAP_320MHZ_IN_6GHZ_SUPPORT_IDX] &
		 EHT_PHYCAP_320MHZ_IN_6GHZ_SUPPORT_MASK) >> 1;
	eht_240_cap->bfmess320mhz =
		(eht_cap->phy_cap[EHT_PHYCAP_BEAMFORMEE_SS_320MHZ_IDX] &
		 EHT_PHYCAP_BEAMFORMEE_SS_320MHZ_MASK) >> 5;
	eht_240_cap->numsound320mhz =
		((eht_cap->phy_cap[EHT_PHYCAP_NUM_SOUND_DIM_320MHZ_IDX] &
		  EHT_PHYCAP_NUM_SOUND_DIM_320MHZ_MASK) >> 6) |
		((eht_cap->phy_cap[EHT_PHYCAP_NUM_SOUND_DIM_320MHZ_IDX_1] &
		  EHT_PHYCAP_NUM_SOUND_DIM_320MHZ_MASK_1) << 2);
	eht_240_cap->nonofdmaulmumimo320mhz =
		(eht_cap->phy_cap[EHT_PHYCAP_MU_CAPABILITY_IDX] &
		 EHT_PHYCAP_NON_OFDMA_UL_MU_MIMO_320MHZ) >> 3;
	eht_240_cap->mubfmr320mhz =
		(eht_cap->phy_cap[EHT_PHYCAP_MU_CAPABILITY_IDX] &
		 EHT_PHYCAP_MU_BEAMFORMER_MASK) >> 6;
	memcpy(&eht_240_cap->mcs_map_320mhz, &eht_cap->mcs,
	       EHT_PHYCAP_MCS_NSS_LEN_160MHZ);
	pos += sizeof(struct ieee80211_240mhz_vendor_oper_extn);

	return pos;
}

int hostapd_dfs_get_start_chan_idx_extn(struct hostapd_iface *iface)
{
	int channel_no;

	switch (hostapd_get_oper_chwidth(iface->conf)) {
	case CONF_OPER_CHWIDTH_320MHZ:
		channel_no = hostapd_get_oper_centr_freq_seg0_idx(iface->conf)
			     - 30;
		break;

	default:
		channel_no = -1;
		break;
	}
	return channel_no;
}

int hostapd_get_dfs_half_chwidth_extn(enum chan_width width)
{
	int half_width;

	switch (width) {
	case CHAN_WIDTH_320:
		half_width = 160;
		break;

	default:
		half_width = 0;
		break;
	}

	return half_width;
}

int hostapd_dfs_get_allowed_channels_extn(int n_chans,
					  int *is_allowed,
					  unsigned int *allowed_no)
{
	/*
	 * EHT320 valid channels based on center frequency:
	 * 100
	 */
	static int allowed_320[] = {100};
	int status;

	switch (n_chans) {
	case 16:
		is_allowed = allowed_320;
		*allowed_no = ARRAY_SIZE(allowed_320);
		status = 0;
		break;

	default:
		is_allowed = NULL;
		*allowed_no = 0;
		status = -1;
		break;
	}
	return status;
}

int hostapd_dfs_adjust_center_freq_extn(int oper_chwidth,
					short chan,
					u8 *oper_centr_freq_seg0_idx,
					u8 *oper_centr_freq_seg1_idx)
{
	int status;

	switch (oper_chwidth) {
	case CONF_OPER_CHWIDTH_320MHZ:
		*oper_centr_freq_seg0_idx = chan + 30;
		status = 0;
		break;

	default:
		*oper_centr_freq_seg0_idx = 0;
		status = -1;
		break;
	}

	return status;
}

int hostapd_get_n_chans_and_frequency_extn(enum oper_chan_width oper_chwidth,
					   int cf1,
					   int *n_chans,
					   int *frequency)
{
	int chans = 0, freq = 0;
	int status;

	switch (oper_chwidth) {
	case CONF_OPER_CHWIDTH_320MHZ:
		chans = 12;
		freq = cf1 - 150;
		status = 0;
		break;

	default:
		status = -1;
		break;
	}

	if (n_chans)
		*n_chans = chans;

	if (frequency)
		*frequency = freq;

	return status;
}

int
hostapd_get_bw_and_startchan_for_240mhz_extn(enum oper_chan_width
					     eht_oper_chwidth,
					     u8 eht_oper_centr_freq_seg0_idx,
					     u16 *bw, u8 *start_chan)
{
	int status = -1;

	switch (eht_oper_chwidth) {
	case 9:
		*bw = 320;
		*start_chan = eht_oper_centr_freq_seg0_idx - 30;
		wpa_printf(MSG_INFO, "5G 240MHz config identified");
		status = 0;
		break;

	default:
		*bw = 0;
		*start_chan = 0;
		wpa_printf(MSG_INFO, "invalid channel width and opclass combo");
		break;
	}

	return status;
}

u16 hostapd_copy_sta_eht_240mhz_cap_extn(struct hostapd_data *hapd,
					 struct sta_info *sta,
					 enum ieee80211_op_mode opmode,
					 struct ieee802_11_elems_extn
					 *elems_extn)
{
	struct sta_info_extn *sta_extn;

	if (!sta)
		return -1;

	sta_extn = (struct sta_info_extn*) &sta->sta_extn;

	if (!elems_extn->eht_240mhz_capab || !hostapd_is_eht_enabled(hapd)) {
		os_free(sta_extn->params_240mhz.eht_240mhz_capab);
		sta_extn->params_240mhz.eht_240mhz_capab = NULL;
		return 0;
	}

	if (!sta_extn->params_240mhz.eht_240mhz_capab) {
		sta_extn->params_240mhz.eht_240mhz_capab =
			os_zalloc(elems_extn->eht_240mhz_capab_len);
		if (!sta_extn->params_240mhz.eht_240mhz_capab)
			return -1;
	}

	os_memcpy(sta_extn->params_240mhz.eht_240mhz_capab,
		  elems_extn->eht_240mhz_capab,
		  elems_extn->eht_240mhz_capab_len);
	sta_extn->params_240mhz.eht_240mhz_capab_len =
		elems_extn->eht_240mhz_capab_len;

	return 0;
}

void hostapd_get_eht_240mhz_cap_extn(struct hostapd_data *hapd,
				     struct sta_info_extn *sta_extn,
				     struct ieee80211_240mhz_vendor_oper_extn
				     *dest)
{
	if (!dest)
		return;

	if (sta_extn->params_240mhz.eht_240mhz_capab_len > sizeof(*dest))
		sta_extn->params_240mhz.eht_240mhz_capab_len = sizeof(*dest);

	os_memcpy(dest, sta_extn->params_240mhz.eht_240mhz_capab,
		  sta_extn->params_240mhz.eht_240mhz_capab_len);
}


void hostapd_sta_os_free_extn(struct sta_info_extn *sta_extn)
{
	os_free(sta_extn->params_240mhz.eht_240mhz_capab);
}

static int hostapd_validate_240mhz_cap_extn(const u8 *capab, size_t len)
{
	const size_t expected = sizeof(struct ieee80211_240mhz_vendor_oper_extn);
	const struct ieee80211_240mhz_vendor_oper_extn *extn;
	u8 diff;

	if (!capab || len != expected)
	    return -EINVAL;

	extn = (const struct ieee80211_240mhz_vendor_oper_extn *) capab;

	if (extn->is5ghz240mhz == 0) {
		wpa_printf(MSG_DEBUG, "240MHZ support not enabled");
		return -EINVAL;
	}

	if (!extn->ccfs0 || !extn->ccfs1) {
		wpa_printf(MSG_DEBUG, "ccfs should be non zero, ccfs0:%u ccfs1:%u",
			   extn->ccfs0, extn->ccfs1);
		return -EINVAL;
	}

	diff = (extn->ccfs0 > extn->ccfs1) ? (extn->ccfs0 - extn->ccfs1) :
		(extn->ccfs1 - extn->ccfs0);
	if (diff != 16) {
		wpa_printf(MSG_DEBUG, "Incompatible ccfs , ccfs0:%u ccfs1:%u",
			   extn->ccfs0, extn->ccfs1);
		return -EINVAL;
	}

	if ((extn->punct_bitmap & PUNCTURING_PATTERN_5G_320MHZ) !=
	    PUNCTURING_PATTERN_5G_320MHZ) {
		wpa_printf(MSG_DEBUG, "Invalid Puncturing bitmap:0x%x",
			   extn->punct_bitmap);
		return -EINVAL;
	}

	return 0;
}

int
ieee802_11_parse_vendor_specific_eht_240mhz_cap_extn(struct ieee802_11_elems
						     *elems,
						     unsigned int oui_flag,
						     const u8 *pos, size_t elen)
{
	size_t off = 0;
	struct ieee802_11_elems_extn *elems_extn = &elems->elems_extn;

	/* Must have at least OUI(3) + type(1) before accessing pos[3]. */
	if (elen < 4)
		return 0;

	/* QCN_OUI_TYPE */
	if (oui_flag != OUI_QCN || pos[3] != 0x1)
		return 0;

	pos += 4;
	elen -=4;

	while (off + 2 <= elen) {
		u8 id   = pos[0];
		u8 tlen = pos[1];

		if (off + (size_t)2 + (size_t)tlen > elen) {
			wpa_printf(MSG_DEBUG,
					"Truncated vendor TLV: id=%u len=%u (off=%zu elen=%zu)",
					id, tlen, off, elen);
			break;
		}

		if (id == QCN_ATTRIB_HE_240_MHZ_SUPP) {
			if (tlen > QCN_HE_240_MHZ_MAX_ELEM_LEN) {
				wpa_printf(MSG_DEBUG,
						"Length %u for 240MHz Vendor IE exceeded (max %u)",
						tlen, (unsigned)QCN_HE_240_MHZ_MAX_ELEM_LEN);
			} else {
				elems_extn->eht_240mhz_capab     = pos + 2;
				elems_extn->eht_240mhz_capab_len = tlen;

				/* Validate the parsed 240 MHz Vendor Capability */
				if (elems_extn->eht_240mhz_capab &&
				    hostapd_validate_240mhz_cap_extn(elems_extn->eht_240mhz_capab,
								     elems_extn->eht_240mhz_capab_len)) {
				    wpa_hexdump(MSG_DEBUG, "240MHz Vendor IE ",
						elems_extn->eht_240mhz_capab,
						elems_extn->eht_240mhz_capab_len);
				    elems_extn->eht_240mhz_capab = NULL;
				    elems_extn->eht_240mhz_capab_len = 0;
				}
			}
		}

		pos += (size_t)2 + (size_t)tlen;
		off += (size_t)2 + (size_t)tlen;
	}

	return 0;
}

void hostapd_copy_sta_add_params_extn(struct hostapd_sta_add_params_extn
				      *params_extn,
				      struct sta_info_extn *sta_extn)
{
	if (!sta_extn || !params_extn)
		return;

	params_extn->params_240mhz.eht_240mhz_capab =
		sta_extn->params_240mhz.eht_240mhz_capab;
	params_extn->params_240mhz.eht_240mhz_capab_len =
		sta_extn->params_240mhz.eht_240mhz_capab_len;

	/* Propagate VHT MCS10/11 negotiation result to the driver.
	 * higher_vhtmcs_supp is stored in sta_info_extn (same struct as sta_extn).
	 */
	params_extn->higher_vhtmcs_supp = sta_extn->higher_vhtmcs_supp;
	params_extn->he_cap_info_internal = sta_extn->he_cap_info_internal;
}

bool hostapd_dfs_get_valid_punc_bitmap_extn(int chan_freq,
					    u16 punct_bitmap,
					    int center_freq,
					    int half_width)
{
	int start_freq, chan_bit_pos;

	if (punct_bitmap) {
		start_freq = center_freq - half_width;
		chan_bit_pos = (chan_freq - start_freq) / 20;

		if (punct_bitmap & BIT(chan_bit_pos))
			return true;
	}

	return false;
}


int hostapd_is_dfs_overlap_extn(struct hostapd_iface *iface,
				enum chan_width width,
				int center_freq, u16 punct_bitmap)
{
	struct hostapd_channel_data *chan;
	struct hostapd_hw_modes *mode = iface->current_mode;
	int half_width;
	int res = 0;
	int i;

	if (!iface->conf->ieee80211h || !mode ||
	    mode->mode != HOSTAPD_MODE_IEEE80211A)
		return 0;

	switch (width) {
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
		half_width = 10;
		break;
	case CHAN_WIDTH_40:
		half_width = 20;
		break;
	case CHAN_WIDTH_80:
	case CHAN_WIDTH_80P80:
		half_width = 40;
		break;
	case CHAN_WIDTH_160:
		half_width = 80;
		break;
	case CHAN_WIDTH_320:
		half_width = 160;
		break;
	default:
		wpa_printf(MSG_WARNING, "DFS chanwidth %d not supported",
			   width);
		return 0;
	}

	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];

		if (!(chan->flag & HOSTAPD_CHAN_RADAR))
			continue;

		if ((chan->flag & HOSTAPD_CHAN_DFS_MASK) ==
		    HOSTAPD_CHAN_DFS_AVAILABLE)
			continue;

		if (hostapd_dfs_get_valid_punc_bitmap_extn(chan->freq,
							   punct_bitmap,
							   center_freq,
							   half_width))
			continue;

		if (center_freq - chan->freq < half_width &&
		    chan->freq - center_freq < half_width)
			res++;
	}

	wpa_printf(MSG_DEBUG, "DFS CAC required: (%d, %d): in range: %s, width %d half_width %d",
		   center_freq - half_width, center_freq + half_width,
		   res ? "yes" : "no", width, half_width);

	return res;
}

int hostapd_find_dfs_range_extn(struct hostapd_iface *iface,
				enum chan_width bandwidth,
				struct hostapd_freq_params *freq_params)
{
	int dfs_range = 0;

	dfs_range += hostapd_is_dfs_overlap_extn(iface, bandwidth,
						 freq_params->center_freq1 ?
						 freq_params->center_freq1 :
						 freq_params->freq,
						 freq_params->punct_bitmap);

	if (freq_params->center_freq2)
		dfs_range += hostapd_is_dfs_overlap_extn(
			iface, bandwidth, freq_params->center_freq2,
			freq_params->punct_bitmap);

	return dfs_range;
}


static bool hostapd_is_5ghz_320mhz_csa_attr_required(struct hostapd_data *hapd)
{
	if (!hapd)
		return false;

	return hapd->cs_freq_params.bandwidth == CHWIDTH_320 &&
	       is_5ghz_freq(hapd->cs_freq_params.freq) &&
	       hapd->cs_freq_params.punct_bitmap;
}

size_t hostapd_qcn_buflen_add_5ghz_320mhz_csa_attr(struct hostapd_data *hapd)
{
	if (!hostapd_is_5ghz_320mhz_csa_attr_required(hapd))
		return 0;

	return QCN_ATTRIB_HDR_LEN +
	       sizeof(struct ieee80211_240mhz_vendor_oper_extn_v2);
}

u8 * hostapd_qcn_eid_add_5ghz_320mhz_csa_attr(struct hostapd_data *hapd,
					       u8 *pos)
{
	struct ieee80211_240mhz_vendor_oper_extn_v2 *cap;
	u8 ccfs0, ccfs1;

	if (!pos)
		return pos;

	if (!hostapd_is_5ghz_320mhz_csa_attr_required(hapd))
		return pos;

	if (ieee80211_freq_to_chan(hapd->cs_freq_params.center_freq1,
				  &ccfs1) != HOSTAPD_MODE_IEEE80211A)
		return pos;

	ccfs0 = hapd->cs_freq_params.channel < ccfs1 ? ccfs1 - 16 : ccfs1 + 16;

	*pos++ = QCN_ATTRIB_5GHZ_320MHZ_CSA;
	*pos++ = sizeof(struct ieee80211_240mhz_vendor_oper_extn_v2);

	cap = (struct ieee80211_240mhz_vendor_oper_extn_v2 *) pos;
	os_memset(cap, 0, sizeof(*cap));
	cap->ccfs0 = ccfs0;
	cap->ccfs1 = ccfs1;
	cap->punct_bitmap = host_to_le16(hapd->cs_freq_params.punct_bitmap);
	cap->is5ghz240mhz = 1;
	pos += sizeof(struct ieee80211_240mhz_vendor_oper_extn_v2);

	wpa_printf(MSG_DEBUG,
		   "5G 320MHz CSA QCN attr: ccfs0=%u ccfs1=%u punct=0x%04x",
		   ccfs0, ccfs1, hapd->cs_freq_params.punct_bitmap);

	return pos;
}
