/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _TARGET_IF_WIFI_RADAR_H_
#define _TARGET_IF_WIFI_RADAR_H_

#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>

/* currently used dma buf size here. increase it as we add host meta data */
#define STREAMFS_WIFI_RADAR_MAX_SUBBUF 61464
#define STREAMFS_WIFI_RADAR_NUM_SUBBUF 127

#define MAX_NUM_LTF_TX 255
#define MAX_NUM_SKIP_LTF_RX 21
#define MAX_NUM_LTF_ACCUMULATION 7
#define DEFAULT_ISOLATED_CHAIN_PAIR_BIT 0

struct wifi_radar_ucode_header {
	uint32_t  ucode_header_tag :8,
		  ucode_header_len_32bit_dwords :8,
		  preamble_type    :1,// vht or he
		  nss_num          :3,
		  nss_rx           :2,
		  tx_pkt_bw        :3,
		  upload_bw        :4,
		  reserved_1       :3;
	uint32_t  tx_gain_index_1  :16,
		  tx_gain_index_2  :8,
		  pri20_loc        :8;
	uint32_t  rx_gain_index_ch0 :8,
		  rx_gain_index_ch1 :8,
		  rx_gain_index_ch2 :8,
		  rx_gain_index_ch3 :8;
	uint32_t  phase_diff;
	uint16_t  capture_len;
	uint16_t  tx_chain_mask     :4,
		  reserved_2_a      :4,
		  reserved_2_b      :8;
	uint32_t  reserved_3;
};

/**
 * target_if_wifi_radar_tx_ops_register() - register tx ops
 * @tx_ops: lmac tx ops to which wifi_radar tx ops to be registered
 *
 * Return: none
 */
void target_if_wifi_radar_tx_ops_register(struct wlan_lmac_if_tx_ops *tx_ops);

/**
 * wifi_radar_init_pdev() - Inits wifi radar pdev and registers necessary
 *	handlers.
 * @psoc: pointer to psoc object
 * @pdev: pointer to pdev object
 *
 * Return: Registration status for necessary handlers
 */
QDF_STATUS wifi_radar_init_pdev(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_pdev *pdev);

/**
 * wifi_radar_deinit_pdev() - De-inits corresponding pdev and handlers.
 * @psoc: pointer to psoc object
 * @pdev: pointer to pdev object
 *
 * Return: De-registration status for necessary handlers
 */
QDF_STATUS wifi_radar_deinit_pdev(struct wlan_objmgr_psoc *psoc,
				  struct wlan_objmgr_pdev *pdev);

/**
 * target_if_wifi_radar_set_support() - Function to set cfr support
 * @psoc: pointer to psoc object
 * @value: value to be set
 */
void target_if_wifi_radar_set_support(struct wlan_objmgr_psoc *psoc,
				      uint8_t value);
#endif /* _TARGET_IF_WIFI_RADAR_H_ */

