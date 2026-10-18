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
#include <osif_private.h>
#include <dp_sawf.h>
#include <wlan_sawf.h>
#include <cdp_txrx_sawf.h>
#include <qca_sawf_if.h>
#ifdef WLAN_SUPPORT_SCS
#include <qca_scs_if.h>
#endif
#include <cdp_txrx_ctrl.h>
#include <wlan_mlo_mgr_ap.h>
#include <wlan_mlo_mgr_peer.h>

#define SAWF_UPLINK 0
#define SAWF_DOWNLINK 1

uint8_t channel_list_2g[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
uint8_t channel_list_5g_lo[] = {36, 40, 44, 48, 52, 56, 60, 64};
uint8_t channel_list_5g_hi[] = {100, 104, 108, 112, 116, 120, 124, 128, 132,
				136, 140, 144, 149, 153, 157, 161, 165, 169,
				173, 177};
uint8_t channel_list_5g[] = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108,
			     112, 116, 120, 124, 128, 132, 136, 140, 144,
			     149, 153, 157, 161, 165, 169, 173, 177};
uint8_t channel_list_6g[] = {1, 2, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57,
			     61, 65, 69, 73, 77, 81, 85, 89, 93, 97, 101, 105, 109, 113,
			     117, 121, 125, 129, 133, 137, 141, 145, 149, 153, 157, 161,
			     165, 169, 173, 177, 181, 185, 189, 193, 197, 201, 205, 209,
			     213, 217, 221, 225, 229, 233};

uint8_t bw_list_2g[] = {SAWF_BW_20MHZ, SAWF_BW_40MHZ};
uint8_t bw_list_5g[] = {SAWF_BW_20MHZ, SAWF_BW_40MHZ, SAWF_BW_80MHZ,
			SAWF_BW_160MHZ, SAWF_BW_80_80MHZ};
uint8_t bw_list_6g[] = {SAWF_BW_20MHZ, SAWF_BW_40MHZ, SAWF_BW_80MHZ,
			SAWF_BW_160MHZ, SAWF_BW_80_80MHZ, SAWF_BW_320MHZ};

struct wifi_params {
	bool band;
	uint8_t channel;
	uint8_t bandwidth;
};

bool wlan_dessired_ssid_found(struct wlan_objmgr_vdev *vdev, u_int8_t *ssid, u_int8_t ssid_len);

/* qca_sawf_get_vdev() - Fetch vdev from netdev
 *
 * @netdev : Netdevice
 * @mac_addr: MAC address
 * @peer_mac: Actual peer address
 *
 * Return: Pointer to struct wlan_objmgr_vdev
 */
static struct wlan_objmgr_vdev *
qca_sawf_get_vdev(struct net_device *netdev,
		  uint8_t *mac_addr, uint8_t *peer_mac)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	osif_dev *osdev = NULL;

	osdev = ath_netdev_priv(netdev);

	if (peer_mac && mac_addr)
		qdf_mem_copy(peer_mac, mac_addr, QDF_MAC_ADDR_SIZE);
#ifdef QCA_SUPPORT_WDS_EXTENDED
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp = NULL;
		osif_dev *parent_osdev = NULL;

		osifp = ath_netdev_priv(netdev);
		parent_osdev = osif_wds_ext_get_parent_osif(osifp);

		if (peer_mac)
			qdf_mem_copy(peer_mac, osifp->peer_mac_addr,
				     QDF_MAC_ADDR_SIZE);

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

		if (mldev->wdev.iftype == NL80211_IFTYPE_AP)
			osdev = osifp_peer_find_hash_find_osdev(mldev, mac_addr);
		else
			osdev = osif_sta_mlo_find_osdev(mldev);
		if (!osdev) {
			sawf_debug("Invalid link osdev");
			return NULL;
		}
	}
#endif
	if (osif_is_vap_in_recovery_mode(osdev))
		return NULL;

	vdev = osdev->ctrl_vdev;

	return vdev;
}

static struct wlan_objmgr_vdev *
qca_sawf_get_ref_vdev(struct net_device *netdev,
		      uint8_t *mac_addr, uint8_t *peer_mac,
		      wlan_objmgr_ref_dbgid id)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	vdev = qca_sawf_get_vdev(netdev, mac_addr, peer_mac);
	if (!vdev)
		return NULL;

	status = wlan_objmgr_vdev_try_get_ref(vdev, id);
	if (QDF_IS_STATUS_ERROR(status))
		return NULL;

	return vdev;
}

uint16_t qca_sawf_get_msduq(struct net_device *netdev, uint8_t *peer_mac,
			    uint32_t service_id)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;
	ol_txrx_soc_handle soc_txrx_handle;
	osif_dev  *osdev;
#ifdef QCA_SUPPORT_WDS_EXTENDED
	osif_peer_dev *osifp;
#endif
	uint8_t vdev_id;
	uint16_t queue_id;

	if (!wlan_service_id_valid(service_id) ||
	    !wlan_service_id_configured(service_id))
		return DP_SAWF_PEER_Q_INVALID;

	vdev = qca_sawf_get_ref_vdev(netdev, peer_mac, NULL, WLAN_SAWF_ID);
	if (!vdev) {
		sawf_debug("Invalid vdev");
		return DP_SAWF_PEER_Q_INVALID;
	}

	vdev_id = wlan_vdev_get_id(vdev);

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		sawf_err("Invalid psoc");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
		return DP_SAWF_PEER_Q_INVALID;
	}

	soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

	osdev = ath_netdev_priv(netdev);
	if (!osdev) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
		sawf_err("Invalid osdev");
		return DP_SAWF_PEER_Q_INVALID;
	}

#ifdef QCA_SUPPORT_WDS_EXTENDED
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osifp = ath_netdev_priv(netdev);

		peer_mac = osifp->peer_mac_addr;
	}
#endif

	queue_id = dp_sawf_get_msduq(soc_txrx_handle, vdev_id,
				     peer_mac, service_id);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);

	return queue_id;
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

	vdev = qca_sawf_get_ref_vdev(netdev, peer_mac, NULL, WLAN_SAWF_ID);
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

		wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
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
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	ol_txrx_soc_handle soc_txrx_handle;
	cdp_config_param_type val = {0};
	uint8_t vdev_id;
	uint16_t queue_id;

	if (!netdev || !netdev->ieee80211_ptr) {
		sawf_err("Invalid netdev");
		return DP_SAWF_PEER_Q_INVALID;
	}

	vdev = qca_sawf_get_ref_vdev(netdev, peer_mac, NULL, WLAN_SAWF_ID);
	if (!vdev)
		return DP_SAWF_PEER_Q_INVALID;

	vdev_id = wlan_vdev_get_id(vdev);

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev) || !vdev->mlo_dev_ctx) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
		return DP_SAWF_PEER_Q_INVALID;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
		return DP_SAWF_PEER_Q_INVALID;
	}

	if (!cfg_get(psoc, CFG_TGT_MLO_3_LINK_TX)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
		return DP_SAWF_PEER_Q_INVALID;
	}

	soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
		if (vdev->mlo_dev_ctx->mlo_max_recom_simult_links != 3) {
			wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
			return DP_SAWF_PEER_Q_INVALID;
		}
	} else if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
		cdp_txrx_get_vdev_param(soc_txrx_handle,
					wlan_vdev_get_id(vdev),
					CDP_ENABLE_WDS, &val);
		if (!val.cdp_vdev_param_wds) {
			wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
			return DP_SAWF_PEER_Q_INVALID;
		}
	}
	/* Check enable config else return DP_SAWF_PEER_Q_INVALID */

	osdev = ath_netdev_priv(netdev);
	if (!osdev) {
		qdf_err("Invalid osdev");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
		return DP_SAWF_PEER_Q_INVALID;
	}

#ifdef QCA_SUPPORT_WDS_EXTENDED
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp;

		osifp = ath_netdev_priv(netdev);

		peer_mac = osifp->peer_mac_addr;

		goto process_peer;
	}
#endif

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
		struct wlan_objmgr_peer *bss_peer;

		bss_peer = wlan_vdev_get_bsspeer(vdev);

		peer_mac = bss_peer->macaddr;
	}

process_peer:
	queue_id = cdp_sawf_get_peer_msduq(soc_txrx_handle, vdev_id,
					   peer_mac, dscp_pcp, pcp);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);

	return queue_id;
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
	struct qdf_mac_addr macaddr = {0};

	if (!netdev || !netdev->ieee80211_ptr)
		return;

	vdev = qca_sawf_get_ref_vdev(netdev, mac_addr, NULL, WLAN_SAWF_ID);
	if (!vdev)
		return;
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
		return;
	}

	if (!cfg_get(psoc, CFG_TGT_MLO_3_LINK_TX)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
		return;
	}

	qdf_mem_copy(macaddr.bytes, mac_addr, QDF_MAC_ADDR_SIZE);

	soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

	osdev = ath_netdev_priv(netdev);
#ifdef QCA_SUPPORT_WDS_EXTENDED
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp = NULL;

		osifp = ath_netdev_priv(netdev);
		peer_id = osifp->peer_id;
	}
#endif
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)
		wlan_vdev_get_bss_peer_mac(vdev, &macaddr);

	cdp_sawf_3_link_peer_flow_count(soc_txrx_handle, macaddr.bytes, peer_id,
					mark_metadata);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
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

	sawf_debug("dev %s mac_addr " QDF_MAC_ADDR_FMT " svc_id %u rule_id %u rule_type %u -> metadata 0x%x",
		   params->netdev->name,
		   QDF_MAC_ADDR_REF(params->peer_mac),
		   params->service_id,
		   params->rule_id, params->sawf_rule_type, mark_metadata);

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

	vdev = qca_sawf_get_ref_vdev(netdev, mac_addr, NULL, WLAN_SAWF_ID);
	if (!vdev)
		return;
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
		return;
	}

	if (cfg_get(psoc, CFG_OL_SAWF_UL_FSE_ENABLE)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
		return;
	}

	soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

	osdev = ath_netdev_priv(netdev);
#ifdef QCA_SUPPORT_WDS_EXTENDED
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp = NULL;

		osifp = ath_netdev_priv(netdev);
		peer_id = osifp->peer_id;
	}
#endif

	cdp_sawf_peer_config_ul(soc_txrx_handle,
				mac_addr, tid,
				service_interval, burst_size,
				min_tput, max_latency,
				add_or_sub, peer_id);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
}

static inline
void qca_sawf_peer_dl_flow_count(struct net_device *netdev, uint8_t *mac_addr,
				 uint8_t svc_id, uint8_t start_or_stop,
				 uint16_t flow_count)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	uint8_t peer_mac_addr[QDF_MAC_ADDR_SIZE];
	uint8_t direction = 1;			/* Down Link*/
	uint16_t peer_id = HTT_INVALID_PEER;
	ol_txrx_soc_handle soc_txrx_handle;
	osif_dev *osdev = NULL;

	if (!netdev || !netdev->ieee80211_ptr)
		return;

	vdev = qca_sawf_get_ref_vdev(netdev, mac_addr, NULL, WLAN_SAWF_ID);
	if (!vdev)
		return;
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
		return;
	}

	soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

	osdev = ath_netdev_priv(netdev);
#ifdef QCA_SUPPORT_WDS_EXTENDED
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		osif_peer_dev *osifp = NULL;

		osifp = ath_netdev_priv(netdev);
		peer_id = osifp->peer_id;
	}
#endif

	cdp_sawf_peer_flow_count(soc_txrx_handle, mac_addr,
				 svc_id, direction, start_or_stop,
				 peer_mac_addr, peer_id, flow_count);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
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
								add_or_sub, 1);

	if (wlan_service_id_valid(rv_service_id))
		qca_sawf_peer_dl_flow_count(src_dev, src_mac, rv_service_id,
								add_or_sub, 1);
	if (fw_mark_metadata != DP_SAWF_META_DATA_INVALID &&
	    (!wlan_service_id_valid(fw_service_id) &&
	     !wlan_service_id_scs_valid(0, fw_service_id)) &&
	    add_or_sub == FLOW_STOP)
		qca_sawf_3_link_peer_dl_flow_count(dst_dev, dst_mac,
						   fw_mark_metadata);

	if (rv_mark_metadata != DP_SAWF_META_DATA_INVALID &&
	    (!wlan_service_id_valid(rv_service_id) &&
	     !wlan_service_id_scs_valid(0, rv_service_id)) &&
	    add_or_sub == FLOW_STOP)
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

void qca_sawf_mcast_connection_sync(qca_sawf_mcast_sync_param_t *params)
{

}

bool qca_sawf_register_flow_deprioritize_callback(void (*sawf_flow_deprioritize_callback)(struct qca_sawf_flow_deprioritize_params *params))
{
	return wlan_sawf_set_flow_deprioritize_callback(sawf_flow_deprioritize_callback);
}

void qca_sawf_unregister_flow_deprioritize_callback(void)
{
	wlan_sawf_set_flow_deprioritize_callback(NULL);
}

void qca_sawf_flow_deprioritize_response(struct qca_sawf_flow_deprioritize_resp_params *params)
{
	if (wlan_service_id_valid(params->service_id))
		qca_sawf_peer_dl_flow_count(params->netdev, params->mac_addr,
					    params->service_id, FLOW_DEPRIORITIZE,
					    params->success_count);
}

static inline bool is_present(uint8_t array[], uint8_t value, uint8_t size)
{
	uint8_t i;

	if (!value)
		return true;

	for (i = 0; i < size; i++) {
		if (value == array[i])
			return true;
	}

	return false;
}

static inline bool
qca_sawf_validate_index_input_params(struct qca_sawf_wifi_port_params *wp, uint8_t i)
{
	uint8_t channel = wp->channel.value[i];
	uint8_t band = wp->band.value[i];
	uint8_t size_ch = 0;
	uint8_t size_band = 0;
	bool ch_valid = true;
	bool band_valid = true;

	switch (i) {
	case QCA_VDEV_BAND_2G:
		size_ch = QDF_ARRAY_SIZE(channel_list_2g);
		size_band = QDF_ARRAY_SIZE(bw_list_2g);
		ch_valid = is_present(channel_list_2g, channel, size_ch);
		band_valid = is_present(bw_list_2g, band, size_band);
		break;
	case QCA_VDEV_BAND_5GL:
		size_ch = QDF_ARRAY_SIZE(channel_list_5g_lo);
		size_band = QDF_ARRAY_SIZE(bw_list_5g);
		ch_valid = is_present(channel_list_5g_lo, channel, size_ch);
		band_valid = is_present(bw_list_5g, band, size_band);
		break;
	case QCA_VDEV_BAND_5GH:
		size_ch = QDF_ARRAY_SIZE(channel_list_5g_hi);
		size_band = QDF_ARRAY_SIZE(bw_list_5g);
		ch_valid = is_present(channel_list_5g_hi, channel, size_ch);
		band_valid = is_present(bw_list_5g, band, size_band);
		break;
	case QCA_VDEV_BAND_5G:
		size_ch = QDF_ARRAY_SIZE(channel_list_5g);
		size_band = QDF_ARRAY_SIZE(bw_list_5g);
		ch_valid = is_present(channel_list_5g, channel, size_ch);
		band_valid = is_present(bw_list_5g, band, size_band);
		break;
	case QCA_VDEV_BAND_6G:
		size_ch = QDF_ARRAY_SIZE(channel_list_6g);
		size_band = QDF_ARRAY_SIZE(bw_list_6g);
		ch_valid = is_present(channel_list_6g, channel, size_ch);
		band_valid = is_present(bw_list_6g, band, size_band);
		break;
	default:
		return false;
	}

	return (ch_valid & band_valid);
}


bool get_channel_bw_band_from_vdev(struct net_device *netdev,
				   struct wlan_objmgr_vdev *vdev,
				   struct wifi_params exist_params[])
{
	struct wlan_channel *bss_chan;
	enum QCA_VDEV_BAND band_index;
	uint8_t channel = 0;
	uint32_t freq = 0;

	bss_chan = wlan_vdev_mlme_get_bss_chan(vdev);
	channel = bss_chan->ch_ieee;
	freq = bss_chan->ch_freq;

	if (freq >= 2412 && freq <= 2484) {
		band_index = QCA_VDEV_BAND_2G;
	} else if (freq >= 5180 && freq <= 5980) {
		band_index = QCA_VDEV_BAND_5G;
	} else if (freq >= 5935 && freq <= 7115) {
		band_index = QCA_VDEV_BAND_6G;
	} else {
		band_index = QCA_VDEV_BAND_INVALID;
		return false;
	}

	exist_params[band_index].band = true;
	exist_params[band_index].channel = channel;
	exist_params[band_index].bandwidth = bss_chan->ch_width + 1;
	return true;
}

bool qca_sawf_match_input_output_params(struct net_device *netdev,
					struct qca_sawf_wifi_port_params *wp,
					struct wifi_params *input_p,
					struct wifi_params *exist_p)

{
	bool band_flag = wp->band.flag;
	bool channel_flag = wp->channel.flag;
	bool bw_flag = wp->bw.flag;
	bool band_match = true;
	bool channel_match = true;
	bool bw_match = true;
	bool null_band = true;
	bool null_channel = true;
	bool null_bw = true;
	int i = 0;

	for (i = 0; i < MAX_NUM_MLO_VDEV; i++) {
		if (input_p[i].band)
			null_band = false;
		if (input_p[i].channel)
			null_channel = false;
		if (input_p[i].bandwidth)
			null_bw = false;

	}

	if (null_band & null_channel & null_bw)
		return true;

	/*
	 * flag 1 = AND
	 * flag 0 = OR
	 */

	/*
	 * Band check
	 */
	for (i = 0; i < MAX_NUM_MLO_VDEV; i++) {
		if (band_flag == 1) {
			band_match = true;
			/*
			 * 2G and 5G- user
			 * 2G, 5G, 6G - APDUT
			 * RESULT- true
			 *
			 * 6G - user
			 * 2G, 5G - APDUT
			 * RESULT- false
			 */
			if (!!input_p[i].band & !exist_p[i].band) {
				band_match = false;
				break;
			}
		} else {
			band_match = false;
			/*
			 * 2G or 5G- user
			 * 5G, 6G - APDUT
			 * RESULT- true
			 *
			 * 2G or 6G - user
			 * 5G - APDUT
			 * RESULT- false
			 */
			if (!!input_p[i].band & !!exist_p[i].band) {
				band_match = true;
				break;
			}
		}
	}

	if (!null_band & !band_match) {
		sawf_debug("band mismatch");
		return false;
	}

	/*
	 * Channel check
	 */
	for (i = 0; i < MAX_NUM_MLO_VDEV; i++) {
		if (channel_flag == 1) {
			channel_match = true;
			if (input_p[i].channel != 0 && exist_p[i].channel != input_p[i].channel) {
				channel_match = false;
				break;
			}
		} else {
			channel_match = false;
			if (input_p[i].channel != 0 && exist_p[i].channel == input_p[i].channel) {
				channel_match = true;
				break;
			}
		}
	}

	if (!null_channel & !channel_match) {
		sawf_debug("channel mismatch");
		return false;
	}

	/*
	 * Bandwidth check
	 */
	for (i = 0; i < MAX_NUM_MLO_VDEV; i++) {
		if (bw_flag == 1) {
			bw_match = true;
			if (input_p[i].bandwidth != 0 &&
			    exist_p[i].bandwidth != input_p[i].bandwidth) {
				bw_match = false;
				break;
			}
		} else {
			bw_match = false;
			if (input_p[i].bandwidth != 0 &&
			    exist_p[i].bandwidth == input_p[i].bandwidth) {
				bw_match = true;
				break;
			}
		}
	}

	if (!null_bw & !bw_match) {
		sawf_debug("band mismatch");
		return false;
	}

	return true;
}

#ifdef WLAN_FEATURE_11BE_MLO
bool qca_sawf_check_mlo_params(struct net_device *netdev,
			       struct wlan_objmgr_peer *ml_peer,
			       struct wlan_objmgr_vdev *vdev,
				struct qca_sawf_wifi_port_params *wp,
				struct wifi_params exist_params[])
{
	uint16_t vdev_count = 0;
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = {NULL};
	int i = 0;
	struct wlan_objmgr_peer *primary_peer = NULL;
	struct mlo_partner_info ml_links = {0};
	bool bssid_match = false;
	bool ra_match = false;
	bool ta_match = false;
	uint8_t *vdev_mac;

	if (qdf_is_macaddr_zero((struct qdf_mac_addr *)wp->bssid))
		bssid_match = true;

	if (qdf_is_macaddr_zero((struct qdf_mac_addr *)wp->ra_mac))
		ra_match = true;

	if (qdf_is_macaddr_zero((struct qdf_mac_addr *)wp->ta_mac))
		ta_match = true;

	mlo_peer_get_vdev_list(ml_peer, &vdev_count, wlan_vdev_list);
	primary_peer = wlan_mlo_peer_get_primary_peer(ml_peer->mlo_peer_ctx);
	wlan_mlo_peer_get_partner_links_info(ml_peer, &ml_links);

	if (!primary_peer) {
		return false;
	}

	for (i = 0; i < vdev_count; i++) {
		vdev_mac = wlan_vdev_mlme_get_macaddr(wlan_vdev_list[i]);
		if (qdf_is_macaddr_equal((struct qdf_mac_addr *)vdev_mac,
					 (struct qdf_mac_addr *)wp->bssid))
			bssid_match = true;

		if (qdf_is_macaddr_equal((struct qdf_mac_addr *)vdev_mac,
					 (struct qdf_mac_addr *)wp->ta_mac))
			ta_match = true;

		get_channel_bw_band_from_vdev(netdev, wlan_vdev_list[i], exist_params);
		wlan_objmgr_vdev_release_ref(wlan_vdev_list[i], WLAN_MLO_MGR_ID);
	}

	if (qdf_is_macaddr_equal((struct qdf_mac_addr *)
				 (struct qdf_mac_addr *)primary_peer->macaddr,
				 (struct qdf_mac_addr *)wp->ra_mac))
		ra_match = true;

	for (i = 0; i < ml_links.num_partner_links; i++) {
		if (qdf_is_macaddr_equal((struct qdf_mac_addr *)
					 &ml_links.partner_link_info[i].link_addr,
					 (struct qdf_mac_addr *)wp->ra_mac))
			ra_match = true;
	}

	if (!bssid_match || !ta_match || !ra_match)
		return false;

	return true;
}
#endif

static inline
bool qca_sawf_check_slo_params(struct net_device *netdev,
				uint8_t *peer_mac,
			       struct wlan_objmgr_vdev *vdev,
				struct qca_sawf_wifi_port_params *wp,
				struct wifi_params exist_params[])
{
	uint8_t *vdev_mac = wlan_vdev_mlme_get_macaddr(vdev);
	bool bssid_match = false;
	bool ra_match = false;
	bool ta_match = false;

	if (qdf_is_macaddr_zero((struct qdf_mac_addr *)wp->bssid))
		bssid_match = true;

	if (qdf_is_macaddr_zero((struct qdf_mac_addr *)wp->ra_mac))
		ra_match = true;

	if (qdf_is_macaddr_zero((struct qdf_mac_addr *)wp->ta_mac))
		ta_match = true;

	/* mismatch if bssid do not match */
	if (!bssid_match &&
	    !qdf_is_macaddr_equal((struct qdf_mac_addr *)vdev_mac,
				  (struct qdf_mac_addr *)wp->bssid))
		return false;

	/*
	 * peer mac should match ra mac in both
	 * 1. 3 address: because RA and DA match
	 * 2. 4 address: In wds ext mode this is RA mac
	 */
	if (!ra_match &&
	    !qdf_is_macaddr_equal((struct qdf_mac_addr *)peer_mac,
				  (struct qdf_mac_addr *)wp->ra_mac))
		return false;

	if (!ta_match &&
	    !qdf_is_macaddr_equal((struct qdf_mac_addr *)vdev_mac,
				  (struct qdf_mac_addr *)wp->ta_mac))
		return false;

	get_channel_bw_band_from_vdev(netdev, vdev, exist_params);

	return true;
}

static inline void
qca_sawf_store_input_params(struct qca_sawf_wifi_port_params *wp,
			    struct wifi_params *user_params)
{
	int i = 0;

	for (i = 1; i < MAX_NUM_MLO_VDEV; i++) {
		if (wp->band.value[i] == 0)
			continue;

		user_params[i].band = true;
		user_params[i].channel = wp->channel.value[i];
		user_params[i].bandwidth = wp->bw.value[i];
	}
}

/*
 * qca_sawf_print_wifi_params
 *
 * @netdev: netdevice
 * @dest_mac: destination mac address
 * @priority: Traffic priority set by user
 * @wp: sdwf wifi parameters parsed and sent by spm
 */
static inline
void qca_sawf_print_wifi_params(struct net_device *dst_dev,
				uint8_t *dest_mac,
				struct net_device *src_dev,
				uint8_t *src_mac,
				struct qca_sawf_wifi_port_params *wp)
{
	uint8_t i = 0;

	sawf_debug("Destination device %s ", dst_dev->name);
	sawf_debug("ssid: %s ssid_len:%d : ", wp->ssid, wp->ssid_len);
	sawf_debug("BSSID: " QDF_MAC_ADDR_FMT,  QDF_MAC_ADDR_REF(wp->bssid));
	sawf_debug("Destination mac: " QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(dest_mac));
	sawf_debug("Source mac: " QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(src_mac));
	sawf_debug("RA mac_addr: " QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(wp->ra_mac));
	sawf_debug("TA mac_addr: " QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(wp->ta_mac));
	sawf_debug("priority: %u ", wp->priority);
	sawf_debug("OR/AND flags band:%d channel:%d bw:%d ", wp->band.flag,
		 wp->channel.flag, wp->bw.flag);

	for (i = 0 ; i < MAX_NUM_MLO_VDEV; i++) {
		sawf_debug("band:%d channel: %d bandwidth: %d ",
			 wp->band.value[i],
			 wp->channel.value[i],
			 wp->bw.value[i]);
	}

	sawf_debug("valid_flag: %d dscp:%u pcp: %u access_class: %u ",
		 wp->valid_flags, wp->dscp, wp->pcp, wp->ac);
}

bool qca_sdwf_validate_wifi_port_params(struct qca_sawf_wifi_port_params *wp)
{
	bool match = false;
	int i = 0;

	/*
	 * Validate channel and bandwidth corresponding to
	 * each band. If any of them do not match, return
	 * false.
	 */
	for (i = 1; i < MAX_NUM_MLO_VDEV; i++) {
		if (wp->band.value[i] == 0)
			continue;
		match = qca_sawf_validate_index_input_params(wp, i);
		if (!match)
			return false;
	}

	/*
	 * Max access category can be 3
	 */
	if (wp->valid_flags & QCA_SAWF_AC_VALID) {
		if (wp->ac >= WME_AC_MAX)
			return false;
	}

	if (wp->valid_flags & QCA_SAWF_DSCP_VALID) {
		if (wp->dscp >= WMI_HOST_DSCP_MAP_MAX)
			return false;
	}

	if (wp->ssid_len > MAX_SSID_LEN)
		return false;

	return true;
}

bool qca_sdwf_validate_wifi_port_params_v2(struct qca_sawf_wifi_port_params_v2 *params)
{
	struct qca_sawf_wifi_port_params wp = {0};
	uint8_t ssid[MAX_SSID_LEN + 1] = {0};
	uint8_t bssid[MAC_ADDR_SIZE] = {0};
	uint8_t ra_mac[MAC_ADDR_SIZE] = {0};
	uint8_t ta_mac[MAC_ADDR_SIZE] = {0};

	qdf_mem_copy(ssid, params->ssid, MAX_SSID_LEN + 1);
	qdf_mem_copy(bssid, params->bssid, MAC_ADDR_SIZE);
	qdf_mem_copy(ra_mac, params->ra_mac, MAC_ADDR_SIZE);
	qdf_mem_copy(ta_mac, params->ta_mac, MAC_ADDR_SIZE);
	wp.ssid = ssid;
	wp.bssid = bssid;
	wp.ra_mac = ra_mac;
	wp.ta_mac = ta_mac;
	qdf_mem_copy(&wp.band, &params->band, sizeof(struct qca_sawf_radio_params));
	qdf_mem_copy(&wp.channel, &params->channel, sizeof(struct qca_sawf_radio_params));
	qdf_mem_copy(&wp.bw, &params->bw, sizeof(struct qca_sawf_radio_params));
	wp.ssid_len = params->ssid_len;
	wp.dscp = params->dscp;
	wp.pcp = params->pcp;
	wp.ac = params->ac;
	wp.valid_flags = params->valid_flags;
	wp.priority = params->priority;

	return qca_sdwf_validate_wifi_port_params(&wp);
}

bool qca_sdwf_match_wifi_port_params(struct net_device *dst_dev,
				     uint8_t dest_mac[],
				     struct net_device *src_dev,
				     uint8_t src_mac[],
				     uint8_t pri,
				     struct qca_sawf_wifi_port_params_v2 *params)
{
	struct qca_sawf_wifi_port_params wp = {0};
	uint8_t ssid[MAX_SSID_LEN + 1] = {0};
	uint8_t bssid[MAC_ADDR_SIZE] = {0};
	uint8_t ra_mac[MAC_ADDR_SIZE] = {0};
	uint8_t ta_mac[MAC_ADDR_SIZE] = {0};

	qdf_mem_copy(ssid, params->ssid, MAX_SSID_LEN + 1);
	qdf_mem_copy(bssid, params->bssid, MAC_ADDR_SIZE);
	qdf_mem_copy(ra_mac, params->ra_mac, MAC_ADDR_SIZE);
	qdf_mem_copy(ta_mac, params->ta_mac, MAC_ADDR_SIZE);
	wp.ssid = ssid;
	wp.bssid = bssid;
	wp.ra_mac = ra_mac;
	wp.ta_mac = ta_mac;
	qdf_mem_copy(&wp.band, &params->band, sizeof(struct qca_sawf_radio_params));
	qdf_mem_copy(&wp.channel, &params->channel, sizeof(struct qca_sawf_radio_params));
	qdf_mem_copy(&wp.bw, &params->bw, sizeof(struct qca_sawf_radio_params));
	wp.ssid_len = params->ssid_len;
	wp.dscp = params->dscp;
	wp.pcp = params->pcp;
	wp.ac = params->ac;
	wp.valid_flags = params->valid_flags;
	wp.priority = params->priority;

	return qca_sdwf_match_wifi_port_params_v2(dst_dev, dest_mac, src_dev, src_mac, pri, &wp);
}

bool qca_sdwf_match_wifi_port_params_v2(struct net_device *dst_dev,
					uint8_t *dest_mac,
					struct net_device *src_dev,
					uint8_t *src_mac,
					uint8_t priority,
					struct qca_sawf_wifi_port_params *wp)
{
	struct wlan_objmgr_vdev *vdev;
	struct wifi_params user_p[MAX_NUM_MLO_VDEV] = {0};
	struct wifi_params present_p[MAX_NUM_MLO_VDEV] = {0};
	uint8_t access_category = 0;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	struct net_device *wifi_netdev = NULL;
	uint8_t wifi_mac[QDF_MAC_ADDR_SIZE];
	bool dir, wifi_flow = false;
	uint8_t tid;
	struct wlan_objmgr_peer *peer = NULL;
#ifdef WLAN_FEATURE_11BE_MLO
	struct wlan_mlo_peer_context *mlo_peer_ctx = NULL;
	struct wlan_mlo_dev_context *mld_dev = NULL;
#endif

	if (!dst_dev || !wp) {
		sawf_err("Null netdev/wifi params");
		return false;
	}

	qca_sawf_print_wifi_params(dst_dev, dest_mac, src_dev, src_mac, wp);

	if (dst_dev && dst_dev->ieee80211_ptr) {
		wifi_netdev = dst_dev;
		qdf_copy_macaddr((struct qdf_mac_addr *)wifi_mac,
				 (struct qdf_mac_addr *)dest_mac);
		dir = SAWF_DOWNLINK;
		wifi_flow = true;

	} else if (src_dev && src_dev->ieee80211_ptr) {
		uint8_t local_mac[QDF_MAC_ADDR_SIZE];
		wifi_netdev = src_dev;
		qdf_copy_macaddr((struct qdf_mac_addr *)wifi_mac,
				 (struct qdf_mac_addr *)src_mac);
		/*
		 * In uplink direction exchange values of TA and RA
		 * Other values are direction independent.
		 */
		qdf_copy_macaddr((struct qdf_mac_addr *)local_mac,
				 (struct qdf_mac_addr *)wp->ta_mac);
		qdf_copy_macaddr((struct qdf_mac_addr *)wp->ta_mac,
				 (struct qdf_mac_addr *)wp->ra_mac);
		qdf_copy_macaddr((struct qdf_mac_addr *)wp->ra_mac,
				 (struct qdf_mac_addr *)local_mac);

		dir = SAWF_UPLINK;
		wifi_flow = true;
	}

	/* Not a wifi flow, mo match required */
	if (!wifi_flow)
		return false;

	/*
	 * If parameters are invalid, return false;
	 */
	if (!qca_sdwf_validate_wifi_port_params(wp))
		return false;

	/*
	 * Store only valid params in user_p array
	 */
	qca_sawf_store_input_params(wp, user_p);

	vdev = qca_sawf_get_ref_vdev(wifi_netdev, wifi_mac, peer_mac,
				     WLAN_SAWF_ID);

	if (!vdev) {
		sawf_err("Invalid vdev");
		return false;
	}

	/*
	 * if ssid_len is 0, ssid match is ignored.
	 */
	if (wp->ssid_len && !wlan_dessired_ssid_found(vdev, wp->ssid, wp->ssid_len)) {
		goto fail;
	}

#ifdef WLAN_FEATURE_11BE_MLO
	mlo_peer_ctx = wlan_mlo_get_mlpeer_by_peer_mladdr
		((struct qdf_mac_addr *)peer_mac, &mld_dev);
	if (!mlo_peer_ctx) {
		peer = wlan_objmgr_get_peer_by_mac(wlan_vdev_get_psoc(vdev), peer_mac,
					   WLAN_SAWF_ID);
		if (!peer) {
			sawf_debug("Direction:%d peer" QDF_MAC_ADDR_FMT" not found ",
				    dir, QDF_MAC_ADDR_REF(peer_mac));
			goto fail;
		}
	} else {
		QDF_STATUS status;

		peer = wlan_mlo_peer_get_assoc_peer(mlo_peer_ctx);
		if (!peer) {
			sawf_debug("Direction:%d link peer not found for mld mac "
					   QDF_MAC_ADDR_FMT,
					   dir, QDF_MAC_ADDR_REF(peer_mac));
			goto fail;
		}

		status = wlan_objmgr_peer_try_get_ref(peer, WLAN_SAWF_ID);
		if (QDF_IS_STATUS_ERROR(status)) {
			peer = NULL;
			sawf_debug("Direction:%d link peer ref failed for mld mac "
					   QDF_MAC_ADDR_FMT,
					   dir, QDF_MAC_ADDR_REF(peer_mac));
			goto fail;
		}
	}

	if (peer && peer->mlo_peer_ctx) {
		if (!qca_sawf_check_mlo_params(wifi_netdev, peer, vdev, wp, present_p))
			goto fail;
	} else
#endif
		if (!qca_sawf_check_slo_params(wifi_netdev, peer_mac, vdev, wp, present_p)) {
			goto fail;
		}

	if (!qca_sawf_match_input_output_params(wifi_netdev, wp, user_p, present_p)) {
		goto fail;
	}

	if (dir == SAWF_DOWNLINK)
		tid = dscp_tid_map[wp->dscp];
	else
		tid = priority;

	access_category = TID_TO_WME_AC(tid);
	if (wp->valid_flags & QCA_SAWF_AC_VALID) {
		if (access_category != wp->ac) {
			goto fail;
		}
	}

	if (peer)
		wlan_objmgr_peer_release_ref(peer, WLAN_SAWF_ID);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
	sawf_err("Direction:%d WiFi rule matched ", dir);

	return true;

fail:
	if (peer)
		wlan_objmgr_peer_release_ref(peer, WLAN_SAWF_ID);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_SAWF_ID);
	sawf_err("Direction:%d WiFi rule mis-matched ", dir);

	return false;
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
struct qca_sawf_wifi_port_params;
struct qca_sawf_wifi_port_params_v2;
typedef struct qca_sawf_connection_sync_param qca_sawf_mcast_sync_param_t;
struct qca_sawf_flow_deprioritize_params;
struct qca_sawf_flow_deprioritize_resp_params;

void qca_sawf_connection_sync(struct qca_sawf_connection_sync_param *params)
{}

void qca_sawf_mcast_connection_sync(qca_sawf_mcast_sync_param_t *params)
{}

bool qca_sawf_register_flow_deprioritize_callback(void (*sawf_flow_deprioritize_callback)(struct qca_sawf_flow_deprioritize_params *params))
{
	return true;
}

void qca_sawf_unregister_flow_deprioritize_callback(void)
{}

void qca_sawf_flow_deprioritize_response(struct qca_sawf_flow_deprioritize_resp_params *params)
{}

bool qca_sdwf_validate_wifi_port_params(struct qca_sawf_wifi_port_params *wp)
{
	return true;
}

bool qca_sdwf_validate_wifi_port_params_v2(struct qca_sawf_wifi_port_params_v2 *wp)
{
	return true;
}

bool qca_sdwf_match_wifi_port_params(struct net_device *dst_dev,
				     uint8_t dest_mac[],
				     struct net_device *src_dev,
				     uint8_t src_mac[],
				     uint8_t priority,
				     struct qca_sawf_wifi_port_params_v2 *wp)
{
	return true;
}

bool qca_sdwf_match_wifi_port_params_v2(struct net_device *dst_dev,
					uint8_t *dest_mac,
					struct net_device *src_dev,
					uint8_t *src_mac,
					uint8_t priority,
					struct qca_sawf_wifi_port_params *wp)
{
	return true;
}
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
qdf_export_symbol(qca_sawf_mcast_connection_sync);
qdf_export_symbol(qca_sawf_register_flow_deprioritize_callback);
qdf_export_symbol(qca_sawf_unregister_flow_deprioritize_callback);
qdf_export_symbol(qca_sawf_flow_deprioritize_response);
qdf_export_symbol(qca_sdwf_match_wifi_port_params);
qdf_export_symbol(qca_sdwf_validate_wifi_port_params);
qdf_export_symbol(qca_sdwf_match_wifi_port_params_v2);
qdf_export_symbol(qca_sdwf_validate_wifi_port_params_v2);
