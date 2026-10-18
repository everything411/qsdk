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

/**
 * DOC: osif_vdev_mgr_util.h
 *
 * This header file maintains declarations of osif APIs corresponding to vdev
 * manager.
 */

#ifndef __OSIF_VDEV_MGR_UTIL_H
#define __OSIF_VDEV_MGR_UTIL_H

#ifdef ENABLE_CFG80211_BACKPORTS_MLO
#include <wlan_mlo_mgr_main.h>
#include <wlan_osif_request_manager.h>
#include <osif_private.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_mlo_mgr_public_structs.h>
#endif

/**
 * struct osif_vdev_mgr_ops - VDEV mgr legacy callbacks
 * @osif_vdev_mgr_set_mac_addr_response: Callback to indicate set MAC address
 *                                       response from FW
 * @osif_vdev_mgr_send_scan_done_complete_cb: send scan done indication to
 * upper layer
 * @osif_vdev_mgr_get_p2p_wdev_cb: Get p2p wdev ptr
 */
struct osif_vdev_mgr_ops {
#if defined(WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE) || defined(ENABLE_CFG80211_BACKPORTS_MLO)
	void (*osif_vdev_mgr_set_mac_addr_response)(uint8_t vdev_id,
						    uint8_t resp_status);
#endif
	void (*osif_vdev_mgr_send_scan_done_complete_cb)(uint8_t vdev_id);
	struct wireless_dev *(*osif_vdev_mgr_get_p2p_wdev_cb)(void);
};

#ifdef ENABLE_CFG80211_BACKPORTS_MLO
static void osif_set_mac_addr_event_cb(uint8_t vdev_id, uint8_t status)
{
	struct wlan_mlo_dev_context *mld_cur;
	struct wlan_mlo_dev_context *mld_next;
	qdf_list_t *ml_list;
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t idx, count = 0;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct osif_request *req = NULL;
	struct mac_addr_set_priv *priv;

	qdf_err("entering osif_set_mac_addr_event_cb");
	if (!mlo_mgr_ctx) {
		qdf_err("mlo_mgr_ctx is null");
		return;
	}

	ml_link_lock_acquire(mlo_mgr_ctx);
	ml_list = &mlo_mgr_ctx->ml_dev_list;
	/* Get first mld context */
	mld_cur = wlan_mlo_list_peek_head(ml_list);

	while (mld_cur) {
		/* get next mld node */
		mlo_dev_lock_acquire(mld_cur);
		if (mld_cur->sta_ctx) {
			count = QDF_ARRAY_SIZE(mld_cur->wlan_vdev_list);
			for (idx = 0; idx < count; idx++) {
				vdev = mld_cur->wlan_vdev_list[idx];
				if (!vdev)
					continue;

				if (wlan_vdev_get_id(vdev) == vdev_id) {
					req = osif_request_get(vdev->vdev_mlme.set_mac_addr_req_ctx);
					if (!req)
						continue;

					mlo_dev_lock_release(mld_cur);
					goto found_req;;
				}

			}
		}
		mld_next = wlan_mlo_get_next_mld_ctx(ml_list, mld_cur);
		mlo_dev_lock_release(mld_cur);
		mld_cur = mld_next;
	}

found_req:
	if (!req) {
		qdf_err("Obsolete request for VDEV ID:%d", vdev_id);
		ml_link_lock_release(mlo_mgr_ctx);
		return;
	}

	priv = (struct mac_addr_set_priv *)osif_request_priv(req);

	priv->fw_resp_status = status;
	osif_request_complete(req);

	osif_request_put(req);
	ml_link_lock_release(mlo_mgr_ctx);
}

extern struct osif_vdev_mgr_ops osif_vdev_mgrlegacy_ops;
#endif

/**
 * osif_vdev_mgr_set_legacy_cb() - Sets legacy callbacks to osif
 * @osif_legacy_ops:  Function pointer to legacy ops structure
 *
 * API to set legacy callbacks to osif
 * Context: Any context.
 *
 * Return: void
 */
void osif_vdev_mgr_set_legacy_cb(struct osif_vdev_mgr_ops *osif_legacy_ops);

/**
 * osif_vdev_mgr_reset_legacy_cb() - Resets legacy callbacks to osif
 *
 * API to reset legacy callbacks to osif
 * Context: Any context.
 *
 * Return: void
 */
void osif_vdev_mgr_reset_legacy_cb(void);

/**
 * osif_vdev_mgr_register_cb() - Register VDEV manager legacy callbacks
 *
 * API to register legavy VDEV manager callbacks
 *
 * Return: QDF_STATUS
 */
QDF_STATUS osif_vdev_mgr_register_cb(void);

/**
 * osif_vdev_mgr_get_p2p_wdev() - Get P2P-device wdev
 *
 * Return: p2p-device wdev ptr
 */
struct wireless_dev *osif_vdev_mgr_get_p2p_wdev(void);
#endif /* __OSIF_CM_UTIL_H */
