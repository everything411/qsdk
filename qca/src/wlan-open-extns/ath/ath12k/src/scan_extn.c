// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include "../wmi.h"
#include "ath12k_cmn_extn.h"
#include "../core.h"

/**
 * ath12k_wmi_prepare_scan_req_extn - Prepare scan request extension data
 * @ar: Pointer to the ath12k radio structure
 * @arg: Pointer to the scan request parameters
 * @ssid: Pointer to the scan request SSID
 * @ssid_len: Length of the scan request SSID
 *
 * Store scan request SSID information for the scan vif and update strict
 * passive scan parameter from userspace radio configuration when applicable.
 */
void ath12k_wmi_prepare_scan_req_extn(struct ath12k *ar,
				      struct ath12k_wmi_scan_req_arg *arg,
				      const u8 *ssid,
				      u8 ssid_len)
{
	struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
	struct ieee80211_channel *channel;

	arvif = ar->scan.arvif;
	if (arvif) {
		ahvif = arvif->ahvif;
		if (!ssid)
			ssid_len = 0;
		else if (ssid_len > IEEE80211_MAX_SSID_LEN)
			ssid_len = IEEE80211_MAX_SSID_LEN;

		spin_lock_bh(&ahvif->ath12k_vif_extn.data_lock);
		ahvif->ath12k_vif_extn.ssid_len = ssid_len;
		memset(ahvif->ath12k_vif_extn.ssid, 0,
		       sizeof(ahvif->ath12k_vif_extn.ssid));
		if (ssid_len)
			memcpy(ahvif->ath12k_vif_extn.ssid, ssid, ssid_len);
		spin_unlock_bh(&ahvif->ath12k_vif_extn.data_lock);
	}

	if (!arg->chan_list.num_chan || !arg->chan_list.chan)
		return;

	channel = ieee80211_get_channel(ar->ah->hw->wiphy,
					arg->chan_list.chan[0].freq);

	if (channel && channel->band == NL80211_BAND_5GHZ)
		arg->scan_f_strict_passive_pch =
			ar->radio_cfg.radio_cfg_extn.scan_f_strict_passive_pch;
}

/**
 * ath12k_wmi_start_scan_init_extn - Initialize scan offset in scan args
 * @ar: Pointer to the ath12k radio structure
 * @arg: Pointer to the scan parameter
 *
 * This function initializes the scan_start_offset field in the scan request
 * argument
 */
void ath12k_wmi_start_scan_init_extn(struct ath12k *ar,
				     struct ath12k_wmi_scan_req_arg *arg)
{
	arg->extn.scan_start_offset = 0;
}

/**
 * ath12k_wmi_send_scan_start_cmd_extn - Copy scan offset into WMI command
 * @ar: Pointer to the ath12k radio structure
 * @cmd: Pointer to the WMI start scan command
 * @arg: Pointer to the scan parameter
 *
 * This function copies the scan_start_offset from the scan request argument
 * into the WMI start scan command.
 */
void ath12k_wmi_send_scan_start_cmd_extn(struct ath12k *ar,
					 struct wmi_start_scan_cmd *cmd,
					 struct ath12k_wmi_scan_req_arg *arg)
{
	cmd->scan_start_offset = cpu_to_le32(arg->extn.scan_start_offset);
}
