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
#include "nss_ppe_gre.h"

static struct nss_ppe_gre_ctx global;

static bool nss_gre_stats_dentry_create(struct nss_ppe_gre_ctx *ctx, struct net_device *dev);
static bool nss_gre_stats_dentry_free(struct nss_ppe_gre_ctx *ctx, struct net_device *dev);

static uint8_t encap_ecn_mode = PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_NO_UPDATE;
module_param(encap_ecn_mode, byte, 0644);
MODULE_PARM_DESC(encap_ecn_mode, "Encap ECN mode 0:NO_UPDATE, 1:RFC3168_LIMIT_RFC6040_CMPAT, 2:RFC3168_FULL, 3:RFC4301_RFC6040_NORMAL");

static uint8_t decap_ecn_mode = PPE_DRV_TUN_CMN_CTX_DECAP_ECN_RFC3168_MODE;
module_param(decap_ecn_mode, byte, 0644);
MODULE_PARM_DESC(decap_ecn_mode, "Decap ECN mode 0:RFC3168, 1:RFC4301, 2:RFC6040");

static bool inherit_dscp = false;
module_param(inherit_dscp, bool, 0644);
MODULE_PARM_DESC(inherit_dscp, "DSCP 0:Dont Inherit inner, 1:Inherit inner");

static bool inherit_ttl = true;
module_param(inherit_ttl, bool, 0644);
MODULE_PARM_DESC(inherit_ttl, "TTL 0:Dont Inherit inner, 1:Inherit inner");

/*
 * nss_ppe_gre_dev_stats_update()
 *	Update gre dev statistics
 */
static bool nss_ppe_gre_dev_stats_update(struct net_device *dev, ppe_tun_hw_stats *stats, ppe_tun_data *tun_data)
{
	struct pcpu_sw_netstats *tstats;

	if (!dev) {
		return false;
	}

	tstats = this_cpu_ptr(dev->tstats);
	u64_stats_update_begin(&tstats->syncp);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	tstats->tx_bytes += stats->tx_byte_cnt;
	tstats->tx_packets += stats->tx_pkt_cnt;
	tstats->rx_bytes += stats->rx_byte_cnt;
	tstats->rx_packets += stats->rx_pkt_cnt;
#else
        u64_stats_add(&tstats->tx_bytes, stats->tx_byte_cnt);
	u64_stats_add(&tstats->tx_packets,  stats->tx_pkt_cnt);
	u64_stats_add(&tstats->rx_bytes, stats->rx_byte_cnt);
	u64_stats_add(&tstats->rx_packets,  stats->rx_pkt_cnt);
#endif
	u64_stats_update_end(&tstats->syncp);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	atomic_long_add(stats->tx_drop_pkt_cnt, &dev->tx_dropped);
	atomic_long_add(stats->rx_drop_pkt_cnt, &dev->rx_dropped);
#endif

	return true;
}

/*
 * nss_ppe_gretun_src_exception()
 *	Handle the source VP exception for GRETUN
 */
static bool nss_ppe_gretun_src_exception(struct ppe_vp_cb_info *info, ppe_tun_data *tun_data)
{
	struct sk_buff *skb = info->skb;
	int ret;
	struct nss_ppe_gre_ctx *ctx = &global;
	unsigned char *data = skb->data;

	skb_reset_network_header(skb);
	if ((data[0] >> 4) == IPVERSION) {
		skb->protocol = htons(ETH_P_IP);
	} else if ((data[0] >> 4) == 6) {
		skb->protocol = htons(ETH_P_IPV6);
	} else {
		dev_kfree_skb_any(skb);
		nss_ppe_gre_warning("%p: Not an IP packet \n", skb->dev);
		atomic64_inc(&ctx->stats.gretun_src_excep_drop_count);
		return true;
	}

	skb->pkt_type = PACKET_HOST;

	/*
	 * Reset skb flags.
	 */
	skb->fast_xmit = 0;
	skb->fast_recycled = 0;
	skb->recycled_for_ds = 0;
	ret = netif_receive_skb(skb);
	if (ret != NET_RX_SUCCESS) {
		nss_ppe_gre_warning("%p: exception packet dropped for gretun\n", skb->dev);
		atomic64_inc(&ctx->stats.gretun_src_excep_drop_count);
	}

	return true;
}

/*
 * nss_ppe_gretap_src_exception()
 *	handle the source VP exception
 */
static bool nss_ppe_gretap_src_exception(struct ppe_vp_cb_info *info, ppe_tun_data *tun_data)
{
	struct nss_ppe_gre_ctx *ctx = &global;

	struct sk_buff *skb = info->skb;
	struct net_device *dev = skb->dev;
	int ret;

	skb->pkt_type = PACKET_HOST;

	/*
	 * Packet type would be updated to "PACKET_OTHERHOST" by eth_type_trans()
	 * for packets which are not destined to tunnel netdevice
	 */
	skb->protocol = eth_type_trans(skb, dev);
	skb_reset_network_header(skb);

	/*
	 * Reset skb flags.
	 */
	skb->fast_xmit = 0;
	skb->fast_recycled = 0;
	skb->recycled_for_ds = 0;
	ret = netif_receive_skb(skb);
	if (ret != NET_RX_SUCCESS) {
		nss_ppe_gre_warning("%p: excpetion packet dropped for gretap\n", dev);
		atomic64_inc(&ctx->stats.gretap_src_excep_drop_count);
	}

	return true;
}

/*
 * nss_ppe_gre_flags_check_cmn()
 * 	API to check the common tunnel create flags between v4 and v6.
 */
static bool nss_ppe_gre_flags_check_cmn(struct net_device *dev, uint16_t i_flags, uint16_t o_flags, enum ppe_drv_tun_cmn_ctx_type type)
{
	struct nss_ppe_gre_ctx *ctx = &global;

	/*
	 * Currently GRE tunnel offload with KEY and CSUM flags are not supported in PPE
	 */
	if (type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN) {
		if (i_flags & TUNNEL_KEY || o_flags & TUNNEL_KEY) {
			nss_ppe_gre_warning("%p: GRE tunnel offload with keys flags are not supported in PPE \n", dev);
			atomic64_inc(&ctx->stats.gretun_key_flag_failure);
			return false;
		}

		if (i_flags & TUNNEL_CSUM || o_flags & TUNNEL_CSUM) {
			nss_ppe_gre_warning("%p: GRE tunnel offload with csum flags are not supporeted in PPE \n", dev);
			atomic64_inc(&ctx->stats.gretun_csum_flag_failure);
			return false;
		}
	}

	if (i_flags & TUNNEL_SEQ) {
		nss_ppe_gre_warning("%p:%s iflag SEQ not supported\n", dev, dev->name);
		atomic64_inc(&ctx->stats.iflag_seq_err);
		return false;
	}

	if (o_flags & TUNNEL_SEQ) {
		nss_ppe_gre_warning("%p:%s oflag SEQ not supported\n", dev, dev->name);
		atomic64_inc(&ctx->stats.oflag_seq_err);
		return false;
	}

	return true;
}

/*
 * nss_ppe_gre_flags_check_v6()
 *	API to check if the v6 tunnel create flags are supported.
 *
 * There are a number if extended feature for GRE tunnels which are specified
 * when creating the tunnel.This API Checks if the flags are supported.
 */
static bool nss_ppe_gre_flags_check_v6(struct net_device *dev, struct ip6_tnl *tun, enum ppe_drv_tun_cmn_ctx_type type)
{
	struct nss_ppe_gre_ctx *ctx = &global;

	if (!(tun->parms.flags & IP6_TNL_F_IGN_ENCAP_LIMIT)) {
		nss_ppe_gre_warning("%p:%s Encap limit should be none", dev, dev->name);
		atomic64_inc(&ctx->stats.enc_lim_err);
		return false;
	}

	return nss_ppe_gre_flags_check_cmn(dev, tun->parms.i_flags, tun->parms.o_flags, type);
}

/*
 * nss_ppe_gre_set_gre_flags()
 *     Set GRE Key, optional flags according to the config
 */
bool nss_ppe_gre_set_gre_flags(struct ppe_drv_tun_cmn_ctx_gre *gre, uint16_t iflags, uint16_t oflags, uint32_t i_key, uint32_t o_key)
{
	if (!gre) {
		return false;
	}

	memset(gre, 0, sizeof(struct ppe_drv_tun_cmn_ctx_gre));

	if (iflags & TUNNEL_KEY) {
		gre->flags |= PPE_DRV_TUN_CMN_CTX_GRE_DECAP_KEY;
		gre->decap_key = i_key;
	}

	if (oflags & TUNNEL_KEY) {
		gre->flags |= PPE_DRV_TUN_CMN_CTX_GRE_ENCAP_KEY;
		gre->encap_key = o_key;
	}

	if (iflags & TUNNEL_CSUM) {
		gre->flags |= PPE_DRV_TUN_CMN_CTX_GRE_DECAP_CSUM;
		nss_ppe_gre_info("%p:ICSUM option enabled for GRE\n", gre);
	}

	if (oflags & TUNNEL_CSUM) {
		gre->flags |= PPE_DRV_TUN_CMN_CTX_GRE_ENCAP_CSUM;
		nss_ppe_gre_info("%p:OCSUM option enabled for GRE\n", gre);
	}

	return true;
}

/*
 * nss_ppe_gre_ip4_dev_parse_param()
 *      Parse IPv4 gre arguments sent to PPE driver
 */
static bool nss_ppe_gre_ip4_dev_parse_param(struct net_device *netdev, struct ppe_drv_tun_cmn_ctx *tun_hdr,
						enum ppe_drv_tun_cmn_ctx_type type)
{
	bool tun_cfg_ol_support;
	struct ip_tunnel *tunnel;
	struct ppe_drv_tun_cmn_ctx_l3 *l3 = &tun_hdr->l3;
	struct iphdr *iphdr;
	struct ppe_drv_tun_cmn_ctx_gre *gre = &tun_hdr->tun.gre;
	tunnel = (struct ip_tunnel *)netdev_priv(netdev);

	tun_cfg_ol_support = nss_ppe_gre_flags_check_cmn(netdev, tunnel->parms.i_flags, tunnel->parms.o_flags, type);
	if (!tun_cfg_ol_support) {
		nss_ppe_gre_warning("%p:Configured GRE extended header not supported\n", netdev);
		return false;
	}

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

	/*
	 * Set PPE flags to inherit TTL values if inherit flag is not set
	 */
	if (inherit_ttl) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_TTL;
	}

	if (inherit_dscp) {
		l3->flags |=  PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP;
	}

	if (iphdr->frag_off & htons(IP_DF)) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_DF_BIT_SET;
	}

	if (encap_ecn_mode <= PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_RFC4301_RFC6040_NORMAL_MODE) {
		l3->encap_ecn_mode = encap_ecn_mode;
	}

	if (decap_ecn_mode <= PPE_DRV_TUN_CMN_CTX_DECAP_ECN_RFC6040_MODE) {
		l3->decap_ecn_mode = decap_ecn_mode;
	}

	tun_hdr->type = type;

	return nss_ppe_gre_set_gre_flags(gre, tunnel->parms.i_flags, tunnel->parms.o_flags, tunnel->parms.i_key, tunnel->parms.o_key);
}

/*
 * nss_ppe_gre_ip6_dev_parse_param()
 *      Parse IPv6 gre arguments sent to PPE driver
 */
static bool nss_ppe_gre_ip6_dev_parse_param(struct net_device *netdev, struct ppe_drv_tun_cmn_ctx *tun_hdr,
						enum ppe_drv_tun_cmn_ctx_type type)
{
	struct ip6_tnl *tunnel;
	struct flowi6 *fl6;
	struct ppe_drv_tun_cmn_ctx_l3 *l3 = &tun_hdr->l3;
	bool tun_cfg_ol_support;

	struct ppe_drv_tun_cmn_ctx_gre *gre = &tun_hdr->tun.gre;
	tunnel = (struct ip6_tnl *)netdev_priv(netdev);

	tun_cfg_ol_support = nss_ppe_gre_flags_check_v6(netdev, tunnel, type);
	if (!tun_cfg_ol_support) {
		nss_ppe_gre_warning("%p:Configured GRE extended header not supported\n", netdev);
		return false;
	}

	/*
	 * Find the Tunnel device flow information
	 */
	fl6 = &tunnel->fl.u.ip6;
	nss_ppe_gre_trace("%px: Tunnel param saddr: %pI6 daddr: %pI6\n", netdev, fl6->saddr.s6_addr32, fl6->daddr.s6_addr32);
	nss_ppe_gre_trace("%px: Hop limit %d\n", netdev, tunnel->parms.hop_limit);
	nss_ppe_gre_trace("%px: Tunnel param flag %x  fl6.flowlabel %x\n", netdev,  tunnel->parms.flags, fl6->flowlabel);

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

	/*
	 * Set PPE flags to inherit TTL values if its not set
	 */
	if (inherit_ttl) {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_TTL;
	}

	if (inherit_dscp) {
		l3->flags |=  PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP;
	}

	if (encap_ecn_mode <= PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_RFC4301_RFC6040_NORMAL_MODE) {
		l3->encap_ecn_mode = encap_ecn_mode;
	}

	if (decap_ecn_mode <= PPE_DRV_TUN_CMN_CTX_DECAP_ECN_RFC6040_MODE) {
		l3->decap_ecn_mode = decap_ecn_mode;
	}

	tun_hdr->type = type;

	return nss_ppe_gre_set_gre_flags(gre, tunnel->parms.i_flags, tunnel->parms.o_flags, tunnel->parms.i_key, tunnel->parms.o_key);
}

/*
 * nss_ppe_gre_dev_event()
 *      Net device notifier for gre module
 */
static int nss_ppe_gre_dev_event(struct notifier_block  *nb,
		unsigned long event, void  *info)
{
	struct nss_ppe_gre_ctx *ctx  = &global;
	struct net_device *netdev = netdev_notifier_info_to_dev(info);
	bool status;
	struct ppe_drv_tun_cmn_ctx *tun_hdr;
	struct ppe_tun_excp *tun_cb = NULL;
	enum ppe_drv_tun_cmn_ctx_type type = 0;

	/*
	 * Proceed to handle event only if it GRE (GRETAP / GRETUN) netdevice
	 * and set the type of tunnel accordingly.
	 */
	if (netif_is_ip6gretap(netdev) || netif_is_gretap(netdev)) {
		type = PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP;
	} else if ((netdev->type == ARPHRD_IPGRE) || (netdev->type == ARPHRD_IP6GRE)) {
		type = PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN;
	} else {
	      return NOTIFY_DONE;
	}

	switch (event) {
	case NETDEV_REGISTER:
		if (gre_tunnel_is_fallback_dev(netdev) || gre6_tunnel_is_fallback_dev(netdev)) {
			nss_ppe_gre_warning("%p: GRE tunnel creation skipped for fb dev %s\n", netdev, netdev->name);
			break;
		}

		status = ppe_tun_alloc(netdev, type);
		if (status) {
			nss_gre_stats_dentry_create(ctx, netdev);
		}
		break;

	case NETDEV_UNREGISTER:
		ppe_tun_free(netdev);
		nss_gre_stats_dentry_free(ctx, netdev);
		break;

	case NETDEV_UP:
		nss_ppe_gre_trace("%px: NETDEV_UP :event %lu name %s\n", netdev, event, netdev->name);

		tun_hdr = kzalloc(sizeof(struct ppe_drv_tun_cmn_ctx), GFP_ATOMIC);
		if (!tun_hdr) {
			nss_ppe_gre_warning("%px: memory allocation for tunnel %s failed\n", netdev, netdev->name);
			break;
		}

		if (netif_is_ip6gretap(netdev) || (netdev->type == ARPHRD_IP6GRE)) {
			status = nss_ppe_gre_ip6_dev_parse_param(netdev, tun_hdr, type);
		} else {
			status = nss_ppe_gre_ip4_dev_parse_param(netdev, tun_hdr, type);
		}

		/*
		 * If we are not able to accelerate the outer flows in PPE.
		 * We can delete the tunnel VP as well since we dont support unidirectional flows.
		 */
		if (!status) {
			kfree(tun_hdr);
			ppe_tun_free(netdev);
			nss_gre_stats_dentry_free(ctx, netdev);
			break;
		}

		tun_cb = kzalloc(sizeof(struct ppe_tun_excp), GFP_ATOMIC);

		if (!tun_cb) {
			nss_ppe_gre_warning("%px: memory allocation for tunnel callback failed for device %s\n", netdev, netdev->name);

			kfree(tun_hdr);
			break;
		}

		if (type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP) {
			tun_cb->src_excp_method = nss_ppe_gretap_src_exception;
		} else if (type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN) {
			tun_cb->src_excp_method = nss_ppe_gretun_src_exception;
		}

		tun_cb->stats_update_method = nss_ppe_gre_dev_stats_update;

		if (!(ppe_tun_configure(netdev, tun_hdr, tun_cb))) {
			nss_ppe_gre_trace("%px: Not able to create tunnel for dev: %s\n", netdev, netdev->name);
		}

		kfree(tun_hdr);
		kfree(tun_cb);
		break;

	case NETDEV_DOWN:
		nss_ppe_gre_trace("%px: NETDEV_DOWN :event %lu name %s\n", netdev, event, netdev->name);
		ppe_tun_deconfigure(netdev);
		break;

	case NETDEV_CHANGEMTU:
		nss_ppe_gre_trace("%px: NETDEV_CHANGEMTU :event %lu name %s\n", netdev, event, netdev->name);
		ppe_tun_mtu_set(netdev, netdev->mtu);
		break;

	/*
	 * Bridge leave / join events are relevant only for GRETAP netdevices and
	 * not for GRETUN dev.
	 */
	case NETDEV_BR_LEAVE:
		if (type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP) {
			nss_ppe_gre_trace("%px: NETDEV_BR_LEAVE: name %s\n", netdev, netdev->name);
			if (!ppe_tun_decap_disable(netdev)) {
				nss_ppe_gre_warning("%p: Failed disabling decap at index %s", netdev, netdev->name);
			}
		}

		break;

	case NETDEV_BR_JOIN:
		if (type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP) {
			nss_ppe_gre_trace("%px: NETDEV_BR_JOIN: name %s\n", netdev,  netdev->name);
			if (!ppe_tun_decap_enable(netdev)) {
				nss_ppe_gre_warning("%p: Failed enabling decap at index %s", netdev, netdev->name);
			}
		}

		break;

	default:
		nss_ppe_gre_trace("%px: Unhandled notifier dev %s event %x\n", netdev, netdev->name, (int)event);
		break;
	}

	return NOTIFY_DONE;
}

/*
 * nss_ppe_gre_stats_show()
 *	Read ppe tunnel statistics.
 */
static int nss_ppe_gre_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct net_device *dev = (struct net_device *)m->private;
	uint64_t exception_packet;
	uint64_t exception_bytes;

	ppe_tun_exception_packet_get(dev, &exception_packet, &exception_bytes);
	seq_printf(m, "\n################ PPE Client gre Statistics Start ################\n");
	seq_printf(m, "dev: %s\n", dev->name);
	seq_printf(m, "  Exception:\n");
	seq_printf(m, "\t exception packet: %llu\n", exception_packet);
	seq_printf(m, "\t exception bytes: %llu\n", exception_bytes);
	seq_printf(m, "\n################ PPE Client gre Statistics End ################\n");

	return 0;
}

/*
 * nss_ppe_gre_stats_open()
 */
static int nss_ppe_gre_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, nss_ppe_gre_stats_show, inode->i_private);
}

/*
 * nss_ppe_gre_stats_ops
 *	File operations for gre tunnel stats
 */
static const struct file_operations nss_ppe_gre_stats_ops = {
	.open = nss_ppe_gre_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * nss_ppe_gre_client_stats_show()
 *	Read GRE client statistics.
 *
 * TODO: Print module parameters and other client level stats here.
 */
static int nss_ppe_gre_client_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct nss_ppe_gre_ctx *ctx = (struct nss_ppe_gre_ctx *)m->private;

	seq_printf(m, "\n################ GRE client statistics Start################\n");
	seq_printf(m, "\tTunnel create request with iflag Sequence number: %llu\n", atomic64_read(&ctx->stats.iflag_seq_err));
	seq_printf(m, "\tTunnel create request with oflag Sequence number: %llu\n", atomic64_read(&ctx->stats.oflag_seq_err));
	seq_printf(m, "\tV6 tunnel create requests with non null encap limit: %llu\n", atomic64_read(&ctx->stats.enc_lim_err));
	seq_printf(m, "\tGRETAP source exception packet drop counter: %llu\n", atomic64_read(&ctx->stats.gretap_src_excep_drop_count));
	seq_printf(m, "\tGRETUN source exception packet drop counter: %llu\n", atomic64_read(&ctx->stats.gretun_src_excep_drop_count));
	seq_printf(m, "\tGRETUN key flags set failure: %llu\n", atomic64_read(&ctx->stats.gretun_key_flag_failure));
	seq_printf(m, "\tGRETUN csum flags set failure: %llu\n", atomic64_read(&ctx->stats.gretun_csum_flag_failure));
	seq_printf(m, "\n################ GRE Client Statistics End ################\n");

	return 0;
}

/*
 * nss_ppe_gre_client_stats_open()
 */
static int nss_ppe_gre_client_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, nss_ppe_gre_client_stats_show, inode->i_private);
}

/*
 * nss_ppe_gre_client_stats_ops
 *	File operations for GRE client stats
 */
static const struct file_operations nss_ppe_gre_client_stats_ops = {
	.open = nss_ppe_gre_client_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * nss_gre_stats_dentry_create()
 *	Create dentry for a given netdevice.
 */
static bool nss_gre_stats_dentry_create(struct nss_ppe_gre_ctx *ctx, struct net_device *dev)
{
	char dentry_name[IFNAMSIZ];
	struct dentry *dentry;

	scnprintf(dentry_name, sizeof(dentry_name), "%s", dev->name);
	dentry = debugfs_create_file(dentry_name, S_IRUGO,
			ctx->dentry, dev, &nss_ppe_gre_stats_ops);
	if (!dentry) {
		nss_ppe_gre_warning("%px: Debugfs file creation failed for device %s\n", dev, dev->name);
		return false;
	}

	return true;
}

/*
 * nss_gre_stats_dentry_free()
 *	Remove dentry for a given netdevice.
 */
static bool nss_gre_stats_dentry_free(struct nss_ppe_gre_ctx *ctx, struct net_device *dev)
{
	char dentry_name[IFNAMSIZ];
	struct dentry *dentry;

	scnprintf(dentry_name, sizeof(dentry_name), "%s", dev->name);
	dentry = debugfs_lookup(dentry_name, ctx->dentry);
	if (dentry) {
		debugfs_remove(dentry);
		nss_ppe_gre_trace("%px: removed stats debugfs entry for dev %s", dev, dentry_name);
		return true;
	}

	nss_ppe_gre_trace("%px: Could not find stats debugfs entry for dev %s", dev, dentry_name);
	return false;
}

/*
 * nss_gre_stats_dentry_deinit()
 *	Cleanup the debugfs tree.
 */
static void nss_ppe_gre_dentry_deinit(struct nss_ppe_gre_ctx *ctx)
{
	debugfs_remove_recursive(ctx->dentry);
	ctx->dentry = NULL;
}

/*
 * nss_ppe_gre_dentry_init()
 *	Create gre tunnel statistics debugfs entry.
 */
static bool nss_ppe_gre_dentry_init(struct nss_ppe_gre_ctx *ctx)
{
	/*
	 * Initialize debugfs directory.
	 */
	struct dentry *parent;
	struct dentry *clients;

	parent = debugfs_lookup("qca-nss-ppe", NULL);
	if (!parent) {
		nss_ppe_gre_warning("parent debugfs entry for qca-nss-ppe not present\n");
		return false;
	}

	clients = debugfs_lookup("clients", parent);
	if (!clients) {
		nss_ppe_gre_warning("clients debugfs entry inside qca-nss-ppe not present\n");
		return false;
	}

	ctx->dentry = debugfs_create_dir("gre", clients);
	if (!ctx->dentry) {
		nss_ppe_gre_warning("gre debugfs entry inside qca-nss-ppe/clients could not be created\n");
		return false;
	}

	clients = debugfs_create_file("client", S_IRUGO,
			ctx->dentry, ctx, &nss_ppe_gre_client_stats_ops);
	if (!clients) {
		nss_ppe_gre_warning("GRE Client debugfs create failed\n");
		debugfs_remove(ctx->dentry);
		return false;
	}

	return true;
}

/*
 * Linux Net device Notifier
 */
struct notifier_block nss_ppe_gre_notifier = {
	.notifier_call = nss_ppe_gre_dev_event,
};

/*
 * nss_ppe_gre_init_module()
 *      Tunnel gre module init function
 */
int __init nss_ppe_gre_init_module(void)
{
	struct nss_ppe_gre_ctx *ctx = &global;
	nss_ppe_gre_info("GRE module with build id %s loaded\n",
			NSS_PPE_GRE_BUILD_ID);

	/*
	 * Create the debugfs directory for statistics.
	 */
	if (!nss_ppe_gre_dentry_init(ctx)) {
		nss_ppe_gre_trace("Failed to initialize debugfs\n");
		return -1;
	}

	if (encap_ecn_mode > PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_RFC4301_RFC6040_NORMAL_MODE) {
		nss_ppe_gre_dentry_deinit(ctx);
		nss_ppe_gre_warning("Invalid Encap ECN mode %u\n", encap_ecn_mode);
		return -1;
	}

	if (decap_ecn_mode > PPE_DRV_TUN_CMN_CTX_DECAP_ECN_RFC6040_MODE) {
		nss_ppe_gre_dentry_deinit(ctx);
		nss_ppe_gre_warning("Invalid Decap ECN mode %u\n", decap_ecn_mode);
		return -1;
	}

	register_netdevice_notifier(&nss_ppe_gre_notifier);
	nss_ppe_gre_trace("gre PPE driver registered\n");

	return 0;
}

/*
 * nss_ppe_gre_exit_module()
 * Tunnel gre module exit function
 */
void __exit nss_ppe_gre_exit_module(void)
{
	struct nss_ppe_gre_ctx *ctx = &global;

	/*
	 * deactivate all GRE PPE instances.
	 */
	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP, false);
	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN, false);

	/*
	 * De-initialize debugfs.
	 */
	nss_ppe_gre_dentry_deinit(ctx);

	/*
	 * Unregister net device notification for standard tunnel.
	 */
	unregister_netdevice_notifier(&nss_ppe_gre_notifier);

	nss_ppe_gre_info("gre module unloaded\n");
}

module_init(nss_ppe_gre_init_module);
module_exit(nss_ppe_gre_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS PPE gre client driver");
