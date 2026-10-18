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

#ifndef _WLAN_WIFI_RADAR_PUBLIC_STRUCTS_H_
#define _WLAN_WIFI_RADAR_PUBLIC_STRUCTS_H_

#define HOST_MAX_CHAINS 8

/**
 * enum wifi_radar_pkt_width - wifi radar packet bandwidth
 * @WIFI_RADAR_PKT_WIDTH20: 20 MHz
 * @WIFI_RADAR_PKT_WIDTH40: 40 MHz
 * @WIFI_RADAR_PKT_WIDTH80: 80 MHz
 * @WIFI_RADAR_PKT_WIDTH160: 160 MHz
 * @WIFI_RADAR_PKT_WIDTH320: 320 MHz
 * @WIFI_RADAR_PKT_WIDTH_MAX: Max width
 * @WIFI_RADAR_PKT_WIDTHINVALID: Invalid width
 */
enum wifi_radar_pkt_width {
	WIFI_RADAR_PKT_WIDTH20,
	WIFI_RADAR_PKT_WIDTH40,
	WIFI_RADAR_PKT_WIDTH80,
	WIFI_RADAR_PKT_WIDTH160,
	WIFI_RADAR_PKT_WIDTH320,

	WIFI_RADAR_PKT_WIDTH_MAX,
	WIFI_RADAR_PKT_WIDTHINVALID = 0xff
};

/**
 * enum wifi_radar_command_type - wifi radar command type
 * @WIFI_RADAR_CMD_CAPTURE_STOP: stop capture
 * @WIFI_RADAR_CMD_CAPTURE_START: start capture
 * @WIFI_RADAR_CMD_RX_CAL: start rx cal
 * @WIFI_RADAR_CMD_TX_CAL: start tx cal
 * @WIFI_RADAR_CMD_INVALID: invalid command
 */
enum wifi_radar_command_type {
	WIFI_RADAR_CMD_CAPTURE_STOP,
	WIFI_RADAR_CMD_CAPTURE_START,
	WIFI_RADAR_CMD_RX_CAL,
	WIFI_RADAR_CMD_TX_CAL,

	WIFI_RADAR_CMD_INVALID = 0xff
};

/**
 * struct wifi_radar_cal_info_params - wifi radar rx calibration status params
 * @timestamp: host reat timestamp in ms
 * @wifi_radar_pkt_bw: BW of wifi radar packets used for calibration
 * @channel_bw: channel bandwidth
 * @band_center_freq: band center frequency
 * @cal_num_ltf_tx: num_ltf_tx used for calibration
 * @cal_num_skip_ltf_rx: num_skip_ltf_rx setting used in calibration
 * @cal_num_ltf_accumulation: num_ltf_accumulation setting used in calibration
 * @per_tx_chain_cal_status: calibration status for each tx chain & rx chainmask
 *	combination. 1 indicating pass and 0 indicating failure.
 */
struct wifi_radar_cal_info_params {
	uint64_t timestamp;
	uint32_t wifi_radar_pkt_bw;
	uint32_t channel_bw;
	uint32_t band_center_freq;
	uint32_t cal_num_ltf_tx;
	uint32_t cal_num_skip_ltf_rx;
	uint32_t cal_num_ltf_accumulation;
	uint8_t per_tx_chain_cal_status[HOST_MAX_CHAINS][HOST_MAX_CHAINS];
};

/**
 * struct wifi_radar_wlanconfig_params - wifi radar configurations from tool
 * @bandwidth: BW requested for capture/cal
 * @periodicity: interval in ms requested for capture
 * @tx_chainmask: tx chain mask to be used for capture/cal
 * @rx_chainmask: rx chain mask to be used for capture/cal
 * @num_ltf_tx: num_ltf_tx config to be used for capture/cal
 * @num_skip_ltf_rx: num_skip_ltf_rx config to be used for capture/cal
 * @num_ltf_accumulation: num_ltf_accumulation config to be used for capture/cal
 */
struct wifi_radar_wlanconfig_params {
	enum wifi_radar_pkt_width bandwidth;
	uint32_t periodicity;
	uint32_t tx_chainmask;
	uint32_t rx_chainmask;
	uint32_t num_ltf_tx;
	uint32_t num_skip_ltf_rx;
	uint32_t num_ltf_accumulation;
};
#endif /* _WLAN_WIFI_RADAR_PUBLIC_STRUCTS_H_ */
