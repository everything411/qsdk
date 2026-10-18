/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.

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

#ifndef _DP_LITE_MON_H_
#define _DP_LITE_MON_H_

#include <dp_types.h>
#include "cdp_txrx_mon_struct.h"
#include <wlan_cmn_ieee80211.h>

#ifdef QCA_SUPPORT_LITE_MONITOR

#define DP_LITE_MON_RTAP_HDR_BITMASK 0x01
#define DP_LITE_MON_META_HDR_BITMASK 0x02

#define DP_LITE_MON_TRACE_DEBUG (3)
#define DP_LITE_MON_DBG_SIGNATURE 0xDEADDA7ADEADDA7A

/**
 * dp_lite_mon_peer - lite mon peer structure
 * @peer_mac: mac addr of peer
 * @rssi: rssi of peer(used only in case of non-assoc peer)
 * @vdev_id: vdev id
 * @peer_list_elem: list element
 */
struct dp_lite_mon_peer {
	union dp_align_mac_addr peer_mac;
	uint8_t rssi;
	uint8_t vdev_id;
	TAILQ_ENTRY(dp_lite_mon_peer) peer_list_elem;
};

/**
 * dp_lite_mon_config - lite mon filter config base structure
 * @enable: lite mon enable/disable
 * @level: MSDU/MPDU/PPDU
 * @mgmt_filter: mgmt pkt filter
 * @ctrl_filter: ctrl pkt filter
 * @data_filter: data pkt filter
 * @len: mgmt/ctrl/data pkt len
 * @fp_enabled: inidicates lite mon fp mode enabled
 * @md_enabled: inidicates lite mon md mode enabled
 * @mo_enabled: inidicates lite mon mo mode enabled
 * @fpmo_enabled: inidicates lite mon fpmo mode enabled
 * @metadata: meta info to be updated
 * @debug: debug info
 * @lite_mon_vdev: output vdev ctx
 * @peer_count: assoc/non-assoc peer count
 * @legacy_filter_enabled: legacy filter currently enabled
 * @peer_list: lite mon peer list
 */
struct dp_lite_mon_config {
	bool enable;
	uint8_t level;
	uint8_t direction;
	uint16_t mgmt_filter[CDP_MON_FRM_FILTER_MODE_MAX];
	uint16_t ctrl_filter[CDP_MON_FRM_FILTER_MODE_MAX];
	uint16_t data_filter[CDP_MON_FRM_FILTER_MODE_MAX];
	uint16_t len[CDP_MON_FRM_TYPE_MAX];
	bool fp_enabled;
	bool md_enabled;
	bool mo_enabled;
	bool fpmo_enabled;
	uint8_t metadata;
	uint8_t debug;
	struct dp_vdev *lite_mon_vdev;
	uint8_t peer_count;
	uint8_t legacy_filter_enabled;
	TAILQ_HEAD(, dp_lite_mon_peer) peer_list;
};

/**
 * dp_lite_mon_tx_config - lite mon tx filter config structure
 * @tx_config: tx filters
 * @lite_mon_tx_lock: lite mon tx config lock
 * @subtype_filtering: Flag to indicate if subtype filtering is needed
 * @sw_peer_filtering: Flag to indicate if sw peer filtering is needed
 */
struct dp_lite_mon_tx_config {
	struct dp_lite_mon_config tx_config;
	/* add tx lite mon specific fields below */
	qdf_spinlock_t lite_mon_tx_lock;
	bool subtype_filtering;
	bool sw_peer_filtering;
};

/**
 * dp_lite_mon_rx_config - lite mon rx filter config structure
 * @rx_config: rx filters
 * @lite_mon_rx_lock: lite mon rx config lock
 * @fp_type_subtype_filter_all: indicates if all type subtype enabled for FP
 */
struct dp_lite_mon_rx_config {
	struct dp_lite_mon_config rx_config;
	/* add rx lite mon specific fields below */
	qdf_spinlock_t lite_mon_rx_lock;
	bool fp_type_subtype_filter_all;
};

static inline int
dp_lite_mon_is_full_len_configured(int len1, int len2, int len3) {
	return (len1 == CDP_LITE_MON_LEN_FULL ||
		len2 == CDP_LITE_MON_LEN_FULL ||
		len3 == CDP_LITE_MON_LEN_FULL);
}

static inline int
dp_lite_mon_get_max_custom_len(int len1, int len2, int len3) {
	/* do not consider full len while
	 * calculating max custom len
	 */
	len1 = (len1 == CDP_LITE_MON_LEN_FULL) ? 0 : len1;
	len2 = (len2 == CDP_LITE_MON_LEN_FULL) ? 0 : len2;
	len3 = (len3 == CDP_LITE_MON_LEN_FULL) ? 0 : len3;
	return ((len1 > len2) ?
		((len1 > len3) ? len1 : len3) :
		((len2 > len3) ? len2 : len3));
}

/**
 * dp_lite_mon_set_config - set lite mon filter config
 * @soc_hdl: soc hdl
 * @mon_config: lite mon filter config
 * @pdev_id: pdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_lite_mon_set_config(struct cdp_soc_t *soc_hdl,
		       struct cdp_lite_mon_filter_config *mon_config,
		       uint8_t pdev_id);

/**
 * dp_lite_mon_get_config - get lite mon filter config
 * @soc_hdl: soc hdl
 * @mon_config: lite mon filter config
 * @pdev_id: pdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_lite_mon_get_config(struct cdp_soc_t *soc_hdl,
		       struct cdp_lite_mon_filter_config *mon_config,
		       uint8_t pdev_id);

/**
 * dp_lite_mon_set_peer_config - set lite mon peer
 * @soc_hdl: soc hdl
 * @peer_config: peer config
 * @pdev_id: pdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_lite_mon_set_peer_config(struct cdp_soc_t *soc_hdl,
			    struct cdp_lite_mon_peer_config *peer_config,
			    uint8_t pdev_id);

/**
 * dp_lite_mon_get_peer_config - get lite mon peers
 * @soc_hdl: soc hdl
 * @peer_info: peer list
 * @pdev_id: pdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_lite_mon_get_peer_config(struct cdp_soc_t *soc_hdl,
			    struct cdp_lite_mon_peer_info *peer_info,
			    uint8_t pdev_id);

/**
 * dp_lite_mon_disable_rx - disables rx lite mon
 * @pdev: dp pdev
 *
 * Return: void
 */
void dp_lite_mon_disable_rx(struct dp_pdev *pdev);

/**
 * dp_lite_mon_is_rx_adv_filter_enable
 *  - checks if advance monitor filter is enabled
 * @pdev: dp_pdev
 *
 * Return: bool
 */
bool dp_lite_mon_is_rx_adv_filter_enable(struct dp_pdev *pdev);
/**
 * dp_lite_mon_is_level_msdu - check if level is msdu
 * @mon_pdev: monitor pdev handle
 *
 * Return: 0 if level is not msdu else return 1
 */
int dp_lite_mon_is_level_msdu(struct dp_mon_pdev *mon_pdev);

/**
 * dp_lite_mon_is_rx_enabled - get lite mon rx enable status
 * @mon_pdev: monitor pdev handle
 *
 * Return: 0 if disabled, 1 if enabled
 */
int dp_lite_mon_is_rx_enabled(struct dp_mon_pdev *mon_pdev);

/**
 * dp_lite_mon_is_tx_enabled - get lite mon tx enable status
 * @mon_pdev: monitor pdev handle
 *
 * Return: 0 if disabled, 1 if enabled
 */
int dp_lite_mon_is_tx_enabled(struct dp_mon_pdev *mon_pdev);

/**
 * dp_lite_mon_is_enabled - get lite mon enable status
 * @soc_hdl: dp soc hdl
 * @pdev_id: pdev id
 * @direction: tx/rx
 *
 * Return: 0 if disabled, 1 if enabled
 */
int
dp_lite_mon_is_enabled(struct cdp_soc_t *soc_hdl,
		       uint8_t pdev_id, uint8_t direction);

/**
 *dp_lite_mon_get_legacy_feature_enabled - get the legacy filter
					currently enabled
 * @soc_hdl: dp soc hdl
 * @pdev_id: pdev id
 * @direction: tx/rx
 *
 * Return: Currently enabled legacy filter
 */
int
dp_lite_mon_get_legacy_feature_enabled(struct cdp_soc_t *soc_hdl,
				       uint8_t pdev_id, uint8_t direction);

/**
 * dp_lite_mon_alloc - alloc lite mon tx/rx config
 * @pdev: dp pdev
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_lite_mon_alloc(struct dp_pdev *pdev);

/**
 * dp_lite_mon_dealloc - free lite mon tx/rx config
 * @pdev: dp pdev
 *
 * Return: void
 */
void
dp_lite_mon_dealloc(struct dp_pdev *pdev);

/**
 * dp_lite_mon_vdev_delete - delete tx/rx lite mon vdev
 * @pdev: dp pdev
 * @vdev: dp vdev being deleted
 *
 * Return: void
 */
void
dp_lite_mon_vdev_delete(struct dp_pdev *pdev, struct dp_vdev *vdev);

/**
 * dp_lite_mon_config_nac_peer - config nac peer and filter
 * @soc_hdl: dp soc hdl
 * @vdev_id: vdev id
 * @cmd: peer cmd
 * @macaddr: peer mac
 *
 * Return: 1 if success, 0 if failure
 */
int
dp_lite_mon_config_nac_peer(struct cdp_soc_t *soc_hdl,
			    uint8_t vdev_id,
			    uint32_t cmd, uint8_t *macaddr);

/**
 * dp_lite_mon_config_nac_rssi_peer - config nac rssi peer
 * @soc_hdl: dp soc hdl
 * @cmd: peer cmd
 * @vdev_id: vdev id
 * @bssid: peer bssid
 * @macaddr: peer mac
 * @chan_num: channel num
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_lite_mon_config_nac_rssi_peer(struct cdp_soc_t *soc_hdl,
				 uint8_t vdev_id,
				 enum cdp_nac_param_cmd cmd,
				 char *bssid, char *macaddr,
				 uint8_t chan_num);

/**
 * dp_lite_mon_get_nac_peer_rssi - get nac peer rssi
 * @soc_hdl: dp soc hdl
 * @vdev_id: vdev id
 * @macaddr: peer mac
 * @rssi: peer rssi
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_lite_mon_get_nac_peer_rssi(struct cdp_soc_t *soc_hdl,
			      uint8_t vdev_id, char *macaddr,
			      uint8_t *rssi);

/**
 * dp_lite_mon_rx_mpdu_process - core lite mon mpdu processing
 * @pdev: pdev context
 * @ppdu_info: ppdu info context
 * @mon_mpdu: mpdu nbuf
 * @mpdu_id: mpdu id
 * @user: user id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_lite_mon_rx_mpdu_process(struct dp_pdev *pdev,
			    struct hal_rx_ppdu_info *ppdu_info,
			    qdf_nbuf_t mon_mpdu, uint16_t mpdu_id,
			    uint8_t user);

/**
 * dp_lite_mon_get_filter_ucast_data - get uncast filter status
 *					for mon 2.0
 * @pdev_handle
 *
 * Return: True/False
 **/
bool
dp_lite_mon_get_filter_ucast_data(struct cdp_pdev *pdev_handle);

/**
 * dp_lite_mon_get_filter_mcast_data - get mcast filter status
 *					for mon 2.0
 * @pdev_handle
 *
 * Return: True/False
 **/
bool
dp_lite_mon_get_filter_mcast_data(struct cdp_pdev *pdev_handle);

/**
 * dp_lite_mon_get_filter_non_data - get non_data filter status
 *					for mon 2.0
 * @pdev_handle
 *
 * Return: True/False
 **/
bool
dp_lite_mon_get_filter_non_data(struct cdp_pdev *pdev_handle);

/**
 * dp_lite_mon_free_peers - Free peers in litemon config
 *
 * @pdev: pdev context
 * @config: Lite monitor config
 *
 * Return: void
 **/
void
dp_lite_mon_free_peers(struct dp_pdev *pdev,
		       struct dp_lite_mon_config *config);

#endif
#endif /* _DP_LITE_MON_H_ */
