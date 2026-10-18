/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <dp_rx_tag.h>
#include "qca_fse_if.h"
#include "cdp_txrx_fse.h"
#include "qdf_trace.h"

#ifdef WLAN_SUPPORT_RX_FLOW_TAG
static inline
struct wlan_objmgr_psoc *qca_fse_get_psoc_from_dev(struct net_device *dev)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	osif_dev *osdev;
#ifdef WLAN_FEATURE_11BE_MLO
	osif_dev  *os_linkdev = NULL;
	struct osif_mldev *mldev = NULL;
	uint8_t primary_chip_id;
	uint8_t primary_pdev_id;
#endif

	osdev = ath_netdev_priv(dev);
	if (!osdev) {
		qdf_err("%p: No osdev found for dev", dev);
		return false;
	}

#ifdef WLAN_FEATURE_11BE_MLO
	if (osdev->dev_type == OSIF_NETDEV_TYPE_MLO) {
		mldev = (struct osif_mldev *)osdev;
		primary_chip_id = mldev->primary_chip_id;
		primary_pdev_id = mldev->primary_pdev_id;

		os_linkdev = mldev->link_dev[primary_chip_id][primary_pdev_id];
		if (!os_linkdev) {
			qdf_err("%p: No osdev found for ML dev", dev);
			return false;
		}
		osdev = os_linkdev;
	}
#endif
#ifdef QCA_SUPPORT_WDS_EXTENDED
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp;
		osif_dev *parent_osdev;

		osifp = ath_netdev_priv(dev);
		parent_osdev = osif_wds_ext_get_parent_osif(osifp);
		if (!parent_osdev) {
			qdf_err("%p: No osdev found for dev", dev);
			return false;
		}

		osdev = parent_osdev;
	}
#endif
	vdev = osdev->ctrl_vdev;
	if (!vdev) {
		qdf_err("%p: No vdev found for osdev", osdev);
		return false;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		qdf_err("%p: No pdev found for vdev", vdev);
		return false;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	return psoc;
}

bool qca_fse_add_rule(struct qca_fse_flow_info *fse_info)
{
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	ol_txrx_soc_handle soc_txrx_handle;

	if (!fse_info->src_dev || !fse_info->dest_dev) {
		qdf_debug("Unable to find dev for FSE rule push");
		return false;
	}

	/*
	 * Return in case of non wlan traffic.
	 */
	if (!fse_info->src_dev->ieee80211_ptr && !fse_info->dest_dev->ieee80211_ptr) {
		qdf_debug("Not a wlan traffic for FSE rule push");
		return false;
	}

	/*
	 * Based on src_dev / dest_dev is a VAP, get the 5 tuple info
	 * to configure the FSE UL flow. If source is VAP, then
	 * 5 tuple info is in UL direction, so straightaway
	 * add a rule. If dest is VAP, it is 5 tuple info has to be
	 * reversed for adding a rule.
	 */
	if (fse_info->src_dev->ieee80211_ptr) {
		psoc = qca_fse_get_psoc_from_dev(fse_info->src_dev);
		if (!psoc)
			return false;

		soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
		ret = cdp_fse_flow_add(soc_txrx_handle,
				       fse_info->src_ip, fse_info->src_port,
				       fse_info->dest_ip, fse_info->dest_port,
				       fse_info->protocol, fse_info->version);
	}

	if (ret != QDF_STATUS_SUCCESS) {
		qdf_debug("%p: Failed to add a rule in FSE", fse_info->src_dev);
		return false;
	}

	if (fse_info->dest_dev->ieee80211_ptr) {
		psoc = qca_fse_get_psoc_from_dev(fse_info->dest_dev);
		if (!psoc) {
			if (fse_info->src_dev->ieee80211_ptr) {
				cdp_fse_flow_delete(soc_txrx_handle,
						    fse_info->src_ip,
						    fse_info->src_port,
						    fse_info->dest_ip,
						    fse_info->dest_port,
						    fse_info->protocol,
						    fse_info->version);
			}

			return false;
		}

		soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
		ret = cdp_fse_flow_add(soc_txrx_handle,
				       fse_info->dest_ip, fse_info->dest_port,
				       fse_info->src_ip, fse_info->src_port,
				       fse_info->protocol, fse_info->version);
	}

	if (ret != QDF_STATUS_SUCCESS) {

		/*
		 * In case of inter VAP flow, if one direction fails
		 * to configure FSE rule, delete the rule added for
		 * the other direction as well.
		 */
		if (fse_info->src_dev->ieee80211_ptr) {
			cdp_fse_flow_delete(soc_txrx_handle,
					    fse_info->src_ip, fse_info->src_port,
					    fse_info->dest_ip, fse_info->dest_port,
					    fse_info->protocol, fse_info->version);
		}

		qdf_debug("%p: Failed to add a rule in FSE", fse_info->dest_dev);
		return false;
	}

	return true;
}

bool qca_fse_delete_rule(struct qca_fse_flow_info *fse_info)
{
	struct wlan_objmgr_psoc *psoc;
	ol_txrx_soc_handle soc_txrx_handle;
	QDF_STATUS fw_ret = QDF_STATUS_SUCCESS;
	QDF_STATUS rv_ret = QDF_STATUS_SUCCESS;

	if (!fse_info->src_dev || !fse_info->dest_dev) {
		qdf_debug("Unable to find dev for FSE rule delete");
		return false;
	}

	/*
	 * Return in case of non wlan traffic.
	 */
	if (!fse_info->src_dev->ieee80211_ptr && !fse_info->dest_dev->ieee80211_ptr) {
		qdf_debug("Not a wlan traffic for FSE rule delete");
		return false;
	}

	/*
	 * Based on src_dev / dest_dev is a VAP, get the 5 tuple info
	 * to delete the FSE UL flow. If source is VAP, then
	 * 5 tuple info is in UL direction, so straightaway
	 * delete a rule. If dest is VAP, it is 5 tuple info has to be
	 * reversed to delete a rule.
	 */
	if (fse_info->src_dev->ieee80211_ptr) {
		psoc = qca_fse_get_psoc_from_dev(fse_info->src_dev);
		if (!psoc)
			return false;

		soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
		fw_ret = cdp_fse_flow_delete(soc_txrx_handle,
					     fse_info->src_ip, fse_info->src_port,
					     fse_info->dest_ip, fse_info->dest_port,
					     fse_info->protocol, fse_info->version);
	}

	if (fse_info->dest_dev->ieee80211_ptr) {
		psoc = qca_fse_get_psoc_from_dev(fse_info->dest_dev);
		if (!psoc)
			return false;

		soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
		rv_ret = cdp_fse_flow_delete(soc_txrx_handle,
					      fse_info->dest_ip, fse_info->dest_port,
					      fse_info->src_ip, fse_info->src_port,
					      fse_info->protocol, fse_info->version);
	}

	if (fw_ret == QDF_STATUS_SUCCESS && rv_ret == QDF_STATUS_SUCCESS)
		return true;

	return false;
}

#else
bool qca_fse_add_rule(struct qca_fse_flow_info *fse_info)
{
	return false;
}

bool qca_fse_delete_rule(struct qca_fse_flow_info *fse_info)
{
	return false;
}
#endif /* WLAN_SUPPORT_RX_FLOW_TAG */

qdf_export_symbol(qca_fse_add_rule);
qdf_export_symbol(qca_fse_delete_rule);
