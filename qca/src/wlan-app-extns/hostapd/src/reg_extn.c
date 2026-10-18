/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "includes.h"
#include <netlink/genl/genl.h>
#include "common.h"
#include "common/hw_features_common.h"
#include "common/qca-vendor.h"
#include "drivers/driver_nl80211.h"
#include "ap/hostapd.h"
#include "ap/hw_features.h"
#include "ap/ieee802_11.h"
#include "reg_extn.h"
#include "ap/ap_drv_ops.h"

static inline bool
hostapd_is_freq_in_between_extn(int freq, int start_freq, int end_freq)
{
	return freq >= start_freq && freq <= end_freq;
}

static inline bool
hostapd_is_range_overlap_extn(int start1, int end1, int start2, int end2)
{
	return (hostapd_is_freq_in_between_extn(start1, start2, end2) ||
		hostapd_is_freq_in_between_extn(end1, start2, end2) ||
		hostapd_is_freq_in_between_extn(start2, start1, end1) ||
		hostapd_is_freq_in_between_extn(end2, start1, end1));
}

static bool
hostapd_is_6ghz_overlap_hw_extn(struct hostapd_multi_hw_info *hw_info)
{
	return hostapd_is_range_overlap_extn(hw_info->start_freq,
					     hw_info->end_freq,
					     DEFAULT_LOW_6GFREQ,
					     DEFAULT_HIGH_6GFREQ);
}

void hostapd_query_hw_blocklist_extn(struct hostapd_iface *iface,
				     struct hostapd_data *hapd)
{
	struct hostapd_multi_hw_info *hw_info = NULL;
	int radio_idx = -1;
	int ret;

	if (!iface || !hapd)
		return;

	if (!hapd->driver || !hapd->drv_priv ||
	    !hapd->driver->is_6ghz_hw_blocked_chans_supported ||
	    !hapd->driver->fetch_hw_blocked_chans)
		return;

	if (!hapd->driver->is_6ghz_hw_blocked_chans_supported(hapd->drv_priv))
		return;

	if (!iface->freq) {
		ret = configured_fixed_chan_to_freq_helper(iface);
		if (ret) {
			wpa_printf(MSG_ERROR, "Configured channel is not valid (%d)",
				   ret);
			return;
		}
	}

	if (iface->freq && iface->num_multi_hws && iface->multi_hw_info)
		hw_info = hostapd_get_current_hw_info(iface, iface->freq);

	if (iface->freq && hw_info) {
		if (!hostapd_is_6ghz_overlap_hw_extn(hw_info)) {
			wpa_printf(MSG_DEBUG,
				   "Current HW (hw_idx=%d start_freq=%d end_freq=%d) does not overlap with 6 GHz, skip querying HW blocklist",
				   hw_info->hw_idx, hw_info->start_freq,
				   hw_info->end_freq);
			return;
		}

		radio_idx = hw_info->hw_idx;
		wpa_printf(MSG_DEBUG,
			   "Query HW blocklist for freq=%d hw_idx=%d",
			   iface->freq, radio_idx);
	} else if (iface->freq) {
		wpa_printf(MSG_DEBUG,
			   "Unable to map freq=%d to hw_idx, query HW blocklist for all radios",
			   iface->freq);
	} else {
		wpa_printf(MSG_DEBUG,
			   "Frequency is unset, query HW blocklist for all radios");
	}

	ret = hapd->driver->fetch_hw_blocked_chans(hapd->drv_priv,
						      radio_idx);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "Failed to fetch HW blocklist channels (radio_idx=%d ret=%d)",
			   radio_idx, ret);
		return;
	}
}


static void hw_blocklist_free_entry_extn(
	struct hostapd_hw_blocklist_info *hw_blocklist_info)
{
	u32 i;

	if (!hw_blocklist_info)
		return;

	for (i = 0; i < hw_blocklist_info->num_pwr_modes; i++) {
		os_free(hw_blocklist_info->pwr_modes[i].fb_chans);
		os_free(hw_blocklist_info->pwr_modes[i].pc_chans);
	}
	os_free(hw_blocklist_info->pwr_modes);
	hw_blocklist_info->pwr_modes = NULL;
	hw_blocklist_info->num_pwr_modes = 0;
}

void hostapd_free_hw_blocklist_info_extn(
	struct hostapd_hw_blocklist_info *hw_blocklist_info,
	unsigned int num_hw_blocklist)
{
	unsigned int i;

	if (!hw_blocklist_info)
		return;

	for (i = 0; i < num_hw_blocklist; i++)
		hw_blocklist_free_entry_extn(&hw_blocklist_info[i]);

	os_free(hw_blocklist_info);
}

static int hw_blocklist_copy_entry_extn(
	struct hostapd_hw_blocklist_info *dst,
	const struct hostapd_hw_blocklist_info *src)
{
	u32 i;

	os_memset(dst, 0, sizeof(*dst));
	dst->hw_idx = src->hw_idx;

	if (!src->num_pwr_modes || !src->pwr_modes)
		return 0;

	dst->pwr_modes = os_zalloc(src->num_pwr_modes * sizeof(*dst->pwr_modes));
	if (!dst->pwr_modes)
		return -ENOMEM;

	for (i = 0; i < src->num_pwr_modes; i++) {
		const struct hostapd_hw_blocklist_pwr_mode *src_mode;
		struct hostapd_hw_blocklist_pwr_mode *dst_mode;

		src_mode = &src->pwr_modes[i];
		dst_mode = &dst->pwr_modes[i];
		dst_mode->pwr_mode_id = src_mode->pwr_mode_id;
		dst_mode->num_fb_chans = src_mode->num_fb_chans;
		dst_mode->num_pc_chans = src_mode->num_pc_chans;

		if (src_mode->num_fb_chans && src_mode->fb_chans) {
			dst_mode->fb_chans = os_memdup(
				src_mode->fb_chans,
				src_mode->num_fb_chans *
					sizeof(*src_mode->fb_chans));
			if (!dst_mode->fb_chans)
				goto fail;
		}

		if (src_mode->num_pc_chans && src_mode->pc_chans) {
			dst_mode->pc_chans = os_memdup(
				src_mode->pc_chans,
				src_mode->num_pc_chans *
					sizeof(*src_mode->pc_chans));
			if (!dst_mode->pc_chans)
				goto fail;
		}

		dst->num_pwr_modes++;
	}

	return 0;

fail:
	hw_blocklist_free_entry_extn(dst);
	return -ENOMEM;
}

int hw_blocklist_update_list_extn(
	struct hostapd_hw_blocklist_info **hw_blocklist_info,
	unsigned int *num_hw_blocklist,
	const struct hostapd_hw_blocklist_info *new_entry)
{
	struct hostapd_hw_blocklist_info *new_hw_blocklist_info;
	struct hostapd_hw_blocklist_info copy;
	unsigned int i;
	int ret;

	if (!hw_blocklist_info || !num_hw_blocklist || !new_entry)
		return -EINVAL;

	ret = hw_blocklist_copy_entry_extn(&copy, new_entry);
	if (ret)
		return ret;

	for (i = 0; i < *num_hw_blocklist; i++) {
		if ((*hw_blocklist_info)[i].hw_idx == copy.hw_idx) {
			hw_blocklist_free_entry_extn(&(*hw_blocklist_info)[i]);
			(*hw_blocklist_info)[i] = copy;
			return 0;
		}
	}

	new_hw_blocklist_info = os_realloc_array(
		*hw_blocklist_info, *num_hw_blocklist + 1,
		sizeof(*new_hw_blocklist_info));
	if (!new_hw_blocklist_info) {
		hw_blocklist_free_entry_extn(&copy);
		return -ENOMEM;
	}

	*hw_blocklist_info = new_hw_blocklist_info;
	(*hw_blocklist_info)[*num_hw_blocklist] = copy;
	(*num_hw_blocklist)++;
	return 0;
}


static void hostapd_reg_dump_hw_blocklist_extn(struct hostapd_iface *iface)
{
	const char *ifname = "unknown";
	unsigned int i, j, k;

	if (!iface)
		return;

	if (iface->num_bss > 0 && iface->bss && iface->bss[0] &&
	    iface->bss[0]->conf && iface->bss[0]->conf->iface[0] != '\0')
		ifname = iface->bss[0]->conf->iface;

	wpa_printf(MSG_DEBUG,
		   "HW blocklist dump on %s: radios=%u",
		   ifname, iface->iface_extn.num_hw_blocklist);

	if (!iface->iface_extn.hw_blocklist_info || !iface->iface_extn.num_hw_blocklist)
		return;

	for (i = 0; i < iface->iface_extn.num_hw_blocklist; i++) {
		const struct hostapd_hw_blocklist_info *info;

		info = &iface->iface_extn.hw_blocklist_info[i];
		wpa_printf(MSG_DEBUG,
			   "HW blocklist[%u]: hw_idx=%u pwr_modes=%u",
			   i, info->hw_idx, info->num_pwr_modes);

		for (j = 0; j < info->num_pwr_modes; j++) {
			const struct hostapd_hw_blocklist_pwr_mode *mode;

			mode = &info->pwr_modes[j];
			wpa_printf(MSG_DEBUG,
				   "HW blocklist[%u]: pwr_mode[%u]=%u fb_chans=%u pc_chans=%u",
				   i, j, mode->pwr_mode_id,
				   mode->num_fb_chans, mode->num_pc_chans);

			for (k = 0; k < mode->num_fb_chans; k++) {
				const struct hostapd_hw_blocklist_fb_chan *fb;

				fb = &mode->fb_chans[k];
				wpa_printf(MSG_DEBUG,
					   "HW blocklist[%u]: FB[%u] pri20_bitmap=0x%x center_freq=%u max_bw=%u",
					   i, k, fb->pri20_bitmap,
					   fb->center_freq, fb->max_bw);
			}

			for (k = 0; k < mode->num_pc_chans; k++) {
				const struct hostapd_hw_blocklist_pc_chan *pc;

				pc = &mode->pc_chans[k];
				wpa_printf(MSG_DEBUG,
					   "HW blocklist[%u]: PC[%u] center_freq=%u max_bw=%u puncture_pattern_bitmap=0x%x",
					   i, k, pc->center_freq,
					   pc->max_bw, pc->punc_pat_bmap);
			}
		}
	}
}

void hostapd_event_hw_blocklist_notify_extn(
	struct hostapd_data *hapd,
	const struct hostapd_hw_blocklist_info *hw_blocklist_info)
{
	struct hostapd_iface *iface;

	if (!hapd || !hapd->iface || !hw_blocklist_info)
		return;

	iface = hostapd_afc_find_iface(hapd, hw_blocklist_info->hw_idx);
	if (!iface) {
		wpa_printf(MSG_ERROR,
			   "HWBL: No iface matched hw_idx=%u",
			   hw_blocklist_info->hw_idx);
		return;
	}

	if (hw_blocklist_update_list_extn(
		    &iface->iface_extn.hw_blocklist_info,
		    &iface->iface_extn.num_hw_blocklist,
		    hw_blocklist_info)) {
		wpa_printf(MSG_ERROR,
			   "Failed to store HW blocklist info for hw_idx=%u",
			   hw_blocklist_info->hw_idx);
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "Stored HW blocklist info for hw_idx=%u on %s (modes=%u, total_hw=%u)",
		   hw_blocklist_info->hw_idx, iface->phy,
		   hw_blocklist_info->num_pwr_modes,
		   iface->iface_extn.num_hw_blocklist);

	hostapd_reg_dump_hw_blocklist_extn(iface);
}

static u16 hw_features_hw_blocklist_bw_to_mhz_extn(u32 max_bw)
{
	switch (max_bw) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		return 20;
	case NL80211_CHAN_WIDTH_40:
		return 40;
	case NL80211_CHAN_WIDTH_80:
		return 80;
	case NL80211_CHAN_WIDTH_160:
		return 160;
	case NL80211_CHAN_WIDTH_320:
		return 320;
	default:
		return 0;
	}
}

static const u16 hw_bl_puncture_pattern_map_80_extn[] = {
	0x1, 0x2, 0x4, 0x8
};

static const u16 hw_bl_puncture_pattern_map_160_extn[] = {
	0x1, 0x2, 0x4, 0x8,
	0x10, 0x20, 0x40, 0x80,
	0x0C, 0x03, 0xC0, 0x30
};

static const u16 hw_bl_puncture_pattern_map_320_extn[] = {
	0x000C, 0x0003, 0x00C0, 0x0030,
	0x0C00, 0x0300, 0xC000, 0x3000,
	0x000F, 0x00F0, 0x0F00, 0xF000,
	0xF003, 0xF00C, 0xF030, 0xF0C0,
	0xF300, 0xFC00, 0x003F, 0x00CF,
	0x030F, 0x0C0F, 0x300F, 0xC00F
};

static int hw_features_get_puncture_pattern_index_extn(u16 bw,
							u16 puncture_pattern)
{
	const u16 *patterns = NULL;
	u8 num_patterns = 0;
	u8 i;

	switch (bw) {
	case 80:
		patterns = hw_bl_puncture_pattern_map_80_extn;
		num_patterns = ARRAY_SIZE(hw_bl_puncture_pattern_map_80_extn);
		break;
	case 160:
		patterns = hw_bl_puncture_pattern_map_160_extn;
		num_patterns = ARRAY_SIZE(hw_bl_puncture_pattern_map_160_extn);
		break;
	case 320:
		patterns = hw_bl_puncture_pattern_map_320_extn;
		num_patterns = ARRAY_SIZE(hw_bl_puncture_pattern_map_320_extn);
		break;
	default:
		return -1;
	}

	for (i = 0; i < num_patterns; i++) {
		if (patterns[i] == puncture_pattern)
			return i;
	}

	return -1;
}

static bool hw_features_is_chan_punc_extn(u16 puncture_pattern, u16 bw)
{
	if (!puncture_pattern)
		return false;

	return hw_features_get_puncture_pattern_index_extn(
		bw, puncture_pattern) >= 0;
}

static bool hw_features_is_chan_in_full_hw_blocklist_extn(
	const struct hostapd_hw_blocklist_fb_chan *fb_chans, u32 num_fb_chans,
	u16 freq, u16 center_freq, u16 bw)
{
	u32 i;

	for (i = 0; i < num_fb_chans; i++) {
		u16 bl_bw = hw_features_hw_blocklist_bw_to_mhz_extn(fb_chans[i].max_bw);
		int start_freq;
		int offset;
		u32 pri20_bit;

		if (!bl_bw || bl_bw != bw || fb_chans[i].center_freq != center_freq)
			continue;

		if (bw == 20) {
			start_freq = freq;
		} else {
			start_freq = (int)center_freq - ((int)bw / 2) + 10;
			if (start_freq <= 0)
				continue;
		}

		offset = (int)freq - start_freq;
		if (offset < 0 || offset % 20)
			continue;

		pri20_bit = offset / 20;
		if (pri20_bit >= 16)
			continue;

		if (fb_chans[i].pri20_bitmap & BIT(pri20_bit))
			return true;
	}

	return false;
}

static bool hw_features_is_chan_in_punc_hw_blocklist_extn(
	const struct hostapd_hw_blocklist_pc_chan *pc_chans, u32 num_pc_chans,
	u16 puncture_pattern, u16 center_freq, u16 bw)
{
	int punc_idx;
	u32 i;

	punc_idx = hw_features_get_puncture_pattern_index_extn(
		bw, puncture_pattern);
	if (punc_idx < 0)
		return false;

	for (i = 0; i < num_pc_chans; i++) {
		u16 bl_bw = hw_features_hw_blocklist_bw_to_mhz_extn(pc_chans[i].max_bw);

		if (!bl_bw || bl_bw != bw || pc_chans[i].center_freq != center_freq)
			continue;

		if (pc_chans[i].punc_pat_bmap & BIT(punc_idx))
			return true;
	}

	return false;
}

static const struct hostapd_hw_blocklist_pwr_mode *
hw_features_get_hw_blocklist_pwr_mode_extn(
	const struct hostapd_hw_blocklist_info *hw_blocklist_info, u8 pwr_mode_id)
{
	u8 i;

	if (!hw_blocklist_info || !hw_blocklist_info->pwr_modes)
		return NULL;

	for (i = 0; i < hw_blocklist_info->num_pwr_modes; i++) {
		if (hw_blocklist_info->pwr_modes[i].pwr_mode_id == pwr_mode_id)
			return &hw_blocklist_info->pwr_modes[i];
	}

	return NULL;
}

static bool hw_features_is_channel_in_hw_blocklist_extn(
	const struct hostapd_hw_blocklist_info *hw_blocklist_info,
	u16 freq, u16 center_freq, u16 bw, u8 pwr_mode_id, u16 puncture_pattern)
{
	const struct hostapd_hw_blocklist_pwr_mode *pwr_mode;

	if (!hw_blocklist_info || !freq || !bw)
		return false;

	if (!center_freq && bw == 20)
		center_freq = freq;

	if (!center_freq)
		return false;

	pwr_mode = hw_features_get_hw_blocklist_pwr_mode_extn(hw_blocklist_info,
							      pwr_mode_id);
	if (!pwr_mode)
		return false;

	if (!hw_features_is_chan_punc_extn(puncture_pattern, bw)) {
		if (!pwr_mode->fb_chans || !pwr_mode->num_fb_chans)
			return false;

		return hw_features_is_chan_in_full_hw_blocklist_extn(
			pwr_mode->fb_chans, pwr_mode->num_fb_chans,
			freq, center_freq, bw);
	}

	if (!pwr_mode->pc_chans || !pwr_mode->num_pc_chans)
		return false;

	return hw_features_is_chan_in_punc_hw_blocklist_extn(
		pwr_mode->pc_chans, pwr_mode->num_pc_chans,
		puncture_pattern, center_freq, bw);
}

bool hw_features_is_channel_in_hw_blocklist_for_hw_idx_extn(
	const struct hostapd_hw_blocklist_info *hw_blocklist_info,
	unsigned int num_hw_blocklist, u8 hw_idx,
	u16 freq, u16 center_freq, u16 bw, u8 pwr_mode_id,
	u16 puncture_pattern)
{
	unsigned int i;
	bool match_all_hw = hw_idx == NL80211_WIPHY_RADIO_ID_MAX;

	if (!hw_blocklist_info || !num_hw_blocklist)
		return false;

	for (i = 0; i < num_hw_blocklist; i++) {
		if (!match_all_hw && hw_blocklist_info[i].hw_idx != hw_idx)
			continue;

		if (hw_features_is_channel_in_hw_blocklist_extn(
			    &hw_blocklist_info[i], freq, center_freq, bw,
			    pwr_mode_id, puncture_pattern))
			return true;
	}

	return false;
}

bool hostapd_is_hw_blocklisted_combo_extn(struct hostapd_iface *iface,
					  u16 freq, u16 center_freq, u16 bw,
					  u16 puncture_pattern, u8 pwr_mode_id)
{
	struct hostapd_multi_hw_info *hw_info = NULL;
	u8 hw_idx = NL80211_WIPHY_RADIO_ID_MAX;

	if (!iface || !is_6ghz_freq(freq) ||
	    pwr_mode_id >= NL80211_REG_NUM_POWER_MODES)
		return false;
	if (!iface->iface_extn.check_hw_blocklist)
		return false;

	if (iface->current_hw_info &&
	    iface->current_hw_info->start_freq <= freq &&
	    iface->current_hw_info->end_freq >= freq)
		hw_info = iface->current_hw_info;
	else
		hw_info = hostapd_get_current_hw_info(iface, freq);

	if (hw_info)
		hw_idx = hw_info->hw_idx;

	if (!iface->iface_extn.hw_blocklist_info ||
	    !iface->iface_extn.num_hw_blocklist)
		return false;

	return hw_features_is_channel_in_hw_blocklist_for_hw_idx_extn(
		iface->iface_extn.hw_blocklist_info,
		iface->iface_extn.num_hw_blocklist, hw_idx, freq, center_freq,
		bw, pwr_mode_id, puncture_pattern);
}

int hostapd_validate_hw_blocklist_for_freq_params_extn(
	struct hostapd_iface *iface,
	const struct hostapd_freq_params *freq_params,
	u8 pwr_mode_id, const char *op_name)
{
	struct hostapd_multi_hw_info *hw_info = NULL;
	u8 hw_idx = NL80211_WIPHY_RADIO_ID_MAX;
	u16 center_freq;
	u16 bw;

	if (!iface || !freq_params || !is_6ghz_freq(freq_params->freq))
		return 0;

	if (!iface->iface_extn.check_hw_blocklist)
		return 0;

	if (!freq_params->center_freq1 || !freq_params->bandwidth) {
		wpa_printf(MSG_ERROR,
			   "Reject %s: invalid 6 GHz params for HW blocklist check (freq=%d center=%d bw=%d)",
			   op_name ? op_name : "operation",
			   freq_params->freq, freq_params->center_freq1,
			   freq_params->bandwidth);
		return -1;
	}

	if (pwr_mode_id >= NL80211_REG_NUM_POWER_MODES ||
	    pwr_mode_id < NL80211_REG_AP_LPI) {
		u8 orig_pwr_mode_id = pwr_mode_id;

		pwr_mode_id = iface->conf->he_6ghz_reg_pwr_type;
		wpa_printf(MSG_DEBUG,
			   "Invalid power mode ID %u for %s, fallback to iface config value %u",
			   orig_pwr_mode_id, op_name ? op_name : "operation",
			   pwr_mode_id);
	}

	if (iface->current_hw_info &&
	    iface->current_hw_info->start_freq <= freq_params->freq &&
	    iface->current_hw_info->end_freq >= freq_params->freq)
		hw_info = iface->current_hw_info;
	else
		hw_info = hostapd_get_current_hw_info(iface, freq_params->freq);

	if (hw_info)
		hw_idx = hw_info->hw_idx;

	center_freq = freq_params->center_freq1;
	bw = freq_params->bandwidth;
	if (iface->iface_extn.hw_blocklist_info &&
	    iface->iface_extn.num_hw_blocklist &&
	    hw_features_is_channel_in_hw_blocklist_for_hw_idx_extn(
		    iface->iface_extn.hw_blocklist_info,
		    iface->iface_extn.num_hw_blocklist, hw_idx,
		    freq_params->freq, center_freq, bw, pwr_mode_id,
		    freq_params->punct_bitmap)) {
		wpa_printf(MSG_ERROR,
			   "Reject %s: 6 GHz HW blocklisted channel (hw_idx=%u freq=%d center=%u bw=%u pwr_mode=%u punct=0x%04x)",
			   op_name ? op_name : "operation", hw_idx,
			   freq_params->freq, center_freq, bw, pwr_mode_id,
			   freq_params->punct_bitmap);
		return -1;
	}

	return 0;
}

int hostapd_validate_current_6ghz_hw_blocklist_extn(
	struct hostapd_iface *iface,
	u8 pwr_mode_id, const char *op_name)
{
	struct hostapd_freq_params freq_params;
	struct hostapd_hw_modes *mode;
	struct hostapd_data *hapd;

	if (!iface || !iface->num_bss || !iface->bss || !iface->bss[0] ||
	    !is_6ghz_freq(iface->freq))
		return 0;

	hapd = iface->bss[0];
	mode = iface->current_mode;

	if (hostapd_set_freq_params(&freq_params, iface->conf->hw_mode,
				    iface->freq, iface->conf->channel,
				    iface->conf->enable_edmg,
				    iface->conf->edmg_channel,
				    iface->conf->ieee80211n,
				    iface->conf->ieee80211ac,
				    iface->conf->ieee80211ax,
				    iface->conf->ieee80211be,
				    iface->conf->ieee80211bn,
				    iface->conf->secondary_channel,
				    hostapd_get_oper_chwidth(iface->conf),
				    hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
				    hostapd_get_oper_centr_freq_seg1_idx(iface->conf),
				    iface->conf->vht_capab,
				    mode ? &mode->he_capab[IEEE80211_MODE_AP] :
				    NULL,
				    mode ? &mode->eht_capab[IEEE80211_MODE_AP] :
				    NULL,
				    mode ? &mode->uhr_capab[IEEE80211_MODE_AP] :
				    NULL,
				    hostapd_get_punct_bitmap(hapd),
				    pwr_mode_id, 0, 0,
				    iface->conf->bandwidth_device,
				    iface->conf->center_freq_device)) {
		wpa_printf(MSG_ERROR,
			   "Reject %s: failed to build current 6 GHz frequency params",
			   op_name ? op_name : "operation");
		return -1;
	}

	return hostapd_validate_hw_blocklist_for_freq_params_extn(
		iface, &freq_params, pwr_mode_id, op_name);
}

