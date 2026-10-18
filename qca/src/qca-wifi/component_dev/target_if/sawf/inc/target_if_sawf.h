/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.

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

#ifndef _TARGET_IF_SAWF_
#define _TARGET_IF_SAWF_

#include <wlan_objmgr_pdev_obj.h>
#include <wlan_lmac_if_def.h>

/*
 * target_if_sawf_svc_create_send() - Send SAWF service class create command
 *
 * @pdev: WLAN PDEV object
 * @svc_params: Service Class Parameters
 *
 * Return QDF_STATUS
 */
QDF_STATUS
target_if_sawf_svc_create_send(struct wlan_objmgr_pdev *pdev, void *svc_params);

/*
 * target_if_sawf_svc_disable_send() - Send SAWF service class disable command
 *
 * @pdev: WLAN PDEV object
 * @svc_params: Service Class Parameters
 *
 * Return QDF_STATUS
 */
QDF_STATUS
target_if_sawf_svc_disable_send(struct wlan_objmgr_pdev *pdev,
				void *svc_params);

/*
 * target_if_sawf_ul_svc_update_params() - Send command to update UL parameters
 *
 * @pdev: WLAN PDEV object
 * @vdev_id: VDEV id
 * @peer_mac: Peer MAC address
 * @ac: Access Category
 * @add_or_sub: Addition or Subtraction of parameters
 * @svc_params: Service Class Parameters
 *
 * Return QDF_STATUS
 */
QDF_STATUS
target_if_sawf_ul_svc_update_params(struct wlan_objmgr_pdev *pdev,
				    uint8_t vdev_id, uint8_t *peer_mac,
				    uint8_t ac, uint8_t add_or_sub,
				    void *svc_params);

/*
 * target_if_sawf_update_ul_params() - Send command to update UL parameters
 *
 * @pdev: WLAN PDEV object
 * @vdev_id: VDEV id
 * @peer_mac: Peer MAC address
 * @ac: Access Category
 * @add_or_sub: Addition or Subtraction of parameters
 * @svc_params: Service Class Parameters
 *
 * Return QDF_STATUS
 */
QDF_STATUS
target_if_sawf_update_ul_params(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
				uint8_t *peer_mac, uint8_t tid, uint8_t ac,
				uint32_t service_interval, uint32_t burst_size,
				uint32_t min_tput, uint32_t max_latency,
				uint8_t add_or_sub);
#endif /* _TARGET_IF_SAWF_ */
