/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <net/addrconf.h>
#include <net/dst.h>
#include <net/flow.h>
#include <net/ipv6.h>
#include <net/route.h>
#include <net/vxlan.h>
#include <net/vxlan.h>

#include "nss_ppe_vxlanmgr_priv.h"
#include "nss_ppe_vxlanmgr_tun_stats.h"
#include "ppe_drv_tun_cmn_ctx.h"
#include <nss_ppe_bridge_mgr.h>

/*
 * VxLAN neighbor-event list.
 */
static LIST_HEAD(rtm_neigh_event_list);
static DEFINE_SPINLOCK(rtm_neigh_event_list_lock);

struct work_struct rtm_neigh_event_work;			/* Work queue */
static const struct net_device_ops nss_ppe_vxlanmgr_nss_netdev_ops;
struct workqueue_struct *nss_ppe_vxlanmgr_rtm_neigh_event_wq;

/*
 * nss_ppe_vxlanmgr_nss_netdev_setup()
 *	Setup nss_netdevice
 */
static void nss_ppe_vxlanmgr_nss_netdev_setup(struct net_device *nss_dev)
{
	nss_ppe_vxlanmgr_trace("%px: setting the nss_dev: %s", nss_dev, nss_dev->name);
}

/*
 * nss_ppe_vxlanmgr_br_join_nss_netdev()
 *	Nss-netdevice join bridge.
 *
 * When the parent netdevice is added to the bridge the child/dummy nss-netdev should also be added to the bridge.
 * Therefore the API is called When the new remote joins the VXLAN network. i.e SWITCHDEV_VXLAN_FDB_ADD_TO_DEVICE event.
 */
static bool nss_ppe_vxlanmgr_br_join_nss_netdev(struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx)
{
	struct net_device *br_dev, *pdev, *nss_dev;
	bool bridge_joined;
	bridge_joined = tun_ctx->remote_info.bridge_joined;
	pdev = tun_ctx->parent_dev;
	nss_dev = tun_ctx->remote_info.nss_netdev;

	rcu_read_lock();
	br_dev = netdev_master_upper_dev_get_rcu(pdev);
	rcu_read_unlock();

	if (!bridge_joined && br_dev) {
		if (!nss_ppe_bridge_mgr_join_bridge(nss_dev, br_dev)) {
			tun_ctx->remote_info.bridge_joined = true;
			nss_ppe_vxlanmgr_trace("%px: Successfully added nss_netdev to bridge. br_dev:%s nss_dev:%s pdev:%s", pdev, br_dev->name, nss_dev->name, pdev->name);
			return true;
		}

		nss_ppe_vxlanmgr_warn("%px: failed to add nss_netdev to bridge. nss_dev:%s pdev:%s bridge_joined:%u br_dev:%s", pdev, nss_dev->name, pdev->name, bridge_joined, br_dev->name);
		return false;
	}

	nss_ppe_vxlanmgr_trace("%px: nss-dev already joined or cannot join the bridge since it is not in bridge. nss_dev:%s pdev:%s bridge_joined:%u br_dev:%p", pdev, nss_dev->name, pdev->name, bridge_joined, br_dev);
	return true;
}

/*
 * nss_ppe_vxlanmgr_br_leave_nss_netdev()
 *	Nss-netdevice leave bridge.
 *
 * When the parent netdevice is removed from the bridge the child/dummy nss-netdev should also be removed from the bridge.
 * Therefore API is called When the new remote leaves the VXLAN network. i.e SWITCHDEV_VXLAN_FDB_DEL_TO_DEVICE event.
 */
static bool nss_ppe_vxlanmgr_br_leave_nss_netdev(struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx)
{
	struct net_device *br_dev, *pdev, *nss_dev;
	bool bridge_joined;

	bridge_joined = tun_ctx->remote_info.bridge_joined;
	pdev = tun_ctx->parent_dev;
	nss_dev = tun_ctx->remote_info.nss_netdev;

	rcu_read_lock();
	br_dev = netdev_master_upper_dev_get_rcu(pdev);
	rcu_read_unlock();

	if (bridge_joined && br_dev) {
		if (!nss_ppe_bridge_mgr_leave_bridge(nss_dev, br_dev)) {
			tun_ctx->remote_info.bridge_joined = false;
			nss_ppe_vxlanmgr_trace("%px: Successfully left nss_netdev from the bridge. br_dev:%s nss_dev:%s pdev:%s", pdev, br_dev->name, nss_dev->name, pdev->name);
			return true;
		}

		nss_ppe_vxlanmgr_warn("%px: failed to leave nss_netdev from the bridge. nss_dev:%s pdev:%s bridge_joined:%u br_dev:%s", pdev, nss_dev->name, pdev->name, bridge_joined, br_dev->name);
		return false;
	}

	nss_ppe_vxlanmgr_trace("%px: nss-dev already left or cannot leave the bridge since it is not in bridge. nss_dev:%s pdev:%s bridge_joined:%u br_dev:%p", pdev, nss_dev->name, pdev->name, bridge_joined, br_dev);
	return true;
}

/*
 * nss_ppe_vxlanmgr_all_remotes_leave_bridge()
 *	Leave all the remotes of the base parent linux netdevice to the bridge
 */
void nss_ppe_vxlanmgr_all_remotes_leave_bridge(struct net_device *pdev)
{
	struct nss_ppe_vxlanmgr_tun_ctx *curr_tun_ctx;
	struct vxlan_dev *priv;
	uint32_t vni_key;

	priv = netdev_priv(pdev);
	vni_key = vxlan_vni_field(priv->cfg.vni);

	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	hash_for_each_possible(nss_ppe_vxlanmgr_tunnel_tbl, curr_tun_ctx, node, vni_key) {
		if ((curr_tun_ctx->vp_status == NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) && (curr_tun_ctx->vni == vni_key)) {
			if (nss_ppe_vxlanmgr_br_leave_nss_netdev(curr_tun_ctx)) {
				nss_ppe_vxlanmgr_trace("%px: Successfully left nss_netdev from bridge. nss_dev:%s pdev:%s", pdev, curr_tun_ctx->remote_info.nss_netdev->name, pdev->name);
			} else {
				nss_ppe_vxlanmgr_warn("%px: failed to add nss_netdev to bridge. nss_dev:%s pdev:%s", pdev, curr_tun_ctx->remote_info.nss_netdev->name, pdev->name);
			}
		}
	}
	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
}

/*
 * nss_ppe_vxlanmgr_all_remotes_join_bridge()
 *	Join all the remotes of the base parent linux netdevice to the bridge
 */
void nss_ppe_vxlanmgr_all_remotes_join_bridge(struct net_device *pdev)
{
	struct nss_ppe_vxlanmgr_tun_ctx *curr_tun_ctx;
	struct vxlan_dev *priv;
	uint32_t vni_key;

	priv = netdev_priv(pdev);
	vni_key = vxlan_vni_field(priv->cfg.vni);

	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	hash_for_each_possible(nss_ppe_vxlanmgr_tunnel_tbl, curr_tun_ctx, node, vni_key) {
		if ((curr_tun_ctx->vp_status == NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) && (curr_tun_ctx->vni == vni_key)) {
			if (nss_ppe_vxlanmgr_br_join_nss_netdev(curr_tun_ctx)) {
				nss_ppe_vxlanmgr_trace("%px: Successfully added nss_netdev to bridge. nss_dev:%s pdev:%s", pdev, curr_tun_ctx->remote_info.nss_netdev->name, pdev->name);
			} else {
				nss_ppe_vxlanmgr_warn("%px: failed to add nss_netdev to bridge. nss_dev:%s pdev:%s", pdev, curr_tun_ctx->remote_info.nss_netdev->name, pdev->name);
			}
		}
	}
	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
}

/*
 * nss_ppe_vxlanmgr_all_remotes_decap_enable()
 *	Enable decap for all the remotes of the base parent linux netdevice
 */
void nss_ppe_vxlanmgr_all_remotes_decap_enable(struct net_device *pdev)
{
	struct nss_ppe_vxlanmgr_tun_ctx *curr_tun_ctx;
	struct vxlan_dev *priv;
	uint32_t vni_key;

	priv = netdev_priv(pdev);
	vni_key = vxlan_vni_field(priv->cfg.vni);

	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	hash_for_each_possible(nss_ppe_vxlanmgr_tunnel_tbl, curr_tun_ctx, node, vni_key) {
		if ((curr_tun_ctx->vp_status == NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) && (curr_tun_ctx->vni == vni_key) && (!ppe_tun_decap_enable(curr_tun_ctx->remote_info.nss_netdev))) {
			nss_ppe_vxlanmgr_warn("%px: Failed enabling decap for netdev %s", pdev, curr_tun_ctx->remote_info.nss_netdev->name);
		}
	}
	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
}

/*
 * nss_ppe_vxlanmgr_all_remotes_decap_disable()
 *	Disable decap for all the remotes of the base parent linux netdevice
 */
void nss_ppe_vxlanmgr_all_remotes_decap_disable(struct net_device *pdev)
{
	struct nss_ppe_vxlanmgr_tun_ctx *curr_tun_ctx;
	struct vxlan_dev *priv;
	uint32_t vni_key;

	priv = netdev_priv(pdev);
	vni_key = vxlan_vni_field(priv->cfg.vni);

	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	hash_for_each_possible(nss_ppe_vxlanmgr_tunnel_tbl, curr_tun_ctx, node, vni_key) {
		if ((curr_tun_ctx->vp_status == NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) && (curr_tun_ctx->vni == vni_key) && (!ppe_tun_decap_disable(curr_tun_ctx->remote_info.nss_netdev))) {
			nss_ppe_vxlanmgr_warn("%px: Failed disabling decap for netdev %s", pdev, curr_tun_ctx->remote_info.nss_netdev->name);
		}
	}
	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
}

/*
 * nss_ppe_vxlanmgr_all_remotes_set_mtu()
 *	Set mtu for all the remotes of the base parent linux netdevice
 */
void nss_ppe_vxlanmgr_all_remotes_set_mtu(struct net_device *pdev, unsigned int mtu)
{
	struct nss_ppe_vxlanmgr_tun_ctx *curr_tun_ctx;
	struct vxlan_dev *priv;
	uint32_t vni_key;

	priv = netdev_priv(pdev);
	vni_key = vxlan_vni_field(priv->cfg.vni);

	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	hash_for_each_possible(nss_ppe_vxlanmgr_tunnel_tbl, curr_tun_ctx, node, vni_key) {
		if ((curr_tun_ctx->vp_status == NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) && (curr_tun_ctx->vni == vni_key) && (!ppe_tun_mtu_set(curr_tun_ctx->remote_info.nss_netdev, mtu))) {
			nss_ppe_vxlanmgr_warn("%px: Failed to set mtu for nss_netdev %s", pdev, curr_tun_ctx->remote_info.nss_netdev->name);
		}
	}
	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
}

/*
 * nss_ppe_vxlanmgr_decap_disable()
 *	Nss-netdevice disable decap.
 *
 * Disable the decap for the NSS netdevice whenever the pdev is outside the bridge.
 */
bool nss_ppe_vxlanmgr_decap_disable(struct net_device *pdev, struct net_device *nss_dev)
{
	struct net_device *br_dev;

	rcu_read_lock();
	br_dev = netdev_master_upper_dev_get_rcu(pdev);
	rcu_read_unlock();

	if (!br_dev && !ppe_tun_decap_disable(nss_dev)) {
		nss_ppe_vxlanmgr_warn("%px: Failed disabling decap for the nss-netdev. nss_dev:%s br_dev:%s  pdev:%s", pdev, nss_dev->name, br_dev->name, pdev->name);
		return false;
	}

	nss_ppe_vxlanmgr_trace("%px: Successfully disabled decap for nss_dev %s, or couldnot disable since it is not added to bridge. pdev:%s br_dev:%p", pdev, nss_dev->name, pdev->name, br_dev);
	return true;
}

/*
 * nss_ppe_vxlanmgr_decap_enable()
 *	Nss-netdevice enable decap.
 *
 * Enable the decap for the NSS netdevice whenever it is in bridge.
 */
static bool nss_ppe_vxlanmgr_decap_enable(struct net_device *pdev, struct net_device *nss_dev)
{
	struct net_device *br_dev;

	rcu_read_lock();
	br_dev = netdev_master_upper_dev_get_rcu(pdev);
	rcu_read_unlock();

	if (br_dev && !ppe_tun_decap_enable(nss_dev)) {
		nss_ppe_vxlanmgr_warn("%px: Failed enabling decap for nss-netdev. nss_dev:%s br_dev:%s  pdev:%s", pdev, nss_dev->name, br_dev->name, pdev->name);
		return false;
	}

	nss_ppe_vxlanmgr_trace("%px: Successfully enabled decap for the nss-netdev or couldnot enable since it is not added to bridge. nss_dev:%s pdev:%s br_dev:%p", pdev, nss_dev->name, pdev->name, br_dev);
	return true;
}

/*
 * nss_ppe_vxlanmgr_tunnel_destroy()
 *	Function to unregister and destroy the PPE tunnel using tunnel context.
 */
static void nss_ppe_vxlanmgr_tunnel_destroy(struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx, struct net_device *dev)
{
	struct nss_ppe_vxlanmgr_remote_info *remote_info;

	/*
	 * Delete the tunnel in UNSUCCESS case.
	 */
	if (tun_ctx->vp_status != NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) {
		kfree(tun_ctx);
		return;
	}

	/*
	 * Delete the tunnel in SUCCESS case.
	 */
	remote_info = &tun_ctx->remote_info;

	ppe_tun_deconfigure(dev);

	/*
	 * The below function sleeps
	 */
	nss_ppe_vxlanmgr_tun_stats_dentry_remove(tun_ctx);

	ppe_tun_free(dev);

	rtnl_lock();
	unregister_netdevice(dev);
	rtnl_unlock();

	free_netdev(dev);

	kfree(tun_ctx->tun_hdr);
	kfree(tun_ctx);
}

/*
 * nss_ppe_vxlanmgr_delete_remote()
 *	Delete the VXLAN remote.
 */
static void nss_ppe_vxlanmgr_delete_remote(struct kref *kref)
{
	struct net_device *nss_dev, *pdev;
	struct nss_ppe_vxlanmgr_remote_info *r_info;
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;

	r_info = container_of(kref, struct nss_ppe_vxlanmgr_remote_info, mac_address_ref);
	nss_dev = r_info->nss_netdev;
	pdev = r_info->pdev;

	nss_ppe_vxlanmgr_trace("%px: deleteing the Remote pdev:%s nss_dev:%s", pdev, pdev->name, nss_dev->name);

	tun_ctx = nss_ppe_vxlanmgr_tunnel_ctx_get_and_dettach(nss_dev);
	if (!tun_ctx) {
		nss_ppe_vxlanmgr_warn("%px: Failed to get tunnel context. Invalid tunnel context", nss_dev);
		return;
	}

	nss_ppe_vxlanmgr_br_leave_nss_netdev(tun_ctx);
	ppe_tun_decap_disable(nss_dev);
	nss_ppe_vxlanmgr_tunnel_destroy(tun_ctx, nss_dev);
}

/*
 * nss_ppe_vxlanmgr_tunnel_parse_end_points()
 *	VxLAN tunnel mac add messages.
 */
static bool nss_ppe_vxlanmgr_tunnel_parse_end_points(struct net_device *dev, struct ppe_drv_tun_cmn_ctx *tun_hdr, union vxlan_addr *rip)
{
	struct vxlan_dev *priv;
	struct ppe_drv_tun_cmn_ctx_l3 *l3 = &tun_hdr->l3;
	struct flowi4 fl4;
	struct flowi6 fl6;
	struct rtable *rt = NULL;
	uint32_t priv_flags;
	union vxlan_addr *src_ip;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 5, 7))
	struct dst_entry *dst = NULL;
	int err;
#else
	const struct in6_addr *final_dst = NULL;
	struct dst_entry *dentry;
	struct vxlan_config *cfg;
#endif

	priv = netdev_priv(dev);
	cfg = &priv->cfg;
	priv_flags = cfg->flags;
	src_ip = &cfg->saddr;

	if (priv_flags & VXLAN_F_IPV6) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_IPV6;
		memcpy(l3->saddr, &src_ip->sin6.sin6_addr, sizeof(struct in6_addr));
		memcpy(l3->daddr, &rip->sin6.sin6_addr, sizeof(struct in6_addr));

		if (priv_flags & VXLAN_F_UDP_ZERO_CSUM6_TX) {
			l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_UDP_ZERO_CSUM_TX;
		}

		if (priv_flags & VXLAN_F_UDP_ZERO_CSUM6_RX) {
			l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_UDP_ZERO_CSUM6_RX;
		}

		if (ipv6_addr_any(&src_ip->sin6.sin6_addr)) {
			/*
			 * Lookup
			 */
			memset(&fl6, 0, sizeof(fl6));
			fl6.flowi6_proto = IPPROTO_UDP;
			fl6.daddr = rip->sin6.sin6_addr;
			fl6.saddr = src_ip->sin6.sin6_addr;
			memcpy(l3->saddr, &src_ip->sin6.sin6_addr, sizeof(struct in6_addr));
			memcpy(l3->daddr, &rip->sin6.sin6_addr, sizeof(struct in6_addr));

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 5, 7))
			err = ipv6_stub->ipv6_dst_lookup(priv->net,
					priv->vn6_sock->sock->sk, &dst, &fl6);
			if (err < 0) {
#else
			dentry = ipv6_stub->ipv6_dst_lookup_flow(priv->net,
					priv->vn6_sock->sock->sk, &fl6, final_dst);
			if (!dentry) {
#endif
				nss_ppe_vxlanmgr_warn("%px: No route, drop packet.\n", dev);
				return false;
			}
			return true;
		}
	} else {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_IPV4;
		l3->saddr[0] = src_ip->sin.sin_addr.s_addr;
		l3->daddr[0] = rip->sin.sin_addr.s_addr;

		if (priv_flags & VXLAN_F_UDP_ZERO_CSUM_TX) {
			l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_UDP_ZERO_CSUM_TX;
		}

		if (src_ip->sin.sin_addr.s_addr == htonl(INADDR_ANY)) {
			/*
			 * Lookup
			 */
			memset(&fl4, 0, sizeof(fl4));
			fl4.flowi4_proto = IPPROTO_UDP;
			fl4.daddr = rip->sin.sin_addr.s_addr;
			fl4.saddr = src_ip->sin.sin_addr.s_addr;
			l3->saddr[0] = src_ip->sin.sin_addr.s_addr;
			l3->daddr[0] = rip->sin.sin_addr.s_addr;

			rt = ip_route_output_key(priv->net, &fl4);
			if (IS_ERR(rt)) {
				nss_ppe_vxlanmgr_warn("%px: No route available", dev);
				return false;
			}
		}
	}
	return true;
}

/*
 * nss_ppe_vxlanmgr_tunnel_fdb_event()
 *	Event handler for VXLAN fdb updates.
 */
static int nss_ppe_vxlanmgr_switchdev_fdb_event(struct notifier_block *nb_unused, unsigned long event, void *data)
{
	struct switchdev_notifier_vxlan_fdb_info *fdb_info;
	struct switchdev_notifier_info *info = data;
	struct nss_ppe_vxlanmgr_rtm_neigh_event_data *rtm_neigh_event_data;
	struct vxlan_dev *priv;
	struct net_device *dev;
	bool restart_work = false;

	if (!((event == SWITCHDEV_VXLAN_FDB_ADD_TO_DEVICE) || (event == SWITCHDEV_VXLAN_FDB_DEL_TO_DEVICE))) {
		return NOTIFY_DONE;
	}

	fdb_info = container_of(info, struct switchdev_notifier_vxlan_fdb_info, info);
	if (!fdb_info) {
		nss_ppe_vxlanmgr_warn("%px: VXLAN FDB information not present", info);
		return NOTIFY_DONE;
	}

	dev = info->dev;
	if (!netif_is_vxlan(dev)) {
		nss_ppe_vxlanmgr_warn("%px: It is not VXLAN netdevice dev:%s", info, dev->name);
		return NOTIFY_DONE;
	}

	/*
	 * Check if VxLAN-GPE, it is used to encap L3 packet and fdb/neigh event is not applicable.
	 */
	priv = netdev_priv(dev);
	if (priv->cfg.flags & VXLAN_F_GPE) {
		nss_ppe_vxlanmgr_trace("%px: Netdevice with VXLAN-GPE configured", fdb_info);
		return NOTIFY_DONE;
	}

	if (dstport != ntohs(priv->cfg.dst_port)) {
		nss_ppe_vxlanmgr_trace("%px: VXLAN: configured PPE dport: %u is not-equal to user given dport:%dn", fdb_info, dstport, ntohs(priv->cfg.dst_port));
		return NOTIFY_DONE;
	}

	if (is_zero_ether_addr(fdb_info->eth_addr)) {
		nss_ppe_vxlanmgr_warn("%px: received a zero mac address", fdb_info);
		return NOTIFY_DONE;
	}

	rtm_neigh_event_data = kzalloc(sizeof(struct nss_ppe_vxlanmgr_rtm_neigh_event_data), GFP_ATOMIC);
	if (!rtm_neigh_event_data) {
		nss_ppe_vxlanmgr_warn("%px: Alloc failed for rtm_neigh_event_data", fdb_info);
		return NOTIFY_DONE;
	}

	nss_ppe_vxlanmgr_trace("%px: FDB event received %s", fdb_info, (event==SWITCHDEV_VXLAN_FDB_ADD_TO_DEVICE)?"SWITCHDEV_VXLAN_FDB_ADD_TO_DEVICE":"SWITCHDEV_VXLAN_FDB_DEL_TO_DEVICE" );

	if (fdb_info->remote_ip.sa.sa_family == AF_INET6) {
		nss_ppe_vxlanmgr_trace("%px: Remote_ip: %pI6h",fdb_info, &fdb_info->remote_ip.sin6.sin6_addr);
	} else {
		nss_ppe_vxlanmgr_trace("%px: Remote_ip: %pI4h",fdb_info, &fdb_info->remote_ip.sin.sin_addr.s_addr);
	}
	nss_ppe_vxlanmgr_trace("%px: MAC address: %pM",fdb_info, fdb_info->eth_addr);


	rtm_neigh_event_data->event = event;
	rtm_neigh_event_data->parent_netdev = dev;
	memcpy(&rtm_neigh_event_data->rip, &fdb_info->remote_ip, sizeof(union vxlan_addr));

	/*
	 * The list is empty, so we need to restart the work queue
	 */
	spin_lock_bh(&rtm_neigh_event_list_lock);
	if (list_empty(&rtm_neigh_event_list)) {
		restart_work = true;
	}

	/*
	 * Add the work at the tail of the list
	 */
	list_add_tail(&rtm_neigh_event_data->rtm_event_list, &rtm_neigh_event_list);
	spin_unlock_bh(&rtm_neigh_event_list_lock);

	if (restart_work) {
		queue_work(nss_ppe_vxlanmgr_rtm_neigh_event_wq, &rtm_neigh_event_work);
		nss_ppe_vxlanmgr_trace("%px: RTM_EVENT_WORK: started the work-queue", dev);
	}

	return NOTIFY_DONE;
}

/*
 * Notifier to receive fdb events from VxLAN
 */
struct notifier_block nss_ppe_vxlanmgr_switchdev_fdb_notifier = {
	.notifier_call = nss_ppe_vxlanmgr_switchdev_fdb_event,
};

/*
 * nss_ppe_vxlan_src_exception()
 * handle the source VP exception.
 */
static bool nss_ppe_vxlan_src_exception(struct ppe_vp_cb_info *info, ppe_tun_data *tun_data)
{
	struct sk_buff *skb = info->skb;
	struct net_device *dev = skb->dev;

	nss_ppe_vxlanmgr_warn("%px: Dropping the skb for dev:%s", dev, dev->name);
	dev_kfree_skb_any(skb);

	return 0;
}

/*
 * nss_ppe_vxlanmgr_tunnel_header_config()
 *	Configure the VXLAN tunnel header.
 */
static bool nss_ppe_vxlanmgr_tunnel_header_config(struct net_device *dev, struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx)
{
	bool ret;
	struct ppe_drv_tun_cmn_ctx *tun_hdr = tun_ctx->tun_hdr;
	struct ppe_tun_excp tun_cb = {0};

	tun_hdr->type = PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN;
	tun_hdr->l3.ttl = tun_ctx->ttl;
	tun_hdr->tun.vxlan.vni = tun_ctx->vni;
	tun_hdr->tun.vxlan.flags = tun_ctx->tunnel_flags;
	tun_hdr->tun.vxlan.src_port_min = tun_ctx->src_port_min;
	tun_hdr->tun.vxlan.src_port_max = tun_ctx->src_port_max;
	tun_hdr->tun.vxlan.dest_port = tun_ctx->dest_port;
	tun_hdr->tun.vxlan.u.policy_id = 0;
	tun_hdr->l3.proto = IPPROTO_UDP;
	tun_hdr->l3.dscp = NSS_PPE_VXLAN_MGR_O_DSCP_GET(tun_ctx->tos, 2);
	tun_hdr->type = PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN;

	tun_cb.src_excp_method = nss_ppe_vxlan_src_exception;
	tun_cb.stats_update_method = nss_ppe_vxlan_dev_stats_update;

	ret = ppe_tun_configure(dev, tun_hdr, &tun_cb);

	nss_ppe_vxlanmgr_info("%px: destport: tun_hdr->tun.vxlan.dest_port:%u tunnel_flags:%u nss_dev_name:%s", dev, tun_hdr->tun.vxlan.dest_port, tun_ctx->tunnel_flags, dev->name);

	return ret;
}

/*
 * nss_ppe_vxlanmgr_tunnel_configure()
 *	Configure VXLAN tunnel.
 */
static int nss_ppe_vxlanmgr_tunnel_configure(struct net_device *dev, struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx)
{
	struct vxlan_dev *priv;
	uint32_t priv_flags;
	struct ppe_drv_tun_cmn_ctx_l3 *l3;

	priv = netdev_priv(dev);

	if (!nss_ppe_vxlanmgr_tun_stats_dentry_create(tun_ctx)) {
		nss_ppe_vxlanmgr_warn("%px: Tun stats dentry init failed\n", dev);
		return -1;
	}

	/*
	 * The EG-header data should be pushed to the PPE in Big-endian format.
	 * The vxlan_dev structue has the contents in the Big-Endian format.
	 */
	tun_ctx->vni = vxlan_vni_field(priv->cfg.vni);
	tun_ctx->tunnel_flags = VXLAN_HF_VNI;
	tun_ctx->src_port_min = priv->cfg.port_min;
	tun_ctx->src_port_max = priv->cfg.port_max;
	tun_ctx->dest_port = priv->cfg.dst_port;
	tun_ctx->tos = priv->cfg.tos;
	tun_ctx->ttl = (priv->cfg.ttl ? priv->cfg.ttl : IPDEFTTL);

	l3 = &tun_ctx->tun_hdr->l3;

	priv_flags = priv->cfg.flags;
	if ((priv_flags & VXLAN_F_TTL_INHERIT) || inherit_ttl) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_TTL;
	}

	if (inherit_dscp) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP;
	}

	if (encap_ecn_mode <= PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_RFC4301_RFC6040_NORMAL_MODE) {
		l3->encap_ecn_mode = encap_ecn_mode;
	}

	if (decap_ecn_mode <= PPE_DRV_TUN_CMN_CTX_DECAP_ECN_RFC6040_MODE) {
		l3->decap_ecn_mode = decap_ecn_mode;
	}

	return 0;
}

/*
 * nss_ppe_vxlanmgr_tunnel_create()
 *	Create VXLAN tunnel.
 */
static int nss_ppe_vxlanmgr_tunnel_create(struct net_device *dev, struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx)
{
	if (!ppe_tun_alloc(dev, PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN)) {
		nss_ppe_vxlanmgr_warn("%px: PPE tunnel creation failed.", dev);
		return -1;
	}

	return 0;
}

/*
 * nss_ppe_vxlanmgr_rtm_newneigh_handler()
 *	New client joining the remote event from the kernel.
 */
static void nss_ppe_vxlanmgr_rtm_newneigh_handler(struct nss_ppe_vxlanmgr_rtm_neigh_event_data *rtm_newneigh_info)
{
	int status;
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;
	struct net_device *nss_netdev;
	struct net_device *pdev = rtm_newneigh_info->parent_netdev;
	struct nss_ppe_vxlanmgr_remote_info *remote_info;
	struct nss_ppe_vxlanmgr_nss_dev_priv *nss_netdev_priv;
	struct vxlan_dev *pdev_priv;
	__be32 vni;

	nss_ppe_vxlanmgr_trace("%px: parent_netdev: %s", rtm_newneigh_info, pdev->name);

	dev_hold(pdev);
	pdev_priv = netdev_priv(pdev);
	vni = vxlan_vni_field(pdev_priv->cfg.vni);
	if (!nss_ppe_vxlanmgr_new_remote(vni, &rtm_newneigh_info->rip)) {
		nss_ppe_vxlanmgr_trace("%px: Remote already present in the hash. Its a known remote!", rtm_newneigh_info);
		tun_ctx = nss_ppe_vxlanmgr_get_tun_ctx_by_vni_and_rip(vni, &rtm_newneigh_info->rip);
		if (tun_ctx) {
			kref_get(&tun_ctx->remote_info.mac_address_ref);
			dev_put(pdev);
			return;
		}
		nss_ppe_vxlanmgr_warn("%px: failed to get the tunnel context", rtm_newneigh_info);
		dev_put(pdev);
		return;
	}

	/*
	 * New remote.
	 */
	if (nss_ppe_vxlanmgr_get_remote_count(pdev, vni) == NSS_PPE_VXLANMGR_MAX_REMOTES) {
		nss_ppe_vxlanmgr_warn("%px: Max number of remotes exist already for the dev:%s", rtm_newneigh_info, pdev->name);
		dev_put(pdev);
		return;
	}

	nss_ppe_vxlanmgr_trace("%px: Tunnel will be created for the new remote", rtm_newneigh_info);

	/*
	 * Allocate ppe tunnel context.
	 */
	tun_ctx = kzalloc(sizeof(struct nss_ppe_vxlanmgr_tun_ctx), GFP_KERNEL);
	if (!tun_ctx) {
		nss_ppe_vxlanmgr_warn("%px: Failed to allocate memory for tun_ctx", rtm_newneigh_info);
		dev_put(pdev);
		return;
	}

	pdev_priv = netdev_priv(pdev);
	tun_ctx->vni = vxlan_vni_field(pdev_priv->cfg.vni);
	tun_ctx->parent_dev = pdev;
	remote_info = &tun_ctx->remote_info;
	memcpy(&remote_info->remote_ip, &rtm_newneigh_info->rip, sizeof(union vxlan_addr));
	tun_ctx->vp_status = NSS_PPE_VXLANMGR_VP_CREATION_IN_PROGRESS;

	kref_init(&remote_info->mac_address_ref);

	/*
	 * Allocate child/dummy nss-netdevice.
	 */
	nss_netdev = alloc_netdev(sizeof(struct nss_ppe_vxlanmgr_nss_dev_priv), "ppe_vxlantun%d", NET_NAME_UNKNOWN, nss_ppe_vxlanmgr_nss_netdev_setup);

	if (!nss_netdev) {
		nss_ppe_vxlanmgr_warn("%px: Failed to allocate nss netdev", rtm_newneigh_info);
		goto vp_failure;
	}

	nss_netdev->netdev_ops = &nss_ppe_vxlanmgr_nss_netdev_ops;
	nss_netdev_priv = netdev_priv(nss_netdev);
	nss_netdev_priv->pdev_ifindex = pdev->ifindex;

	rtnl_lock();
	status = register_netdevice(nss_netdev);
	if (status) {
		nss_ppe_vxlanmgr_warn("%px: VXLAN nss-netdev register Failed.", rtm_newneigh_info);
		rtnl_unlock();
		goto dealloc_netdev;
	}

	rtnl_unlock();

	/*
	 * Allocate PPE tunnel.
	 */
	if (nss_ppe_vxlanmgr_tunnel_create(nss_netdev, tun_ctx) < 0) {
		nss_ppe_vxlanmgr_warn("%px: failed to create tunnel using nss-netdev", rtm_newneigh_info);
		goto unregister_netdev;
	}

	remote_info->nss_netdev = nss_netdev;
	remote_info->pdev = pdev;

	/*
	 * Allocate PPE tunnel header.
	 */
	tun_ctx->tun_hdr = kzalloc(sizeof(struct ppe_drv_tun_cmn_ctx), GFP_KERNEL);
	if (!tun_ctx->tun_hdr) {
		nss_ppe_vxlanmgr_warn("%px: Failed to allocate memory for tun_hdr", rtm_newneigh_info);
		goto dealloc_tunnel;
	}

	/*
	 * configure PPE tunnel in VXLANMgr locally.
	 */
	if (nss_ppe_vxlanmgr_tunnel_configure(pdev, tun_ctx) < 0) {
		nss_ppe_vxlanmgr_warn("%px: failed to configure tunnel", rtm_newneigh_info);
		goto dealloc_tun_hdr;
	}

	ppe_tun_mtu_set(nss_netdev, pdev->mtu);
	nss_ppe_vxlanmgr_tunnel_parse_end_points(pdev, tun_ctx->tun_hdr, &rtm_newneigh_info->rip);

	/*
	 * configure PPE tunnel header.
	 */
	if (!nss_ppe_vxlanmgr_tunnel_header_config(nss_netdev, tun_ctx)) {
		nss_ppe_vxlanmgr_trace("%px: Successfully created and configured the nss-netdevice %s", rtm_newneigh_info, nss_netdev->name);
		goto dealloc_tun_hdr;
	}

	/*
	 * Enable Decap and Join nssdev to bridge.
	 */
	nss_ppe_vxlanmgr_decap_enable(pdev, nss_netdev);
	if (!nss_ppe_vxlanmgr_br_join_nss_netdev(tun_ctx)) {
		nss_ppe_vxlanmgr_warn("%px: Failed to join the nss_dev:%s to the bridge. pdev:%s", rtm_newneigh_info, nss_netdev->name, pdev->name);
		goto deconfig_tun_hdr;
	}

	tun_ctx->vp_status = NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS;
	nss_ppe_vxlanmgr_tunnel_ctx_attach(tun_ctx);
	dev_put(pdev);

	return;

deconfig_tun_hdr:
	ppe_tun_deconfigure(nss_netdev);

dealloc_tun_hdr:
	kfree(tun_ctx->tun_hdr);

dealloc_tunnel:
	ppe_tun_free(nss_netdev);

unregister_netdev:
	rtnl_lock();
	unregister_netdevice(nss_netdev);
	rtnl_unlock();

dealloc_netdev:
	free_netdev(nss_netdev);

vp_failure:
	/*
	 * Hash add will be done even in the case of VP failure.This is because, ECM-module will fetch the vp_status from the hash table.
	 * And hence hash table will have vp_status as SUCCESS or FAILURE, but not IN_PROGRESS.
	 * At this time only the vp_status, pdev, remote_ip will be filled.
	 */
	tun_ctx->vp_status = NSS_PPE_VXLANMGR_VP_CREATION_FAILED;
	nss_ppe_vxlanmgr_tunnel_ctx_attach(tun_ctx);
	dev_put(pdev);
}

/*
 * nss_ppe_vxlanmgr_rtm_delneigh_handler()
 *	Delete client leaving the remote event from the kernel.
 */
static void nss_ppe_vxlanmgr_rtm_delneigh_handler(struct nss_ppe_vxlanmgr_rtm_neigh_event_data *rtm_delneigh_info)
{
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;
	struct net_device *pdev = rtm_delneigh_info->parent_netdev;
	struct vxlan_dev *priv;
	__be32 vni;

	nss_ppe_vxlanmgr_trace("%px: Executing the SWITCHDEV_VXLAN_FDB_DEL_TO_DEVICE handler pdev:%s", rtm_delneigh_info, pdev->name);

	/*
	 * Remote that does-not exists in the database.
	 */
	dev_hold(pdev);
	priv = netdev_priv(pdev);
	vni = vxlan_vni_field(priv->cfg.vni);
	if (nss_ppe_vxlanmgr_new_remote(vni, &rtm_delneigh_info->rip)) {
		nss_ppe_vxlanmgr_trace("%px: It is the new remote. pdev:%s", rtm_delneigh_info, pdev->name);
		dev_put(pdev);
		return;
	}

	nss_ppe_vxlanmgr_trace("%px: Remote already present in the hash. Its a known remote!", rtm_delneigh_info);
	tun_ctx = nss_ppe_vxlanmgr_get_tun_ctx_by_vni_and_rip(vni, &rtm_delneigh_info->rip);
	if (!tun_ctx) {
		nss_ppe_vxlanmgr_warn("%px: failed to get the tunnel context", rtm_delneigh_info);
		dev_put(pdev);
		return;
	}

	kref_put(&tun_ctx->remote_info.mac_address_ref, nss_ppe_vxlanmgr_delete_remote);
	dev_put(pdev);
}

/*
 * nss_ppe_vxlanmgr_rtm_event_handler()
 *	New client joining/leaving the remote event from the kernel.
 */
static void nss_ppe_vxlanmgr_rtm_event_handler(struct work_struct *neigh_event_work)
{
	struct nss_ppe_vxlanmgr_rtm_neigh_event_data *rtm_neigh_event_info;
	uint8_t event;

	/*
	 * Dequeue the work from the head of the list and process the statistics sync for it
	 */
	spin_lock_bh(&rtm_neigh_event_list_lock);

	if (list_empty(&rtm_neigh_event_list)) {
		spin_unlock_bh(&rtm_neigh_event_list_lock);
		return;
	}

	rtm_neigh_event_info = list_first_entry(&rtm_neigh_event_list, struct nss_ppe_vxlanmgr_rtm_neigh_event_data, rtm_event_list);

	list_del(&rtm_neigh_event_info->rtm_event_list);

	spin_unlock_bh(&rtm_neigh_event_list_lock);

	event = rtm_neigh_event_info->event;
	if (event == SWITCHDEV_VXLAN_FDB_ADD_TO_DEVICE) {
		nss_ppe_vxlanmgr_rtm_newneigh_handler(rtm_neigh_event_info);
	} else {
		nss_ppe_vxlanmgr_rtm_delneigh_handler(rtm_neigh_event_info);
	}

	kfree(rtm_neigh_event_info);
	queue_work(nss_ppe_vxlanmgr_rtm_neigh_event_wq, &rtm_neigh_event_work);
}

/*
 * nss_ppe_vxlanmgr_delete_all_remotes()
 *	Delete all the remotes that are created so far.
 */
void nss_ppe_vxlanmgr_delete_all_remotes()
{
	struct nss_ppe_vxlanmgr_tun_ctx *curr_tun_ctx;
	unsigned bkt;
	struct hlist_node *temp;

	spin_lock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
	hash_for_each_safe(nss_ppe_vxlanmgr_tunnel_tbl, bkt, temp, curr_tun_ctx, node) {
		hash_del(&curr_tun_ctx->node);
		nss_ppe_vxlanmgr_tunnel_destroy(curr_tun_ctx, curr_tun_ctx->remote_info.nss_netdev);
	}
	spin_unlock_bh(&nss_ppe_vxlanmgr_tunnel_tbl_lock);
}

/*
 * nss_ppe_vxlanmgr_wq_exit()
 *	Destroy the work_queue
 */
int nss_ppe_vxlanmgr_wq_exit(void)
{
	destroy_workqueue(nss_ppe_vxlanmgr_rtm_neigh_event_wq);
	return 0;
}

/*
 * nss_ppe_vxlanmgr_wq_init()
 *	Initialize the work_queue
 */
int nss_ppe_vxlanmgr_wq_init(void)
{
	nss_ppe_vxlanmgr_rtm_neigh_event_wq = create_singlethread_workqueue("wq_rtm_newneigh");

	if (!nss_ppe_vxlanmgr_rtm_neigh_event_wq){
		nss_ppe_vxlanmgr_warn("work queue allocation failed for VXLAN switchdev events");
		return -1;
	}

	INIT_WORK(&rtm_neigh_event_work, nss_ppe_vxlanmgr_rtm_event_handler);

	return 0;
}
