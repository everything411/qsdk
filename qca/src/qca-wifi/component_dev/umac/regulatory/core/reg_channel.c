/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: reg_channel.c
 * This file defines the API to access/update/modify regulatory current channel
 * list by WIN host umac components.
 */

#include <wlan_cmn.h>
#include <reg_services_public_struct.h>
#include <reg_build_chan_list.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <reg_priv_objs.h>
#include <reg_services_common.h>
#include "reg_channel.h"
#include <wlan_reg_channel_api.h>
#include <wlan_reg_services_api.h>

#ifdef CONFIG_HOST_FIND_CHAN

#ifdef WLAN_FEATURE_11BE
static inline int is_11be_supported(uint64_t wireless_modes, uint32_t phybitmap)
{
	return (WIRELESS_11BE_MODES & wireless_modes) &&
		!(phybitmap & REGULATORY_PHYMODE_NO11BE);
}
#else
static inline int is_11be_supported(uint64_t wireless_modes, uint32_t phybitmap)
{
	return false;
}
#endif

static inline int is_11ax_supported(uint64_t wireless_modes, uint32_t phybitmap)
{
	return (WIRELESS_11AX_MODES & wireless_modes) &&
		!(phybitmap & REGULATORY_PHYMODE_NO11AX);
}

static inline int is_11ac_supported(uint64_t wireless_modes, uint32_t phybitmap)
{
	return (WIRELESS_11AC_MODES & wireless_modes) &&
		!(phybitmap & REGULATORY_PHYMODE_NO11AC);
}

static inline int is_11n_supported(uint64_t wireless_modes, uint32_t phybitmap)
{
	return (WIRELESS_11N_MODES & wireless_modes) &&
		!(phybitmap & REGULATORY_CHAN_NO11N);
}

static inline int is_11g_supported(uint64_t wireless_modes, uint32_t phybitmap)
{
	return (WIRELESS_11G_MODES & wireless_modes) &&
		!(phybitmap & REGULATORY_PHYMODE_NO11G);
}

static inline int is_11b_supported(uint64_t wireless_modes, uint32_t phybitmap)
{
	return (WIRELESS_11B_MODES & wireless_modes) &&
		!(phybitmap & REGULATORY_PHYMODE_NO11B);
}

static inline int is_11a_supported(uint64_t wireless_modes, uint32_t phybitmap)
{
	return (WIRELESS_11A_MODES & wireless_modes) &&
		!(phybitmap & REGULATORY_PHYMODE_NO11A);
}

#ifdef WLAN_FEATURE_11BE
static void fill_11be_max_phymode_chwidth(uint64_t wireless_modes,
					  uint32_t phybitmap,
					  enum phy_ch_width *max_chwidth,
					  enum reg_phymode *max_phymode)
{
	*max_phymode = REG_PHYMODE_11BE;
	if (wireless_modes & WIRELESS_320_MODES)
		*max_chwidth = CH_WIDTH_320MHZ;
	else if (wireless_modes & WIRELESS_160_MODES)
		*max_chwidth = CH_WIDTH_160MHZ;
	else if (wireless_modes & WIRELESS_80_MODES)
		*max_chwidth = CH_WIDTH_80MHZ;
	else if (wireless_modes & WIRELESS_40_MODES)
		*max_chwidth = CH_WIDTH_40MHZ;
}
#else
static inline void
fill_11be_max_phymode_chwidth(uint64_t wireless_modes,
			      uint32_t phybitmap,
			      enum phy_ch_width *max_chwidth,
			      enum reg_phymode *max_phymode)
{
}
#endif

void reg_update_max_phymode_chwidth_for_pdev(struct wlan_objmgr_pdev *pdev)
{
	uint64_t wireless_modes;
	uint32_t phybitmap;
	enum phy_ch_width max_chwidth = CH_WIDTH_20MHZ;
	enum reg_phymode max_phymode = REG_PHYMODE_MAX;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	wireless_modes = pdev_priv_obj->wireless_modes;
	phybitmap = pdev_priv_obj->phybitmap;

	if (is_11be_supported(wireless_modes, phybitmap)) {
		fill_11be_max_phymode_chwidth(wireless_modes, phybitmap,
					      &max_chwidth, &max_phymode);
	} else if (is_11ax_supported(wireless_modes, phybitmap)) {
		max_phymode = REG_PHYMODE_11AX;
		if (wireless_modes & WIRELESS_160_MODES)
			max_chwidth = CH_WIDTH_160MHZ;
		else if (wireless_modes & WIRELESS_80_MODES)
			max_chwidth = CH_WIDTH_80MHZ;
		else if (wireless_modes & WIRELESS_40_MODES)
			max_chwidth = CH_WIDTH_40MHZ;
	} else if (is_11ac_supported(wireless_modes, phybitmap)) {
		max_phymode = REG_PHYMODE_11AC;
		if (wireless_modes & WIRELESS_160_MODES)
			max_chwidth = CH_WIDTH_160MHZ;
		else if (wireless_modes & WIRELESS_80_MODES)
			max_chwidth = CH_WIDTH_80MHZ;
		else if (wireless_modes & WIRELESS_40_MODES)
			max_chwidth = CH_WIDTH_40MHZ;
	} else if (is_11n_supported(wireless_modes, phybitmap)) {
		max_phymode = REG_PHYMODE_11N;
		if (wireless_modes & WIRELESS_40_MODES)
			max_chwidth = CH_WIDTH_40MHZ;
	} else if (is_11g_supported(wireless_modes, phybitmap)) {
		max_phymode = REG_PHYMODE_11G;
	} else if (is_11b_supported(wireless_modes, phybitmap)) {
		max_phymode = REG_PHYMODE_11B;
	} else if (is_11a_supported(wireless_modes, phybitmap)) {
		max_phymode = REG_PHYMODE_11A;
	} else {
		reg_err("Device does not support any wireless_mode! %0llx",
			wireless_modes);
	}

	pdev_priv_obj->max_phymode = max_phymode;
	pdev_priv_obj->max_chwidth = max_chwidth;
}

/**
 * chwd_2_contbw_lst - Conversion array from channel width enum to value.
 * Array index of type phy_ch_width, return of type uint16_t.
 */
static uint16_t chwd_2_contbw_lst[CH_WIDTH_MAX + 1] = {
	BW_20_MHZ,   /* CH_WIDTH_20MHZ */
	BW_40_MHZ,   /* CH_WIDTH_40MHZ */
	BW_80_MHZ,   /* CH_WIDTH_80MHZ */
	BW_160_MHZ,  /* CH_WIDTH_160MHZ */
	BW_80_MHZ,   /* CH_WIDTH_80P80MHZ */
	BW_5_MHZ,    /* CH_WIDTH_5MHZ */
	BW_10_MHZ,   /* CH_WIDTH_10MHZ */
#ifdef WLAN_FEATURE_11BE
	BW_320_MHZ,  /* CH_WIDTH_320MHZ */
#endif
	0,           /* CH_WIDTH_INVALID */
#ifdef WLAN_FEATURE_11BE
	BW_320_MHZ,  /* CH_WIDTH_MAX */
#else
	BW_160_MHZ,  /* CH_WIDTH_MAX */
#endif

};

#ifdef WLAN_FEATURE_11BE
enum phy_ch_width
reg_get_max_channel_width(struct wlan_objmgr_pdev *pdev,
			  qdf_freq_t freq,
			  enum phy_ch_width g_max_width,
			  enum supported_6g_pwr_types in_6g_pwr_mode,
			  uint16_t input_puncture_bitmap)
{
	struct reg_channel_list chan_list = {0};
	uint16_t i, max_bw = 0;
	enum phy_ch_width output_width = CH_WIDTH_INVALID;

	for (i = 0; i < MAX_NUM_CHAN_PARAM; i++) {
	    chan_list.chan_param[i].input_punc_bitmap = input_puncture_bitmap;
	}

	wlan_reg_fill_channel_list_for_pwrmode(pdev, freq, 0,
					       g_max_width, 0,
					       &chan_list,
					       in_6g_pwr_mode, false);

	for (i = 0; i < chan_list.num_ch_params; i++) {
		struct ch_params *ch_param = &chan_list.chan_param[i];
		uint16_t cont_bw = chwd_2_contbw_lst[ch_param->ch_width];

		if (max_bw < cont_bw) {
			output_width = ch_param->ch_width;
			max_bw = cont_bw;
		}
	}
	return output_width;
}
#else
enum phy_ch_width
reg_get_max_channel_width(struct wlan_objmgr_pdev *pdev,
			  qdf_freq_t freq,
			  enum phy_ch_width g_max_width,
			  enum supported_6g_pwr_types in_6g_pwr_mode,
			  uint16_t input_puncture_bitmap)
{
	struct ch_params chan_params = {0};

	chan_params.ch_width = g_max_width;
	reg_get_channel_params(pdev, freq, 0, &chan_params, in_6g_pwr_mode);
	return chan_params.ch_width;
}
#endif

/**
 * reg_is_per_channel_bw_update_needed() - Returns true if BW correction is
 * needed, false otherwise.
 * @pdev_priv_obj: Pointer to struct wlan_regulatory_pdev_priv_obj
 * @reg_chan: Pointer to struct regulatory_channel
 *
 * In case of 6 GHz, VLP and LPI power modes operate on the full bandwidth
 * advertised by regulatory. In case of standard power mode, the regulatory
 * provided bandwidth will be restricted by AFC and the max_bandwidth is
 * reduced. However, if puncture bitmap is applied on SP channels, max_bw
 * of a channel can vary.
 *
 * For example: channel 149 cannot operate on EHT320 in SP power mode.
 * However, if puncture pattern of 0xC000 is applied, max BW of 149 can be
 * 320 MHz. Hence do not restrict the max BW with SP power mode.
 */
static bool
reg_is_per_channel_bw_update_needed(struct wlan_regulatory_pdev_priv_obj
				    *pdev_priv_obj,
				    struct regulatory_channel *reg_chan)
{
	if (pdev_priv_obj->reg_cur_6g_ap_pwr_type == REG_STANDARD_POWER_AP &&
	    REG_IS_6GHZ_FREQ(reg_chan->center_freq))
		return false;

	return true;
}


void reg_modify_chan_list_for_max_chwidth_for_pwrmode(
		struct wlan_objmgr_pdev *pdev,
		struct regulatory_channel *cur_chan_list,
		enum supported_6g_pwr_types in_6g_pwr_mode)
{
	int i;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	for (i = 0; i < NUM_CHANNELS; i++) {
		enum phy_ch_width g_max_width = pdev_priv_obj->max_chwidth;
		enum phy_ch_width output_width = CH_WIDTH_INVALID;
		qdf_freq_t freq = cur_chan_list[i].center_freq;

		if (cur_chan_list[i].chan_flags & REGULATORY_CHAN_DISABLED)
			continue;

		/*
		 * For 5 GHz band,  bandwidth correction is needed.
		 * Eg: For freq range: 5490- 5730,  max_bw 160 is
		 * advertised by FW in reg rules. However, channels 132 - 144
		 * support only 80 MHz. Hence BW correction is needed.
		 * For 6Ghz SP channels, we are skipping the BW correction.
		 */
		if (!reg_is_per_channel_bw_update_needed(pdev_priv_obj,
							 &cur_chan_list[i]))
			continue;
		/*
		 * Correct the max bandwidths if they were not taken care of
		 * while parsing the reg rules.
		 */
		output_width = reg_get_max_channel_width(pdev, freq,
							 g_max_width,
							 in_6g_pwr_mode,
							 NO_SCHANS_PUNC);

		if (output_width != CH_WIDTH_INVALID)
			cur_chan_list[i].max_bw =
				qdf_min(cur_chan_list[i].max_bw,
					chwd_2_contbw_lst[output_width]);
	}
}

static uint64_t convregphymode2wirelessmodes[REG_PHYMODE_MAX] = {
	0xFFFFFFFF,                  /* REG_PHYMODE_INVALID */
	WIRELESS_11B_MODES,          /* REG_PHYMODE_11B     */
	WIRELESS_11G_MODES,          /* REG_PHYMODE_11G     */
	WIRELESS_11A_MODES,          /* REG_PHYMODE_11A     */
	WIRELESS_11N_MODES,          /* REG_PHYMODE_11N     */
	WIRELESS_11AC_MODES,         /* REG_PHYMODE_11AC    */
	WIRELESS_11AX_MODES,         /* REG_PHYMODE_11AX    */
#ifdef WLAN_FEATURE_11BE
	WIRELESS_11BE_MODES,         /* REG_PHYMODE_11BE    */
#endif
};

static uint64_t reg_is_phymode_in_wireless_modes(enum reg_phymode phy_in,
						 uint64_t wireless_modes)
{
	uint64_t sup_wireless_modes = convregphymode2wirelessmodes[phy_in];

	return sup_wireless_modes & wireless_modes;
}

bool reg_is_phymode_chwidth_allowed(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
		enum reg_phymode phy_in,
		enum phy_ch_width ch_width,
		qdf_freq_t freq,
		enum supported_6g_pwr_types in_6g_pwr_mode,
		uint16_t input_puncture_bitmap)
{
	uint32_t phymode_bitmap;
	uint64_t wireless_modes;
	enum phy_ch_width output_width = CH_WIDTH_INVALID;

	if (ch_width == CH_WIDTH_INVALID)
		return false;

	phymode_bitmap = pdev_priv_obj->phybitmap;
	wireless_modes = pdev_priv_obj->wireless_modes;

	if (reg_is_phymode_unallowed(phy_in, phymode_bitmap) ||
	    !reg_is_phymode_in_wireless_modes(phy_in, wireless_modes))
		return false;

	output_width = reg_get_max_channel_width(pdev_priv_obj->pdev_ptr,
						 freq,
						 ch_width,
						 in_6g_pwr_mode,
						 input_puncture_bitmap);

	if (output_width != ch_width)
		return false;

	return true;
}

void reg_set_chan_blocked(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *cur_chan_list;
	int i;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	for (i = 0; i < NUM_CHANNELS; i++) {
		if (cur_chan_list[i].center_freq == freq) {
			cur_chan_list[i].is_chan_hop_blocked = true;
			break;
		}
	}
}

bool reg_is_chan_blocked(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *cur_chan_list;
	int i;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return false;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	for (i = 0; i < NUM_CHANNELS; i++)
		if (cur_chan_list[i].center_freq == freq)
			return cur_chan_list[i].is_chan_hop_blocked;

	return false;
}

void reg_clear_allchan_blocked(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *cur_chan_list;
	int i;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	for (i = 0; i < NUM_CHANNELS; i++)
		cur_chan_list[i].is_chan_hop_blocked = false;
}

void reg_set_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			    enum ht40_intol ht40intol_flags)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *cur_chan_list;
	int i;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	for (i = 0; i < NUM_CHANNELS; i++) {
		if (cur_chan_list[i].center_freq == freq)
			cur_chan_list[i].ht40intol_flags |=
					BIT(ht40intol_flags);
	}
}

void reg_clear_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			      enum ht40_intol ht40intol_flags)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *cur_chan_list;
	int i;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	for (i = 0; i < NUM_CHANNELS; i++) {
		if (cur_chan_list[i].center_freq == freq)
			cur_chan_list[i].ht40intol_flags &=
				~(BIT(ht40intol_flags));
	}
}

bool reg_is_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			   enum ht40_intol ht40intol_flags)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *cur_chan_list;
	int i;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return false;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	for (i = 0; i < NUM_CHANNELS; i++)
		if (cur_chan_list[i].center_freq == freq)
			return (cur_chan_list[i].ht40intol_flags &
				BIT(ht40intol_flags));

	return false;
}

void reg_clear_allchan_ht40intol(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *cur_chan_list;
	int i;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	for (i = 0; i < NUM_CHANNELS; i++)
		cur_chan_list[i].ht40intol_flags = 0;
}

/*
 * reg_is_band_found_internal - Check if a band channel is found in the
 * current channel list.
 *
 * @start_idx - Start index.
 * @end_idx - End index.
 * @cur_chan_list - Pointer to cur_chan_list.
 */
static bool reg_is_band_found_internal(enum channel_enum start_idx,
				       enum channel_enum end_idx,
				       struct regulatory_channel *cur_chan_list)
{
	uint8_t i;

	for (i = start_idx; i <= end_idx; i++)
		if (!(reg_is_chan_disabled_and_not_nol(&cur_chan_list[i])))
			return true;

	return false;
}

bool reg_is_band_present(struct wlan_objmgr_pdev *pdev,
			 enum reg_wifi_band reg_band)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *cur_chan_list;
	enum channel_enum min_chan_idx, max_chan_idx;

	switch (reg_band) {
	case REG_BAND_2G:
		min_chan_idx = MIN_24GHZ_CHANNEL;
		max_chan_idx = MAX_24GHZ_CHANNEL;
		break;
	case REG_BAND_5G:
		min_chan_idx = MIN_49GHZ_CHANNEL;
		max_chan_idx = MAX_5GHZ_CHANNEL;
		break;
	case REG_BAND_6G:
		min_chan_idx = MIN_6GHZ_CHANNEL;
		max_chan_idx = MAX_6GHZ_CHANNEL;
		break;
	default:
		return false;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return false;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	return reg_is_band_found_internal(min_chan_idx, max_chan_idx,
					  cur_chan_list);
}

#endif /* CONFIG_HOST_FIND_CHAN */

bool reg_is_nol_for_freq(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	enum channel_enum chan_enum;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	chan_enum = reg_get_chan_enum_for_freq(freq);
	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err("chan freq is not valid");
		return false;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg obj is NULL");
		return false;
	}

	return pdev_priv_obj->cur_chan_list[chan_enum].nol_chan;
}

bool reg_is_nol_hist_for_freq(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	enum channel_enum chan_enum;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	chan_enum = reg_get_chan_enum_for_freq(freq);
	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err("chan freq is not valid");
		return false;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg obj is NULL");
		return false;
	}

	return pdev_priv_obj->cur_chan_list[chan_enum].nol_history;
}

/**
 * reg_is_freq_band_dfs() - Find the bonded pair for the given frequency
 * and check if any of the sub frequencies in the bonded pair is DFS.
 * @pdev: Pointer to the pdev object.
 * @freq: Input frequency.
 * @bonded_chan_ptr: Frequency range of the given channel and width.
 *
 * Return: True if any of the channels in the bonded_chan_ar that contains
 * the input frequency is dfs, else false.
 */
static bool
reg_is_freq_band_dfs(struct wlan_objmgr_pdev *pdev,
		     qdf_freq_t freq,
		     const struct bonded_channel_freq *bonded_chan_ptr)
{
	qdf_freq_t chan_cfreq;
	bool is_dfs = false;

	chan_cfreq =  bonded_chan_ptr->start_freq;
	while (chan_cfreq <= bonded_chan_ptr->end_freq) {
		/* If any of the channel is disabled by regulatory, return. */
		if (reg_is_disable_for_pwrmode(pdev, chan_cfreq,
					       REG_CURRENT_PWR_MODE) &&
		    !reg_is_nol_for_freq(pdev, chan_cfreq))
			return false;
		if (reg_is_dfs_for_freq(pdev, chan_cfreq))
			is_dfs = true;
		chan_cfreq = chan_cfreq + NEXT_20_CH_OFFSET;
	}

	return is_dfs;
}

static
void reg_intersect_chan_list_power(struct wlan_objmgr_pdev *pdev,
				   struct regulatory_channel *pri_chan_list,
				   struct regulatory_channel *sec_chan_list,
				   uint32_t chan_list_size)
{
	bool chan_found_in_sec_list;
	uint32_t i, j;

	if (!pdev) {
		reg_err_rl("invalid pdev");
		return;
	}

	if (!pri_chan_list) {
		reg_err_rl("invalid pri_chan_list");
		return;
	}

	if (!sec_chan_list) {
		reg_err_rl("invalid sec_chan_list");
		return;
	}

	for (i = 0; i < chan_list_size; i++) {
		if ((pri_chan_list[i].state == CHANNEL_STATE_DISABLE) ||
		    (pri_chan_list[i].chan_flags & REGULATORY_CHAN_DISABLED)) {
			continue;
		}

		chan_found_in_sec_list = false;
		for (j = 0; j < chan_list_size; j++) {
			if ((sec_chan_list[j].state ==
						CHANNEL_STATE_DISABLE) ||
			    (sec_chan_list[j].chan_flags &
						REGULATORY_CHAN_DISABLED)) {
				continue;
			}

			if (pri_chan_list[i].center_freq ==
				sec_chan_list[j].center_freq) {
				chan_found_in_sec_list = true;
				break;
			}
		}

		if (!chan_found_in_sec_list) {
			pri_chan_list[i].state = CHANNEL_STATE_DISABLE;
			continue;
		}

		pri_chan_list[i].psd_flag = pri_chan_list[i].psd_flag &
						sec_chan_list[j].psd_flag;
		pri_chan_list[i].tx_power = QDF_MIN(
						pri_chan_list[i].tx_power,
						sec_chan_list[j].tx_power);
		pri_chan_list[i].psd_eirp = QDF_MIN(
					(int16_t)pri_chan_list[i].psd_eirp,
					(int16_t)sec_chan_list[j].psd_eirp);
	}
}

QDF_STATUS reg_get_ap_chan_list(struct wlan_objmgr_pdev *pdev,
				struct regulatory_channel *chan_list,
				bool get_cur_chan_list,
				enum reg_6g_ap_type ap_pwr_type)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct wlan_objmgr_psoc *psoc;
	uint8_t i;
	const uint8_t reg_ap_pwr_type_2_supp_pwr_type[] = {
		[REG_INDOOR_AP] = REG_AP_LPI,
		[REG_STANDARD_POWER_AP] = REG_AP_SP,
		[REG_VERY_LOW_POWER_AP] = REG_AP_VLP,
	};


	if (!pdev) {
		reg_err_rl("invalid pdev");
		return QDF_STATUS_E_INVAL;
	}

	if (!chan_list) {
		reg_err_rl("invalid chanlist");
		return QDF_STATUS_E_INVAL;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	psoc = wlan_pdev_get_psoc(pdev);

	if (get_cur_chan_list) {
		qdf_mem_copy(chan_list, pdev_priv_obj->cur_chan_list,
			NUM_CHANNELS * sizeof(struct regulatory_channel));
	} else {
		/* Get the current channel list for 2.4GHz and 5GHz */
		qdf_mem_copy(chan_list, pdev_priv_obj->cur_chan_list,
			NUM_CHANNELS * sizeof(struct regulatory_channel));

		/*
		 * If 6GHz channel list is present, populate it with desired
		 * power type
		 */
		if (pdev_priv_obj->is_6g_channel_list_populated) {
			if (ap_pwr_type >= REG_CURRENT_MAX_AP_TYPE) {
				reg_debug("invalid 6G AP power type");
				return QDF_STATUS_E_INVAL;
			}

			qdf_mem_copy(&chan_list[MIN_6GHZ_CHANNEL],
				pdev_priv_obj->mas_chan_list_6g_ap[ap_pwr_type],
				NUM_6GHZ_CHANNELS *
					sizeof(struct regulatory_channel));

#ifdef CONFIG_AFC_SUPPORT
			if (ap_pwr_type == REG_STANDARD_POWER_AP) {
				/*
				 * If the AP type is standard power, intersect
				 * the SP channel list with the AFC master
				 * channel list
				 */
				reg_intersect_chan_list_power(
					pdev,
					&chan_list[MIN_6GHZ_CHANNEL],
					pdev_priv_obj->mas_chan_list_6g_afc,
					NUM_6GHZ_CHANNELS);
			}
#endif

			/*
			 * Intersect the hardware frequency range with the
			 * 6GHz channels.
			 */
			for (i = 0; i < NUM_6GHZ_CHANNELS; i++) {
				if ((chan_list[MIN_6GHZ_CHANNEL+i].center_freq <
					pdev_priv_obj->range_5g_low) ||
				    (chan_list[MIN_6GHZ_CHANNEL+i].center_freq >
					pdev_priv_obj->range_5g_high)) {
					chan_list[MIN_6GHZ_CHANNEL+i].chan_flags
						|= REGULATORY_CHAN_DISABLED;
					chan_list[MIN_6GHZ_CHANNEL+i].state =
						CHANNEL_STATE_DISABLE;
				}
			}

			/*
			 * Check for edge channels
			 */
			if (!reg_is_lower_6g_edge_ch_supp(psoc)) {
				chan_list[CHAN_ENUM_5935].state =
						CHANNEL_STATE_DISABLE;
				chan_list[CHAN_ENUM_5935].chan_flags |=
						REGULATORY_CHAN_DISABLED;
			}

			if (reg_is_upper_6g_edge_ch_disabled(psoc)) {
				chan_list[CHAN_ENUM_7115].state =
						CHANNEL_STATE_DISABLE;
				chan_list[CHAN_ENUM_7115].chan_flags |=
						REGULATORY_CHAN_DISABLED;
			}
			reg_modify_chan_list_for_max_chwidth_for_pwrmode(pdev,
					chan_list,
					reg_ap_pwr_type_2_supp_pwr_type
					[ap_pwr_type]);
		}
	}

	return QDF_STATUS_SUCCESS;
}

bool reg_is_freq_width_dfs(struct wlan_objmgr_pdev *pdev,
			   qdf_freq_t freq,
			   enum phy_ch_width ch_width)
{
	const struct bonded_channel_freq *bonded_chan_ptr;

	if (ch_width == CH_WIDTH_20MHZ)
		return reg_is_dfs_for_freq(pdev, freq);

	bonded_chan_ptr = reg_get_bonded_chan_entry(freq, ch_width, 0);

	if (!bonded_chan_ptr)
		return false;

	return reg_is_freq_band_dfs(pdev, freq, bonded_chan_ptr);
}

/**
 * reg_get_5g_channel_params ()- Set channel parameters like center
 * frequency for a bonded channel state. Also return the maximum bandwidth
 * supported by the channel.
 * @pdev: Pointer to pdev.
 * @freq: Channel center frequency.
 * @ch_params: Pointer to ch_params.
 * @in_6g_pwr_mode: Input power mode which decides the 6G channel list to be
 *
 * Return: void
 */
static void
reg_get_5g_channel_params(struct wlan_objmgr_pdev *pdev,
			  uint16_t freq,
			  struct ch_params *ch_params,
			  enum supported_6g_pwr_types in_6g_pwr_mode)
{
	/*
	 * Set channel parameters like center frequency for a bonded channel
	 * state. Also return the maximum bandwidth supported by the channel.
	*/

	enum channel_state chan_state = CHANNEL_STATE_ENABLE;
	enum channel_state chan_state2 = CHANNEL_STATE_ENABLE;
	const struct bonded_channel_freq *bonded_chan_ptr = NULL;
	const struct bonded_channel_freq *bonded_chan_ptr2 = NULL;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	enum channel_enum chan_enum, sec_5g_chan_enum;
	uint16_t bw_80, sec_5g_freq_max_bw = 0;
	uint16_t max_bw;
	uint16_t in_punc_bitmap = reg_fetch_punc_bitmap(ch_params);

	if (!ch_params) {
		reg_err("ch_params is NULL");
		return;
	}

	chan_enum = reg_get_chan_enum_for_freq(freq);
	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err("chan freq is not valid");
		return;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	if (ch_params->ch_width >= CH_WIDTH_MAX) {
		if (ch_params->mhz_freq_seg1 != 0)
			ch_params->ch_width = CH_WIDTH_80P80MHZ;
		else
			ch_params->ch_width = CH_WIDTH_160MHZ;
	}

	if (reg_get_min_max_bw_reg_chan_list(pdev, chan_enum, in_6g_pwr_mode,
					     NULL, &max_bw)) {
		ch_params->ch_width = CH_WIDTH_INVALID;
		return;
	}

	bw_80 = reg_get_bw_value(CH_WIDTH_80MHZ);

	if (ch_params->ch_width == CH_WIDTH_80P80MHZ) {
		sec_5g_chan_enum =
			reg_get_chan_enum_for_freq(
				ch_params->mhz_freq_seg1 -
				NEAREST_20MHZ_CHAN_FREQ_OFFSET);
		if (reg_is_chan_enum_invalid(sec_5g_chan_enum)) {
			reg_err("secondary channel freq is not valid");
			return;
		}

		if (reg_get_min_max_bw_reg_chan_list(pdev, chan_enum,
						     in_6g_pwr_mode, NULL,
						     &sec_5g_freq_max_bw)) {
			ch_params->ch_width = CH_WIDTH_INVALID;
			return;
		}
	}

	while (ch_params->ch_width != CH_WIDTH_INVALID) {
		if (ch_params->ch_width == CH_WIDTH_80P80MHZ) {
			if ((max_bw < bw_80) || (sec_5g_freq_max_bw < bw_80))
				goto update_bw;
		} else if (max_bw < reg_get_bw_value(ch_params->ch_width)) {
			goto update_bw;
		}

		bonded_chan_ptr = NULL;
		bonded_chan_ptr2 = NULL;
		bonded_chan_ptr =
		    reg_get_bonded_chan_entry(freq, ch_params->ch_width, 0);

		chan_state =
		    reg_get_5g_chan_state(pdev, freq, ch_params->ch_width,
					  in_6g_pwr_mode,
					  in_punc_bitmap);

		if (ch_params->ch_width == CH_WIDTH_80P80MHZ) {
			chan_state2 = reg_get_5g_chan_state(
					pdev, ch_params->mhz_freq_seg1 -
					NEAREST_20MHZ_CHAN_FREQ_OFFSET,
					CH_WIDTH_80MHZ,
					in_6g_pwr_mode,
					in_punc_bitmap);

			chan_state = reg_combine_channel_states(
					chan_state, chan_state2);
		}

		if ((chan_state != CHANNEL_STATE_ENABLE) &&
		(chan_state != CHANNEL_STATE_DFS))
			goto update_bw;
		if (ch_params->ch_width <= CH_WIDTH_20MHZ) {
			ch_params->sec_ch_offset = NO_SEC_CH;
			ch_params->mhz_freq_seg0 = freq;
			ch_params->center_freq_seg0 =
				reg_freq_to_chan(pdev,
						 ch_params->mhz_freq_seg0);
			break;
		} else if (ch_params->ch_width >= CH_WIDTH_40MHZ) {
			bonded_chan_ptr2 =
				reg_get_bonded_chan_entry(freq, CH_WIDTH_40MHZ, 0);

			if (!bonded_chan_ptr || !bonded_chan_ptr2)
				goto update_bw;
			if (freq == bonded_chan_ptr2->start_freq)
				ch_params->sec_ch_offset = LOW_PRIMARY_CH;
			else
				ch_params->sec_ch_offset = HIGH_PRIMARY_CH;

			ch_params->mhz_freq_seg0 =
				(bonded_chan_ptr->start_freq +
				 bonded_chan_ptr->end_freq) / 2;
			ch_params->center_freq_seg0 =
				reg_freq_to_chan(pdev,
						 ch_params->mhz_freq_seg0);
			break;
		}
update_bw:
		ch_params->ch_width =
			get_next_lower_bandwidth(ch_params->ch_width);
	}

	if (ch_params->ch_width == CH_WIDTH_160MHZ) {
		ch_params->mhz_freq_seg1 = ch_params->mhz_freq_seg0;
		ch_params->center_freq_seg1 =
			reg_freq_to_chan(pdev,
					   ch_params->mhz_freq_seg1);

		bonded_chan_ptr =
			reg_get_bonded_chan_entry(freq, CH_WIDTH_80MHZ, 0);
		if (bonded_chan_ptr) {
			ch_params->mhz_freq_seg0 =
				(bonded_chan_ptr->start_freq +
		 bonded_chan_ptr->end_freq) / 2;
			ch_params->center_freq_seg0 =
				reg_freq_to_chan(pdev,
						 ch_params->mhz_freq_seg0);
		}
	}

	/* Overwrite mhz_freq_seg1 to 0 for non 160 and 80+80 width */
	if (!(ch_params->ch_width == CH_WIDTH_160MHZ ||
		ch_params->ch_width == CH_WIDTH_80P80MHZ)) {
		ch_params->mhz_freq_seg1 = 0;
		ch_params->center_freq_seg1 = 0;
	}
}

void reg_get_channel_params(struct wlan_objmgr_pdev *pdev,
			    qdf_freq_t freq,
			    qdf_freq_t sec_ch_2g_freq,
			    struct ch_params *ch_params,
			    enum supported_6g_pwr_types in_6g_pwr_mode)
{
    if (reg_is_5ghz_ch_freq(freq) || reg_is_6ghz_chan_freq(freq))
	reg_get_5g_channel_params(pdev, freq, ch_params, in_6g_pwr_mode);
    else if  (reg_is_24ghz_ch_freq(freq))
	reg_set_2g_channel_params_for_freq(pdev, freq, ch_params,
					   sec_ch_2g_freq);
}

/**
 * reg_get_max_channel_width_without_radar() - Get the maximum channel width
 * supported given a frequency and a global maximum channel width.
 * The radar infected subchannel are not part of the max bandwidth.
 * @pdev: Pointer to PDEV object.
 * @freq: Input frequency.
 * @g_max_width: Global maximum channel width.
 *
 * Return: Maximum channel width of type phy_ch_width.
 */
#ifdef WLAN_FEATURE_11BE
static enum phy_ch_width
reg_get_max_channel_width_without_radar(struct wlan_objmgr_pdev *pdev,
					qdf_freq_t freq,
					enum phy_ch_width g_max_width)
{
	struct reg_channel_list chan_list = {0};
	uint16_t i, max_bw = 0;
	enum phy_ch_width output_width = CH_WIDTH_INVALID;

	wlan_reg_fill_channel_list_for_pwrmode(pdev, freq, 0,
					       g_max_width, 0,
					       &chan_list,
					       REG_CURRENT_PWR_MODE, true);

	for (i = 0; i < chan_list.num_ch_params; i++) {
		struct ch_params *ch_param = &chan_list.chan_param[i];
		uint16_t cont_bw = chwd_2_contbw_lst[ch_param->ch_width];

		if (max_bw < cont_bw) {
			output_width = ch_param->ch_width;
			max_bw = cont_bw;
		}
	}
	return output_width;
}
#else
static enum phy_ch_width
reg_get_max_channel_width_without_radar(struct wlan_objmgr_pdev *pdev,
					qdf_freq_t freq,
					enum phy_ch_width g_max_width)
{
	struct ch_params chan_params = {0};
	enum reg_6g_ap_type in_6g_pwr_mode;

	reg_get_cur_6g_ap_pwr_type(pdev, &in_6g_pwr_mode);
	chan_params.ch_width = g_max_width;
	reg_set_channel_params_for_pwrmode(pdev, freq, 0, &chan_params,
					   in_6g_pwr_mode, true);
	return chan_params.ch_width;
}
#endif

#ifdef WLAN_FEATURE_11BE
static void
reg_remove_320mhz_modes(int max_bw, uint64_t *wireless_modes)
{
	/**
	 * Check for max_bw greater than 160 to include both 240 and
	 * 320MHz support as 320 modes.
	 */
	if (max_bw <= BW_160_MHZ)
		*wireless_modes &= (~WIRELESS_320_MODES);
}
#else
static inline void
reg_remove_320mhz_modes(int max_bw, uint64_t *wireless_modes)
{
}
#endif

uint16_t reg_get_wmodes_and_max_chwidth(struct wlan_objmgr_pdev *pdev,
					uint64_t *mode_select,
					bool include_nol_chan)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint64_t in_wireless_modes = *mode_select;
	struct regulatory_channel *chan_list;
	enum supported_6g_pwr_types pwr_mode;
	int i, max_bw = BW_20_MHZ;
	uint64_t band_modes = 0;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg obj is NULL");
		return 0;
	}

	chan_list = qdf_mem_malloc(NUM_CHANNELS * sizeof(*chan_list));
	if (!chan_list)
		return 0;

	for (pwr_mode = REG_AP_LPI; pwr_mode <= REG_CLI_SUB_VLP; pwr_mode++) {

		qdf_mem_zero(chan_list, NUM_CHANNELS * sizeof(*chan_list));
		if (reg_get_pwrmode_chan_list(pdev, chan_list, pwr_mode)) {
			qdf_mem_free(chan_list);
			return 0;
		}

		for (i = 0; i < NUM_CHANNELS; i++) {
			qdf_freq_t freq = chan_list[i].center_freq;
			uint16_t cur_bw = chan_list[i].max_bw;

			if (reg_is_chan_disabled_and_not_nol(&chan_list[i]))
				continue;

			if (WLAN_REG_IS_24GHZ_CH_FREQ(freq))
				band_modes |= WIRELESS_2G_MODES;

			if (WLAN_REG_IS_49GHZ_FREQ(freq))
				band_modes |= WIRELESS_49G_MODES;

			if (WLAN_REG_IS_5GHZ_CH_FREQ(freq))
				band_modes |= WIRELESS_5G_MODES;

			if (WLAN_REG_IS_6GHZ_CHAN_FREQ(freq))
				band_modes |= WIRELESS_6G_MODES;

			if (!include_nol_chan &&
			    WLAN_REG_IS_5GHZ_CH_FREQ(freq)) {
				enum phy_ch_width in_chwidth, out_chwidth;

				in_chwidth = reg_find_chwidth_from_bw(cur_bw);
				out_chwidth =
				    reg_get_max_channel_width_without_radar(
								pdev,
								freq,
								in_chwidth);
				cur_bw = chwd_2_contbw_lst[out_chwidth];
			}

			if (max_bw < cur_bw)
				max_bw = cur_bw;
		}
	}
	qdf_mem_free(chan_list);

	in_wireless_modes &= band_modes;

	if (max_bw < BW_40_MHZ)
		in_wireless_modes &= (~WIRELESS_40_MODES);

	if (max_bw < BW_80_MHZ)
		in_wireless_modes &= (~WIRELESS_80_MODES);

	if (max_bw < BW_160_MHZ) {
		in_wireless_modes &= (~WIRELESS_160_MODES);
		if (include_nol_chan)
			in_wireless_modes &= (~WIRELESS_80P80_MODES);
	}

	reg_remove_320mhz_modes(max_bw, &in_wireless_modes);
	*mode_select = in_wireless_modes;

	return max_bw;
}

QDF_STATUS
reg_get_client_power_for_rep_ap(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_ap_type ap_pwr_type,
				enum reg_6g_client_type client_type,
				qdf_freq_t chan_freq,
				bool *is_psd, uint16_t *reg_eirp,
				uint16_t *reg_psd)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *master_chan_list;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	master_chan_list = pdev_priv_obj->
			mas_chan_list_6g_client[ap_pwr_type][client_type];

	reg_find_txpower_from_6g_list(chan_freq, master_chan_list,
				      reg_eirp);

	*is_psd = reg_is_6g_psd_power(pdev);
	if (*is_psd)
		status = reg_get_6g_chan_psd_eirp_power(chan_freq,
							master_chan_list,
							reg_psd);

	return status;
}

#ifdef CONFIG_AFC_SUPPORT
/**
 * reg_get_afc_psd() - For a given frequency, get the psd from the AFC channel
 * list
 * @freq: Channel frequency
 * @afc_chan_list: Pointer to afc_chan_list
 * @afc_psd: Pointer to afc_psd
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS reg_get_afc_psd(qdf_freq_t freq,
				  struct regulatory_channel *afc_chan_list,
				  uint16_t *afc_psd)
{
	uint8_t i;

	for (i = 0; i < NUM_6GHZ_CHANNELS; i++) {
		if (freq == afc_chan_list[i].center_freq) {
			*afc_psd = afc_chan_list[i].psd_eirp;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS reg_get_client_psd_for_ap(struct wlan_objmgr_pdev *pdev,
				     enum reg_6g_ap_type ap_pwr_type,
				     enum reg_6g_client_type client_type,
				     qdf_freq_t chan_freq,
				     uint16_t *reg_psd)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *master_chan_list, *afc_chan_list;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint16_t afc_psd = 0;
	bool is_psd;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg obj is NULL");
		*reg_psd = 0;
		return QDF_STATUS_E_FAILURE;
	}

	master_chan_list = pdev_priv_obj->
			mas_chan_list_6g_client[ap_pwr_type][client_type];
	afc_chan_list = pdev_priv_obj->mas_chan_list_6g_afc;

	is_psd = reg_is_6g_psd_power(pdev);
	if (is_psd)
		status = reg_get_6g_chan_psd_eirp_power(chan_freq,
							master_chan_list,
							reg_psd);
	else {
		*reg_psd = 0;
		return QDF_STATUS_E_FAILURE;
	}

	if (ap_pwr_type != REG_STANDARD_POWER_AP)
		return QDF_STATUS_SUCCESS;

	status = reg_get_afc_psd(chan_freq, afc_chan_list, &afc_psd);
	*reg_psd = QDF_MIN(afc_psd - SP_AP_AND_CLIENT_POWER_DIFF_IN_DBM,
			  *reg_psd);

	return status;
}
#endif

static struct regulatory_channel
reg_create_empty_reg_chan_obj(void)
{
	struct regulatory_channel reg_chan = {0};

	reg_chan.chan_flags = REGULATORY_CHAN_DISABLED;
	reg_chan.state = CHANNEL_STATE_DISABLE;

	return reg_chan;
}

struct regulatory_channel
reg_get_reg_chan_list_based_on_freq(struct wlan_objmgr_pdev *pdev,
				    qdf_freq_t freq,
				    enum supported_6g_pwr_types
				    in_6g_pwr_mode)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel reg_chan;
	enum channel_enum chan_enum;
	uint16_t sup_idx;

	reg_chan = reg_create_empty_reg_chan_obj();

	if (!freq) {
		reg_debug("Input freq is zero");
		return reg_chan;
	}
	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev private obj is NULL");
		return reg_chan;
	}

	chan_enum = reg_get_chan_enum_for_freq(freq);
	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err_rl("Invalid chan enum %d", chan_enum);
		return reg_chan;
	}

	if (chan_enum < MIN_6GHZ_CHANNEL) {
		reg_chan = pdev_priv_obj->cur_chan_list[chan_enum];
		return reg_chan;
	} else if (chan_enum >= MIN_6GHZ_CHANNEL && chan_enum <= MAX_6GHZ_CHANNEL) {
		if (!pdev_priv_obj->is_6g_channel_list_populated) {
			reg_debug("6G channel list is empty");
			return reg_chan;
		}
	}
	switch (in_6g_pwr_mode) {
	case REG_CURRENT_PWR_MODE:
		reg_chan = pdev_priv_obj->cur_chan_list[chan_enum];
		return reg_chan;
	case REG_BEST_PWR_MODE:
	default:
		sup_idx = reg_convert_enum_to_6g_idx(chan_enum);
		if (sup_idx >= NUM_6GHZ_CHANNELS) {
			reg_debug("sup_idx is out of bounds");
			return reg_chan;
		}
		reg_chan = pdev_priv_obj->cur_chan_list[chan_enum];
		reg_copy_from_super_chan_info_to_reg_channel(
							&reg_chan,
							pdev_priv_obj->super_chan_list[sup_idx],
							in_6g_pwr_mode);
		return reg_chan;
	}
}

static QDF_STATUS
reg_get_first_valid_freq_on_power_mode(struct wlan_regulatory_pdev_priv_obj
				       *pdev_priv_obj,
				       enum channel_enum freq_idx,
				       struct regulatory_channel *reg_chan,
				       enum supported_6g_pwr_types
				       in_6g_pwr_mode,
				       qdf_freq_t *first_valid_freq,
				       int bw)
{
	const struct super_chan_info *sup_chan_entry;
	enum channel_state pm_state_arr;
	uint32_t pm_chan_flag;
	QDF_STATUS status;

	status = reg_get_superchan_entry(pdev_priv_obj->pdev_ptr, freq_idx,
					 &sup_chan_entry);

	if (QDF_IS_STATUS_ERROR(status)) {
		reg_debug("Failed to get super channel entry for freq_idx %d",
			  freq_idx);
		return QDF_STATUS_E_INVAL;
	}

	if (in_6g_pwr_mode == REG_BEST_PWR_MODE)
		in_6g_pwr_mode = sup_chan_entry->best_power_mode;

	if (reg_is_supp_pwr_mode_invalid(in_6g_pwr_mode))
		return QDF_STATUS_E_INVAL;

	pm_state_arr = sup_chan_entry->state_arr[in_6g_pwr_mode];
	pm_chan_flag = sup_chan_entry->chan_flags_arr[in_6g_pwr_mode];

	if ((pm_chan_flag & REGULATORY_CHAN_DISABLED) &&
	    (pm_state_arr == CHANNEL_STATE_DISABLE) &&
	    (!reg_chan->nol_chan) &&
	    (!reg_chan->nol_history))
		return QDF_STATUS_SUCCESS;

	if (!*first_valid_freq)
		*first_valid_freq = reg_chan->center_freq;

	if (!bw)
		return QDF_STATUS_SUCCESS;

	if (sup_chan_entry->min_bw[in_6g_pwr_mode] <= bw &&
	    sup_chan_entry->max_bw[in_6g_pwr_mode]  >= bw) {
		*first_valid_freq = reg_chan->center_freq;
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * reg_get_first_valid_freq_on_cur_chan() - Get first valid frequency for
 * the given current channel list.
 * @cur_chan_list: pointer to regulatory current channel list.
 * @first_valid_freq: This is the output argument. If a valid frequency is
 * found this is set to a non-zero positive value else it is set to zero.
 * @bw: The input bandwidth may be zero and it is a valid input, because
 * sometimes even if the bandwidth is not available, the caller may still
 * want to know a valid frequency, which may be used later.
 *
 * Return: QDF_STATUS_SUCCESS.
 */
static QDF_STATUS
reg_get_first_valid_freq_on_cur_chan(struct regulatory_channel *cur_chan_list,
				     qdf_freq_t *first_valid_freq,
				     int bw)
{
	if (!first_valid_freq)
	    return QDF_STATUS_SUCCESS;

	*first_valid_freq = 0;

	if (wlan_reg_is_chan_disabled_and_not_nol(cur_chan_list))
		return QDF_STATUS_SUCCESS;

	if (!bw) {
		*first_valid_freq = cur_chan_list->center_freq;
		return QDF_STATUS_SUCCESS;
	}

	if (cur_chan_list->min_bw <= bw &&
	    cur_chan_list->max_bw >= bw)
		*first_valid_freq = cur_chan_list->center_freq;

	return QDF_STATUS_SUCCESS;
}

/**
 * reg_get_first_valid_6ghz_freq() - Get first valid 6 GHz frequency for
 * the given input bw.
 * @pdev_priv_obj: pointer to regulatory pdev private object
 * @freq_idx: channel enum
 * @in_6g_pwr_mode: Input 6g power mode
 * @first_valid_freq: Pointer to first valid frequency
 * @bw: Bandwidth
 *
 * Return: First valid 6 GHz frequency
 */
static qdf_freq_t
reg_get_first_valid_6ghz_freq(struct wlan_regulatory_pdev_priv_obj
			      *pdev_priv_obj,
			      enum channel_enum freq_idx,
			      enum supported_6g_pwr_types
			      in_6g_pwr_mode,
			      qdf_freq_t *first_valid_freq,
			      int bw)
{
	struct regulatory_channel *cur_chan_list;

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	switch (in_6g_pwr_mode) {
	case REG_CURRENT_PWR_MODE:
		reg_get_first_valid_freq_on_cur_chan(&cur_chan_list[freq_idx],
						     first_valid_freq,
						     bw);
		break;

	case REG_BEST_PWR_MODE:
	default:
		reg_get_first_valid_freq_on_power_mode(pdev_priv_obj,
						       freq_idx,
						       &cur_chan_list[freq_idx],
						       in_6g_pwr_mode,
						       first_valid_freq,
						       bw);
		break;
	}
	return *first_valid_freq;
}

/**
 * reg_get_first_valid_frequency() - Find first valid frequency for the
 * given input bw. In case of 2.4 GHz/ 5 GHz, use current channel to
 * find the first valid frequency. In case of 6 GHz band, use super channel
 * list to find the first valid channel based on the input power mode.
 *
 * @pdev_priv_obj: Pointer to struct wlan_regulatory_pdev_priv_obj
 * @freq_idx: Frequency index
 * @in_6g_pwr_mode: Input 6 GHz power type
 * @first_valid_freq: First valid freq
 * @bw: Bandwidth
 */
static void
reg_get_first_valid_frequency(struct wlan_regulatory_pdev_priv_obj
			      *pdev_priv_obj,
			      enum channel_enum freq_idx,
			      enum supported_6g_pwr_types
			      in_6g_pwr_mode,
			      qdf_freq_t *first_valid_freq,
			      int bw)
{
	struct regulatory_channel *cur_chan_list = pdev_priv_obj->cur_chan_list;

	if (freq_idx < MIN_6GHZ_CHANNEL)
		reg_get_first_valid_freq_on_cur_chan(&cur_chan_list[freq_idx],
						     first_valid_freq,
						     bw);
	else
		reg_get_first_valid_6ghz_freq(pdev_priv_obj, freq_idx,
					      in_6g_pwr_mode,
					      first_valid_freq,
					      bw);
}

/**
 * reg_is_sec_40_valid_for_freq() - Return true if the secondary frequency
 * of HT40 channel is valid, false otherwise.
 * @pdev_priv_obj: Pointer to struct wlan_regulatory_pdev_priv_obj
 * @freq: Primary frequency
 * @sec_40_offset: Offset of HT40 chan
 * @in_6g_pwr_mode: Input 6 GHz power mode
 * @bw: Bandwidth
 */
static bool
reg_is_sec_40_valid_for_freq(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
			     qdf_freq_t freq, int8_t sec_40_offset,
			     enum supported_6g_pwr_types in_6g_pwr_mode)
{
	enum channel_enum sec_chan_enum;
	qdf_freq_t sec_40mhz_freq = freq + sec_40_offset;
	qdf_freq_t first_valid_sec_freq = 0;

	if (!sec_40_offset)
		return true;

	sec_chan_enum = reg_get_chan_enum_for_freq(sec_40mhz_freq);

	if (sec_chan_enum >= NUM_CHANNELS)
	    return false;

	reg_get_first_valid_frequency(pdev_priv_obj, sec_chan_enum,
				      in_6g_pwr_mode,
				      &first_valid_sec_freq, BW_40_MHZ);

	return !!first_valid_sec_freq;
}

QDF_STATUS
reg_get_first_valid_freq(struct wlan_objmgr_pdev *pdev,
			 enum supported_6g_pwr_types
			 in_6g_pwr_mode,
			 qdf_freq_t *first_valid_freq,
			 int bw, int sec_40_offset,
			 enum channel_enum start_chan,
			 enum channel_enum end_chan)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *cur_chan_list;
	enum channel_enum freq_idx;

	if (!pdev) {
		reg_err_rl("invalid pdev");
		return QDF_STATUS_E_INVAL;
	}
	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err_rl("reg pdev priv obj is NULL");
		return QDF_STATUS_E_INVAL;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	for (freq_idx = start_chan; freq_idx <= end_chan; freq_idx++) {
	    reg_get_first_valid_frequency(pdev_priv_obj, freq_idx,
					  in_6g_pwr_mode,
					  first_valid_freq, bw);
	    if (*first_valid_freq) {
		if (!reg_is_sec_40_valid_for_freq(pdev_priv_obj,
						  *first_valid_freq,
						  sec_40_offset,
						  in_6g_pwr_mode))
		    *first_valid_freq = 0;
	    }
	    if (*first_valid_freq)
		break;
	}

	return QDF_STATUS_SUCCESS;
}

bool reg_is_6g_domain_jp(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	if (!pdev) {
		reg_err_rl("invalid pdev");
		return false;
	}
	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err_rl("reg pdev priv obj is NULL");
		return false;
	}
	return pdev_priv_obj->reg_6g_superid == MKK1_6G_0B;
}

#ifdef CONFIG_BAND_6GHZ
/**
 * reg_copy_power_to_eirp_power_list() - Copy power and channel information
 * from regulatory master channel list to chan_eirp_list.
 * @chan_eirp_list: Pointer to chan_eirp_list
 * @mas_chan_list: Pointer to mas_chan_list
 * @num_6g_chans: Number of 6G channels
 */
static void
reg_copy_power_to_chan_eirp_list(struct channel_power *chan_eirp_list,
				 struct regulatory_channel *mas_chan_list,
				 uint8_t num_6g_chans)
{
	uint8_t i;

	if (!mas_chan_list) {
		reg_err("mas_chan_list is NULL");
		return;
	}

	for (i = 0; i < num_6g_chans; i++) {
		if (mas_chan_list[i].state == CHANNEL_STATE_ENABLE) {
			chan_eirp_list[i].chan_num = mas_chan_list[i].chan_num;
			chan_eirp_list[i].center_freq =
						mas_chan_list[i].center_freq;
			chan_eirp_list[i].tx_power = mas_chan_list[i].tx_power;
		}
	}
}

QDF_STATUS reg_get_max_reg_eirp_from_list(struct wlan_objmgr_pdev *pdev,
					  enum reg_6g_ap_type ap_pwr_type,
					  bool is_client_power_needed,
					  enum reg_6g_client_type client_type,
					  struct channel_power *chan_eirp_list,
					  uint8_t num_6g_chans)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *mas_chan_list;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (ap_pwr_type >= REG_MAX_SUPP_AP_TYPE) {
		reg_err("Unsupported 6G AP power type %d", ap_pwr_type);
		return QDF_STATUS_E_FAILURE;
	}

	if (!chan_eirp_list) {
		reg_err("chan_eirp_list is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!num_6g_chans || num_6g_chans > NUM_6GHZ_CHANNELS) {
		reg_err("Incorrect number of channels %d", num_6g_chans);
		return QDF_STATUS_E_FAILURE;
	}

	if (is_client_power_needed) {
		if (client_type >= REG_MAX_CLIENT_TYPE) {
			reg_err("Incorrect client type %d", client_type);
			return QDF_STATUS_E_FAILURE;
		}

		mas_chan_list =
			pdev_priv_obj->mas_chan_list_6g_client[ap_pwr_type][client_type];
	} else {
		mas_chan_list =
			pdev_priv_obj->mas_chan_list_6g_ap[ap_pwr_type];
	}
	reg_copy_power_to_chan_eirp_list(chan_eirp_list, mas_chan_list, num_6g_chans);

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * reg_quick_compute_pdev_current_chan_list - Compute regulatory current
 * channel list but only the 6GHz portion based on the input power mode.
 * @pdev: Pointer to pdev.
 * @in_6g_pwr_mode: Input 6GHz power mode.
 *
 * Return: Void.
 */
static void
reg_quick_compute_pdev_current_chan_list(struct wlan_objmgr_pdev *pdev,
					 enum supported_6g_pwr_types in_6g_pwr_mode)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return;
	}

	reg_get_6g_pwrmode_chan_list(pdev_priv_obj,
				     pdev_priv_obj->cur_chan_list,
				     in_6g_pwr_mode);
	reg_modify_chan_list_for_max_chwidth_for_pwrmode(
						pdev_priv_obj->pdev_ptr,
						pdev_priv_obj->cur_chan_list,
						in_6g_pwr_mode);
}

QDF_STATUS
reg_quick_set_ap_pwr_and_update_chan_list(struct wlan_objmgr_pdev *pdev,
					  enum reg_6g_ap_type ap_pwr_type)
{
	enum supported_6g_pwr_types supp_pwr_type;

	if (!reg_get_num_rules_of_ap_pwr_type(pdev, ap_pwr_type))
		return QDF_STATUS_E_FAILURE;

	reg_set_cur_6g_ap_pwr_type(pdev, ap_pwr_type);
	supp_pwr_type =
		reg_conv_6g_ap_type_to_supported_6g_pwr_types(ap_pwr_type);
	reg_quick_compute_pdev_current_chan_list(pdev, supp_pwr_type);

	return QDF_STATUS_SUCCESS;
}

/**
 * reg_is_non_6g_freq_txable() - Check if the given non-6 GHz frequency is
 * tx-able.
 * @pdev: Pointer to pdev
 * @freq: Frequency in MHz
 * @in_6ghz_pwr_mode: Input AP power type
 *
 * As long as the frequency is enabled in regulatory, a non-6 GHz frequency is
 * always tx-able.
 *
 * Return: True if the frequency is tx-able, else false.
 */
static inline bool
reg_is_non_6g_freq_txable(struct wlan_objmgr_pdev *pdev,
			  qdf_freq_t freq,
			  enum supported_6g_pwr_types in_6ghz_pwr_mode)
{
	return reg_is_freq_enabled(pdev, freq, in_6ghz_pwr_mode);
}

/**
 * reg_is_6g_freq_txable() - Check if the given 6 GHz frequency is tx-able.
 * @pdev: Pointer to pdev
 * @freq: Frequency in MHz
 * @in_6ghz_pwr_mode: Input AP power type
 *
 * For outdoor mode, an SP channel is tx-able if the channel is present in the
 * AFC response. In case of indoor mode, a channel is always tx-able (Assuming
 * it is enabled by regulatory).
 *
 * Return: True if the frequency is tx-able, else false.
 */
static bool
reg_is_6g_freq_txable(struct wlan_objmgr_pdev *pdev,
		      qdf_freq_t freq,
		      enum supported_6g_pwr_types in_6ghz_pwr_mode)
{
	bool is_freq_enabled;
	enum reg_afc_dev_deploy_type reg_afc_deploy_type;
	enum channel_enum chan_idx;

	is_freq_enabled = reg_is_freq_enabled(pdev, freq, in_6ghz_pwr_mode);
	/* If the frequency is not enabled, then return. */
	if (!is_freq_enabled)
		return false;

	reg_get_afc_dev_deploy_type(pdev, &reg_afc_deploy_type);

	/* Outdoor deployment requires additional checks if power mode is SP.
	 * If the deployment is indoor, return if the frequency is enabled or
	 * or not.
	 */
	if (reg_afc_deploy_type != AFC_DEPLOYMENT_OUTDOOR)
		return is_freq_enabled;

	if (in_6ghz_pwr_mode == REG_BEST_PWR_MODE) {
		in_6ghz_pwr_mode = reg_get_best_6g_pwr_type(pdev, freq);
	} else if (in_6ghz_pwr_mode == REG_CURRENT_PWR_MODE) {
		enum reg_6g_ap_type cur_6g_ap_pwr_mode;

		reg_get_cur_6g_ap_pwr_type(pdev, &cur_6g_ap_pwr_mode);
		in_6ghz_pwr_mode = reg_conv_6g_ap_type_to_supported_6g_pwr_types(cur_6g_ap_pwr_mode);
	}

	/* If the power mode is VLP in outdoor, then return if the frequency is
	 * enabled or not.
	 */
	if (in_6ghz_pwr_mode == REG_AP_VLP)
		return is_freq_enabled;

	chan_idx = reg_get_chan_enum_for_freq(freq);
	if (reg_is_chan_enum_invalid(chan_idx))
		return false;

	/* If power mode is SP, check if AFC is done. */
	return reg_is_sup_chan_entry_afc_done(pdev, chan_idx, in_6ghz_pwr_mode);
}

bool
reg_is_freq_txable(struct wlan_objmgr_pdev *pdev,
		   qdf_freq_t freq,
		   enum supported_6g_pwr_types in_6ghz_pwr_mode)
{
	if (!reg_is_6ghz_chan_freq(freq))
		return reg_is_non_6g_freq_txable(pdev, freq, in_6ghz_pwr_mode);

	return reg_is_6g_freq_txable(pdev, freq, in_6ghz_pwr_mode);
}

