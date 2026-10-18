/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
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
#include "qca_mscs.h"
#include "qca_mscs_if.h"

static int mscs_soc_attached_count;
extern ol_ath_soc_softc_t *ol_global_soc[GLOBAL_SOC_SIZE];

/**
 * qca_mscs_peer_lookup_status - mscs lookup status
 * @QCA_MSCS_PEER_LOOKUP_STATUS_ALLOW_MSCS_QOS_TAG_UPDATE
 * @QCA_MSCS_PEER_LOOKUP_STATUS_DENY_QOS_TAG_UPDATE
 * @QCA_MSCS_PEER_LOOKUP_STATUS_ALLOW_INVALID_QOS_TAG_UPDATE
 * @QCA_MSCS_PEER_LOOKUP_STATUS_PEER_NOT_FOUND
 */
enum qca_mscs_peer_lookup_status {
	QCA_MSCS_PEER_LOOKUP_STATUS_ALLOW_MSCS_QOS_TAG_UPDATE,
	QCA_MSCS_PEER_LOOKUP_STATUS_ALLOW_INVALID_QOS_TAG_UPDATE,
	QCA_MSCS_PEER_LOOKUP_STATUS_DENY_QOS_TAG_UPDATE,
	QCA_MSCS_PEER_LOOKUP_STATUS_PEER_NOT_FOUND,
};


/* qca_mscs_get_vdev() - Fetch vdev from netdev
 *
 * @netdev : Netdevice
 * @mac_addr : MAC address
 *
 * Return: Pointer to struct wlan_objmgr_vdev
 */
static struct wlan_objmgr_vdev *
qca_mscs_get_vdev(struct net_device *netdev, uint8_t *mac_addr)
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
 * qca_mscs_peer_lookup_n_get_priority() - Find MSCS enabled peer and priority
 * @src_mac - src mac address to be used for peer lookup
 * @nbuf - network buffer
 * @priority - priority/tid to be updated
 *
 * Return: QDF_STATUS_SUCCESS for successful peer lookup
 */
int qca_mscs_peer_lookup_n_get_priority(uint8_t *src_mac, uint8_t *dst_mac, struct sk_buff *skb)
{
	uint8_t i = 0;
	ol_ath_soc_softc_t *soc = NULL;
	int status = QDF_STATUS_SUCCESS;
	ol_txrx_soc_handle soc_txrx_handle = NULL;
	qdf_nbuf_t nbuf = (qdf_nbuf_t)skb;

	/*
	 * Loop over all the attached soc for peer lookup
	 * with given src mac address
	 */
	for (i = 0; i < GLOBAL_SOC_SIZE; i++) {
		if (!ol_global_soc[i])
			continue;

		soc = ol_global_soc[i];
		soc_txrx_handle = wlan_psoc_get_dp_handle(soc->psoc_obj);
		status = cdp_mscs_peer_lookup_n_get_priority(soc_txrx_handle,
				src_mac, dst_mac, nbuf);
		/*
		 * wifi peer is found with this mac address.there can
		 * be 3 possiblities -
		 * 1. peer has no active MSCS session.
		 * 2. peer has active MSCS session but priority not valid.
		 * 3. peer has active MSCS session and priority is valid.
		 * return the status to ECM classifier.
		 */
		if (status != QCA_MSCS_PEER_LOOKUP_STATUS_PEER_NOT_FOUND)
			return status;
		/*
		 * no wifi peer exists in this soc with given src mac address
		 * iterate over next soc
		 */
	}

	if (i == GLOBAL_SOC_SIZE)
		/*
		 * No wlan peer is found in any of attached
		 * soc with given mac address
		 * return the status to ECM classifier.
		 */
		status = QDF_STATUS_E_FAILURE;

	return status;
}

qdf_export_symbol(qca_mscs_peer_lookup_n_get_priority);

int qca_mscs_peer_lookup_n_get_priority_v2(
		struct qca_mscs_get_priority_param *params)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	ol_txrx_soc_handle soc_txrx_handle;
	osif_dev *osdev = NULL;

	if (!params->src_dev->ieee80211_ptr)
		return QDF_STATUS_E_FAILURE;

	vdev = qca_mscs_get_vdev(params->src_dev, params->src_mac);
	if (!vdev)
		return QDF_STATUS_E_FAILURE;
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;
	soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

	osdev = ath_netdev_priv(params->src_dev);
#ifdef QCA_SUPPORT_WDS_EXTENDED
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp = NULL;

		osifp = ath_netdev_priv(params->src_dev);
		params->src_mac = osifp->peer_mac_addr;
	}
#endif
	return cdp_mscs_peer_lookup_n_get_priority(soc_txrx_handle,
			params->src_mac, params->dst_mac, params->skb);
}

qdf_export_symbol(qca_mscs_peer_lookup_n_get_priority_v2);

/**
 * qca_mscs_module_init() - Initialize the MSCS module
 * @soc - Pointer to soc getting attached
 *
 * Return: void
 */

void qca_mscs_module_init(ol_ath_soc_softc_t *soc)
{
	/* Check if soc max init count is reached */
	if (mscs_soc_attached_count >= GLOBAL_SOC_SIZE)
		return;

	QDF_TRACE(QDF_MODULE_ID_MSCS, QDF_TRACE_LEVEL_INFO,
			     FL("\n****QCA MSCS Initialization Done**** SoC %pK"), soc);

	mscs_soc_attached_count++;
}

qdf_export_symbol(qca_mscs_module_init);

/**
 * qca_mscs_module_deinit() - De-Initialize MSCS module
 * @soc - Pointer to soc getting detached
 *
 * Return: void
 */
void qca_mscs_module_deinit(ol_ath_soc_softc_t *soc)
{
	if (!soc)
		return;

	/*
	 * All soc are detached by now
	 */
	if (mscs_soc_attached_count < 0)
		return;

	QDF_TRACE(QDF_MODULE_ID_MSCS, QDF_TRACE_LEVEL_INFO,
			FL("\n****QCA MSCS De-Initialization Done**** SoC %pK"), soc);
	mscs_soc_attached_count--;
}

qdf_export_symbol(qca_mscs_module_deinit);
