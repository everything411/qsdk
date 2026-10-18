/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * Channel and band utilities implementation
 *
 * Implements helpers that operate on channel parameters and band math:
 *  - DFS frequency detection
 *  - Channel state equality/update helpers
 *  - HW index lookup by frequency using cfg ranges
 *  - Channel number <-> frequency mapping across bands
 *  - Secondary channel offset derivation
 */
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "mld_types.h"
#include "mld_chan.h"

/*
 * is_dfs_freq - Return non-zero if 'freq' (MHz) is in DFS ranges;
 * else 0
 */
int is_dfs_freq(int freq)
{
	if (freq)
		return (freq >= 5260 && freq <= 5320) ||
		       (freq >= 5500 && freq <= 5720);

	return 0;
}

/*
 * chan_params_equal - Return non-zero if given parameters match
 * stored channel state exactly
 */
int chan_params_equal(const struct chan_params_info *st,
			int freq,
			int c_freq1,
			int c_freq2,
			int bandwidth,
			int punc_bitmap)
{
	if (!st)
		return 0;

	return (st->freq == freq && st->c_freq1 == c_freq1 &&
		st->c_freq2 == c_freq2 && st->width == bandwidth &&
		st->punc_bitmap == punc_bitmap);
}

/*
 *chan_params_update - Update stored channel state to latest values
 */
void chan_params_update(struct chan_params_info *st,
			int freq,
			int c_freq1,
			int c_freq2,
			int sec_off,
			int bandwidth,
			int punc_bitmap,
			int is_dfs)
{
	if (!st)
		return;

	st->freq        = freq;
	st->c_freq1     = c_freq1;
	st->c_freq2     = c_freq2;
	st->sec_off     = sec_off;
	st->width       = bandwidth;
	st->punc_bitmap = punc_bitmap;
	st->is_dfs      = is_dfs;
}

/*
 * wpa_get_hw_idx_by_freq - Map operating frequency to hardware index
 * using configured ranges
 */
int wpa_get_hw_idx_by_freq(int freq)
{
	if (freq <= 0)
		return -1;

	for (int i = 0; i < cfg.ap_vap_count; i++) {
		if (cfg.ap_vaps[i].link_count == 0) {
			struct multi_hw_info *info =
				&cfg.ap_vaps[i].hw_info;
			if (info->start_freq > 0) {
				if ((info->end_freq > 0 &&
				     freq >= info->start_freq &&
				     freq <= info->end_freq) ||
				    (info->end_freq <= 0 &&
				     freq >= info->start_freq))
					return (int) info->hw_idx;
			}
		} else {
			for (int j = 0; j < cfg.ap_vaps[i].link_count; j++) {
				struct multi_hw_info *info =
					&cfg.ap_vaps[i].links[j].hw_info;
				if (info->start_freq <= 0)
					continue;
				if (info->end_freq > 0) {
					if (freq < info->start_freq ||
					    freq > info->end_freq)
						continue;
				} else {
					if (freq < info->start_freq)
						continue;
				}
				return (int) info->hw_idx;
			}
		}
	}
	return -1;
}

/*
 * wifi_channel_to_freq - Convert channel number to frequency (MHz)
 * for the specified band
 */
int wifi_channel_to_freq(int channel, int band)
{
	if (channel <= 0)
		return -1;
	switch (band) {
	case WIFI_BAND_2GHZ:
		if (channel == 14)
			return 2484;
		if (channel >= 1 && channel <= 13)
			return 2412 + 5 * (channel - 1);
		return -1;
	case WIFI_BAND_5GHZ:
		return 5000 + 5 * channel;
	case WIFI_BAND_6GHZ:
		if (channel >= 1 && channel <= 233)
			return 5955 + 5 * (channel - 1);
		return -1;
	default:
		return -1;
	}
}

/*
 * wifi_get_band_by_freq - Determine Wi-Fi band from operating
 * frequency (MHz)
 */
enum wifi_band wifi_get_band_by_freq(int freq)
{
	if (freq >= 5925)
		return WIFI_BAND_6GHZ;
	if (freq >= 2400 && freq < 2500)
		return WIFI_BAND_2GHZ;
	return WIFI_BAND_5GHZ;
}

/*
 * calc_sec_channel_offset - Calculate the sec_off for the given
 * primary freq, center_freq1 and bandwidth
 */
int calc_sec_channel_offset(int primary_freq, int center_freq1, int bandwidth)
{
	int offset_from_center;

	/* For 20 MHz bandwidth, there is no secondary channel */
	if (bandwidth == 20 || bandwidth == 0)
		return 0;

	/* Validate inputs */
	if (primary_freq <= 0 || center_freq1 <= 0)
		return 0;

	/* Calculate the offset between primary and center frequency */
	offset_from_center = primary_freq - center_freq1;

	switch (bandwidth) {
	case 40:
		if (offset_from_center == -10)
			return 1;
		else if (offset_from_center == 10)
			return -1;
		break;

	case 80:
		if (offset_from_center == -30 || offset_from_center == 10)
			return 1;
		else if (offset_from_center == -10 || offset_from_center == 30)
			return -1;
		break;

	case 160:
		if (offset_from_center == -70 || offset_from_center == 50 ||
		    offset_from_center == -30 || offset_from_center == 10)
			return 1;
		else if (offset_from_center == 70 || offset_from_center == -50 ||
			 offset_from_center == 30 || offset_from_center == -10)
			return -1;
		break;

	case 320:
		if (offset_from_center == -150 || offset_from_center == 130 ||
		    offset_from_center == -110 || offset_from_center == 90 ||
		    offset_from_center == -70 || offset_from_center == 50 ||
		    offset_from_center == -30 || offset_from_center == 10)
			return 1;
		else if (offset_from_center == 150 || offset_from_center == -130 ||
			 offset_from_center == 110 || offset_from_center == -90 ||
			 offset_from_center == 70 || offset_from_center == -50 ||
			 offset_from_center == 30 || offset_from_center == -10)
			return -1;
		break;

	default:
		/* Unsupported bandwidth */
		return 0;
	}

	/* Invalid configuration */
	return 0;
}
