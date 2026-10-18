/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "nss_ppe_vxlanmgr_priv.h"
#include "nss_ppe_vxlanmgr_tun_stats.h"
#include "ppe_drv_tun_cmn_ctx.h"
#include "ppe_drv_iface.h"

/*
 * FIB update event list.
 */
static LIST_HEAD(fib_event_list);
static DEFINE_SPINLOCK(fib_event_list_lock);

static struct work_struct fib_event_work;			/* Work queue */
static struct workqueue_struct *nss_ppe_vxlanmgr_fib_event_wq;
static const struct net_device_ops nss_ppe_vxlanmgr_gpe_nss_netdev_ops;

/*
 * nss_ppe_vxlanmgr_gpe_nss_netdev_setup_dummy()
 *	Setup nss_netdevice
 */
static void nss_ppe_vxlanmgr_gpe_nss_netdev_setup_dummy(struct net_device *nss_dev)
{
	/*
	 * Not doing anything here.
	 */
	nss_ppe_vxlanmgr_trace("%px: setting the nss_dev: %s\n", nss_dev, nss_dev->name);
}

/*
 * nss_ppe_vxlanmgr_gpe_decap_enable()
 *	Nss-netdevice enable decap.
 */
static bool nss_ppe_vxlanmgr_gpe_decap_enable(struct net_device *pdev, struct net_device *nss_dev)
{
	if (!ppe_tun_decap_enable(nss_dev)) {
		nss_ppe_vxlanmgr_warn("%px: Failed enabling decap for nss-netdev. nss_dev:%s\n", pdev, nss_dev->name);
		return false;
	}

	nss_ppe_vxlanmgr_trace("%px: Successfully enabled decap for the nss-netdev. nss_dev:%s pdev:%s\n", pdev, nss_dev->name, pdev->name);
	return true;
}

/*
 * nss_ppe_vxlanmgr_gpe_tunnel_destroy()
 *	Function to unregister and destroy the PPE tunnel using tunnel context.
 */
static void nss_ppe_vxlanmgr_gpe_tunnel_destroy(struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx, struct net_device *dev)
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
 * nss_ppe_vxlanmgr_gpe_delete_remote()
 *	Delete the VXLAN-GPE remote.
 */
static void nss_ppe_vxlanmgr_gpe_delete_remote(struct kref *kref)
{
	struct nss_ppe_vxlanmgr_remote_info *r_info;
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;
	struct net_device *nss_dev, *pdev;

	r_info = container_of(kref, struct nss_ppe_vxlanmgr_remote_info, mac_address_ref);
	nss_dev = r_info->nss_netdev;
	pdev = r_info->pdev;

	nss_ppe_vxlanmgr_trace("%px: deleting the Remote pdev:%s nss_dev:%s\n", pdev, pdev->name, nss_dev->name);

	tun_ctx = nss_ppe_vxlanmgr_tunnel_ctx_get_and_dettach(nss_dev);
	if (!tun_ctx) {
		nss_ppe_vxlanmgr_warn("%px: Failed to get tunnel context. Invalid tunnel context\n", nss_dev);
		return;
	}

	ppe_tun_decap_disable(nss_dev);
	nss_ppe_vxlanmgr_gpe_tunnel_destroy(tun_ctx, nss_dev);
}

/*
 * nss_ppe_vxlanmgr_gpe_tunnel_parse_end_points()
 *	VxLAN-GPE tunnel parse src and dst addresses.
 */
static bool nss_ppe_vxlanmgr_gpe_tunnel_parse_end_points(struct net_device *dev, struct ppe_drv_tun_cmn_ctx *tun_hdr,
		union vxlan_addr *rip, union vxlan_addr *sip, bool udp_csum)
{
	struct vxlan_dev *priv;
	struct ppe_drv_tun_cmn_ctx_l3 *l3 = &tun_hdr->l3;

	if (!udp_csum) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_UDP_ZERO_CSUM_TX;
	}

	priv = netdev_priv(dev);
	if (rip->sa.sa_family == AF_INET) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_IPV4;
		l3->saddr[0] = sip->sin.sin_addr.s_addr;
		l3->daddr[0] = rip->sin.sin_addr.s_addr;

		/*
		 * Lookup to get the source address if not specified
		 */
		if (sip->sin.sin_addr.s_addr == htonl(INADDR_ANY)) {
			struct flowi4 fl4;
			struct rtable *rt = NULL;

			memset(&fl4, 0, sizeof(fl4));
			fl4.flowi4_proto = IPPROTO_UDP;
			fl4.daddr = rip->sin.sin_addr.s_addr;
			fl4.saddr = sip->sin.sin_addr.s_addr;

			rt = ip_route_output_key(priv->net, &fl4);
			if (IS_ERR(rt)) {
				nss_ppe_vxlanmgr_warn("%px: [IPv4] No route available\n", dev);
				return false;
			}

			l3->saddr[0] = fl4.saddr;
			nss_ppe_vxlanmgr_warn("%px: [IPv4] src ip: %pI4, dst ip: %pI4\n", dev, &l3->saddr, &l3->daddr);
		}
	} else {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_IPV6;
		memcpy(l3->saddr, &sip->sin6.sin6_addr, sizeof(struct in6_addr));
		memcpy(l3->daddr, &rip->sin6.sin6_addr, sizeof(struct in6_addr));

		/*
		 * Use the zero checksum rx flag for IPv6 from host netdevice
		 */
		if (priv->cfg.flags & VXLAN_F_UDP_ZERO_CSUM6_RX) {
			l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_UDP_ZERO_CSUM6_RX;
		}

		/*
		 * Lookup to get the source address if not specified
		 */
		if (ipv6_addr_any(&sip->sin6.sin6_addr)) {
			struct flowi6 fl6;
			struct dst_entry *dentry;

			memset(&fl6, 0, sizeof(fl6));
			fl6.flowi6_proto = IPPROTO_UDP;
			fl6.daddr = rip->sin6.sin6_addr;
			fl6.saddr = sip->sin6.sin6_addr;

			dentry = ipv6_stub->ipv6_dst_lookup_flow(priv->net, priv->vn6_sock->sock->sk, &fl6, NULL);
			if (!dentry) {
				nss_ppe_vxlanmgr_warn("%px: [IPv6] No route available.\n", dev);
				return false;
			}

			memcpy(l3->saddr, &fl6.saddr, sizeof(struct in6_addr));
			nss_ppe_vxlanmgr_warn("%px: [IPv6] src ip: %pI6, dst ip: %pI6\n", dev, &l3->saddr, &l3->daddr);
		}
	}

	return true;
}

/*
 * nss_ppe_vxlanmgr_gpe_src_exception()
 *	Handle the source VP exception.
 */
static bool nss_ppe_vxlanmgr_gpe_src_exception(struct ppe_vp_cb_info *info, ppe_tun_data *tun_data)
{
	struct sk_buff *skb = info->skb;
	const struct iphdr *iph;
	int ret;

	skb_reset_network_header(skb);
	iph = (const struct iphdr *)skb->data;
	skb->protocol = (iph->version == IPVERSION) ? htons(ETH_P_IP) : htons(ETH_P_IPV6);

	/*
	 * Reset Skb flags
	 */
	skb->fast_xmit = 0;
	skb->fast_recycled = 0;
	skb->recycled_for_ds = 0;
	ret = netif_receive_skb(skb);
	if (ret != NET_RX_SUCCESS) {
		nss_ppe_vxlanmgr_trace("%p: exception packet dropped\n", skb->dev);
	}

	return true;
}

/*
 * nss_ppe_vxlanmgr_gpe_tunnel_update_l3_if_config()
 *	Configure PPE L3 interface to forward packet when udp csum is zero.
 */
static bool nss_ppe_vxlanmgr_gpe_tunnel_update_l3_if_config(struct net_device *dev)
{
	struct ppe_drv_iface *iface;

	iface = ppe_drv_iface_get_by_dev(dev);
	if (!iface) {
		nss_ppe_vxlanmgr_warn("%px: Failed to find iface.\n", dev);
		return false;
	}

	return ppe_drv_iface_udp_zero_csum_action_set(iface, PPE_DRV_IFACE_ZERO_CSUM_ACTION_FRWRD);
}

/*
 * nss_ppe_vxlanmgr_gpe_tunnel_header_config()
 *	Configure the VXLAN tunnel header.
 */
static void nss_ppe_vxlanmgr_gpe_tunnel_header_config(struct net_device *dev, struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx)
{
	struct ppe_drv_tun_cmn_ctx *tun_hdr = tun_ctx->tun_hdr;
	struct ppe_drv_tun_cmn_ctx_l3 *tun_hdr_l3 = &tun_hdr->l3;
	struct vxlan_dev *priv;
	uint32_t priv_flags;

	priv = netdev_priv(dev);

	/*
	 * The EG-header data should be pushed to the PPE in Big-endian format.
	 * The vxlan_dev structue has the contents in the Big-Endian format.
	 */
	tun_hdr->type = PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN_GPE;
	tun_hdr->tun.vxlan.vni = tun_ctx->vni;
	tun_hdr->tun.vxlan.flags = VXLAN_HF_VNI | VXLAN_HF_NP;
	tun_hdr->tun.vxlan.src_port_min = priv->cfg.port_min;
	tun_hdr->tun.vxlan.src_port_max = priv->cfg.port_max;
	tun_hdr->tun.vxlan.dest_port = priv->cfg.dst_port;
	tun_hdr->tun.vxlan.u.next_proto = 0; /* To be updated by PPE based on inner payload */

	tun_hdr_l3->proto = IPPROTO_UDP;
	tun_hdr_l3->ttl = (tun_ctx->ttl ? : IPDEFTTL);
	tun_hdr_l3->dscp = NSS_PPE_VXLAN_MGR_O_DSCP_GET(tun_ctx->tos, 2);

	/*
	 * Update details in tunnel context also which can be accessed in debugfs.
	 */
	tun_ctx->tunnel_flags = tun_hdr->tun.vxlan.flags;
	tun_ctx->src_port_min = priv->cfg.port_min;
	tun_ctx->src_port_max = priv->cfg.port_max;
	tun_ctx->dest_port = priv->cfg.dst_port;
	tun_ctx->ttl = tun_hdr_l3->ttl;

	priv_flags = priv->cfg.flags;
	if ((priv_flags & VXLAN_F_TTL_INHERIT) || inherit_ttl) {
		tun_hdr_l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_TTL;
	}

	if (inherit_dscp) {
		tun_hdr_l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP;
	}

	if (encap_ecn_mode <= PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_RFC4301_RFC6040_NORMAL_MODE) {
		tun_hdr_l3->encap_ecn_mode = encap_ecn_mode;
	}

	if (decap_ecn_mode <= PPE_DRV_TUN_CMN_CTX_DECAP_ECN_RFC6040_MODE) {
		tun_hdr_l3->decap_ecn_mode = decap_ecn_mode;
	}
}

/*
 * nss_ppe_vxlanmgr_fib_add_event_handler()
 *	Handler for FIB add event from the kernel.
 */
static void nss_ppe_vxlanmgr_fib_add_event_handler(struct nss_ppe_vxlanmgr_fib_event_data *fib_newneigh_info)
{
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;
	struct net_device *nss_netdev;
	struct net_device *pdev = fib_newneigh_info->parent_netdev;
	struct nss_ppe_vxlanmgr_remote_info *remote_info;
	struct nss_ppe_vxlanmgr_nss_dev_priv *nss_netdev_priv;
	struct ppe_tun_excp tun_cb = {0};
	struct vxlan_dev *pdev_priv;
	bool udp_csum;
	int status;

	/*
	 * Check if a new PPE tunnel context creation is required for the received event
	 * TODO: Since the FIB event is same for add/append/replace, it could be possible
	 * that details for existing tunnel is modified -- HANDLE THIS CASE
	 */
	dev_hold(pdev);
	if (!nss_ppe_vxlanmgr_new_remote(fib_newneigh_info->vni, &fib_newneigh_info->rip)) {
		tun_ctx = nss_ppe_vxlanmgr_get_tun_ctx_by_vni_and_rip(fib_newneigh_info->vni, &fib_newneigh_info->rip);
		if (tun_ctx) {
			kref_get(&tun_ctx->remote_info.mac_address_ref);
			dev_put(pdev);
			return;
		}

		nss_ppe_vxlanmgr_warn("%px: failed to get the tunnel context\n", fib_newneigh_info);
		dev_put(pdev);
		return;
	}

	/*
	 * New remote.
	 */
	if (nss_ppe_vxlanmgr_get_remote_count(pdev, fib_newneigh_info->vni) == NSS_PPE_VXLANMGR_MAX_REMOTES) {
		nss_ppe_vxlanmgr_warn("%px: Max number of remotes exist already for the dev:%s\n", fib_newneigh_info, pdev->name);
		dev_put(pdev);
		return;
	}

	nss_ppe_vxlanmgr_trace("%px: Tunnel will be created for the new remote\n", fib_newneigh_info);

	/*
	 * Allocate ppe tunnel context.
	 */
	tun_ctx = kzalloc(sizeof(struct nss_ppe_vxlanmgr_tun_ctx), GFP_KERNEL);
	if (!tun_ctx) {
		nss_ppe_vxlanmgr_warn("%px: Failed to allocate memory for tun_ctx\n", fib_newneigh_info);
		dev_put(pdev);
		return;
	}

	pdev_priv = netdev_priv(pdev);
	tun_ctx->vni = fib_newneigh_info->vni;
	tun_ctx->ttl = fib_newneigh_info->ttl;
	tun_ctx->tos = fib_newneigh_info->tos;
	tun_ctx->parent_dev = pdev;
	remote_info = &tun_ctx->remote_info;
	memcpy(&remote_info->remote_ip, &fib_newneigh_info->rip, sizeof(union vxlan_addr));
	tun_ctx->vp_status = NSS_PPE_VXLANMGR_VP_CREATION_IN_PROGRESS;

	/*
	 * deref in nss_ppe_vxlanmgr_fib_del_event_handler
	 */
	kref_init(&remote_info->mac_address_ref);

	/*
	 * Allocate child/dummy nss-netdevice.
	 */
	nss_netdev = alloc_netdev(sizeof(struct nss_ppe_vxlanmgr_nss_dev_priv), "ppe_vxlantun%d", NET_NAME_UNKNOWN, nss_ppe_vxlanmgr_gpe_nss_netdev_setup_dummy);

	if (!nss_netdev) {
		nss_ppe_vxlanmgr_warn("%px: Failed to allocate nss netdev\n", fib_newneigh_info);
		goto vp_failure;
	}

	nss_netdev->netdev_ops = &nss_ppe_vxlanmgr_gpe_nss_netdev_ops;
	nss_netdev_priv = netdev_priv(nss_netdev);
	nss_netdev_priv->pdev_ifindex = pdev->ifindex;

	rtnl_lock();
	status = register_netdevice(nss_netdev);
	if (status) {
		nss_ppe_vxlanmgr_warn("%px: VXLAN nss-netdev register Failed.\n", fib_newneigh_info);
		rtnl_unlock();
		goto dealloc_netdev;
	}

	rtnl_unlock();

	/*
	 * Allocate PPE tunnel.
	 */
	if (!ppe_tun_alloc(nss_netdev, PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN_GPE)) {
		nss_ppe_vxlanmgr_warn("%px: PPE tunnel allocation failed.\n", nss_netdev);
		goto unregister_netdev;
	}

	nss_ppe_vxlanmgr_warn("%px: PPE tunnel allocation success.\n", nss_netdev);

	/*
	 * Create debugfs directory, this gets removed while destroying the tunnel.
	 */
	if (!nss_ppe_vxlanmgr_tun_stats_dentry_create(tun_ctx)) {
		nss_ppe_vxlanmgr_warn("%px: Tun stats dentry init failed\n", nss_netdev);
		goto dealloc_tunnel;
	}

	remote_info->nss_netdev = nss_netdev;
	remote_info->pdev = pdev;

	/*
	 * Allocate PPE tunnel header.
	 */
	tun_ctx->tun_hdr = kzalloc(sizeof(struct ppe_drv_tun_cmn_ctx), GFP_KERNEL);
	if (!tun_ctx->tun_hdr) {
		nss_ppe_vxlanmgr_warn("%px: Failed to allocate memory for tun_hdr\n", fib_newneigh_info);
		goto dealloc_tunnel;
	}

	tun_cb.src_excp_method = nss_ppe_vxlanmgr_gpe_src_exception;
	tun_cb.stats_update_method = nss_ppe_vxlan_dev_stats_update;
	udp_csum = !!(fib_newneigh_info->tun_flags & TUNNEL_CSUM);
	status = nss_ppe_vxlanmgr_gpe_tunnel_parse_end_points(pdev, tun_ctx->tun_hdr, &fib_newneigh_info->rip, &fib_newneigh_info->sip, udp_csum);
	if (!status) {
		nss_ppe_vxlanmgr_warn("%px: Failed to parse endpoints for the tunnel\n", fib_newneigh_info);
		goto dealloc_tun_hdr;
	}

	/*
	 * Set MTU for nss netdev and configure PPE tunnel for VxLAN-GPE.
	 */
	ppe_tun_mtu_set(nss_netdev, pdev->mtu);
	nss_ppe_vxlanmgr_gpe_tunnel_header_config(pdev, tun_ctx);
	if (!ppe_tun_configure(nss_netdev, tun_ctx->tun_hdr, &tun_cb)) {
		nss_ppe_vxlanmgr_warn("%px: Failed to configure PPE tunnel for nss-netdev: %s\n", fib_newneigh_info, nss_netdev->name);
		goto dealloc_tun_hdr;
	}

	/*
	 * Update PPE tunnel interface config.
	 */
	if (!nss_ppe_vxlanmgr_gpe_tunnel_update_l3_if_config(nss_netdev)) {
		nss_ppe_vxlanmgr_trace("%px: Failed to update PPE L3 interface config for %s\n", fib_newneigh_info, nss_netdev->name);
		goto deconfig_tun_hdr;
	}

	/*
	 * Enable Decap for nssdev.
	 */
	nss_ppe_vxlanmgr_gpe_decap_enable(pdev, nss_netdev);
	nss_ppe_vxlanmgr_trace("%px: Marking VP creation successful, vni: %X\n", fib_newneigh_info, tun_ctx->vni);

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
 * nss_ppe_vxlanmgr_fib_del_event_handler()
 *	Handler for FIB delete event from the kernel.
 */
static void nss_ppe_vxlanmgr_fib_del_event_handler(struct nss_ppe_vxlanmgr_fib_event_data *fib_delneigh_info)
{
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;
	struct net_device *pdev = fib_delneigh_info->parent_netdev;

	/*
	 * Remote that does-not exists in the database.
	 */
	dev_hold(pdev);
	if (nss_ppe_vxlanmgr_new_remote(fib_delneigh_info->vni, &fib_delneigh_info->rip)) {
		nss_ppe_vxlanmgr_trace("%px: It is the new remote. pdev:%s\n", fib_delneigh_info, pdev->name);
		dev_put(pdev);
		return;
	}

	nss_ppe_vxlanmgr_trace("%px: Remote already present in the hash. Its a known remote!\n", fib_delneigh_info);
	tun_ctx = nss_ppe_vxlanmgr_get_tun_ctx_by_vni_and_rip(fib_delneigh_info->vni, &fib_delneigh_info->rip);
	if (!tun_ctx) {
		nss_ppe_vxlanmgr_warn("%px: failed to get the tunnel context\n", fib_delneigh_info);
		dev_put(pdev);
		return;
	}

	kref_put(&tun_ctx->remote_info.mac_address_ref, nss_ppe_vxlanmgr_gpe_delete_remote);
	dev_put(pdev);
}

/*
 * nss_ppe_vxlanmgr_fib_update_event_handler()
 *	Handler for FIB update event received from kernel.
 */
static void nss_ppe_vxlanmgr_fib_update_event_handler(struct work_struct *neigh_event_work)
{
	struct nss_ppe_vxlanmgr_fib_event_data *fib_event_info;
	uint8_t event;

	/*
	 * Dequeue the work from the head of the list and process it
	 */
	spin_lock_bh(&fib_event_list_lock);

	if (list_empty(&fib_event_list)) {
		spin_unlock_bh(&fib_event_list_lock);
		return;
	}

	fib_event_info = list_first_entry(&fib_event_list, struct nss_ppe_vxlanmgr_fib_event_data, fib_event_list);

	list_del(&fib_event_info->fib_event_list);

	spin_unlock_bh(&fib_event_list_lock);

	event = fib_event_info->event;
	if (event == FIB_EVENT_ENTRY_REPLACE) {
		nss_ppe_vxlanmgr_trace("FIB add/append/replace event received: %X\n", event);
		nss_ppe_vxlanmgr_fib_add_event_handler(fib_event_info);
	} else {
		nss_ppe_vxlanmgr_trace("FIB delete event received: %X\n", event);
		nss_ppe_vxlanmgr_fib_del_event_handler(fib_event_info);
	}

	kfree(fib_event_info);
	queue_work(nss_ppe_vxlanmgr_fib_event_wq, &fib_event_work);
}

/*
 * nss_ppe_vxlanmgr_fib_update_event()
 *	This is a call back for FIB table update event.
 */
static int nss_ppe_vxlanmgr_fib_update_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct nss_ppe_vxlanmgr_fib_event_data *fib_event_data;
	struct fib_notifier_info *info = ptr;
	struct lwtunnel_state *lwtstate;
	struct ip_tunnel_info *tun_info;
	bool restart_work = false;
	struct net_device *dev;
	struct vxlan_dev *priv;
	__be32 vni = 0;

	/*
	 * FIB update event from kernel is same for route add/replace/append which is
	 * of type 'FIB_EVENT_ENTRY_REPLACE'.
	 */
	if (event != FIB_EVENT_ENTRY_REPLACE && event != FIB_EVENT_ENTRY_DEL) {
		nss_ppe_vxlanmgr_warn("%px: Unsupported event type: [%lu] received \n", info, event);
		return NOTIFY_DONE;
	}

	nss_ppe_vxlanmgr_trace("%px: Event type: [%lu] received", info, event);

	if (info->family == AF_INET) {
		struct fib_entry_notifier_info *fen_info;
		struct fib_nh *nh;

		fen_info = container_of(info, struct fib_entry_notifier_info, info);
		nh = &fen_info->fi->fib_nh[0];
		if (!nh) {
			nss_ppe_vxlanmgr_warn("%px: Next hop entry for IPv4 is NULL \n", info);
			return NOTIFY_DONE;
		}

		dev = nh->fib_nh_dev;
		lwtstate = nh->fib_nh_lws;
		nss_ppe_vxlanmgr_trace("IPv4: event for prefix: %pI4, prefix_len: %d \n", &fen_info->dst, fen_info->dst_len);
	} else if (info->family == AF_INET6) {
		struct fib6_entry_notifier_info *fen_info6;
		struct fib6_nh *nh6;

		fen_info6 = container_of(info, struct fib6_entry_notifier_info, info);
		nh6 = &fen_info6->rt->fib6_nh[0];
		if (!nh6) {
			nss_ppe_vxlanmgr_trace("%px: Next hop entry for IPv6 is NULL", info);
			return NOTIFY_DONE;
		}

		dev = nh6->nh_common.nhc_dev;
		lwtstate = nh6->nh_common.nhc_lwtstate;
		nss_ppe_vxlanmgr_trace("IPv6: event for prefix: %pI6, prefix_len: %d", &fen_info6->rt->fib6_dst.addr, fen_info6->rt->fib6_dst.plen);
	} else {
		nss_ppe_vxlanmgr_warn("Unsupported address family: %X ! \n", info->family);
		return NOTIFY_DONE;
	}

	/*
	 * Check if LW state is valid and tunnel type is ENCAP IP or IP6, used for VXLAN-GPE encapsulation
	 */
	if (!lwtstate) {
		nss_ppe_vxlanmgr_trace("lwt state is NULL for %s \n", (info->family == AF_INET) ? "IPv4" : "IPv6");
		return NOTIFY_DONE;
	}

	if (lwtstate->type != LWTUNNEL_ENCAP_IP && lwtstate->type != LWTUNNEL_ENCAP_IP6) {
		nss_ppe_vxlanmgr_trace("%px: Unsupported tunnel encap type: %u", info, lwtstate->type);
		return NOTIFY_DONE;
	}

	/*
	 * Check if VXLAN-GPE net device
	 */
	if (!netif_is_vxlan(dev)) {
		nss_ppe_vxlanmgr_trace("%px: It is not VXLAN netdevice dev:%s", dev, dev->name);
		return NOTIFY_DONE;
	}

	priv = netdev_priv(dev);
	if (!(priv->cfg.flags & VXLAN_F_GPE)) {
		nss_ppe_vxlanmgr_warn("%px: It is not VXLAN-GPE netdevice dev:%s", dev, dev->name);
		return NOTIFY_DONE;
	}

	if (dstport_gpe != ntohs(priv->cfg.dst_port)) {
		nss_ppe_vxlanmgr_trace("%px: VXLAN: configured PPE dport: %u is not-equal to user given dport:%dn", dev, dstport_gpe, ntohs(priv->cfg.dst_port));
		return NOTIFY_DONE;
	}

	fib_event_data = kzalloc(sizeof(struct nss_ppe_vxlanmgr_fib_event_data), GFP_ATOMIC);
	if (!fib_event_data) {
		nss_ppe_vxlanmgr_warn("%px: Alloc failed for fib_event_data", dev);
		return NOTIFY_DONE;
	}

	/*
	 * Fill the vxlan-gpe related event data
	 */
	tun_info = lwt_tun_info(lwtstate);
	vni = tunnel_id_to_key32(tun_info->key.tun_id);
	fib_event_data->vni = vxlan_vni_field(vni);
	fib_event_data->tos = tun_info->key.tos;
	fib_event_data->ttl = tun_info->key.ttl;
	fib_event_data->event = event;
	fib_event_data->tun_flags = tun_info->key.tun_flags;
	fib_event_data->parent_netdev = dev;

	fib_event_data->rip.sa.sa_family = ip_tunnel_info_af(tun_info);
	if (ip_tunnel_info_af(tun_info) == AF_INET) {
		fib_event_data->rip.sin.sin_addr.s_addr = tun_info->key.u.ipv4.dst;
		fib_event_data->sip.sin.sin_addr.s_addr = tun_info->key.u.ipv4.src;
		nss_ppe_vxlanmgr_trace("%px: Local ip: %pI4, Remote_ip: %pI4", tun_info, &tun_info->key.u.ipv4.src, &tun_info->key.u.ipv4.dst);
	} else {
		fib_event_data->rip.sin6.sin6_addr = tun_info->key.u.ipv6.dst;
		fib_event_data->sip.sin6.sin6_addr = tun_info->key.u.ipv6.src;
		nss_ppe_vxlanmgr_trace("%px: Local ip: %pI6, Remote ip: %pI6", tun_info, &tun_info->key.u.ipv6.src, &tun_info->key.u.ipv6.dst);
	}

	nss_ppe_vxlanmgr_trace("%px: VxLAN-GPE - dev_name: %s, vni: %X, tos: %X, ttl: %X, tun_flags:%X \n",
			tun_info, dev->name, fib_event_data->vni, fib_event_data->tos, fib_event_data->ttl, fib_event_data->tun_flags);

	/*
	 * We use workqueue to process the event asynchronously as we will be configuring/deconfiguring
	 * PPE hardware for VxLAN-GPE. It may take some time during this process and we don't want the
	 * kernel process to be blocked here. Also, we may sleep during the event processing.
	 *
	 * The list is empty, so we need to restart the work queue.
	 */
	spin_lock_bh(&fib_event_list_lock);
	if (list_empty(&fib_event_list)) {
		restart_work = true;
	}

	/*
	 * Add the work at the tail of the list
	 */
	list_add_tail(&fib_event_data->fib_event_list, &fib_event_list);
	spin_unlock_bh(&fib_event_list_lock);

	if (restart_work) {
		queue_work(nss_ppe_vxlanmgr_fib_event_wq, &fib_event_work);
		nss_ppe_vxlanmgr_trace("%px: FIB_EVENT_WORK: started the work-queue", dev);
	}

	return NOTIFY_DONE;
}

struct notifier_block nss_ppe_vxlanmgr_fib_update_nb = {
	.notifier_call = nss_ppe_vxlanmgr_fib_update_event,
};

/*
 * nss_ppe_vxlanmgr_gpe_wq_exit()
 *	Destroy the work_queue for GPE
 */
int nss_ppe_vxlanmgr_gpe_wq_exit(void)
{
	destroy_workqueue(nss_ppe_vxlanmgr_fib_event_wq);
	return 0;
}

/*
 * nss_ppe_vxlanmgr_wq_init()
 *	Initialize the work_queue for GPE
 */
int nss_ppe_vxlanmgr_gpe_wq_init(void)
{
	nss_ppe_vxlanmgr_fib_event_wq = create_singlethread_workqueue("wq_fib_update");

	if (!nss_ppe_vxlanmgr_fib_event_wq){
		nss_ppe_vxlanmgr_warn("work queue allocation failed for VXLAN GPE fib update events");
		return -1;
	}

	INIT_WORK(&fib_event_work, nss_ppe_vxlanmgr_fib_update_event_handler);

	return 0;
}
