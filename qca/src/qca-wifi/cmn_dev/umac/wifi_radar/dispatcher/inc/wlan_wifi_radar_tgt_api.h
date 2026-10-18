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

#ifndef _WLAN_WIFI_RADAR_TGT_API_H_
#define _WLAN_WIFI_RADAR_TGT_API_H_

#include <wlan_objmgr_peer_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_cmn.h>
#include <qdf_types.h>
#include <wlan_wifi_radar_utils_api.h>

/* tgt layer has APIs in application, to access functions in target
 * through tx_ops.
 */

/**
 * tgt_wifi_radar_init_pdev() - API that registers wifi radar to handlers.
 * @pdev: pointer to pdev_object
 *
 * Return: success/failure of init
 */
QDF_STATUS tgt_wifi_radar_init_pdev(struct wlan_objmgr_pdev *pdev);

/**
 * tgt_wifi_radar_deinit_pdev() - API that de-registers wifi radar to handlers.
 * @pdev: pointer to pdev_object
 *
 * Return: success/failure of de-init
 */
QDF_STATUS tgt_wifi_radar_deinit_pdev(struct wlan_objmgr_pdev *pdev);

/**
 * tgt_wifi_radar_support_set() - API to set wifi radar support
 * @psoc: pointer to psoc_object
 * @value: value to be set
 */
void tgt_wifi_radar_support_set(struct wlan_objmgr_psoc *psoc, uint32_t value);

/**
 * tgt_wifi_radar_info_send() - API to send wifi radar info
 * @pdev: pointer to pdev_object
 * @head: pointer to wifi radar info head
 * @hlen: head len
 * @data: pointer to wifi radar info data
 * @dlen: data len
 * @tail: pointer to wifi radar info tail
 * @tlen: tail len
 *
 * Return: success/failure of wifi radar info send
 */
uint32_t tgt_wifi_radar_info_send(struct wlan_objmgr_pdev *pdev, void *head,
				  size_t hlen, void *data, size_t dlen,
				  void *tail, size_t tlen);

/**
 * tgt_wifi_radar_validate_period() - API to validate wifi radar periodicity
 * @psoc: pointer to the psoc object
 * @period: period value to validate
 *
 * Return: success/failure of periodicity validation
 */
int tgt_wifi_radar_validate_period(struct wlan_objmgr_psoc *psoc,
				   u_int32_t period);

/**
 * tgt_wifi_radar_validate_chainmask() - API to validate chain mask
 * @pwr: pointer to wifi radar pdev private object
 * @tx_chainmask: tx chain mask input from user for capture command
 * @rx_chainmask: rx chain mask input from user for capture command
 *
 * Return: success/failure of chainmask validation
 */
int tgt_wifi_radar_validate_chainmask(struct pdev_wifi_radar *pwr,
				      u_int32_t tx_chainmask,
				      u_int32_t rx_chainmask);

/**
 * tgt_wifi_radar_command() - API to send wifi radar commands to target
 * @pdev: pointer to the pdev object
 * @params: pointer to the configuration parameters
 *
 * Return: success/failure of command
 */
int tgt_wifi_radar_command(struct wlan_objmgr_pdev *pdev,
			   struct wifi_radar_command_params *params);
#endif /* _WLAN_WIFI_RADAR_TGT_API_H_ */
