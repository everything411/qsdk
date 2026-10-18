/*
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifdef CONFIG_SAWF
#include <dp_sawf.h>
#include <wlan_sawf.h>
#include <cdp_txrx_sawf.h>
#include <qca_sawf_if.h>
#ifdef WLAN_SUPPORT_SCS
#include <qca_scs_if.h>
#endif
uint16_t qca_sawf_get_msduq(struct net_device *netdev, uint8_t *peer_mac,
			    uint32_t service_id)
{
	osif_dev  *osdev;

	if (!wlan_service_id_valid(service_id) ||
	    !wlan_service_id_configured(service_id)) {
	   return DP_SAWF_PEER_Q_INVALID;
	}

	osdev = ath_netdev_priv(netdev);
	if (!osdev) {
		qdf_err("Invalid osdev");
		return DP_SAWF_PEER_Q_INVALID;
	}

#ifdef WLAN_FEATURE_11BE_MLO
	if (osdev->dev_type == OSIF_NETDEV_TYPE_MLO) {
		struct osif_mldev *mldev;

		mldev = ath_netdev_priv(netdev);
		if (!mldev) {
			qdf_err("Invalid mldev");
			return DP_SAWF_PEER_Q_INVALID;
		}

		osdev = osifp_peer_find_hash_find_osdev(mldev, peer_mac);
		if (!osdev) {
			qdf_err("Invalid link osdev");
			return DP_SAWF_PEER_Q_INVALID;
		}

		netdev = osdev->netdev;
	}
#endif

	return dp_sawf_get_msduq(netdev, peer_mac, service_id);
}

/* qca_sawf_get_vdev() - Fetch vdev from netdev
 *
 * @netdev : Netdevice
 * @mac_addr: MAC address
 *
 * Return: Pointer to struct wlan_objmgr_vdev
 */
static struct wlan_objmgr_vdev *qca_sawf_get_vdev(struct net_device *netdev,
						  uint8_t *mac_addr)
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

/* qca_sawf_get_default_msduq() - Return default msdu queue_id
 *
 * @netdev : Netdevice
 * @peer_mac : Destination peer mac address
 * @service_id : Service class id
 * @rule_id : Rule id
 *
 * Return: 16 bits msdu queue_id
 */
#ifdef WLAN_SUPPORT_SCS

#define SVC_ID_TO_QUEUE_ID(svc_id) (svc_id - SAWF_SCS_SVC_CLASS_MIN)

static uint16_t qca_sawf_get_default_msduq(struct net_device *netdev,
					   uint8_t *peer_mac,
					   uint32_t service_id,
					   uint32_t rule_id)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	uint16_t queue_id = DP_SAWF_PEER_Q_INVALID;

	vdev = qca_sawf_get_vdev(netdev, peer_mac);
	if (vdev) {
		psoc = wlan_vdev_get_psoc(vdev);

		/**
		 * When device is operating in WDS_EXT mode, then mac address
		 * is also populated in the rule which makes the rule peer
		 * specific and hence there is not need of rule check against
		 * the peer, else rule check has to be against the peer when
		 * operating in non WDS_EXT mode.
		 */
		if (psoc &&
		    wlan_psoc_nif_feat_cap_get(psoc, WLAN_SOC_F_WDS_EXTENDED)) {
			queue_id = SVC_ID_TO_QUEUE_ID(service_id);
		} else {
			if (qca_scs_peer_lookup_n_rule_match(rule_id,
							     peer_mac))
				queue_id = SVC_ID_TO_QUEUE_ID(service_id);
		}
	}

	return queue_id;
}
#else
static uint16_t qca_sawf_get_default_msduq(struct net_device *netdev,
					   uint8_t *peer_mac,
					   uint32_t service_id,
					   uint32_t rule_id)
{
	return DP_SAWF_PEER_Q_INVALID;
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO_3_LINK_TX
uint32_t qca_sawf_get_peer_msduq(struct net_device *netdev, uint8_t *peer_mac,
				 uint32_t dscp_pcp, bool pcp)
{
	osif_dev *osdev;
	struct net_device *dev = netdev;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	ol_txrx_soc_handle soc_txrx_handle;

	if (!netdev->ieee80211_ptr)
		return DP_SAWF_PEER_Q_INVALID;

	vdev = qca_sawf_get_vdev(netdev, peer_mac);
	if (!vdev)
		return DP_SAWF_PEER_Q_INVALID;
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return DP_SAWF_PEER_Q_INVALID;

	if (!cfg_get(psoc, CFG_TGT_MLO_3_LINK_TX))
		return DP_SAWF_PEER_Q_INVALID;

	soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

	/* Check enable config else return DP_SAWF_PEER_Q_INVALID */

	osdev = ath_netdev_priv(netdev);
	if (!osdev) {
		qdf_err("Invalid osdev");
		return DP_SAWF_PEER_Q_INVALID;
	}

#ifdef WLAN_FEATURE_11BE_MLO
	if (osdev->dev_type == OSIF_NETDEV_TYPE_MLO) {
		struct osif_mldev *mldev;

		mldev = ath_netdev_priv(netdev);
		if (!mldev) {
			qdf_err("Invalid mldev");
			return DP_SAWF_PEER_Q_INVALID;
		}

		osdev = osifp_peer_find_hash_find_osdev(mldev, peer_mac);
		if (!osdev) {
			qdf_err("Invalid link osdev");
			return DP_SAWF_PEER_Q_INVALID;
		}

		dev = osdev->netdev;
	}
#endif
	return cdp_sawf_get_peer_msduq(soc_txrx_handle, dev,
				       peer_mac, dscp_pcp, pcp);
}

static inline
void qca_sawf_3_link_peer_dl_flow_count(struct net_device *netdev, uint8_t *mac_addr,
					uint32_t mark_metadata)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	uint16_t peer_id = HTT_INVALID_PEER;
	ol_txrx_soc_handle soc_txrx_handle;
	osif_dev *osdev = NULL;

	if (!netdev->ieee80211_ptr)
		return;

	vdev = qca_sawf_get_vdev(netdev, mac_addr);
	if (!vdev)
		return;
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return;

	if (!cfg_get(psoc, CFG_TGT_MLO_3_LINK_TX))
		return;

	soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

	osdev = ath_netdev_priv(netdev);
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp = NULL;

		osifp = ath_netdev_priv(netdev);
		peer_id = osifp->peer_id;
	}

	cdp_sawf_3_link_peer_flow_count(soc_txrx_handle, mac_addr, peer_id,
					mark_metadata);
}
#else
uint32_t qca_sawf_get_peer_msduq(struct net_device *netdev, uint8_t *peer_mac,
				 uint32_t dscp_pcp, bool pcp)
{
	return DP_SAWF_PEER_Q_INVALID;
}

static inline
void qca_sawf_3_link_peer_dl_flow_count(struct net_device *netdev, uint8_t *mac_addr,
					uint32_t mark_metadata)
{ }
#endif /* WLAN_FEATURE_11BE_MLO_3_LINK_SUPPORT */

uint16_t qca_sawf_get_msduq_v2(struct net_device *netdev, uint8_t *peer_mac,
			       uint32_t service_id, uint32_t dscp_pcp,
			       uint32_t rule_id, uint8_t sawf_rule_type,
			       bool pcp)
{
	if (!netdev->ieee80211_ptr)
		return DP_SAWF_PEER_Q_INVALID;

	/* Return default queue_id in case of valid SAWF_SCS */
	if (wlan_service_id_scs_valid(sawf_rule_type, service_id)) {
		return qca_sawf_get_default_msduq(netdev, peer_mac,
						  service_id, rule_id);
	} else if (!wlan_service_id_valid(service_id) ||
		 !wlan_service_id_configured(service_id)) {
		/* For 3-Link MLO Clients TX link management get msduq */
		return qca_sawf_get_peer_msduq(netdev, peer_mac, dscp_pcp, pcp);
	}

	return qca_sawf_get_msduq(netdev, peer_mac, service_id);
}

uint32_t qca_sawf_get_mark_metadata(struct qca_sawf_metadata_param *params)
{
	uint16_t queue_id;
	uint32_t mark_metadata = 0;
	uint32_t dscp_pcp = 0;
	bool pcp = false;

	if (params->valid_flag & QCA_SAWF_PCP_VALID) {
		dscp_pcp = params->pcp;
		pcp = true;
	} else {
		dscp_pcp = params->dscp;
	}

	queue_id =  qca_sawf_get_msduq_v2(params->netdev, params->peer_mac,
					  params->service_id, dscp_pcp,
					  params->rule_id,
					  params->sawf_rule_type, pcp);

	if (queue_id != DP_SAWF_PEER_Q_INVALID) {
		if (params->service_id == QCA_SAWF_SVC_ID_INVALID) {
			DP_SAWF_METADATA_SET(mark_metadata,
					     (queue_id + SAWF_SCS_SVC_CLASS_MIN),
					     queue_id);
		} else {
			DP_SAWF_METADATA_SET(mark_metadata,
					     params->service_id,
					     queue_id);
		}
	} else {
		mark_metadata = DP_SAWF_META_DATA_INVALID;
	}

	return mark_metadata;
}

static inline
void qca_sawf_peer_config_ul(struct net_device *netdev, uint8_t *mac_addr,
			     uint8_t tid, uint32_t service_interval,
			     uint32_t burst_size, uint32_t min_tput,
			     uint32_t max_latency, uint8_t add_or_sub)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	uint16_t peer_id = HTT_INVALID_PEER;
	ol_txrx_soc_handle soc_txrx_handle;
	osif_dev *osdev = NULL;

	if (!netdev->ieee80211_ptr)
		return;

	vdev = qca_sawf_get_vdev(netdev, mac_addr);
	if (!vdev)
		return;
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return;
	soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

	osdev = ath_netdev_priv(netdev);
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp = NULL;

		osifp = ath_netdev_priv(netdev);
		peer_id = osifp->peer_id;
	}

	cdp_sawf_peer_config_ul(soc_txrx_handle,
				mac_addr, tid,
				service_interval, burst_size,
				min_tput, max_latency,
				add_or_sub, peer_id);
}

static inline
void qca_sawf_peer_dl_flow_count(struct net_device *netdev, uint8_t *mac_addr,
				 uint8_t svc_id, uint8_t start_or_stop)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	uint8_t peer_mac_addr[QDF_MAC_ADDR_SIZE];
	uint8_t direction = 1;			/* Down Link*/
	uint16_t peer_id = HTT_INVALID_PEER;
	ol_txrx_soc_handle soc_txrx_handle;
	osif_dev *osdev = NULL;

	if (!netdev->ieee80211_ptr)
		return;

	vdev = qca_sawf_get_vdev(netdev, mac_addr);
	if (!vdev)
		return;
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return;
	soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

	osdev = ath_netdev_priv(netdev);
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp = NULL;

		osifp = ath_netdev_priv(netdev);
		peer_id = osifp->peer_id;
	}

	cdp_sawf_peer_flow_count(soc_txrx_handle, mac_addr,
				 svc_id, direction, start_or_stop,
				 peer_mac_addr, peer_id);
}


void qca_sawf_config_ul(struct net_device *dst_dev, struct net_device *src_dev,
			uint8_t *dst_mac, uint8_t *src_mac,
			uint8_t fw_service_id, uint8_t rv_service_id,
			uint8_t add_or_sub, uint32_t fw_mark_metadata,
			uint32_t rv_mark_metadata)
{
	uint32_t svc_interval = 0, burst_size = 0;
	uint32_t min_tput = 0, max_latency = 0;
	uint8_t tid = 0;

	qdf_nofl_debug("src " QDF_MAC_ADDR_FMT " dst " QDF_MAC_ADDR_FMT
		       " fwd_service_id %u rvrs_service_id %u add_or_sub %u"
		       " fw_mark_metadata 0x%x rv_mark_metadata 0x%x",
		       QDF_MAC_ADDR_REF(src_mac), QDF_MAC_ADDR_REF(dst_mac),
		       fw_service_id, rv_service_id, add_or_sub,
		       fw_mark_metadata, rv_mark_metadata);

	if (QDF_IS_STATUS_SUCCESS(wlan_sawf_get_uplink_params(fw_service_id,
							      &tid,
							      &svc_interval,
							      &burst_size,
							      &min_tput,
							      &max_latency))) {
		if (svc_interval && burst_size)
			qca_sawf_peer_config_ul(src_dev, src_mac, tid,
						svc_interval, burst_size,
						min_tput, max_latency,
						add_or_sub);
	}

	svc_interval = 0;
	burst_size = 0;
	tid = 0;
	if (QDF_IS_STATUS_SUCCESS(wlan_sawf_get_uplink_params(rv_service_id,
							      &tid,
							      &svc_interval,
							      &burst_size,
							      &min_tput,
							      &max_latency))) {
		if (svc_interval && burst_size)
			qca_sawf_peer_config_ul(dst_dev, dst_mac, tid,
						svc_interval, burst_size,
						min_tput, max_latency,
						add_or_sub);
	}

	if (wlan_service_id_valid(fw_service_id))
		qca_sawf_peer_dl_flow_count(dst_dev, dst_mac, fw_service_id,
								add_or_sub);

	if (wlan_service_id_valid(rv_service_id))
		qca_sawf_peer_dl_flow_count(src_dev, src_mac, rv_service_id,
								add_or_sub);
	if (fw_mark_metadata != DP_SAWF_META_DATA_INVALID)
		qca_sawf_3_link_peer_dl_flow_count(dst_dev, dst_mac,
						   fw_mark_metadata);

	if (rv_mark_metadata != DP_SAWF_META_DATA_INVALID)
		qca_sawf_3_link_peer_dl_flow_count(src_dev, src_mac,
						   rv_mark_metadata);
}

void qca_sawf_connection_sync(struct qca_sawf_connection_sync_param *params)
{
	qca_sawf_config_ul(params->dst_dev, params->src_dev, params->dst_mac,
			   params->src_mac, params->fw_service_id,
			   params->rv_service_id, params->start_or_stop,
			   params->fw_mark_metadata, params->rv_mark_metadata);
}
#else

#include "qdf_module.h"
#define DP_SAWF_PEER_Q_INVALID 0xffff
#define DP_SAWF_META_DATA_INVALID 0xffffffff
uint16_t qca_sawf_get_msduq(struct net_device *netdev, uint8_t *peer_mac,
			    uint32_t service_id)
{
	return DP_SAWF_PEER_Q_INVALID;
}

uint16_t qca_sawf_get_msduq_v2(struct net_device *netdev, uint8_t *peer_mac,
			       uint32_t service_id, uint32_t dscp_pcp,
			       uint32_t rule_id, uint8_t sawf_rule_type,
			       bool pcp)
{
	return DP_SAWF_PEER_Q_INVALID;
}

/* Forward declaration */
struct qca_sawf_metadata_param;

uint32_t qca_sawf_get_mark_metadata(struct qca_sawf_metadata_param *params)
{
	return DP_SAWF_META_DATA_INVALID;
}

void qca_sawf_config_ul(struct net_device *dst_dev, struct net_device *src_dev,
			uint8_t *dst_mac, uint8_t *src_mac,
			uint8_t fw_service_id, uint8_t rv_service_id,
			uint8_t add_or_sub, uint32_t fw_mark_metadata,
			uint32_t rv_mark_metadata)
{}

/* Forward declaration */
struct qca_sawf_connection_sync_param;

void qca_sawf_connection_sync(struct qca_sawf_connection_sync_param *params)
{}
#endif

uint16_t qca_sawf_get_msdu_queue(struct net_device *netdev,
				 uint8_t *peer_mac, uint32_t service_id,
				 uint32_t dscp, uint32_t rule_id)
{
	return qca_sawf_get_msduq(netdev, peer_mac, service_id);
}

qdf_export_symbol(qca_sawf_get_msduq);
qdf_export_symbol(qca_sawf_get_msduq_v2);
qdf_export_symbol(qca_sawf_get_mark_metadata);
qdf_export_symbol(qca_sawf_get_msdu_queue);
qdf_export_symbol(qca_sawf_config_ul);
qdf_export_symbol(qca_sawf_connection_sync);
