/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <qdf_status.h>
#include "target_if_dp_mon.h"
#include <init_deinit_lmac.h>

#ifdef QCA_SUPPORT_LITE_MONITOR
int target_if_config_lite_mon_peer(struct cdp_ctrl_objmgr_psoc *psoc, uint8_t pdev_id,
				   uint8_t vdev_id, enum cdp_nac_param_cmd cmd,
				   uint8_t *peer_mac)
{
	void *pdev_wmi_handle;
	struct set_neighbour_rx_params param = {0};
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct wlan_objmgr_pdev *pdev =
		wlan_objmgr_get_pdev_by_id((struct wlan_objmgr_psoc *)psoc,
					   pdev_id, WLAN_VDEV_TARGET_IF_ID);

	if (!pdev) {
		target_if_err("pdev with id %d is NULL", pdev_id);
		return status;
	}

	pdev_wmi_handle = lmac_get_pdev_wmi_handle(pdev);
	if (!pdev_wmi_handle) {
		target_if_err("WMI handle is NULL");
		goto end;
	}

	param.vdev_id = vdev_id;
	param.idx = 1;
	param.action = cmd;
	param.type = CDP_LITE_MON_PEER_MAC_TYPE_CLIENT;
	status = wmi_unified_vdev_set_neighbour_rx_cmd_send(pdev_wmi_handle, peer_mac, &param);

end:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_VDEV_TARGET_IF_ID);

	return status;
}

int target_if_config_lite_mon_tx_peer(struct cdp_ctrl_objmgr_psoc *psoc,
				      uint8_t pdev_id, uint8_t vdev_id,
				      enum cdp_tx_filter_action cmd,
				      uint8_t *peer_mac)
{
	void *pdev_wmi_handle;
	struct set_tx_peer_filter param = {0};
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct wlan_objmgr_pdev *pdev =
		wlan_objmgr_get_pdev_by_id((struct wlan_objmgr_psoc *)psoc,
					   pdev_id, WLAN_VDEV_TARGET_IF_ID);

	if (!pdev) {
		target_if_err("pdev with id %d is NULL", pdev_id);
		return status;
	}

	pdev_wmi_handle = lmac_get_pdev_wmi_handle(pdev);
	if (!pdev_wmi_handle) {
		target_if_err("WMI handle is NULL");
		goto end;
	}

	param.vdev_id = vdev_id;
	param.idx = 1;
	param.action = cmd;
	status = wmi_unified_peer_filter_set_tx_cmd_send(pdev_wmi_handle,
							 peer_mac, &param);

end:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_VDEV_TARGET_IF_ID);

	return status;
}
#endif
