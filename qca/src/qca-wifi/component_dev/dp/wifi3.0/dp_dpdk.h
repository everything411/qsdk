/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef _DP_DPDK_H_
#define _DP_DPDK_H_

#ifdef WLAN_SUPPORT_DPDK

#include <qdf_types.h>
#include <cdp_txrx_cmn_struct.h>
#include <cdp_txrx_cmn.h>

/*
 * struct dp_dpdk_wlan_peer_info: structre to hold peer info
 * @idx: current index of peer_info array
 * @ev_buf: per peer info buffer pointer
 */
struct dp_dpdk_wlan_peer_info {
	uint8_t idx;
	struct dpdk_wlan_peer_info *ev_buf;
};


/*
 * dp_dpdk_get_ring_info - get dp ring info for dpdk
 * @soc: soc handle
 * @uio_info: pointer to fill dp ring info
 *
 * Return: no. of mappings filled
 */
uint8_t dp_dpdk_get_ring_info(struct cdp_soc_t *soc_hdl,
			      qdf_uio_info_t *uio_info);

/*
 * dp_cfgmgr_get_soc_info - get soc info for dpdk
 * @soc_hdl: soc handle
 * @soc_id: soc id
 * @ev_buf: pointer to fill soc info
 *
 * Return: 0 if info filled successful, error otherwise
 */
int dp_cfgmgr_get_soc_info(struct cdp_soc_t *soc_hdl, uint8_t soc_id,
			   struct dpdk_wlan_soc_info_event *ev_buf);

/*
 * dp_cfgmgr_get_vdev_info - get vdev info of a soc for dpdk
 * @soc: soc handle
 * @soc_id: soc id
 * @ev_buf: pointer to fill vdev info
 *
 * Return: 0 if info filled successful, error otherwise
 */
int dp_cfgmgr_get_vdev_info(struct cdp_soc_t *soc_hdl, uint8_t soc_id,
			    struct dpdk_wlan_vdev_info_event *ev_buf);

/*
 * dp_cfgmgr_get_peer_info - get peer info of a soc for dpdk
 * @soc: soc handle
 * @soc_id: soc id
 * @ev_buf: pointer to fill peer info
 *
 * Return: 0 if info filled successful, error otherwise
 */
int dp_cfgmgr_get_peer_info(struct cdp_soc_t *soc_hdl, uint8_t soc_id,
			    struct dpdk_wlan_peer_info *ev_buf);

/*
 * dp_cfgmgr_get_vdev_create_evt_info - get vdev create info of a soc for dpdk
 * @soc: soc handle
 * @vdev_id: vdev id
 * @ev_buf: pointer to fill vdev info
 *
 * Return: 0 if info filled successful, error otherwise
 */
int dp_cfgmgr_get_vdev_create_evt_info(struct cdp_soc_t *soc, uint8_t vdev_id,
				       struct dpdk_wlan_vdev_create_info *ev_buf);

/*
 * dp_cfgmgr_get_peer_create_evt_info - get peer create info of a soc for dpdk
 * @soc: soc handle
 * @peer_id: peer id
 * @ev_buf: pointer to fill peer info
 *
 * Return: 0 if info filled successful, error otherwise
 */
int dp_cfgmgr_get_peer_create_evt_info(struct cdp_soc_t *soc, uint16_t peer_id,
				       struct dpdk_wlan_peer_create_info *ev_buf);
#endif /* WLAN_SUPPORT_DPDK */
#endif /* _DP_DPDK_H_ */
