/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2002-2006, Atheros Communications Inc.
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
 * DOC: This file contains the DFS Bangradar functionality.
 *
 */

#include "dfs.h"
#include "dfs_zero_cac.h"
#include "wlan_dfs_lmac_api.h"
#include "wlan_dfs_mlme_api.h"
#include "wlan_dfs_tgt_api.h"
#include "dfs_internal.h"
#include <dfs_process_radar_found_ind.h>

/**
 * dfs_is_320mhz_offset_invalid() - Check if the frequency offset is within the
 * bounds of the 5G 320MHz channel. The allowed frequency offset is within the
 * range [-160MHz, 80MHz].
 * @freq_offset: Radar frequency offset.
 *
 * @Return: Checks for invalid frequency offset. So if the frequency offset is
 * within the range then returns false, if not returns true.
 */
static inline
bool dfs_is_320mhz_offset_invalid(int32_t freq_offset)
{
	return ((freq_offset < -BW_160) || (freq_offset > BW_80));
}

/**
 * dfs_is_below320mhz_offset_invalid() - Check if the frequency offset is within
 * the bounds of the given channel width. The allowed frequency offset is
 * within the range [-(ch_width/2)MHz, (ch_width)MHz].
 * @freq_offset: Radar frequency offset.
 * @ch_width: Channel width of the channel on which radar is hit.
 *
 * @Return: Checks for invalid frequency offset. So if the frequency offset is
 * within the range then returns false, if not returns true.
 */
static inline
bool dfs_is_below320mhz_offset_invalid(int32_t freq_offset, uint16_t ch_width)
{
	uint16_t half_bw = ch_width / 2;

	return ((freq_offset < -half_bw) || (freq_offset > half_bw));
}

static
bool dfs_is_offset_invalid_for_bw(int32_t freq_offset, uint16_t ch_width)
{
	switch (ch_width) {
	case BW_320:
		return dfs_is_320mhz_offset_invalid(freq_offset);
	case BW_160:
	case BW_80:
	case BW_40:
	case BW_20:
		return dfs_is_below320mhz_offset_invalid(freq_offset, ch_width);
	default:
		dfs_debug(NULL, WLAN_DEBUG_DFS_ALWAYS,
			"Invalid channel width");
		return true;
	}
}

/**
 * dfs_11be_homechan_detector_id() - Get Detector id for Home channel
 */
static
enum detector_id dfs_11be_homechan_detector_id(void)
{
	return DETECTOR_ID_0;
}

/**
 * dfs_11be_agile_detector_id() - Get Detector id for Agile 11BE channel
 */
static
enum detector_id dfs_11be_agile_detector_id(void)
{
	return AGILE_DETECTOR_11BE;
}

/**
 * dfs_check_bangradar_sanity_for_11be() - Check the sanity of bangradar for
 * 11BE chispets
 * @dfs: Pointer to wlan_dfs structure.
 * @bangradar_params: Parameters of the radar to be simulated.
 */
static QDF_STATUS
dfs_check_bangradar_sanity_for_11be(struct wlan_dfs *dfs,
				    struct dfs_bangradar_params *brdr_prm)
{
	struct dfs_channel *chan = dfs->dfs_curchan;
	uint16_t width;
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	if (brdr_prm->detector_id == dfs_11be_homechan_detector_id()) {
	    chan = dfs->dfs_curchan;
	    width = dfs_chan_to_ch_width(chan);
	} else if (brdr_prm->detector_id == dfs_11be_agile_detector_id()) {
		    width = dfs_translate_chwidth_enum2val(dfs,
						dfs->dfs_precac_chwidth);
	} else {
		dfs_debug(dfs, WLAN_DEBUG_DFS_ALWAYS, "Invalid Detector ID %d",
			  brdr_prm->detector_id);
		return status;
	}
	if (brdr_prm->seg_id != SEG_ID_PRIMARY) {
		dfs_debug(dfs, WLAN_DEBUG_DFS_ALWAYS,
			  "Invalid seg id");
		return status;
	}

	if (dfs_is_offset_invalid_for_bw(brdr_prm->freq_offset, width)) {
		dfs_debug(dfs, WLAN_DEBUG_DFS_ALWAYS,
			  "Invalid freq offset");
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dfs_is_agile_radar_and_adfs_disabled() - If radar is injected on the agile
 * channel using agile detector id and if the agile config is disabled, return
 * true, else return false.
 *
 * @dfs: Pointer to struct wlan_dfs
 * @bgrdr_params: Pointer to struct dfs_bangradar_params
 */
static bool
dfs_is_agile_radar_and_adfs_disabled(struct wlan_dfs *dfs,
				     struct dfs_bangradar_params *bgrdr_params)
{
	if ((dfs_get_agile_detector_id(dfs) == bgrdr_params->detector_id)
	    && !dfs_is_agile_cac_enabled(dfs)) {
		dfs_debug(dfs, WLAN_DEBUG_DFS_ALWAYS, "Agile not enabled but radar is injected on agile chan");
		return true;
	}
	return false;
}

/**
 * dfs_check_bangradar_sanity() - Check the sanity of bangradar
 * @dfs: Pointer to wlan_dfs structure.
 * @bangradar_params: Parameters of the radar to be simulated.
 */
static QDF_STATUS
dfs_check_bangradar_sanity(struct wlan_dfs *dfs,
			   struct dfs_bangradar_params *bangradar_params)
{
	if (!bangradar_params) {
		dfs_debug(dfs, WLAN_DEBUG_DFS_ALWAYS,
			  "bangradar params is NULL");
		return -EINVAL;
	}

	if (dfs_is_agile_radar_and_adfs_disabled(dfs, bangradar_params))
		return -EINVAL;

	if (dfs->dfs_is_radar_found_chan_freq_eq_center_freq) {
		if (dfs_check_bangradar_sanity_for_11be(dfs,
							bangradar_params)) {
			return -EINVAL;
		}
		return QDF_STATUS_SUCCESS;
	}
	if (dfs_is_true_160mhz_supported(dfs)) {
		if (abs(bangradar_params->freq_offset) >
		    FREQ_OFFSET_BOUNDARY_FOR_160MHZ) {
			dfs_debug(dfs, WLAN_DEBUG_DFS_ALWAYS,
				  "Frequency Offset out of bound");
			return -EINVAL;
		}
	} else if (abs(bangradar_params->freq_offset) >
		   FREQ_OFFSET_BOUNDARY_FOR_80MHZ) {
		dfs_debug(dfs, WLAN_DEBUG_DFS_ALWAYS,
			  "Frequency Offset out of bound");
		return -EINVAL;
	}
	if (bangradar_params->seg_id > SEG_ID_SECONDARY) {
		dfs_debug(dfs, WLAN_DEBUG_DFS_ALWAYS, "Invalid segment ID");
		return -EINVAL;
	}
	if ((bangradar_params->detector_id > dfs_get_agile_detector_id(dfs)) ||
	    ((bangradar_params->detector_id ==
	      dfs_get_agile_detector_id(dfs)) &&
	      !dfs->dfs_is_offload_enabled)) {
		dfs_debug(dfs, WLAN_DEBUG_DFS_ALWAYS, "Invalid detector ID");
		return -EINVAL;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * dfs_start_host_based_bangradar() - Mark as bangradar and start
 * wlan_dfs_task_timer.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if defined(WLAN_DFS_PARTIAL_OFFLOAD)
static int dfs_start_host_based_bangradar(struct wlan_dfs *dfs)
{
	dfs->wlan_radar_tasksched = 1;
	qdf_timer_mod(&dfs->wlan_dfs_task_timer, 0);

	return 0;
}
#else
static inline int dfs_start_host_based_bangradar(struct wlan_dfs *dfs)
{
	return 0;
}
#endif

/**
 * dfs_fill_emulate_bang_radar_test() - Update dfs unit test arguments and
 * send bangradar command to firmware.
 * @dfs: Pointer to wlan_dfs structure.
 * @bangradar_params: Parameters of the radar to be simulated.
 *
 * Return: If the event is received return 0.
 */
#if defined(WLAN_DFS_FULL_OFFLOAD)
#define FREQ_SIGNBIT_OFFSET 8
#define FREQ_SIGNBIT_MASK 0x1
#define ADD_TO_32BYTE(_arg, _shift, _mask) (((_arg) & (_mask)) << (_shift))
static int
dfs_fill_emulate_bang_radar_test(struct wlan_dfs *dfs,
				 struct dfs_bangradar_params *bangradar_params)
{
	struct dfs_emulate_bang_radar_test_cmd dfs_unit_test;
	uint32_t packed_args = 0;
	bool freq_offset_signbit;
	enum detector_id agile_detector_id = dfs_get_agile_detector_id(dfs);

	/* It is possible that home/operating channel is nonDFS
	 * and the Agile channel is DFS. Therefore, whether the
	 * home/operating channel is a DFS or not should not affect
	 * the agile bangradar.
	 */
	if ((bangradar_params->detector_id != agile_detector_id) &&
		!(WLAN_IS_PRIMARY_OR_SECONDARY_CHAN_DFS(dfs->dfs_curchan))) {
		dfs_err(dfs, WLAN_DEBUG_DFS_ALWAYS,
			"Ignore bangradar on a NON-DFS channel");
		return -EINVAL;
	}

	packed_args =
		ADD_TO_32BYTE(bangradar_params->seg_id,
			      SEG_ID_SHIFT,
			      SEG_ID_MASK) |
		ADD_TO_32BYTE(bangradar_params->is_chirp,
			      IS_CHIRP_SHIFT,
			      IS_CHIRP_MASK) |
		ADD_TO_32BYTE(bangradar_params->freq_offset,
			      FREQ_OFF_SHIFT,
			      FREQ_OFFSET_MASK) |
		ADD_TO_32BYTE(bangradar_params->detector_id,
			      DET_ID_SHIFT,
			      DET_ID_MASK);

	if (dfs->dfs_is_bangradar_320_supported) {
		freq_offset_signbit = (
			bangradar_params->freq_offset >> FREQ_SIGNBIT_OFFSET) &
							 FREQ_SIGNBIT_MASK;
		packed_args |= ADD_TO_32BYTE(freq_offset_signbit,
					     FREQ_OFFSET_SIGNBIT_SHIFT,
					     FREQ_OFFSET_SIGNBIT_MASK);
	}

	qdf_mem_zero(&dfs_unit_test, sizeof(dfs_unit_test));
	dfs_unit_test.num_args = DFS_UNIT_TEST_NUM_ARGS;
	dfs_unit_test.args[IDX_CMD_ID] =
			DFS_PHYERR_OFFLOAD_TEST_SET_RADAR;
	dfs_unit_test.args[IDX_PDEV_ID] =
			wlan_objmgr_pdev_get_pdev_id(dfs->dfs_pdev_obj);
	dfs_unit_test.args[IDX_RADAR_PARAM1_ID] = packed_args;

	if (tgt_dfs_process_emulate_bang_radar_cmd(dfs->dfs_pdev_obj,
						   &dfs_unit_test) ==
			QDF_STATUS_E_FAILURE) {
		return -EINVAL;
	}

	return 0;
}
#else
static inline int
dfs_fill_emulate_bang_radar_test(struct wlan_dfs *dfs,
				 struct dfs_bangradar_params *bangradar_params)
{
	return 0;
}
#endif

/*
 * Handle all types of Bangradar here.
 * Bangradar arguments:
 * seg_id      : Segment ID where radar should be injected.
 * is_chirp    : Is chirp radar or non chirp radar.
 * freq_offset : Frequency offset from center frequency.
 *
 * Type 1 (DFS_BANGRADAR_FOR_ALL_SUBCHANS): To add all subchans.
 * Type 2 (DFS_BANGRADAR_FOR_ALL_SUBCHANS_OF_SEGID): To add all
 *               subchans of given segment_id.
 * Type 3 (DFS_BANGRADAR_FOR_SPECIFIC_SUBCHANS): To add specific
 *               subchans based on the arguments.
 *
 * The arguments will already be filled in the indata structure
 * based on the type.
 * If an argument is not specified by user, it will be set to
 * default (0) in the indata already and correspondingly,
 * the type will change.
 */

int dfs_bang_radar(struct wlan_dfs *dfs, void *indata, uint32_t insize)
{
	struct dfs_bangradar_params *bangradar_params;
	int error = -EINVAL;

	if (!dfs) {
		dfs_err(NULL, WLAN_DEBUG_DFS_ALWAYS,  "dfs is NULL");
		return error;
	}

	if (insize < sizeof(struct dfs_bangradar_params) || !indata) {
		dfs_debug(dfs, WLAN_DEBUG_DFS1,
			  "insize = %d, expected = %zu bytes, indata = %pK",
			  insize,
			  sizeof(struct dfs_bangradar_params),
			  indata);
		return error;
	}
	bangradar_params = (struct dfs_bangradar_params *)indata;
	error = dfs_check_bangradar_sanity(dfs, bangradar_params);
	if (error != QDF_STATUS_SUCCESS)
		return error;
	dfs->dfs_bangradar_type = bangradar_params->bangradar_type;
	dfs->dfs_seg_id = bangradar_params->seg_id;
	dfs->dfs_is_chirp = bangradar_params->is_chirp;
	dfs->dfs_freq_offset = bangradar_params->freq_offset;

	if (dfs->dfs_is_offload_enabled) {
		error = dfs_fill_emulate_bang_radar_test(
				dfs,
				bangradar_params);
	} else {
		error = dfs_start_host_based_bangradar(dfs);
	}
	return error;
}
