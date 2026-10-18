/*
 * Copyright (c) 2021, 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _WLAN_WIFI_RADAR_UTILS_API_H_
#define _WLAN_WIFI_RADAR_UTILS_API_H_

#include <wlan_objmgr_cmn.h>
#include <qdf_streamfs.h>

#define wifi_radar_alert(format, args...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_WIFI_RADAR, format, ## args)

#define wifi_radar_err(format, args...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_WIFI_RADAR, format, ## args)

#define wifi_radar_warn(format, args...) \
	QDF_TRACE_WARN(QDF_MODULE_ID_WIFI_RADAR, format, ## args)

#define wifi_radar_info(format, args...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_WIFI_RADAR, format, ## args)

#define wifi_radar_debug(format, args...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_WIFI_RADAR, format, ## args)

#define DBR_EVENT_TIMEOUT_IN_MS_WIFI_RADAR 1
#define DBR_NUM_RESP_PER_EVENT_WIFI_RADAR 1

#define HOST_MAX_CHAINS 8

#define MAX_WIFI_RADAR_PRD (10 * 60 * 1000)   /* 10 minutes */
#define MIN_WIFI_RADAR_PRD 10 /* 10ms */

enum wifi_radar_data_version {
	WR_DATA_VERSION_NONE,
	WR_DATA_VERSION_1,
	WR_DATA_VERSION_MAX = 0xFF,
};

enum wifi_radar_platform_type {
	WR_PLATFORM_TYPE_NONE,
	WR_PLATFORM_TYPE_MIPS,
	WR_PLATFORM_TYPE_ARM,
	WR_PLATFFORM_TYPE_MAX = 0xFF,
};

enum wifi_radar_radio_type {
	WIFI_RADAR_RADIO_WAIKIKI = 0,
	WIFI_RADAR_RADIO_MAX = 0xFF,
};

/* struct wifi_radar_header - structure holding wifi radar metadata
 * @start_magic_num: magic number from which wifi radar capture starts
 * @vendorid: vendor id
 * @metadata_version: header format version info
 * @data_version: wifi radar data format version info
 * @chip_type: chip type which is defined in enum wifi_radar_radio_type
 * @pltform_type: platform type
 * @metadata_len: length of the metadata
 * @host_real_ts: timestamp at host when metadata prepared
 * @fw_timestamp_us: timestamp at FW during capture
 * @phy_mode: phy mode of the channel at the time of capture
 * @prim20_chan: frequency (in MHz) of the primary 20 MHz channel
 * @center_freq1: Center frequency 1 in MHz
 * @center_freq2: Center frequency 2 in MHz
 * @tx_chain_mask: tx chain mask
 * @rx_chain_mask: rx chain mask
 * @num_ltf_tx: number of LTFs sent for capture
 * @num_skip_ltf_rx: number of LTFs skipped in rx
 * @num_ltf_accumulation: number of LTFs used for accumulation
 * @cal_num_ltf_tx: Number of LTF configured in the WiFi Radar Tx packet during
 *	calibration
 * @cal_num_skip_ltf_rx: Number of LTF skipped during Rx of the calibration pkt
 * @cal_num_ltf_accumulation: Number of LTF accumulated during Rx of the
 *	calibration packet
 * @cal_band_center_freq: center freq used in wifi radar calibration
 * @per_chain_cal_status: per chain calibration status
 */
struct wifi_radar_header {
	u_int32_t start_magic_num;
	u_int32_t vendorid;
	u_int8_t metadata_version;
	u_int8_t data_version;
	u_int8_t chip_type;
	u_int8_t pltform_type;
	u_int32_t metadata_len;
	u_int64_t host_real_ts;
	u_int32_t fw_timestamp_us;
	u_int8_t phy_mode;
	u_int16_t prim20_chan;
	u_int16_t center_freq1;
	u_int16_t center_freq2;
	u_int16_t tx_chain_mask;
	u_int16_t rx_chain_mask;
	u_int32_t num_ltf_tx;
	u_int32_t num_skip_ltf_rx;
	u_int32_t num_ltf_accumulation;
	u_int32_t cal_num_ltf_tx;
	u_int32_t cal_num_skip_ltf_rx;
	u_int32_t cal_num_ltf_accumulation;
	u_int16_t cal_band_center_freq;
	uint8_t per_chain_cal_status[HOST_MAX_CHAINS][HOST_MAX_CHAINS];
} __attribute__ ((__packed__));

/**
 * struct psoc_wifi_radar - private psoc object for WiFi Radar
 * @psoc_obj: pointer to psoc object
 * @is_wifi_radar_capable: flag to determine if wifi radar is enabled or not
 */
struct psoc_wifi_radar {
	struct wlan_objmgr_psoc *psoc_obj;
	uint8_t is_wifi_radar_capable;
};

/**
 * struct pdev_wifi_radar - private pdev object for WiFi Radar
 * @pdev_obj: pointer to pdev object
 * @is_wifi_radar_capable: flag to determine if wifi radar is enabled or not
 * @chan_ptr: Channel in relayfs
 * @dir_ptr: Parent directory of relayfs file
 * @num_subbufs: No. of sub-buffers used in relayfs
 * @subbuf_size: Size of sub-buffer used in relayfs
 * @chip_type: chip type which is defined in enum wifi_radar_radio_type
 * @header: header table used to store dbr events meta data & host header
 *	associated with each wifi radar capture
 * @header_entries: number of entries in @header
 * @wifi_radar_pkt_bw: Packet bandwidth of WiFi Radar packet used in calibration
 * @channel_bw: Channel bandwidth
 * @band_center_freq: Channel Center frequency in MHz used in calibration
 * @cal_num_ltf_tx: Number of LTF configured in the WiFi Radar Tx packet during
 *	calibration
 * @cal_num_skip_ltf_rx: Number of LTF skipped during Rx of the calibration
 *	packet
 * @cal_num_ltf_accumulation: Number of LTF accumulated during Rx of the
 *	calibration packet
 * @per_chain_comb_cal_status: each tx rx chain combination calibration status
 * @host_timestamp_of_cal_status_evt: host timestamp at the reception of cal
 *	status event
 * @cal_status_lock: Lock to protect access to wifi radar cal status params
 * @cal_status_lock_initialized: Check status lock initialized or not
 * @max_num_ltf_tx: target specific max allowed num_ltf_tx
 * @max_num_skip_ltf_rx: target specific max allowed num_skip_ltf_rx
 * @max_num_ltf_accumulation: target specific max allowed num_ltf_accumulation.
 * @max_num_rx_chain: target specific max allowed rx chains
 * @best_isolated_chain_pair_sel: target specific best_isolated_chain_pair_sel
 * @header_lock: Lock to protect the access to headers
 * @header_lock_initialized: Check header lock initialized or not
 */
struct pdev_wifi_radar {
	struct wlan_objmgr_pdev *pdev_obj;
	uint8_t is_wifi_radar_capable;
	qdf_streamfs_chan_t chan_ptr;
	qdf_dentry_t dir_ptr;
	uint32_t num_subbufs;
	uint32_t subbuf_size;
	uint8_t chip_type;
	struct wifi_radar_header **header;
	uint32_t header_entries;
	uint32_t wifi_radar_pkt_bw;
	uint32_t channel_bw;
	uint32_t band_center_freq;
	uint32_t cal_num_ltf_tx;
	uint32_t cal_num_skip_ltf_rx;
	uint32_t cal_num_ltf_accumulation;
	bool per_chain_comb_cal_status[HOST_MAX_CHAINS][HOST_MAX_CHAINS];
	uint64_t host_timestamp_of_cal_status_evt;
	qdf_spinlock_t cal_status_lock;
	bool cal_status_lock_initialized;
	uint32_t max_num_ltf_tx;
	uint32_t max_num_skip_ltf_rx;
	uint32_t max_num_ltf_accumulation;
	uint32_t max_num_rx_chain;
	uint32_t best_isolated_chain_pair_sel;
	qdf_spinlock_t header_lock;
	bool header_lock_initialized;
};

/**
 * struct peer_wifi_radar - private peer object for WiFi Radar
 * @peer_obj: pointer to peer_obj
 */
struct peer_wifi_radar {
	struct wlan_objmgr_peer *peer_obj;
};

/**
 * struct wifi_radar_command_params - params to be passed for capture/cal cmd
 * @cmd_type: type of command indicating cal_type/capture
 * @bandwidth: pkt bandwidth to be used for cal/capture
 * @periodicity: capture interval in case of capture command
 * @tx_chainmask: tx chain mask to be used for cal/capture
 * @rx_chainmask: rx chain mask to be used for cal/capture
 * @num_ltf_tx: num_ltf_tx config to be used for cal/capture
 * @num_skip_ltf_rx: num_skip_ltf_rx config to be used for cal/capture
 * @num_ltf_accumulation: num_ltf_accumulation config to be used foe cal/capture
 */
struct wifi_radar_command_params {
	uint8_t cmd_type;
	uint8_t bandwidth;
	uint32_t periodicity;
	uint32_t tx_chainmask;
	uint32_t rx_chainmask;
	uint32_t num_ltf_tx;
	uint32_t num_skip_ltf_rx;
	uint32_t num_ltf_accumulation;
};

/**
 * wlan_wifi_radar_init() - Global init for wifi radar.
 *
 * Return: status of global init pass/fail
 */
QDF_STATUS wlan_wifi_radar_init(void);

/**
 * wlan_wifi_radar_deinit() - Global de-init for wifi radar.
 *
 * Return: status of global de-init pass/fail
 */
QDF_STATUS wlan_wifi_radar_deinit(void);

/**
 * wlan_wifi_radar_pdev_open() - pdev_open function for wifi radar.
 * @pdev: pointer to pdev object
 *
 * Return: status of pdev_open pass/fail
 */
QDF_STATUS wlan_wifi_radar_pdev_open(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_wifi_radar_pdev_close() - pdev_close function for wifi radar.
 * @pdev: pointer to pdev object
 *
 * Return: status of pdev_close pass/fail
 */
QDF_STATUS wlan_wifi_radar_pdev_close(struct wlan_objmgr_pdev *pdev);

/**
 * wifi_radar_initialize_pdev() - pdev_initialize function for wifi radar.
 * @pdev: pointer to pdev object
 *
 * Return: status of pdev_initialize pass/fail
 */
QDF_STATUS wifi_radar_initialize_pdev(struct wlan_objmgr_pdev *pdev);

/**
 * wifi_radar_deinitialize_pdev() - pdev_deinitialize function for wifi radar.
 * @pdev: pointer to pdev object
 *
 * Return: status of pdev_deinitialize pass/fail
 */
QDF_STATUS wifi_radar_deinitialize_pdev(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_wifi_radar_is_feature_disabled() - Check if wifi radarfeature
 * is disabled
 * @pdev: the physical device object.
 *
 * Return : true if wifi radar is disabled, else false.
 */
bool wlan_wifi_radar_is_feature_disabled(struct wlan_objmgr_pdev *pdev);

#endif
