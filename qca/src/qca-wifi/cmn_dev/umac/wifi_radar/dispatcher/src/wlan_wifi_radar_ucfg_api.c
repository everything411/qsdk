/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <wlan_wifi_radar_ucfg_api.h>
#include "wifi_radar_defs_i.h"
#include <wlan_wifi_radar_utils_api.h>
#include <wlan_wifi_radar_tgt_api.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_wifi_radar_public_structs.h>

static int
wifi_radar_validate_capture_command(struct wlan_objmgr_pdev *pdev,
				    struct wifi_radar_command_params *params)
{
	struct wlan_objmgr_psoc *psoc;
	struct pdev_wifi_radar *pwr;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		wifi_radar_err("psoc is null!");
		return 0;
	}

	pwr = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						    WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pwr) {
		wifi_radar_err("wifi radar pdev object is NULL");
		return 0;
	}

	if (params->cmd_type == WIFI_RADAR_CMD_CAPTURE_START) {
		/**
		 * periodicity 0 is valid for 1 shot capture.
		 * If not one shot, min periodicity should be MIN_WIFI_RADAR_PRD
		 */
		if (params->periodicity) {
			if (!tgt_wifi_radar_validate_period
				(psoc, params->periodicity)) {
				wifi_radar_err("Invalid period value: %d",
					       params->periodicity);
				return 0;
			}
		}

		if (!tgt_wifi_radar_validate_chainmask(pwr,
						       params->tx_chainmask,
						       params->rx_chainmask)) {
			wifi_radar_err("Invalid txrx chain combination");
			return 0;
		}
	}

	return 1;
}

static int
wifi_radar_validate_common_params(struct pdev_wifi_radar *pwr,
				  struct wifi_radar_command_params *params)
{
	uint32_t pow2_ltf = 0;

	if (params->bandwidth >= WIFI_RADAR_PKT_WIDTH_MAX) {
		wifi_radar_err("Invalid bandwidth for wifi radar operation");
		return 0;
	}

	if (params->bandwidth == WIFI_RADAR_PKT_WIDTH320) {
		wifi_radar_err("320MHz pkt BW not supported");
		return 0;
	}

	qdf_spin_lock_bh(&pwr->cal_status_lock);
	if (params->bandwidth > pwr->channel_bw) {
		qdf_spin_unlock_bh(&pwr->cal_status_lock);
		wifi_radar_err("BW configured exceeds channel BW used for cal");
		return 0;
	}
	qdf_spin_unlock_bh(&pwr->cal_status_lock);

	if (params->num_ltf_tx > pwr->max_num_ltf_tx) {
		wifi_radar_err("Invalid num_ltf_tx, max value %d",
			       pwr->max_num_ltf_tx);
		return 0;
	}

	if (params->num_skip_ltf_rx > pwr->max_num_skip_ltf_rx) {
		wifi_radar_err("Invalid num_skip_ltf_rx, max value %d",
			       pwr->max_num_skip_ltf_rx);
		return 0;
	}

	if (params->num_ltf_accumulation > pwr->max_num_ltf_accumulation) {
		wifi_radar_err("Invalid num_ltf_accumulation, max value %d",
			       pwr->max_num_ltf_accumulation);
		return 0;
	}

	if (!QDF_IS_STATUS_SUCCESS
			(qdf_get_power2_of_n(params->num_ltf_accumulation,
					     &pow2_ltf))) {
		wifi_radar_err("Failed to get power2 of %d",
			       params->num_ltf_accumulation);
		return 0;
	}

	if ((pow2_ltf + params->num_skip_ltf_rx) >= params->num_ltf_tx) {
		wifi_radar_err("2^ltf_accum + skip_ltf_rx exceeds ltf_tx");
		return 0;
	}

	return 1;
}

int ucfg_wifi_radar_get_cal_param(struct wlan_objmgr_pdev *pdev,
				  struct wifi_radar_cal_info_params *params)
{
	struct pdev_wifi_radar *pwr;
	uint8_t i, j;

	pwr = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						    WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pwr) {
		wifi_radar_err("wifi radar pdev object is NULL");
		return -EINVAL;
	}

	if (!(pwr->is_wifi_radar_capable)) {
		wifi_radar_err("wifi radar is not supported on this chip");
		return -EINVAL;
	}

	qdf_spin_lock_bh(&pwr->cal_status_lock);
	params->timestamp = pwr->host_timestamp_of_cal_status_evt;
	params->wifi_radar_pkt_bw = pwr->wifi_radar_pkt_bw;
	params->channel_bw = pwr->channel_bw;
	params->band_center_freq = pwr->band_center_freq;
	params->cal_num_ltf_tx = pwr->cal_num_ltf_tx;
	params->cal_num_skip_ltf_rx = pwr->cal_num_skip_ltf_rx;
	params->cal_num_ltf_accumulation = pwr->cal_num_ltf_accumulation;

	for (i = 0; i < HOST_MAX_CHAINS; i++) {
		for (j = 0; j < HOST_MAX_CHAINS; j++) {
			params->per_tx_chain_cal_status[i][j] =
				pwr->per_chain_comb_cal_status[i][j];
		}
	}
	qdf_spin_unlock_bh(&pwr->cal_status_lock);

	return 0;
}

int ucfg_wifi_radar_command(struct wlan_objmgr_pdev *pdev,
			    struct wifi_radar_command_params *params)
{
	struct pdev_wifi_radar *pwr;
	struct wlan_objmgr_psoc *psoc;

	pwr = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						    WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pwr) {
		wifi_radar_err("wifi radar pdev object is NULL");
		return -EINVAL;
	}

	if (!(pwr->is_wifi_radar_capable)) {
		wifi_radar_err("wifi radar is not supported on this chip");
		return -EINVAL;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		wifi_radar_err("psoc is null!");
		return -EINVAL;
	}

	if ((params->cmd_type == WIFI_RADAR_CMD_CAPTURE_START) ||
	    (params->cmd_type == WIFI_RADAR_CMD_RX_CAL)) {
		if (!wifi_radar_validate_common_params(pwr, params)) {
			wifi_radar_err("invalid wifi radar cal/capture params");
			return -EINVAL;
		}
	}

	if (params->cmd_type == WIFI_RADAR_CMD_CAPTURE_START) {
		if (!wifi_radar_validate_capture_command(pdev, params)) {
			wifi_radar_err("fail to start capture");
			return -EINVAL;
		}
	}

	return tgt_wifi_radar_command(pdev, params);
}

