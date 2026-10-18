/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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
#include <qdf_module.h>
#include <qdf_nbuf.h>
#include <qdf_trace.h>
#include <ol_if_athvar.h>
#include "qca_mscs_if.h"
#include "qca_mesh_latency_if.h"

extern ol_ath_soc_softc_t *ol_global_soc[GLOBAL_SOC_SIZE];


/* qca_mesh_get_vdev() - Fetch vdev from netdev
 *
 * @netdev : Netdevice
 * @mac_addr : MAC address
 *
 * Return: Pointer to struct wlan_objmgr_vdev
 */
static struct wlan_objmgr_vdev *
qca_mesh_get_vdev(struct net_device *netdev, uint8_t *mac_addr)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	osif_dev *osdev = NULL;

	osdev = ath_netdev_priv(netdev);

#ifdef QCA_SUPPORT_WDS_EXTENDED
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp = NULL;
		osif_dev *parent_osdev = NULL;

		osifp = ath_netdev_priv(netdev);
		parent_osdev = osif_wds_ext_get_parent_osif(osifp);
		if (!parent_osdev)
			return NULL;

		osdev = parent_osdev;
	}
#endif
#ifdef WLAN_FEATURE_11BE_MLO
	if (osdev->dev_type == OSIF_NETDEV_TYPE_MLO) {
		struct osif_mldev *mldev;

		mldev = ath_netdev_priv(netdev);
		if (!mldev) {
			qdf_err("Invalid mldev");
			return NULL;
		}

		osdev = osifp_peer_find_hash_find_osdev(mldev, mac_addr);
		if (!osdev) {
			qdf_err("Invalid link osdev");
			return NULL;
		}
	}
#endif

	vdev = osdev->ctrl_vdev;
	return vdev;
}

/**
 * qca_mesh_latency_update_peer_parameter() - Update peer mesh latency parameter
 * @dest_mac - destination mac address
 * @service_interval_dl - Service interval associated with the tid in DL
 * @burst_size_dl - Burst size corresponding to the tid in DL
 * @service_interval_ul- Service interval associated with the tid in UL
 * @burst_size_u - Burst size corresponding to the tid in UL
 * @priority - priority/tid associated with the peer
 * @add_or_sub -  ADD/SUB command to WMI
 *
 * Return: QDF_STATUS_SUCCESS for successful peer lookup
 */
int qca_mesh_latency_update_peer_parameter(uint8_t *dest_mac,
				    uint32_t service_interval_dl, uint32_t burst_size_dl,
				    uint32_t service_interval_ul, uint32_t burst_size_ul,
					uint16_t priority, uint8_t add_or_sub)
{
	uint8_t i = 0;
	ol_ath_soc_softc_t *soc = NULL;
	ol_txrx_soc_handle soc_txrx_handle = NULL;

	/*
	 * Loop over all the attached soc for peer lookup
	 * with given mac address
	 */
	for (i = 0; i < GLOBAL_SOC_SIZE; i++) {
		if (!ol_global_soc[i])
			continue;

		soc = ol_global_soc[i];
		soc_txrx_handle = wlan_psoc_get_dp_handle(soc->psoc_obj);
		cdp_mesh_latency_update_peer_parameter(soc_txrx_handle,
				dest_mac, service_interval_dl, burst_size_dl,
				service_interval_ul, burst_size_ul,
				priority, add_or_sub);

	}

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(qca_mesh_latency_update_peer_parameter);

int qca_mesh_latency_update_peer_parameter_v2(
		struct qca_mesh_latency_update_peer_param *params)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	ol_txrx_soc_handle soc_txrx_handle;
	osif_dev *osdev = NULL;

	if (!params->dst_dev->ieee80211_ptr)
		return QDF_STATUS_E_FAILURE;

	vdev = qca_mesh_get_vdev(params->dst_dev, params->dest_mac);
	if (!vdev)
		return QDF_STATUS_E_FAILURE;
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;
	soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

	osdev = ath_netdev_priv(params->dst_dev);
#ifdef QCA_SUPPORT_WDS_EXTENDED
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp = NULL;

		osifp = ath_netdev_priv(params->dst_dev);
		params->dest_mac = osifp->peer_mac_addr;
	}
#endif
	return cdp_mesh_latency_update_peer_parameter(soc_txrx_handle,
				params->dest_mac, params->service_interval_dl,
				params->burst_size_dl,
				params->service_interval_ul,
				params->burst_size_ul, params->priority,
				params->add_or_sub);
}

qdf_export_symbol(qca_mesh_latency_update_peer_parameter_v2);
