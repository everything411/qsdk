// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "common/defs.h"
#include "drivers/driver.h"
#include "common/hw_features_common.h"
#include "ap/hostapd.h"
#include "ap/hw_features.h"
#include "cmn.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_ctrl.h"
#include "ap/sta_info.h"
#include "ap/dfs.h"
#include "ap/acs.h"
#include "ap/ap_drv_ops.h"

/**
 * check_40mhz_2g4_bss_snr_below_threshold_extn - Check if a BSS should be
 * ignored in the 40 MHz 2.4 GHz coexistence check due to low SNR
 * @bss: Scan result entry to evaluate
 * @extn_args: Extension arguments carrying the configured SNR threshold
 *
 * Returns true if @bss->snr is below the configured threshold and the entry
 * should be skipped (i.e., treated as non-interfering). Logs a debug message
 * when a BSS is filtered out.
 *
 * Return: true if the BSS should be ignored, false otherwise.
 */
bool check_40mhz_2g4_bss_snr_below_threshold_extn(
	const struct wpa_scan_res *bss,
	const struct check_40mhz_2g4_extn_args *extn_args)
{
	int snr;

	if (!extn_args || !bss)
		return false;

	snr = bss->level - bss->noise;

	if (snr >= extn_args->threshold)
		return false;

	wpa_printf(MSG_DEBUG,
		   "check_40mhz_2g4: ignoring BSS " MACSTR
		   " freq=%d snr=%d (threshold=%u)",
		   MAC2STR(bss->bssid), bss->freq,
		   snr, extn_args->threshold);
	return true;
}

/**
 * hostapd_rssi_to_snr_extn - Convert a received frame's RSSI to SNR
 * @hapd: Pointer to hostapd data (used to read the lowest observed noise floor)
 * @ssi_signal: Signal level (dBm) of the received frame
 *
 * Computes SNR as ssi_signal - lowest_nf. If no noise floor measurement is
 * available (lowest_nf == 0), ssi_signal is returned directly as a proxy.
 *
 * Return: SNR in dB, or ssi_signal if noise floor is unavailable.
 */
int hostapd_rssi_to_snr_extn(struct hostapd_data *hapd, int ssi_signal)
{
	if (!hapd || !hapd->iface)
		return ssi_signal;

	return ssi_signal - DEFAULT_NOISE_FLOOR_2GHZ;
}

/**
 * hostapd_2040_coex_action_snr_below_threshold_extn - Check if a 20/40 MHz
 * coexistence action frame should be ignored due to low signal level
 * @hapd: Pointer to hostapd data (used to read obss_rx_snr_threshold)
 * @ssi_signal: Signal level (dBm) of the received action frame
 *
 * Returns true if the action frame's signal level is below the configured
 * obss_rx_snr_threshold and the frame should be silently discarded.
 *
 * Return: true if the action frame should be ignored, false otherwise.
 */
bool hostapd_2040_coex_action_snr_below_threshold_extn(
	struct hostapd_data *hapd, int rssi)
{
	u8 threshold;
	int snr;

	if (!hapd || !hapd->iconf)
		return false;

	threshold = hapd->iconf->conf_extn.obss_rx_snr_threshold;
        snr = hostapd_rssi_to_snr_extn(hapd, rssi);

        if ((u8)snr >= threshold)
		return false;

	wpa_printf(MSG_DEBUG,
		   "2040 coex: ignoring action frame snr=%d"
		   " (obss_rx_snr_threshold=%d)",
		   snr, threshold);
	return true;
}

/**
 * hostapd_ht40_intolerant_snr_below_threshold_extn - Check if a station's
 * assoc frame SNR is too low to honour its HT 40 MHz Intolerant indication
 * @hapd: Pointer to hostapd data (used to read obss_rx_snr_threshold)
 * @sta: Station that set the HT_CAP_INFO_40MHZ_INTOLERANT bit
 *
 * Returns true if the station's assoc frame SNR is below the configured
 * obss_rx_snr_threshold, meaning the ht40_intolerant_add() call should be
 * skipped for this station.
 *
 * Return: true if ht40_intolerant_add() should be skipped, false otherwise.
 */
bool hostapd_ht40_intolerant_snr_below_threshold_extn(
	struct hostapd_data *hapd, struct sta_info *sta)
{
	int threshold;

	if (!hapd || !hapd->iconf || !sta)
		return false;

	threshold = hapd->iconf->conf_extn.obss_rx_snr_threshold;
	if (!threshold)
		return false;

	if (sta->sta_extn.assoc_snr >= threshold)
		return false;

	wpa_printf(MSG_DEBUG,
		   "HT: Skipping ht40_intolerant_add for STA " MACSTR
		   " assoc_snr=%d (obss_rx_snr_threshold=%d)",
		   MAC2STR(sta->addr), sta->sta_extn.assoc_snr, threshold);
	return true;
}

enum oper_chan_width hostapd_get_oper_chwidth_from_width_extn(u16 width)
{
	enum oper_chan_width oper_chwidth;

	switch (width) {
	case 320:
		oper_chwidth = CONF_OPER_CHWIDTH_320MHZ;
		break;
	case 160:
		oper_chwidth = CONF_OPER_CHWIDTH_160MHZ;
		break;
	case 80:
		oper_chwidth = CONF_OPER_CHWIDTH_80MHZ;
		break;
	case 40:
	case 20:
		oper_chwidth = CONF_OPER_CHWIDTH_USE_HT;
		break;
	default:
		oper_chwidth = CONF_OPER_CHWIDTH_USE_HT;
	}

	return oper_chwidth;
}

u16 hostapd_get_width_from_oper_chwidth_extn(enum oper_chan_width oper_chwidth,
					     int secondary_channel)
{
	u16 width;

	switch (oper_chwidth) {
	case CONF_OPER_CHWIDTH_320MHZ:
		width = 320;
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
	case CONF_OPER_CHWIDTH_80P80MHZ:
		width = 160;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		width = 80;
		break;
	case CONF_OPER_CHWIDTH_USE_HT:
		if (secondary_channel)
			width = 40;
		else
			width = 20;
		break;
	default:
		width = 20;
	}

	return width;
}

int hostapd_validate_mbssid_configuration_extn(struct hostapd_data *hapd)
{
	if (!hapd->conf->mld_ap)
		return 0;

	if (hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED &&
	    hapd->iface->num_bss > EMA_MLO_BSS_MAX_LIMIT) {
		wpa_printf(MSG_ERROR,
			   "Number of BSS (%zu) exceeds the EMA MLO BSS limit (%d)",
			   hapd->iface->num_bss, EMA_MLO_BSS_MAX_LIMIT);
		return -1;
	}

	if ((hapd->iface->conf->group_size == MULTI_MBSSID_GROUP_SIZE_MAX) &&
	    (hapd->iface->max_mgmt_frm_sz < MGMT_MIN_FRAME_SIZE_REQUIRED_MLO_MBSSID)) {
		wpa_printf(MSG_ERROR,
			   "Invalid MBSSID group size (%u) for MLD AP with mgmt frame size (%d)",
			   hapd->iface->conf->group_size, MGMT_MIN_FRAME_SIZE_REQUIRED_MLO_MBSSID);
		return -1;
	}

	return 0;
}

bool hostapd_is_mesh_vap_extn(struct hostapd_bss_config *conf)
{
	return (conf->bss_extn.vap_submode == QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_MESH ||
		conf->bss_extn.vap_submode == QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_ETH_OFFLOAD_MESH ||
		conf->bss_extn.vap_submode == QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_RAW_MODE_MESH);
}

/* Returns the reserved trailing mesh MBSSID group, if any. The slot is
 * retained (and its is_mesh_group marker stays set) across mesh VAP
 * remove so it can be reused on the next mesh VAP add instead of
 * growing multi_mbssid->group[] again. */
struct hostapd_multi_mbssid_group *
hostapd_get_mesh_group_extn(struct hostapd_multi_mbssid *multi_mbssid)
{
	struct hostapd_multi_mbssid_group *group;

	if (!multi_mbssid->group || multi_mbssid->num_mbssid_groups < 1)
		return NULL;

	group = multi_mbssid->group[multi_mbssid->num_mbssid_groups - 1];
	if (group && group->is_mesh_group)
		return group;

	return NULL;
}

bool hostapd_has_mesh_vap_in_group_extn(struct hostapd_data *hapd,
					struct hostapd_multi_mbssid *multi_mbssid)
{
	struct hostapd_multi_mbssid_group *group;

	if (!hapd || !hapd->conf)
		return false;

	if (hapd->iconf->mbssid != MULTI_MBSSID_GROUP_ENABLED)
		return false;

	group = hostapd_get_mesh_group_extn(multi_mbssid);
	if (group) {
		wpa_printf(MSG_DEBUG,
			   "Found mesh MBSSID group %zu (occupied=%d)",
			   (multi_mbssid->num_mbssid_groups - 1), group->num_bss > 0);
		return true;
	}

	return false;
}

/* If mesh vap is the 1st vap to come up, adjust the num_mbssid_group to
 * include the mesh group and assign the last group for the mesh vap. */
void hostapd_mesh_mbssid_reserve_group_extn(struct hostapd_multi_mbssid *multi_mbssid,
					    u8 max_bssid_indicator,
					    u8 *group_index, u64 *prefix_mask)
{
	multi_mbssid->num_mbssid_groups++;

	*group_index = multi_mbssid->num_mbssid_groups - 1;

	wpa_printf(MSG_INFO, "Mesh vap detected, assigning to last group %d",
		   *group_index);

	*prefix_mask = UINT64_MAX << max_bssid_indicator;
}

/* Reject only if the mesh MBSSID group already has an active mesh VAP. A
 * reserved-but-emptied slot, retained after a previous mesh VAP removal,
 * is meant to be reused rather than rejected. */
bool hostapd_mesh_mbssid_reject_duplicate_extn(struct hostapd_data *hapd,
					       struct hostapd_multi_mbssid *multi_mbssid)
{
	struct hostapd_multi_mbssid_group *mesh_group =
		hostapd_get_mesh_group_extn(multi_mbssid);

	if (mesh_group && mesh_group->num_bss) {
		wpa_printf(MSG_ERROR,
			   "Failed to add %s: Mesh vap MBSSID group exists already",
			   hapd->conf->iface);
		return true;
	}

	return false;
}

/* If mesh VAP is being added and the group array was allocated before the
 * mesh VAP existed, reuse a previously retained empty mesh slot, or
 * reallocate to accommodate a new last group for the mesh VAP. The
 * reallocation case handles AP VAPs being brought up first, then the mesh
 * VAP being added later. Returns -1 on allocation failure. */
int hostapd_mesh_mbssid_grow_or_reuse_group_extn(struct hostapd_data *hapd,
						 struct hostapd_multi_mbssid *multi_mbssid,
						 u8 max_bssid_indicator,
						 u8 *group_index, u64 *prefix_mask)
{
	struct hostapd_multi_mbssid_group *mesh_group =
		hostapd_get_mesh_group_extn(multi_mbssid);
	struct hostapd_multi_mbssid_group **new_group;

	if (mesh_group) {
		/* A previous mesh VAP remove left this slot allocated but
		 * empty; reuse it instead of growing the group array again. */
		*group_index = mesh_group->group_id;
		*prefix_mask = UINT64_MAX << max_bssid_indicator;
		wpa_printf(MSG_INFO, "Mesh vap detected: %s, reusing retained group %d",
			   hapd->conf->iface, *group_index);
		return 0;
	}

	new_group = os_realloc_array(multi_mbssid->group,
				     multi_mbssid->num_mbssid_groups + 1,
				     sizeof(struct hostapd_multi_mbssid_group *));

	if (!new_group) {
		wpa_printf(MSG_ERROR, "Failed to allocate MBSSID group for mesh VAP");
		return -1;
	}

	multi_mbssid->num_mbssid_groups++;
	*group_index = multi_mbssid->num_mbssid_groups - 1;
	wpa_printf(MSG_INFO, "Mesh vap detected: %s, assigning to last group %d",
		   hapd->conf->iface, *group_index);

	*prefix_mask = UINT64_MAX << max_bssid_indicator;
	multi_mbssid->group = new_group;
	multi_mbssid->group[multi_mbssid->num_mbssid_groups - 1] = NULL;

	return 0;
}

void hostapd_mesh_mbssid_mark_group_extn(struct hostapd_data *hapd,
					 struct hostapd_multi_mbssid_group *group)
{
	group->is_mesh_group = true;
}

#ifdef HOSTAPD
static bool regdom_is_chan_bw_valid(struct hostapd_iface *iface,
				    u16 freq,
				    u16 center_freq,
				    enum oper_chan_width try_width,
				    int secondary,
				    u16 punct_bitmap)
{
	u16 bw_mhz = hostapd_get_width_from_oper_chwidth_extn(try_width,
							      secondary);
	u16 cen_freq = freq;
	u8 pwr_mode = iface->conf->he_6ghz_reg_pwr_type;
	u8 best = pwr_mode;

	if (bw_mhz > 20)
		cen_freq = hostapd_get_bonded_chan_center_freq(freq,
							       bw_mhz,
							       center_freq,
							       (s8)secondary);

	if (bw_mhz > 20 && !cen_freq)
		return false;

	if (is_6ghz_freq(freq) && iface->conf->enable_best_power_mode) {
		best = hostapd_get_best_ap_6ghz_power_mode(iface, freq,
							   cen_freq,
							   bw_mhz,
							   punct_bitmap);

		if (best < NL80211_REG_NUM_POWER_MODES)
			pwr_mode = best;

		if (pwr_mode >= NL80211_REG_NUM_POWER_MODES)
			return false;
	}

	return hostapd_validate_chan_bw_in_pwr_mode(iface, freq, cen_freq,
						    bw_mhz, punct_bitmap, pwr_mode);
}


bool regdom_subchans_valid(struct hostapd_iface *iface)
{
	enum oper_chan_width cur_width = hostapd_get_oper_chwidth(iface->conf);
	int center_freq = 0;

	/* No channel is configured yet. Skip validation. Continue with regular flow */
	if (!iface->freq || !iface->current_mode)
		return true;

	/* Currently 80P80 support is not present, treat as valid to continue regular flow */
	if (cur_width == CONF_OPER_CHWIDTH_80P80MHZ)
		return true;

	/* 320 MHz blocks overlap; validate the configured segment. */
	if (cur_width == CONF_OPER_CHWIDTH_320MHZ) {
		u8 center = hostapd_get_oper_centr_freq_seg0_idx(iface->conf);

		if (!center)
			return false;

		center_freq = iface->freq +
			5 * (center - iface->conf->channel);
	}

	if (!regdom_is_chan_bw_valid(iface, iface->freq, center_freq, cur_width,
				     iface->conf->secondary_channel,
				     iface->conf->punct_bitmap))
		return false;

	wpa_printf(MSG_DEBUG,
		   "REGDOM: channel %d (%d MHz) valid in new country",
		   iface->conf->channel, iface->freq);
	return true;
}


/*
 * regdom_chan_allowed_bw - Validate driver allowed_bw flags for BW >= 40 MHz
 * and resolve the secondary_channel (HT40 sub-block direction: +1/-1).
 *
 * The secondary_channel is required for ALL widths >= 40 MHz
 * (hostapd_set_freq_params rejects sec_channel_offset=0 for 80/160/320 MHz).
 * It always reflects the primary's position within its 40 MHz sub-block.
 *
 * 5/6 GHz: hostapd_get_bonded_chan_center_freq(freq, 40) gives the 40 MHz
 *   bonded pair center — if center > primary: +1; else: -1.
 *
 * 2.4 GHz: the 40 MHz bonded table has no 2.4 GHz entries (circular: the
 *   center depends on the direction we're trying to discover).  Use the same
 *   approach as hw_features.c:ieee80211n_check_40mhz_2g4():
 *   probe freq+20 with hostapd_is_usable_chan() + allowed_bw WIDTH_40P;
 *   probe freq-20 with hostapd_is_usable_chan() + allowed_bw WIDTH_40M.
 */
static bool regdom_chan_allowed_bw(struct hostapd_iface *iface,
				   const struct hostapd_channel_data *chan,
				   enum oper_chan_width try_width,
				   int *secondary)
{
	switch (try_width) {
	case CONF_OPER_CHWIDTH_320MHZ:
		if (!(chan->allowed_bw & HOSTAPD_CHAN_WIDTH_320))
			return false;
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		if (!(chan->allowed_bw & HOSTAPD_CHAN_WIDTH_160))
			return false;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		if (!(chan->allowed_bw & HOSTAPD_CHAN_WIDTH_80))
			return false;
		break;
	case CONF_OPER_CHWIDTH_USE_HT:
		if (!*secondary)
			return true; /* HT20: no secondary needed */
		break;
	default:
		return true;
	}

	/* Resolve secondary direction — applies to all BW >= 40 MHz */
	if (chan->freq >= 2412 && chan->freq <= 2472) {
		if (hostapd_is_usable_chan(iface, chan->freq + 20, 0) > 0 &&
		    (chan->allowed_bw & HOSTAPD_CHAN_WIDTH_40P)) {
			*secondary = 1;
		} else if (hostapd_is_usable_chan(iface, chan->freq - 20, 0) > 0 &&
			   (chan->allowed_bw & HOSTAPD_CHAN_WIDTH_40M)) {
			*secondary = -1;
		} else {
			return false;
		}
	} else {
		u16 cen40 = hostapd_get_bonded_chan_center_freq(chan->freq, 40, 0, 0);

		if (!cen40)
			return false;

		*secondary = (cen40 > chan->freq) ? 1 : -1;
	}

	return true;
}


/*
 * regdom_chan_seg0 - Validate bonded channel group and compute seg0 center
 * channel index.
 *
 * Uses acs_get_bw_center_chan() (ACS table) for the center channel index,
 * then checks that the selected bonded group exists.
 *
 * @secondary: secondary channel offset for HT40+/-.
 * @bw320: 320 MHz layout to try; ignored for other widths.
 * @out_center: set to the center channel index on success.
 * @out_center_freq: set to the center frequency in MHz on success.
 * Return: true if seg0 could be determined; false to skip channel.
 */
static bool regdom_chan_seg0(const struct hostapd_channel_data *chan,
			     enum oper_chan_width try_width,
			     int secondary, enum bw_type bw320,
			     int *out_center, int *out_center_freq)
{
	u16 bw_mhz = hostapd_get_width_from_oper_chwidth_extn(try_width,
							      secondary);
	int center = 0, center_freq;

	switch (try_width) {
	case CONF_OPER_CHWIDTH_USE_HT:
		if (secondary && chan->freq >= 2400 && chan->freq < 2500) {
			center = chan->chan + 2 * secondary; /* 2.4 GHz HT40 */
		} else if (secondary) {
			center = acs_get_bw_center_chan(chan->freq, ACS_BW40);
			if (!center)
				return false;
		} else {
			center = chan->chan; /* HT20 */
		}
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		center = acs_get_bw_center_chan(chan->freq, ACS_BW80);
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		center = acs_get_bw_center_chan(chan->freq, ACS_BW160);
		break;
	case CONF_OPER_CHWIDTH_320MHZ:
		center = acs_get_bw_center_chan(chan->freq, bw320);
		break;
	default:
		center = chan->chan;
		break;
	}

	if (!center)
		return false;

	center_freq = chan->freq + 5 * (center - chan->chan);

	if (bw_mhz > 20 &&
	    !hostapd_get_bonded_chan_center_freq(chan->freq, bw_mhz,
						 center_freq,
						 (s8)secondary))
		return false;

	*out_center = center;
	*out_center_freq = center_freq;
	return true;
}


/*
 * regdom_apply_channel - Apply a chosen channel to the iface configuration.
 *
 * Updates freq, channel, secondary_channel, oper_chwidth, seg0/seg1,
 * and current_hw_info.
 *
 * Return: 0 on success, -1 if set_current_hw_info fails (try next channel).
 */
static int regdom_apply_channel(struct hostapd_iface *iface,
				const struct hostapd_channel_data *chan,
				enum oper_chan_width try_width,
				int secondary, int center,
				struct hostapd_freq_params *freq_params,
				u16 pp)
{
	struct hostapd_hw_modes *mode = iface->current_mode;
	int ret;

	if (!mode)
		return -1;

	if (hostapd_set_current_hw_info(iface, chan->freq)) {
		wpa_printf(MSG_DEBUG,
			   "REGDOM: set_current_hw_info failed for freq %d, trying next",
			   iface->freq);
		return -1;
	}

	ret = hostapd_set_freq_params(freq_params,
				      iface->conf->hw_mode,
				      chan->freq, chan->chan,
				      iface->conf->enable_edmg,
				      iface->conf->edmg_channel,
				      iface->conf->ieee80211n,
				      iface->conf->ieee80211ac,
				      iface->conf->ieee80211ax,
				      iface->conf->ieee80211be,
				      iface->conf->ieee80211bn,
				      secondary,
				      try_width,
				      center,
				      0, /* Does not support 80P80 */
				      iface->conf->vht_capab,
				      mode ? &mode->he_capab[IEEE80211_MODE_AP] : NULL,
				      mode ? &mode->eht_capab[IEEE80211_MODE_AP] : NULL,
				      mode ? &mode->uhr_capab[IEEE80211_MODE_AP] : NULL,
				      pp,
				      iface->conf->he_6ghz_reg_pwr_type, /* BPM would override later */
				      0, 0,
				      iface->conf->bandwidth_device,
				      iface->conf->center_freq_device);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "REGDOM: Failed to build freq parameters");
		return -1;
	}

	wpa_printf(MSG_INFO,
		   "REGDOM: %s selected channel %d (%d MHz) seg0=%d width=%d pp 0x%x after country change",
		   iface->conf->bss[0]->iface,
		   chan->chan, chan->freq, center,
		   hostapd_get_width_from_oper_chwidth_extn(try_width, secondary),
		   pp);

	return 0;
}


/*
 * Scan the channels in one hardware mode and apply the first valid candidate.
 *
 * Return: true if a channel was successfully applied; false otherwise.
 */
static bool
regdom_select_channel_from_mode(struct hostapd_iface *iface, struct hostapd_hw_modes *mode,
				enum oper_chan_width try_width, int secondary,
				struct hostapd_freq_params *freq_params)
{
	static const enum bw_type bw320[] = { ACS_BW320_1, ACS_BW320_2 };
	int num_centers = (mode->is_6ghz &&
			   try_width == CONF_OPER_CHWIDTH_320MHZ) ?
		ARRAY_SIZE(bw320) : 1;
	int j, k;

	for (j = 0; j < mode->num_channels; j++) {
		struct hostapd_channel_data *chan = &mode->channels[j];
		int center = 0, center_freq;
		int sec = secondary;
		u16 pp = 0;

		if (!chan_in_current_hw_info(iface->current_hw_info, chan))
			continue;

		if (!regdom_chan_allowed_bw(iface, chan, try_width, &sec))
			continue;

		if (try_width == CONF_OPER_CHWIDTH_320MHZ && is_5ghz_freq(chan->freq))
			pp = RIGHT80_240MHZ_PUNC;

		/* Only 6 GHz has two overlapping 320 MHz layouts. */
		for (k = 0; k < num_centers; k++) {
			if (!regdom_chan_seg0(chan, try_width, sec, bw320[k],
					      &center, &center_freq))
				continue;

			/* Validate the same segment that will be applied. */
			if (!regdom_is_chan_bw_valid(iface, chan->freq,
						     center_freq,
						     try_width, sec, pp))
				continue;

			if (regdom_apply_channel(iface, chan, try_width, sec,
						 center, freq_params, pp) == 0)
				return true;
		}
	}

	return false;
}


/*
 * Scan matching hardware modes for a channel supporting the requested width.
 *
 * Return: true if a channel was successfully applied; false otherwise.
 */
static bool
regdom_select_channel_for_width(struct hostapd_iface *iface, enum oper_chan_width try_width,
				int secondary, struct hostapd_freq_params *freq_params)
{
	int i;

	for (i = 0; i < iface->num_hw_features; i++) {
		struct hostapd_hw_modes *mode = &iface->hw_features[i];

		if (mode->mode != iface->conf->hw_mode)
			continue;

		if (mode->is_6ghz != is_6ghz_freq(iface->freq))
			continue;

		if (regdom_select_channel_from_mode(iface, mode, try_width,
						    secondary, freq_params))
			return true;
	}

	return false;
}


/*
 * hostapd_country_change_select_channel - Select a valid channel after a
 * country change.
 *
 * 1. If all sub-channels of the current channel are still valid → keep it.
 * 2. Otherwise waterfall: 320 → 160 → 80 → 40 → 20 MHz, each width scanning
 *    all hw_mode channels.  For each candidate: regulatory support, 6 GHz
 *    power-mode, driver allowed_bw, bonded-group existence, seg0 computation.
 *
 * Return: 0 if current channel is valid channel.
 *         1 if a valid channel is computed.
 *        -1 if no valid channel exists (caller should park iface to NO_IR).
 */
static int hostapd_country_change_select_channel(struct hostapd_iface *iface,
						 struct hostapd_freq_params *freq_params)
{
	static const enum oper_chan_width width_waterfall[] = {
		CONF_OPER_CHWIDTH_320MHZ,
		CONF_OPER_CHWIDTH_160MHZ,
		CONF_OPER_CHWIDTH_80MHZ,
		CONF_OPER_CHWIDTH_USE_HT, /* 40 MHz (secondary = +/-1) */
		CONF_OPER_CHWIDTH_USE_HT, /* 20 MHz (secondary = 0) */
	};
	enum oper_chan_width cur_width = hostapd_get_oper_chwidth(iface->conf);
	int w;
	u16 cur_mhz;

	if (!iface->hw_features || !iface->num_hw_features)
		return -1;

	if (regdom_subchans_valid(iface))
		return 0;

	cur_mhz = hostapd_get_width_from_oper_chwidth_extn(
			cur_width, iface->conf->secondary_channel);

	wpa_printf(MSG_DEBUG,
		   "REGDOM: Trying to find a channel for cur width %d", cur_mhz);
	for (w = 0; w < (int)ARRAY_SIZE(width_waterfall); w++) {
		enum oper_chan_width try_width = width_waterfall[w];
		u16 try_mhz;
		int secondary = (w == 3) ? 1 : 0; /* index 3 = HT40 seed */

		try_mhz = hostapd_get_width_from_oper_chwidth_extn(try_width,
								   secondary);
		if (try_mhz > cur_mhz)
			continue; /* skip widths wider than current */

		if (regdom_select_channel_for_width(iface, try_width, secondary,
						    freq_params))
			return 1;
	}

	wpa_printf(MSG_WARNING,
		   "REGDOM: no valid channel found for %s after country change",
		   iface->conf->bss[0]->iface);
	return -1;
}


/*
 * hostapd_country_change_enable_dep_repeater - Handle dependent-repeater
 * bring-up after a country change.
 *
 * Leave the AP in DISABLED state. The ucode event handler
 * (uc_hostapd_iface_start in ucode.c) fires when the BH STA reconnects. It
 * sets sta_wpa_state="COMPLETED" and calls hostapd_enable_iface(), which
 * brings the AP up after the BH STA association is complete. This is the
 * standard dependent-repeater bring-up path; no additional state tracking is
 * required.
 */
static int hostapd_country_change_enable_dep_repeater(
	struct hostapd_iface *iface)
{
	wpa_printf(MSG_INFO,
		   "REGDOM: dep repeater %s left DISABLED; AP comes up when BH STA reconnects via ucode",
		   iface->conf->bss[0]->iface);
	return 0;
}


/*
 * hostapd_ind_rptr_country_change_ap_enable - eloop timeout handler for
 * independent-repeater country-change AP enable.
 *
 * Fired 500 ms after hostapd_country_change_handle() set
 * country_change_ap_enable_pending.  By this point the BH STA (wpa_supplicant)
 * will have processed the deauthentication and the radio is free.
 *
 * Flag cleanup on ALL paths:
 *   a. Normal path (this handler) — cleared here before enable.
 *   b. Cancelled via eloop_cancel_timeout — flag left set; caller must clear.
 *   c. Interface disabled externally while pending — hostapd_disable_iface
 *      does not clear the flag, so we check iface->state at entry and skip
 *      enable if the iface was already brought down by another path.
 */
static
void hostapd_ind_rptr_country_change_ap_enable(void *eloop_ctx,
					       void *timeout_ctx)
{
	struct hostapd_iface *iface = eloop_ctx;
	int ret;

	if (!iface)
		return;

	if (iface->state != HAPD_IFACE_DISABLED &&
	    iface->state != HAPD_IFACE_NO_IR) {
		/* Already recovered elsewhere. */
		wpa_printf(MSG_INFO,
			   "REGDOM: ind-RE %s already recovered (state=%d forced=%d), skipping deferred enable",
			   iface->conf->bss[0]->iface,
			   iface->state,
			   iface->iface_extn.is_regdom_forced_down);
		return;
	}

	wpa_printf(MSG_INFO,
		   "REGDOM: ind-RE %s deferred enable firing (BH STA should be disconnected)",
		   iface->conf->bss[0]->iface);

	ret = hostapd_enable_iface(iface);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "REGDOM: ind-RE %s enable failed in deferred enable (%d)",
			   iface->conf->bss[0]->iface, ret);
}


/*
 * hostapd_country_change_enable_ind_repeater - Handle independent-repeater
 * bring-up after a country change.
 *
 * The AP must not come up until the BH STA has fully disconnected. Since both
 * use the same radio, the STA radio needs to re-tune before the AP restarts on
 * the new channel. Defer AP enablement for 500 ms so the disconnect sequence
 * triggered by wpas_country_change_check_current_bss() or
 * hostapd_disconnect_backhaul_sta() can complete. The timeout handler clears
 * country_change_ap_enable_pending on all paths.
 */
static int hostapd_country_change_enable_ind_repeater(struct hostapd_iface *iface)
{
	eloop_cancel_timeout(hostapd_ind_rptr_country_change_ap_enable,
			     iface, NULL);
	eloop_register_timeout(0, 500000,
			       hostapd_ind_rptr_country_change_ap_enable,
			       iface, NULL);
	wpa_printf(MSG_INFO,
		   "REGDOM: ind-RE %s AP enable deferred 500 ms (waiting for BH STA disconnect)",
		   iface->conf->bss[0]->iface);
	return 0;
}


/*
 * hostapd_country_change_enable_ap - Re-enable a plain AP immediately after a
 * country change.
 */
static int hostapd_country_change_enable_ap(struct hostapd_iface *iface)
{
	int ret;

	wpa_printf(MSG_INFO,
		   "REGDOM: re-enabling %s after country change",
		   iface->conf->bss[0]->iface);
	ret = hostapd_enable_iface(iface);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "REGDOM: failed to re-enable %s (%d)",
			   iface->conf->bss[0]->iface, ret);

	return ret;
}


/*
 * hostapd_country_change_do_enable - Dispatch interface enablement after a
 * country change.
 *
 * Called after the interface has already been disabled.
 */
static int hostapd_country_change_do_enable(struct hostapd_iface *iface)
{
	if (!iface->conf->conf_extn.repeater)
		return hostapd_country_change_enable_ap(iface);

	if (iface->conf->conf_extn.ind_rptr)
		return hostapd_country_change_enable_ind_repeater(iface);

	return hostapd_country_change_enable_dep_repeater(iface);
}


static void hostapd_regdom_force_disable_iface(struct hostapd_iface *iface,
					       const char *reason)
{
	if (iface->state != HAPD_IFACE_ENABLED &&
	    iface->state != HAPD_IFACE_DFS) {
		wpa_printf(MSG_DEBUG, "Skip NO IR handling for iface state %d",
			   iface->state);
		return;
	}

	iface->iface_extn.is_regdom_forced_down = true;
	wpa_printf(MSG_INFO,
		   "REGDOM: parking %s to NO_IR (%s)",
		   iface->conf->bss[0]->iface, reason);
	hostapd_set_no_ir_state(iface);
}


/*
 * hostapd_country_change_post_cac_abort_extn - Continue the country change
 * flow after receiving DFS_CAC_ABORTED for a CAC that was aborted explicitly
 * by hostapd_country_change_handle() via hostapd_drv_abort_cac().
 *
 * At this point the CAC is stopped but the driver is still alive (interface
 * not yet disabled).  Perform the full disable + enable sequence here so
 * the new session starts cleanly.
 */
void hostapd_country_change_post_cac_abort_extn(struct hostapd_iface *iface)
{
	int ret;
	struct hostapd_freq_params freq_params;
	bool apply_new_config = false;

	if (!iface->iface_extn.country_change_post_cac_abort)
		return;

	iface->iface_extn.country_change_post_cac_abort = false;
	wpa_printf(MSG_DEBUG,
		   "REGDOM: CAC aborted for country change on %s, proceeding with disable+enable",
		   iface->conf->bss[0]->iface);

	ret = hostapd_country_change_select_channel(iface, &freq_params);
	if (ret < 0) {
		/* No valid channel -- park to NO_IR for future recovery. */
		hostapd_regdom_force_disable_iface(iface,
						   "no valid channel in new country");
		return;
	}

	if (ret == 1)
		apply_new_config = true;

	ret = hostapd_disable_iface(iface);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "REGDOM: failed to disable %s in post-CAC-abort handler (%d)",
			   iface->conf->bss[0]->iface, ret);
		return;
	}

	if (apply_new_config) {
		wpa_printf(MSG_INFO,
			   "REGDOM: Applying new channel before enable");
		ret = hostapd_change_config_freq(iface->bss[0], iface->conf,
						 &freq_params, NULL);
		if (ret) {
			wpa_printf(MSG_ERROR, "REGDOM: failed to apply new config");
			return;
		}
	}

	hostapd_country_change_do_enable(iface);
}


/*
 * hostapd_country_change_handle - Handle a runtime country change for an
 * already-enabled interface.
 *
 * Called only when the new country differs from the current one.
 *
 * Sequence for all cases:
 *   1. Abort in-progress CAC.
 *   2. Disable AP (sends deauth to clients). Regulatory obligation: cannot
 *      keep beaconing on a channel that may be illegal in the new country.
 *   3. Clear is_afc_repeater_power_sync_pending: the pending AFC sync targeted
 *      the old country/channel and is now stale.
 *   4. Select a valid channel for the new country.
 *
 *   5a. Non-repeater or independent repeater (ind_rptr=true):
 *       Call hostapd_enable_iface() immediately. The AP comes up on the
 *       selected channel. For ind-repeater, the BH STA reconnects autonomously.
 *
 *   5b. Dependent repeater (ind_rptr=false):
 *       Leave the AP iface in DISABLED state. Do NOT call hostapd_enable_iface().
 *       When the BH STA reconnects, the ucode event handler (uc_hostapd_iface_start
 *       in ucode.c) fires. It sets sta_wpa_state="COMPLETED" and calls
 *       hostapd_enable_iface(), which then runs hostapd_start_bss().
 *       This is already the existing dep-repeater bring-up path — no new flag needed.
 *
 * Return: 0 on success, negative on error.
 */
static int hostapd_country_change_handle(struct hostapd_iface *iface)
{
	bool cac_in_progress = iface->cac_started || iface->bootup_cac_in_progress;
	int ret;
	struct hostapd_freq_params freq_params;
	bool apply_new_config = false;

	iface->is_afc_repeater_power_sync_pending = false;

	if (cac_in_progress) {
		iface->iface_extn.country_change_post_cac_abort = true;
		wpa_printf(MSG_DEBUG,
			   "REGDOM: %s aborting CAC for country change (defer disable+enable)",
			   iface->conf->bss[0]->iface);
		ret = hostapd_drv_abort_cac(iface->bss[0]);
		if (ret) {
			wpa_printf(MSG_WARNING,
				   "REGDOM: CAC abort failed for %s (%d), falling back to direct disable",
				   iface->conf->bss[0]->iface, ret);
			iface->iface_extn.country_change_post_cac_abort = false;
		} else {
			return 0;
		}
	}

	ret = hostapd_country_change_select_channel(iface, &freq_params);
	if (ret < 0) {
		/* No valid channel -- park to NO_IR for future recovery. */
		hostapd_regdom_force_disable_iface(iface,
						   "no valid channel in new country");
		return 0;
	}

	if (ret == 1)
		apply_new_config = true;

	wpa_printf(MSG_INFO,
		   "REGDOM: disabling %s for country change", iface->conf->bss[0]->iface);
	ret = hostapd_disable_iface(iface);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "REGDOM: failed to disable %s for country change (%d)",
			   iface->conf->bss[0]->iface, ret);
		return ret;
	}

	if (apply_new_config) {
		wpa_printf(MSG_INFO,
			   "REGDOM: Applying new channel before enable");
		ret = hostapd_change_config_freq(iface->bss[0], iface->conf,
						 &freq_params, NULL);
		if (ret) {
			wpa_printf(MSG_ERROR, "REGDOM: failed to apply new config");
			return ret;
		}
	}

	return hostapd_country_change_do_enable(iface);
}

static int hostapd_regdom_restore_iface(struct hostapd_iface *iface)
{
	int ret;
	struct hostapd_freq_params freq_params;

	if (!iface->iface_extn.is_regdom_forced_down)
		return 0;

	if (iface->state != HAPD_IFACE_NO_IR) {
		/* Already recovered elsewhere. */
		iface->iface_extn.is_regdom_forced_down = false;
		return 0;
	}

	ret = hostapd_country_change_select_channel(iface, &freq_params);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG,
			   "REGDOM: still no valid channel for %s, staying in NO_IR",
			   iface->conf->bss[0]->iface);
		return 0;
	}

	if (ret == 1) {
		wpa_printf(MSG_INFO,
			   "REGDOM: Applying new channel before enable");
		ret = hostapd_change_config_freq(iface->bss[0], iface->conf,
						 &freq_params, NULL);
		if (ret) {
			wpa_printf(MSG_ERROR, "REGDOM: failed to apply new config");
			return ret;
		}
	}

	iface->iface_extn.is_regdom_forced_down = false;

	ret = hostapd_country_change_do_enable(iface);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "REGDOM: failed to re-enable %s after restore (%d)",
			   iface->conf->bss[0]->iface, ret);

	return ret;
}


int hostapd_handle_regchannel_update_extn(struct hostapd_iface *iface, bool afc_regd)
{
	char prev_country[3];
	bool regdom_reenable = iface->iface_extn.is_regdom_forced_down;
	int i, ret;

	if (!iface->freq) {
		ret = configured_fixed_chan_to_freq_helper(iface);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "REGDOM: configured channel invalid for %s (%d)",
				   iface->conf->bss[0]->iface, ret);
			hostapd_regdom_force_disable_iface(iface,
							   "configured channel invalid");
			return ret;
		}
	}

	os_memcpy(prev_country, iface->conf->country, sizeof(prev_country));
	for (i = 0; i < iface->num_bss; i++) {
		if (!iface->bss[i])
			continue;
		hostapd_sync_country_from_driver(iface->bss[i]);
		break;
	}

	if (os_memcmp(prev_country, iface->conf->country, 2) || !afc_regd) {
		wpa_printf(MSG_INFO,
			   "REGDOM: country changed %c%c -> %c%c on %s",
			   prev_country[0], prev_country[1],
			   iface->conf->country[0], iface->conf->country[1],
			   iface->conf->bss[0]->iface);
		ret = hostapd_country_change_handle(iface);
		return ret ? ret : 1;
	}

	if (regdom_reenable) {
		wpa_printf(MSG_DEBUG,
			   "REGDOM: restoring forced-down iface %s",
			   iface->conf->bss[0]->iface);
		ret = hostapd_regdom_restore_iface(iface);
		return ret ? ret : 1;
	}

	return 0;
}

bool hostapd_regdom_bringup_failed_extn(struct hostapd_iface *iface)
{
	if (!iface->hw_features || !iface->num_hw_features)
		return false;

	wpa_printf(MSG_INFO,
		   "REGDOM: bring-up failed for %s, parking to NO_IR for recovery on next country change",
		   iface->conf->bss[0]->iface);
	iface->iface_extn.is_regdom_forced_down = true;
	hostapd_set_no_ir_state(iface);
	return true;
}

void hostapd_set_afc_regd_wait(struct hostapd_iface *orig_iface)
{
	int i;
	const char *phy_name = hostapd_drv_get_radio_name(orig_iface->bss[0]);

	if (!phy_name)
		return;

	for (i = 0; i < orig_iface->interfaces->count; i++) {
		struct hostapd_iface *iface = orig_iface->interfaces->iface[i];
		const char *name;

		if (!iface || !iface->num_bss || !iface->bss[0])
			continue;
		name = hostapd_drv_get_radio_name(iface->bss[0]);
		if (!name || os_strcmp(name, phy_name) != 0)
			continue;

		iface->iface_extn.is_waiting_for_afc_regd = true;
	}
}
#else
bool regdom_subchans_valid(struct hostapd_iface *iface)
{
	return false;
}

int hostapd_handle_regchannel_update_extn(struct hostapd_iface *iface,
					bool afc_regd)
{
	return 0;
}

bool hostapd_regdom_bringup_failed_extn(struct hostapd_iface *iface)
{
	return false;
}

void hostapd_country_change_post_cac_abort_extn(struct hostapd_iface *iface)
{
}
#endif

void hostapd_override_ht_capabilities_extn(const struct hostapd_data *hapd,
					   struct ieee80211_ht_capabilities *cap)
{
	if (!hapd || !hapd->conf || !cap)
		return;

	if (hapd->conf->bss_extn.ht40_intol.is_overridden) {
		if (hapd->conf->bss_extn.ht40_intol.value)
			cap->ht_capabilities_info |= HT_CAP_INFO_40MHZ_INTOLERANT;
		else
			cap->ht_capabilities_info &= ~HT_CAP_INFO_40MHZ_INTOLERANT;
	}
}
