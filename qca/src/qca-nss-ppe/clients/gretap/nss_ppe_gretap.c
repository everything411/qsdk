/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/rwlock_types.h>
#include <linux/hashtable.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ipv6.h>
#include <linux/if_arp.h>
#include <net/route.h>
#include <net/ip.h>
#include <linux/if_bridge.h>
#include <net/bonding.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <net/gre.h>
#include <ppe_drv.h>
#include <net/ip6_tunnel.h>
#include <nss_ppe_tun_drv.h>
#include <linux/debugfs.h>
#include <ppe_drv_public.h>
#include <ppe_vp_public.h>
#include "nss_ppe_gretap.h"

static struct dentry *gretap_dentry;

static bool nss_gretap_stats_dentry_create(struct net_device *dev);
static bool nss_gretap_stats_dentry_free(struct net_device *dev);

/*
 * nss_ppe_gretap_src_exception()
 *	handle the source VP exception
 */
static bool nss_ppe_gretap_src_exception(struct net_device *dev, struct sk_buff *skb)
{
	int ret;

	skb->protocol = eth_type_trans(skb, dev);
	skb->dev = dev;
	/*
	 * Packet type is updated to PACKET_OTHERHOST in eth_type_trans. Since the packet
	 * is already decapsulated set it to PACKET_HOST for futher processing
	 */
	skb->pkt_type = PACKET_HOST;
	skb->skb_iif = dev->ifindex;
	skb_reset_network_header(skb);
	ret = netif_receive_skb(skb);
	if (ret != NET_RX_SUCCESS) {
		nss_ppe_gretap_warning("%p: excpetion packet dropped\n", dev);
	}

	return true;
}

/*
 * nss_ppe_gretap_set_gre_key_flags()
 *     Set GRE Key flags according to the config
 */
static void nss_ppe_gretap_set_gre_key_flags(struct ppe_drv_tun_cmn_ctx_gretap *gre, uint16_t iflags, uint16_t oflags, uint32_t i_key, uint32_t o_key)
{
	if (!gre) {
		return;
	}

	memset(gre, 0, sizeof(struct ppe_drv_tun_cmn_ctx_gretap));

	if (iflags & TUNNEL_KEY) {
		gre->flags |= PPE_DRV_TUN_CMN_CTX_GRE_L_KEY;
		gre->local_key = i_key;
	}

	if (oflags & TUNNEL_KEY) {
		gre->flags |= PPE_DRV_TUN_CMN_CTX_GRE_R_KEY;
		gre->remote_key = o_key;
	}

	if (iflags & TUNNEL_CSUM) {
		gre->flags |= PPE_DRV_TUN_CMN_CTX_GRE_L_CSUM;
	}

	if (oflags & TUNNEL_CSUM) {
		gre->flags |= PPE_DRV_TUN_CMN_CTX_GRE_R_CSUM;
	}
}

/*
 * nss_ppe_gretap_ip4_dev_parse_param()
 *      Parse IPv4 gretap arguments sent to PPE driver
 */
static bool nss_ppe_gretap_ip4_dev_parse_param(struct net_device *netdev, struct ppe_drv_tun_cmn_ctx *tun_hdr)
{
	struct ip_tunnel *tunnel;
	struct ppe_drv_tun_cmn_ctx_l3 *l3 = &tun_hdr->l3;
	struct iphdr *iphdr;
	struct ppe_drv_tun_cmn_ctx_gretap *gre = &tun_hdr->tun.gre;
	tunnel = (struct ip_tunnel *)netdev_priv(netdev);

	iphdr = &tunnel->parms.iph;
	/*
	 * Prepare The Tunnel configuration parameter to send to PPE
	 */
	l3->saddr[0] = iphdr->saddr;
	l3->saddr[1] = 0;
	l3->saddr[2] = 0;
	l3->saddr[3] = 0;
	l3->daddr[0] = iphdr->daddr;
	l3->daddr[1] = 0;
	l3->daddr[2] = 0;
	l3->daddr[3] = 0;
	l3->ttl = iphdr->ttl;
	l3->dscp = iphdr->tos >> 2;
	l3->proto = IPPROTO_GRE;
	l3->flags = PPE_DRV_TUN_CMN_CTX_L3_IPV4;

	/* Set PPE flags to inherit DSCP/TTL values if its not set */
	if (!l3->ttl) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_TTL;
	}

	if (!l3->dscp) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP;
	}

	tun_hdr->type = PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP;
	nss_ppe_gretap_set_gre_key_flags(gre, tunnel->parms.i_flags, tunnel->parms.o_flags, tunnel->parms.i_key, tunnel->parms.o_key);

	return true;
}

/*
 * nss_ppe_gretap_ip6_dev_parse_param()
 *      Parse IPv4 gretap arguments sent to PPE driver
 */
static bool nss_ppe_gretap_ip6_dev_parse_param(struct net_device *netdev, struct ppe_drv_tun_cmn_ctx *tun_hdr)
{
	struct ip6_tnl *tunnel;
	struct flowi6 *fl6;
	struct ppe_drv_tun_cmn_ctx_l3 *l3 = &tun_hdr->l3;

	struct ppe_drv_tun_cmn_ctx_gretap *gre = &tun_hdr->tun.gre;
	tunnel = (struct ip6_tnl *)netdev_priv(netdev);

	if (!(tunnel->parms.flags & IP6_TNL_F_IGN_ENCAP_LIMIT)) {
		nss_ppe_gretap_warning("%p: Encap limit should be none", netdev);
		return false;
	}

	/*
	 * Find the Tunnel device flow information
	 */
	fl6 = &tunnel->fl.u.ip6;
	nss_ppe_gretap_trace("%px: Tunnel param saddr: %pI6 daddr: %pI6\n", netdev, fl6->saddr.s6_addr32, fl6->daddr.s6_addr32);
	nss_ppe_gretap_trace("%px: Hop limit %d\n", netdev, tunnel->parms.hop_limit);
	nss_ppe_gretap_trace("%px: Tunnel param flag %x  fl6.flowlabel %x\n", netdev,  tunnel->parms.flags, fl6->flowlabel);

	/*
	 * Prepare The Tunnel configuration parameter to send to PPE
	 */
	l3->saddr[0] = (fl6->saddr.s6_addr32[0]);
	l3->saddr[1] = (fl6->saddr.s6_addr32[1]);
	l3->saddr[2] = (fl6->saddr.s6_addr32[2]);
	l3->saddr[3] = (fl6->saddr.s6_addr32[3]);
	l3->daddr[0] = (fl6->daddr.s6_addr32[0]);
	l3->daddr[1] = (fl6->daddr.s6_addr32[1]);
	l3->daddr[2] = (fl6->daddr.s6_addr32[2]);
	l3->daddr[3] = (fl6->daddr.s6_addr32[3]);
	l3->ttl = tunnel->parms.hop_limit;
	l3->dscp = ip6_tclass(tunnel->parms.flowinfo) & 0xfc;
	l3->proto = IPPROTO_GRE;
	l3->flags = PPE_DRV_TUN_CMN_CTX_L3_IPV6;

	/* Set PPE flags to inherit DSCP/TTL values if its not set */
	if (!l3->ttl) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_TTL;
	}

	if (!l3->dscp) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP;
	}

	tun_hdr->type = PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP;
	nss_ppe_gretap_set_gre_key_flags(gre, tunnel->parms.i_flags, tunnel->parms.o_flags, tunnel->parms.i_key, tunnel->parms.o_key);

	return true;
}

/*
 * nss_ppe_gretap_dev_event()
 *      Net device notifier for gretap module
 */
static int nss_ppe_gretap_dev_event(struct notifier_block  *nb,
		unsigned long event, void  *info)
{
	struct net_device *netdev = netdev_notifier_info_to_dev(info);
	bool status;
	struct ppe_drv_tun_cmn_ctx *tun_hdr;

	/*
	 * Proceed to handle event only if it GRE netdevice
	 */
	if (!netif_is_ip6gretap(netdev) && !netif_is_gretap(netdev)) {
	      return NOTIFY_DONE;
	}

	switch (event) {
	case NETDEV_REGISTER:
		if (gre_tunnel_is_fallback_dev(netdev)) {
			nss_ppe_gretap_warning("%p: GRETAP tunnel creation skipped for fb dev %s\n", netdev, netdev->name);
			break;
		}

		status = ppe_tun_alloc(netdev, PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP);
		if (status) {
			nss_gretap_stats_dentry_create(netdev);
		}
		break;

	case NETDEV_UNREGISTER:
		ppe_tun_free(netdev);
		nss_gretap_stats_dentry_free(netdev);
		break;

	case NETDEV_UP:
		nss_ppe_gretap_trace("%px: NETDEV_UP :event %lu name %s\n", netdev, event, netdev->name);

		tun_hdr = kzalloc(sizeof(struct ppe_drv_tun_cmn_ctx), GFP_ATOMIC);
		if (!tun_hdr) {
			nss_ppe_gretap_warning("%px: memory allocation for tunnel %s failed\n", netdev, netdev->name);
			break;
		}

		if (netif_is_ip6gretap(netdev)) {
			status = nss_ppe_gretap_ip6_dev_parse_param(netdev, tun_hdr);
		} else {
			status = nss_ppe_gretap_ip4_dev_parse_param(netdev, tun_hdr);
		}

		if (!status) {
			kfree(tun_hdr);
			break;
		}

		if (!(ppe_tun_configure(netdev, tun_hdr, nss_ppe_gretap_src_exception, NULL))) {
			nss_ppe_gretap_trace("%px: Not able to create tunnel for dev: %s\n", netdev, netdev->name);
		}

		kfree(tun_hdr);
		break;

	case NETDEV_DOWN:
		nss_ppe_gretap_trace("%px: NETDEV_DOWN :event %lu name %s\n", netdev, event, netdev->name);
		ppe_tun_deconfigure(netdev);
		break;

	case NETDEV_CHANGEMTU:
		nss_ppe_gretap_trace("%px: NETDEV_CHANGEMTU :event %lu name %s\n", netdev, event, netdev->name);
		ppe_tun_mtu_set(netdev, netdev->mtu);
		break;

	case NETDEV_BR_LEAVE:
		nss_ppe_gretap_trace("%px: NETDEV_BR_LEAVE: name %s\n", netdev, netdev->name);
		if (!ppe_tun_decap_disable(netdev)) {
			nss_ppe_gretap_warning("%p: Failed disabling decap at index %s", netdev, netdev->name);
		}
		break;

	case NETDEV_BR_JOIN:
		nss_ppe_gretap_trace("%px: NETDEV_BR_JOIN: name %s\n", netdev,  netdev->name);
		if (!ppe_tun_decap_enable(netdev)) {
			nss_ppe_gretap_warning("%p: Failed enabling decap at index %s", netdev, netdev->name);
		}
		break;

	default:
		nss_ppe_gretap_trace("%px: Unhandled notifier dev %s event %x\n", netdev, netdev->name, (int)event);
		break;
	}

	return NOTIFY_DONE;
}

/*
 * nss_ppe_gretap_stats_show()
 *	Read ppe tunnel statistics.
 */
static int nss_ppe_gretap_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct net_device *dev = (struct net_device *)m->private;
	uint64_t exception_packet;
	uint64_t exception_bytes;

	ppe_tun_exception_packet_get(dev, &exception_packet, &exception_bytes);
	seq_printf(m, "\n################ PPE Client gretap Statistics Start ################\n");
	seq_printf(m, "dev: %s\n", dev->name);
	seq_printf(m, "  Exception:\n");
	seq_printf(m, "\t exception packet: %llu\n", exception_packet);
	seq_printf(m, "\t exception bytes: %llu\n", exception_bytes);
	seq_printf(m, "\n################ PPE Client gretap Statistics End ################\n");

	return 0;
}

/*
 * nss_ppe_gretap_stats_open()
 */
static int nss_ppe_gretap_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, nss_ppe_gretap_stats_show, inode->i_private);
}

/*
 * nss_ppe_gretap_stats_ops
 *	File operations for gretap tunnel stats
 */
static const struct file_operations nss_ppe_gretap_stats_ops = {
	.open = nss_ppe_gretap_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * nss_gretap_stats_dentry_create()
 *	Create dentry for a given netdevice.
 */
static bool nss_gretap_stats_dentry_create(struct net_device *dev)
{
	char dentry_name[IFNAMSIZ];
	struct dentry *dentry;
	scnprintf(dentry_name, sizeof(dentry_name), "%s", dev->name);

	dentry = debugfs_create_file(dentry_name, S_IRUGO,
			gretap_dentry, dev, &nss_ppe_gretap_stats_ops);
	if (!dentry) {
		nss_ppe_gretap_warning("%px: Debugfs file creation failed for device %s\n", dev, dev->name);
		return false;
	}

	return true;
}

/*
 * nss_gretap_stats_dentry_free()
 *	Remove dentry for a given netdevice.
 */
static bool nss_gretap_stats_dentry_free(struct net_device *dev)
{
	char dentry_name[IFNAMSIZ];
	struct dentry *dentry;
	scnprintf(dentry_name, sizeof(dentry_name), "%s", dev->name);

	dentry = debugfs_lookup(dentry_name, gretap_dentry);
	if (dentry) {
		debugfs_remove(dentry);
		nss_ppe_gretap_trace("%px: removed stats debugfs entry for dev %s", dev, dentry_name);
		return true;
	}

	nss_ppe_gretap_trace("%px: Could not find stats debugfs entry for dev %s", dev, dentry_name);
	return false;
}

/*
 * nss_gretap_stats_dentry_deinit()
 *	Cleanup the debugfs tree.
 */
static void nss_ppe_gretap_dentry_deinit(void)
{
	debugfs_remove_recursive(gretap_dentry);
	gretap_dentry = NULL;
}

/*
 * nss_ppe_gretap_dentry_init()
 *	Create gretap tunnel statistics debugfs entry.
 */
static bool nss_ppe_gretap_dentry_init(void)
{
	/*
	 * Initialize debugfs directory.
	 */
	struct dentry *parent;
	struct dentry *clients;

	parent = debugfs_lookup("qca-nss-ppe", NULL);
	if (!parent) {
		nss_ppe_gretap_warning("parent debugfs entry for qca-nss-ppe not present\n");
		return false;
	}

	clients = debugfs_lookup("clients", parent);
	if (!clients) {
		nss_ppe_gretap_warning("clients debugfs entry inside qca-nss-ppe not present\n");
		return false;
	}

	gretap_dentry = debugfs_create_dir("gretap", clients);
	if (!gretap_dentry) {
		nss_ppe_gretap_warning("gretap debugfs entry inside qca-nss-ppe/clients could not be created\n");
		return false;
	}

	return true;
}

/*
 * Linux Net device Notifier
 */
struct notifier_block nss_ppe_gretap_notifier = {
	.notifier_call = nss_ppe_gretap_dev_event,
};

/*
 * nss_ppe_gretap_init_module()
 *      Tunnel gretap module init function
 */
int __init nss_ppe_gretap_init_module(void)
{
	nss_ppe_gretap_info("module (platform - IPQ95xx , %s) loaded\n",
			NSS_PPE_GRETAP_BUILD_ID);

	/*
	 * Create the debugfs directory for statistics.
	 */
	if (!nss_ppe_gretap_dentry_init()) {
		nss_ppe_gretap_trace("Failed to initialize debugfs\n");
		return -1;
	}

	register_netdevice_notifier(&nss_ppe_gretap_notifier);
	nss_ppe_gretap_trace("gretap PPE driver registered\n");

	return 0;
}

/*
 * nss_ppe_gretap_exit_module()
 * Tunnel gretap module exit function
 */
void __exit nss_ppe_gretap_exit_module(void)
{
	/*
	 * deactivate all GRE PPE instances.
	 */
	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP, false);

	/*
	 * De-initialize debugfs.
	 */
	nss_ppe_gretap_dentry_deinit();

	/*
	 * Unregister net device notification for standard tunnel.
	 */
	unregister_netdevice_notifier(&nss_ppe_gretap_notifier);

	nss_ppe_gretap_info("gretap module unloaded\n");
}

module_init(nss_ppe_gretap_init_module);
module_exit(nss_ppe_gretap_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS PPE gretap client driver");
