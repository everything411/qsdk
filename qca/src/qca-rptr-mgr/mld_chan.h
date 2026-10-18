/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef MLD_CHAN_H
#define MLD_CHAN_H

/*
 * Channel and band utilities for MLD interfaces
 *
 * These helpers provide common operations used by iface-mgr to:
 *  - Detect DFS frequencies
 *  - Map HE/EHT segment indices to center frequencies
 *  - Determine operating band from frequency
 *  - Track and compare cached channel state
 *  - Derive the secondary channel offset from freq/center_freq1
 */

#include "mld_types.h"

enum wifi_band {
	WIFI_BAND_2GHZ = 2,
	WIFI_BAND_5GHZ = 5,
	WIFI_BAND_6GHZ = 6,
};

/*
 * Return non-zero if 'freq' (MHz) falls in DFS ranges; else 0
 */
int is_dfs_freq(int freq);

/*
 * Return hardware index mapped from 'freq' using cfg ranges; -1 on failure
 */
int wpa_get_hw_idx_by_freq(int freq);

/*
 * Convert channel number to frequency (MHz) for the given band
 */
int wifi_channel_to_freq(int channel, int band);

/*
 * Determine Wi-Fi band from operating frequency (MHz)
 */
enum wifi_band wifi_get_band_by_freq(int freq);

/*
 * Compare channel params against stored state; return non-zero if equal
 */
int chan_params_equal(const struct chan_params_info *st, int freq, int c_freq1,
				int c_freq2, int bandwidth, int punc_bitmap);

/*
 * Update stored channel state with latest values
 */
void chan_params_update(struct chan_params_info *st, int freq, int c_freq1,
	int c_freq2, int sec_off, int bandwidth, int punc_bitmap, int is_dfs);

/*
 * Derive secondary channel offset from primary freq and center_freq1.
 * - Returns 0 for 20 MHz or invalid inputs
 * - Returns 1 if center_freq1 > freq (secondary above)
 * - Returns -1 if center_freq1 < freq (secondary below)
 */
int calc_sec_channel_offset(int freq, int center_freq1, int bandwidth_mhz);

#endif /* MLD_CHAN_H */
