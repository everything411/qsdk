/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifdef CONFIG_SAWF
#include "wlan_sawf.h"
#endif

#define QCA_FSE_TID_INVALID 0xff

#ifdef WLAN_SUPPORT_RX_FLOW_TAG
static inline
struct wlan_objmgr_vdev *qca_fse_get_vdev_from_dev(struct net_device *dev)
{
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
		qdf_err("No osdev found for dev %s", dev->name);
		return NULL;
	}

#ifdef WLAN_FEATURE_11BE_MLO
	if (osdev->dev_type == OSIF_NETDEV_TYPE_MLO) {
		mldev = (struct osif_mldev *)osdev;
		primary_chip_id = mldev->primary_chip_id;
		primary_pdev_id = mldev->primary_pdev_id;

		os_linkdev = mldev->link_dev[primary_chip_id][primary_pdev_id];
		if (!os_linkdev) {
			qdf_err("No osdev found for ML dev", dev->name);
			return NULL;
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
			qdf_err("No osdev found for dev %s", dev->name);
			return NULL;
		}

		osdev = parent_osdev;
	}
#endif
	if (osif_is_vap_in_recovery_mode(osdev))
		return NULL;

	vdev = osdev->ctrl_vdev;

	return vdev;
}

static inline
struct wlan_objmgr_psoc *qca_fse_get_psoc_from_dev(struct net_device *dev)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;

	vdev = qca_fse_get_vdev_from_dev(dev);
	if (!vdev) {
		qdf_err("No vdev found for dev %s", dev->name);
		return NULL;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		qdf_err("No pdev found for dev %s", dev->name);
		return NULL;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	return psoc;
}

#ifdef CONFIG_SAWF
QDF_STATUS qca_fse_get_sawf_params(struct net_device *dev,
				   uint8_t *mac_addr,
				   uint32_t svc_id,
				   uint8_t **p_peer_mac_addr,
				   uint8_t *p_tid)
{
	uint8_t *peer_mac_addr = mac_addr;
	uint8_t tid;
	QDF_STATUS status;
	osif_dev *osdev;

	osdev = ath_netdev_priv(dev);

#ifdef QCA_SUPPORT_WDS_EXTENDED
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp;

		osifp = ath_netdev_priv(dev);

		peer_mac_addr = osifp->peer_mac_addr;
	}
#endif

	status = wlan_sawf_get_uplink_params(svc_id, &tid,
					     NULL, NULL, NULL, NULL);

	if (QDF_IS_STATUS_ERROR(status)) {
		return status;
	}

	if (p_peer_mac_addr)
		*p_peer_mac_addr = peer_mac_addr;

	if (p_tid)
		*p_tid = tid;

	return QDF_STATUS_SUCCESS;
}
#endif

bool qca_fse_add_rule(struct qca_fse_flow_info *fse_info)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	ol_txrx_soc_handle soc_txrx_handle;
	uint8_t pdev_id;
	uint8_t tid;
	uint8_t *peer_mac_addr;

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
		vdev = qca_fse_get_vdev_from_dev(fse_info->src_dev);
		if (!vdev)
			return false;
		pdev = wlan_vdev_get_pdev(vdev);
		if (!pdev)
			return false;
		psoc = wlan_pdev_get_psoc(pdev);
		if (!psoc)
			return false;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
		soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

		tid = QCA_FSE_TID_INVALID;
		peer_mac_addr = fse_info->src_mac;

#ifdef CONFIG_SAWF
		qca_fse_get_sawf_params(fse_info->src_dev,
					fse_info->src_mac,
					fse_info->fw_svc_id,
					&peer_mac_addr, &tid);
#endif

		qdf_debug("src dev %s src_mac %pM, fw_svc_id %u -> tid %u peer_mac %pM pdev_id %u",
			  fse_info->src_dev->name, fse_info->src_mac,
			  fse_info->fw_svc_id, tid, peer_mac_addr, pdev_id);

		ret = cdp_fse_flow_add(soc_txrx_handle,
				       fse_info->src_ip, fse_info->src_port,
				       fse_info->dest_ip, fse_info->dest_port,
				       fse_info->protocol, fse_info->version,
				       fse_info->fw_svc_id, tid,
				       peer_mac_addr, pdev_id,
				       fse_info->drop_flow, fse_info->ring_id);
	}

	if (ret != QDF_STATUS_SUCCESS) {
		qdf_debug("%p: Failed to add a rule in FSE", fse_info->src_dev);
		return false;
	}

	if (fse_info->dest_dev->ieee80211_ptr) {
		vdev = qca_fse_get_vdev_from_dev(fse_info->dest_dev);
		if (!vdev)
			return false;
		pdev = wlan_vdev_get_pdev(vdev);
		if (!pdev)
			return false;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

		psoc = wlan_pdev_get_psoc(pdev);

		if (!psoc) {
			if (fse_info->src_dev->ieee80211_ptr) {
				cdp_fse_flow_delete(soc_txrx_handle,
						    fse_info->src_ip,
						    fse_info->src_port,
						    fse_info->dest_ip,
						    fse_info->dest_port,
						    fse_info->protocol,
						    fse_info->version, pdev_id);
			}

			return false;
		}

		soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

		tid = QCA_FSE_TID_INVALID;
		peer_mac_addr = fse_info->dest_mac;
#ifdef CONFIG_SAWF
		qca_fse_get_sawf_params(fse_info->dest_dev,
					fse_info->dest_mac,
					fse_info->rv_svc_id,
					&peer_mac_addr, &tid);
#endif
		qdf_debug("dest dev %s dest_mac %pM, rv_svc_id %u -> tid %u peer_mac %pM pdev_id %u",
			  fse_info->dest_dev->name, fse_info->dest_mac,
			  fse_info->rv_svc_id, tid, peer_mac_addr, pdev_id);

		ret = cdp_fse_flow_add(soc_txrx_handle,
				       fse_info->dest_ip, fse_info->dest_port,
				       fse_info->src_ip, fse_info->src_port,
				       fse_info->protocol, fse_info->version,
				       fse_info->rv_svc_id, tid,
				       peer_mac_addr, pdev_id,
				       fse_info->drop_flow, fse_info->ring_id);
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
					    fse_info->protocol, fse_info->version,
					    pdev_id);
		}

		qdf_debug("%p: Failed to add a rule in FSE", fse_info->dest_dev);
		return false;
	}

	return true;
}

bool qca_fse_delete_rule(struct qca_fse_flow_info *fse_info)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_vdev *vdev;
	ol_txrx_soc_handle soc_txrx_handle;
	QDF_STATUS fw_ret = QDF_STATUS_SUCCESS;
	QDF_STATUS rv_ret = QDF_STATUS_SUCCESS;
	uint8_t pdev_id;

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
		vdev = qca_fse_get_vdev_from_dev(fse_info->src_dev);
		if (!vdev)
			return false;
		pdev = wlan_vdev_get_pdev(vdev);
		if (!pdev)
			return false;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

		psoc = wlan_pdev_get_psoc(pdev);

		if (!psoc)
			return false;

		qdf_debug("src dev %s pdev_id %u",
			  fse_info->src_dev->name, pdev_id);

		soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
		fw_ret = cdp_fse_flow_delete(soc_txrx_handle,
					     fse_info->src_ip, fse_info->src_port,
					     fse_info->dest_ip, fse_info->dest_port,
					     fse_info->protocol, fse_info->version,
					     pdev_id);

	}

	if (fse_info->dest_dev->ieee80211_ptr) {
		vdev = qca_fse_get_vdev_from_dev(fse_info->dest_dev);
		if (!vdev)
			return false;
		pdev = wlan_vdev_get_pdev(vdev);
		if (!pdev)
			return false;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

		psoc = wlan_pdev_get_psoc(pdev);
		if (!psoc)
			return false;

		qdf_debug("dst dev %s pdev_id %u",
			  fse_info->dest_dev->name, pdev_id);

		soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
		rv_ret = cdp_fse_flow_delete(soc_txrx_handle,
					      fse_info->dest_ip, fse_info->dest_port,
					      fse_info->src_ip, fse_info->src_port,
					      fse_info->protocol, fse_info->version,
					      pdev_id);
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
