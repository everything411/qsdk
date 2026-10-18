/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

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
#include "osif_private.h"
#include <qdf_nbuf.h>
#include <qdf_net_if.h>

#define IFNAME_SIZE 16

void monitor_osif_process_rx_mpdu(osif_dev *osifp, qdf_nbuf_t mpdu_ind)
{
	skb_reset_mac_header(mpdu_ind);
	mpdu_ind->dev       = osif_get_monitor_netdev(osifp);
	mpdu_ind->pkt_type  = PACKET_USER;
	mpdu_ind->ip_summed = CHECKSUM_UNNECESSARY;
	mpdu_ind->protocol  = qdf_cpu_to_le16(ETH_P_802_2);
	nbuf_debug_del_record(mpdu_ind);
	netif_rx(mpdu_ind);
}

void monitor_osif_deliver_tx_capture_data(osif_dev *osifp, struct sk_buff *skb)
{
	skb->dev = osif_get_monitor_netdev(osifp);
	skb->protocol = eth_type_trans(skb, osif_get_monitor_netdev(osifp));
	skb->pkt_type = PACKET_USER;
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	nbuf_debug_del_record(skb);
	netif_rx(skb);
}

#ifdef QCA_UNDECODED_METADATA_SUPPORT
void monitor_osif_deliver_rx_capture_undecoded_metadata(osif_dev *osifp,
							struct sk_buff *skb)
{
	skb->dev = osif_get_monitor_netdev(osifp);
	skb->protocol = eth_type_trans(skb, osif_get_monitor_netdev(osifp));
	skb->pkt_type = PACKET_USER;
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	nbuf_debug_del_record(skb);
	netif_rx(skb);
}
#endif

#ifdef ENABLE_CFG80211_BACKPORTS_MLO
void *monitor_osif_get_vdev_by_name(struct ieee80211com *ic, char *name)
#else
void *monitor_osif_get_vdev_by_name(char *name)
#endif
{
	struct net_device *dev;
	osif_dev *osifp = NULL;
	char dev_name[IFNAME_SIZE];
	void *data = NULL;
#ifdef ENABLE_CFG80211_BACKPORTS_MLO
	struct osif_mldev *mldev = NULL;
	uint8_t link_id;
	osif_dev *link_vif = NULL;
#endif

	if (strlcpy(dev_name, name, IFNAME_SIZE) >= IFNAME_SIZE)
		return NULL;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 23))
	dev = (struct net_device *)qdf_net_if_get_dev_by_name(dev_name);
	if (dev != NULL) {
#else
	dev = dev_get_by_name(dev_name);
	if (dev != NULL) {
#endif
		osifp = ath_netdev_priv(dev);
#ifdef ENABLE_CFG80211_BACKPORTS_MLO
		if (!osifp) {
			qdf_err("No osifp found for dev %s", dev->name);
			qdf_net_if_release_dev((struct qdf_net_if *)dev);
			return NULL;
		}

		if (osifp->dev_type == OSIF_NETDEV_TYPE_MLO) {
			mldev = (struct osif_mldev *)osifp;
			for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++) {
				link_vif = mldev->link_vifs[link_id];
				if (link_vif && link_vif->os_if->iv_ic == ic) {
					osifp = mldev->link_vifs[link_id];
					break;
				}
			}
			if (link_id == IEEE80211_MLD_MAX_NUM_LINKS) {
				qdf_err("Requested link is not part of monitor mld %s", dev->name);
				qdf_net_if_release_dev((struct qdf_net_if *)dev);
				return NULL;
			}
		}
#endif

		data = (void *)osifp->ctrl_vdev;
		qdf_net_if_release_dev((struct qdf_net_if *)dev);
	}

	return data;
}

#ifdef ENABLE_CFG80211_BACKPORTS_MLO
bool monitor_osif_is_mld_ifname(char *name)
{
	char dev_name[IFNAME_SIZE];
	struct net_device *dev;
	osif_dev *osifp = NULL;

	if (strlcpy(dev_name, name, IFNAME_SIZE) >= IFNAME_SIZE)
		return false;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 23))
	dev = (struct net_device *)qdf_net_if_get_dev_by_name(dev_name);
#else
	dev = dev_get_by_name(dev_name);
#endif
	if (dev) {
		osifp = ath_netdev_priv(dev);
		if (osifp && osifp->dev_type == OSIF_NETDEV_TYPE_MLO) {
#ifdef ENABLE_CFG80211_BACKPORTS_MLO
			struct osif_mldev *mldev;
			mldev = ath_netdev_priv(dev);
			if (mldev->wdev.iftype == NL80211_IFTYPE_MONITOR) {
				/*
				 * Block all MLD interfaces expect monitor MLD interface
				 * Monitor MLD interface is valid, hence return false
				 */
				qdf_net_if_release_dev((struct qdf_net_if *)dev);
				return false;
			}
#endif
			qdf_net_if_release_dev((struct qdf_net_if *)dev);
			return true;
		}
		qdf_net_if_release_dev((struct qdf_net_if *)dev);
	}
	return false;
}
#endif
