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

#ifndef _WMI_UNIFIED_WIFI_RADAR_PARAM_H_
#define _WMI_UNIFIED_WIFI_RADAR_PARAM_H_

/**
 * enum wmi_host_wifi_radar_cmd - wifi radar command type
 * @wmi_host_wifi_radar_capture_disable: disable wifi radar capture
 * @wmi_host_wifi_radar_capture_enable: enable wifi radar capture
 * @wmi_host_wifi_radar_rx_cal: trigger wifi radar rx cal
 * @wmi_host_wifi_radar_tx_cal: trigger wifi radar tx gain cal
 * @wmi_host_wifi_radar_cmd_max: max cmd id
 */
enum wmi_host_wifi_radar_cmd {
	wmi_host_wifi_radar_capture_disable,
	wmi_host_wifi_radar_capture_enable,
	wmi_host_wifi_radar_rx_cal,
	wmi_host_wifi_radar_tx_cal,
	wmi_host_wifi_radar_cmd_max = 0xff,
};

/**
 * struct wmi_wifi_radar_command_params - wifi radar command parameters
 * @pdev_id: pdev id of the pdev object
 * @cmd_type: type of command <enum wmi_host_wifi_radar_cmd>
 * @bandwidth: bandwidth in which capture/calibration to start
 * @periodicity: period or interval in ms to use for capture command
 * @tx_chainmask: txchain on which wifi radar capture/cal to take place
 * @rx_chainmask: rx chain mask indicating chains involved in capture/cal
 * @num_ltf_tx:
 * @num_skip_ltf_rx:
 * @num_ltf_accumulation:
 */
struct wmi_wifi_radar_command_params {
	uint32_t pdev_id;
	uint32_t cmd_type;
	uint32_t bandwidth;
	uint32_t periodicity;
	uint32_t tx_chainmask;
	uint32_t rx_chainmask;
	uint32_t num_ltf_tx;
	uint32_t num_skip_ltf_rx;
	uint32_t num_ltf_accumulation;
};
#endif /* _WMI_UNIFIED_WIFI_RADAR_PARAM_H_ */

