/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "nss_ppe_tun_drv.h"
#include "ppe_drv_tun_cmn_ctx.h"

/*
 * VxLAN context
 */
extern struct nss_ppe_vxlanmgr_ctx vxlan_ctx;

/*
 * nss_ppe_vxlanmgr_tunnel_ctx_dev_get()
 *	Find VxLAN tunnel context using netdev.
 *	Context lock must be held before calling this API.
 */
struct nss_ppe_vxlanmgr_tun_ctx *nss_ppe_vxlanmgr_tunnel_ctx_dev_get(struct net_device *dev)
{
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;

	spin_lock_bh(&vxlan_ctx.tun_lock);
	list_for_each_entry(tun_ctx, &vxlan_ctx.list, head) {
		if (tun_ctx->dev == dev) {
			spin_unlock_bh(&vxlan_ctx.tun_lock);
			return tun_ctx;
		}
	}
	spin_unlock_bh(&vxlan_ctx.tun_lock);

	return NULL;
}

/*
 * nss_ppe_vxlanmgr_tunnel_parse_end_points()
 *	VxLAN tunnel mac add messages.
 */
bool nss_ppe_vxlanmgr_tunnel_parse_end_points(struct net_device *dev, struct ppe_drv_tun_cmn_ctx *tun_hdr, union vxlan_addr *rem_ip)
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
		l3->flags = PPE_DRV_TUN_CMN_CTX_L3_IPV6 | PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP;
		memcpy(l3->saddr, &src_ip->sin6.sin6_addr, sizeof(struct in6_addr));
		memcpy(l3->daddr, &rem_ip->sin6.sin6_addr, sizeof(struct in6_addr));

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
			fl6.daddr = rem_ip->sin6.sin6_addr;
			fl6.saddr = src_ip->sin6.sin6_addr;
			memcpy(l3->saddr, &src_ip->sin6.sin6_addr, sizeof(struct in6_addr));
			memcpy(l3->daddr, &rem_ip->sin6.sin6_addr, sizeof(struct in6_addr));

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 5, 7))
			err = ipv6_stub->ipv6_dst_lookup(priv->net,
					priv->vn6_sock->sock->sk, &dst, &fl6);
			if (err < 0) {
#else
			dentry = ipv6_stub->ipv6_dst_lookup_flow(priv->net,
					priv->vn6_sock->sock->sk, &fl6, final_dst);
			if (!dentry) {
#endif
				nss_ppe_vxlanmgr_warn("No route, drop packet.\n");
				return false;
			}
			return true;
		}
	} else {
		l3->flags = PPE_DRV_TUN_CMN_CTX_L3_IPV4 | PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP;
		l3->saddr[0] = src_ip->sin.sin_addr.s_addr;
		l3->daddr[0] = rem_ip->sin.sin_addr.s_addr;

		if (priv_flags & VXLAN_F_UDP_ZERO_CSUM_TX) {
			l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_UDP_ZERO_CSUM_TX;
		}

		if (src_ip->sin.sin_addr.s_addr == htonl(INADDR_ANY)) {
			/*
			 * Lookup
			 */
			memset(&fl4, 0, sizeof(fl4));
			fl4.flowi4_proto = IPPROTO_UDP;
			fl4.daddr = rem_ip->sin.sin_addr.s_addr;
			fl4.saddr = src_ip->sin.sin_addr.s_addr;
			l3->saddr[0] = src_ip->sin.sin_addr.s_addr;
			l3->daddr[0] = rem_ip->sin.sin_addr.s_addr;

			rt = ip_route_output_key(priv->net, &fl4);
			if (IS_ERR(rt)) {
				nss_ppe_vxlanmgr_warn("No route available.\n");
				return false;
			}
		}
	}
	return true;
}

/*
 * nss_ppe_vxlanmgr_tunnel_fdb_event()
 *	Event handler for VxLAN fdb updates
 */
static int nss_ppe_vxlanmgr_tunnel_fdb_event(struct notifier_block *nb, unsigned long event, void *data)
{
	struct vxlan_fdb_event *vfe;
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;
	struct net_device *dev;

	vfe = (struct vxlan_fdb_event *)data;
	dev = vfe->dev;

	tun_ctx = nss_ppe_vxlanmgr_tunnel_ctx_dev_get(dev);
	if (!tun_ctx) {
		nss_ppe_vxlanmgr_warn("%px: Invalid tunnel context\n", dev);
		return NOTIFY_DONE;
	}

	switch(event) {
	case RTM_DELNEIGH:
		break;

	case RTM_NEWNEIGH:
		nss_ppe_vxlanmgr_trace("%px: remote detected: %u", dev, tun_ctx->remote_detected);
		if (!tun_ctx->remote_detected && !is_zero_ether_addr(vfe->eth_addr)) {
			nss_ppe_vxlanmgr_tunnel_parse_end_points(dev, tun_ctx->tun_hdr, &vfe->rdst->remote_ip);
			nss_ppe_vxlanmgr_tunnel_config(dev, tun_ctx->tun_hdr);
			tun_ctx->remote_detected = true;
		}
		break;

	default:
		nss_ppe_vxlanmgr_warn("%lu: Unknown FDB event received.\n", event);
	}

	return NOTIFY_DONE;
}

/*
 * Notifier to receive fdb events from VxLAN
 */
static struct notifier_block nss_ppe_vxlanmgr_tunnel_fdb_notifier = {
	.notifier_call = nss_ppe_vxlanmgr_tunnel_fdb_event,
};

/*
 * nss_ppe_vxlan_src_exception()
 *	handle the source VP exception.
 */
static bool nss_ppe_vxlan_src_exception(struct net_device *dev, struct sk_buff *skb)
{
	int ret;

	skb->dev = dev;
	skb->skb_iif = dev->ifindex;
	skb->protocol = eth_type_trans(skb, dev);
	skb_reset_network_header(skb);

	ret = netif_receive_skb(skb);
	if (ret != NET_RX_SUCCESS) {
		nss_ppe_vxlanmgr_warn("%p: excpetion packet dropped. err:%d \n", dev, ret);
	}

	return true;
}

/*
 * nss_ppe_vxlanmgr_tunnel_config()
 *	Function to send dynamic interface enable message
 */
int nss_ppe_vxlanmgr_tunnel_config(struct net_device *dev, struct ppe_drv_tun_cmn_ctx *tun_hdr)
{
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;
	bool ret;

	dev_hold(dev);
	tun_ctx = nss_ppe_vxlanmgr_tunnel_ctx_dev_get(dev);
	if (!tun_ctx) {
		nss_ppe_vxlanmgr_warn("Invalid tunnel context\n");
		dev_put(dev);
		return NOTIFY_DONE;
	}

	tun_hdr->type = PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN;
	tun_hdr->l3.ttl = tun_ctx->ttl;
	tun_hdr->tun.vxlan.vni = tun_ctx->vni;
	tun_hdr->tun.vxlan.flags = tun_ctx->tunnel_flags;
	tun_hdr->tun.vxlan.src_port_min = tun_ctx->src_port_min;
	tun_hdr->tun.vxlan.src_port_max = tun_ctx->src_port_max;
	tun_hdr->tun.vxlan.dest_port = tun_ctx->dest_port;
	nss_ppe_vxlanmgr_info("destport: tun_hdr->tun.vxlan.dest_port:%u tunnel_flags:%u", tun_hdr->tun.vxlan.dest_port, tun_ctx->tunnel_flags);

	tun_hdr->tun.vxlan.policy_id = 0;
	tun_hdr->l3.proto = IPPROTO_UDP;
	tun_hdr->l3.dscp = 0;
	tun_hdr->type = PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN;

	ret = ppe_tun_configure(dev, tun_hdr, nss_ppe_vxlan_src_exception, NULL);
	if (!ret) {
		nss_ppe_vxlanmgr_warn("Getting  PPE VXLAN tunnel failed.\n");
		dev_put(dev);
		return NOTIFY_DONE;
	}

	dev_put(dev);
	return NOTIFY_DONE;
}

/*
 * nss_ppe_vxlanmgr_tunnel_destroy()
 *	Function to unregister and destroy dynamic interfaces.
 */
int nss_ppe_vxlanmgr_tunnel_destroy(struct net_device *dev)
{
	uint32_t tun_count;
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;

	spin_lock_bh(&vxlan_ctx.tun_lock);
	tun_count = vxlan_ctx.tun_count;
	spin_unlock_bh(&vxlan_ctx.tun_lock);
	if (!tun_count) {
		nss_ppe_vxlanmgr_warn("%px: No more tunnels to destroy.\n", dev);
		return NOTIFY_DONE;
	}

	dev_hold(dev);
	tun_ctx = nss_ppe_vxlanmgr_tunnel_ctx_dev_get(dev);
	if (!tun_ctx) {
		nss_ppe_vxlanmgr_warn("%px: Invalid tunnel context\n", dev);
		dev_put(dev);
		return NOTIFY_DONE;
	}

	/*
	 * Remove tunnel from global list.
	 * Decrement interface count.
	 */
	spin_lock_bh(&vxlan_ctx.tun_lock);
	list_del(&tun_ctx->head);
	vxlan_ctx.tun_count--;
	spin_unlock_bh(&vxlan_ctx.tun_lock);
	nss_ppe_vxlanmgr_tun_stats_dentry_remove(tun_ctx);

	/*
	 * Unregister fdb notifier chain if
	 * all vxlan tunnels are destroyed.
	 */
	spin_lock_bh(&vxlan_ctx.tun_lock);
	tun_count = vxlan_ctx.tun_count;
	spin_unlock_bh(&vxlan_ctx.tun_lock);
	if (!tun_count) {
		vxlan_fdb_unregister_notify(&nss_ppe_vxlanmgr_tunnel_fdb_notifier);
	}
	nss_ppe_vxlanmgr_info("%px: VxLAN interface count is #%d\n", dev, tun_count);

	kfree(tun_ctx->tun_hdr);
	kfree(tun_ctx);
	ppe_tun_free(dev);
	dev_put(dev);

	return NOTIFY_DONE;
}

/*
 * nss_ppe_vxlanmgr_tunnel_create()
 *	Function to create and register dynamic interfaces.
 */
int nss_ppe_vxlanmgr_tunnel_create(struct net_device *dev)
{
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;
	struct vxlan_dev *priv;
	uint32_t priv_flags;
	struct ppe_drv_tun_cmn_ctx_l3 *l3;

	dev_hold(dev);
	if (!ppe_tun_alloc(dev, PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN)) {
		nss_ppe_vxlanmgr_warn("%px: PPE tunnel creation failed \n", dev);
		goto ctx_alloc_fail;
	}

	tun_ctx = kzalloc(sizeof(struct nss_ppe_vxlanmgr_tun_ctx), GFP_ATOMIC);
	if (!tun_ctx) {
		nss_ppe_vxlanmgr_warn("Failed to allocate memory for tun_ctx\n");
		goto ctx_alloc_fail;
	}
	tun_ctx->dev = dev;
	tun_ctx->vxlan_ctx = &vxlan_ctx;

	tun_ctx->tun_hdr = kzalloc(sizeof(struct ppe_drv_tun_cmn_ctx), GFP_ATOMIC);
	if (!tun_ctx->tun_hdr) {
		nss_ppe_vxlanmgr_warn("Failed to allocate memory for tun_hdr\n");
		kfree(tun_ctx);
		goto ctx_alloc_fail;
	}

	INIT_LIST_HEAD(&tun_ctx->head);

	if (!nss_ppe_vxlanmgr_tun_stats_dentry_create(tun_ctx)) {
		nss_ppe_vxlanmgr_warn("%px: Tun stats dentry init failed\n", dev);
		goto config_fail;
	}

	priv = netdev_priv(dev);

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
	if (priv_flags & VXLAN_F_TTL_INHERIT) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_TTL;
	}

	spin_lock_bh(&vxlan_ctx.tun_lock);
	/*
	 * Add tunnel to global list.
	 */
	list_add(&tun_ctx->head, &vxlan_ctx.list);
	if (!vxlan_ctx.tun_count) {
		vxlan_fdb_register_notify(&nss_ppe_vxlanmgr_tunnel_fdb_notifier);
	}

	/*
	 * Increment vxlan tunnel interface count
	 */
	vxlan_ctx.tun_count++;
	spin_unlock_bh(&vxlan_ctx.tun_lock);

	nss_ppe_vxlanmgr_info("%px: VxLAN interface count is #%d\n", dev, vxlan_ctx.tun_count);

	dev_put(dev);
	return NOTIFY_DONE;

config_fail:
	kfree(tun_ctx->tun_hdr);
	kfree(tun_ctx);

ctx_alloc_fail:
	dev_put(dev);
	return NOTIFY_DONE;
}
