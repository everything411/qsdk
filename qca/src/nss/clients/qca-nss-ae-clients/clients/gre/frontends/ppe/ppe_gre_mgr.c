/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/version.h>
#include <linux/inetdevice.h>
#include <net/ip.h>
#include <net/gre.h>
#include <ppe_drv.h>
#include <net/ip6_tunnel.h>
#include <linux/debugfs.h>
#include <ppe_drv_public.h>
#include "ppe_gre_mgr.h"
#include "gre_mgr.h"
#include "ppe_gre_mgr_stats.h"
#include <nss_client_mgr.h>

struct ppe_gre_mgr_ctx *global_ctx;

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

static uint8_t ipv6_flow_label = PPE_DRV_TUN_CMN_CTX_FLOW_LABEL_FIX ;
module_param(ipv6_flow_label, byte, 0444);
MODULE_PARM_DESC(ipv6_flow_label, "IPv6 flow label mode 0:Fix, 1:Hash, 2:Copy from Inner");

/*
 * ppe_gre_mgr_gretun_src_exception()
 *	Handle the source VP exception for GRETUN
 */
static bool ppe_gre_mgr_gretun_src_exception(struct ppe_vp_cb_info *info, ppe_tun_data *tun_data)
{
	struct sk_buff *skb = info->skb;
	int ret;
	struct ppe_gre_mgr_ctx *ctx = global_ctx;
	unsigned char *data = skb->data;

	skb_reset_network_header(skb);
	if ((data[0] >> 4) == IPVERSION) {
		skb->protocol = htons(ETH_P_IP);
	} else if ((data[0] >> 4) == 6) {
		skb->protocol = htons(ETH_P_IPV6);
	} else {
		dev_kfree_skb_any(skb);
		gre_mgr_warning("%p: Not an IP packet \n", skb->dev);
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
		gre_mgr_warning("%p: exception packet dropped for gretun\n", skb->dev);
		atomic64_inc(&ctx->stats.gretun_src_excep_drop_count);
	}

	return true;
}

/*
 * ppe_gre_mgr_gretap_src_exception()
 *	Handle the source VP exception
 */
static bool ppe_gre_mgr_gretap_src_exception(struct ppe_vp_cb_info *info, ppe_tun_data *tun_data)
{
	struct ppe_gre_mgr_ctx *ctx = global_ctx;

	struct sk_buff *skb = info->skb;
	struct net_device *dev = skb->dev;
	int ret;

	skb->pkt_type = PACKET_HOST;
	/*
	 * Packet type is updated to PACKET_OTHERHOST in eth_type_trans. Since the packet
	 * is already decapsulated set it to PACKET_HOST for futher processing
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
		gre_mgr_warning("%p: excpetion packet dropped for gretap\n", dev);
		atomic64_inc(&ctx->stats.gretap_src_excep_drop_count);
	}

	return true;
}

/*
 * ppe_gre_mgr_flags_check_cmn()
 * 	API to check the common tunnel create flags between v4 and v6.
 */
static bool ppe_gre_mgr_flags_check_cmn(struct net_device *dev, uint16_t i_flags, uint16_t o_flags, enum ppe_drv_tun_cmn_ctx_type type)
{
	struct ppe_gre_mgr_ctx *ctx = global_ctx;

	/*
	 * Currently GRE tunnel offload with KEY and CSUM flags are not supported in PPE
	 */
	if (type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN) {
		if (i_flags & TUNNEL_KEY || o_flags & TUNNEL_KEY) {
			gre_mgr_warning("%p: GRE tunnel offload with keys flags are not supported in PPE \n", dev);
			atomic64_inc(&ctx->stats.gretun_key_flag_failure);
			return false;
		}

		if (i_flags & TUNNEL_CSUM || o_flags & TUNNEL_CSUM) {
			gre_mgr_warning("%p: GRE tunnel offload with csum flags are not supporeted in PPE \n", dev);
			atomic64_inc(&ctx->stats.gretun_csum_flag_failure);
			return false;
		}
	}

	if (i_flags & TUNNEL_SEQ) {
		gre_mgr_warning("%p:%s iflag SEQ not supported\n", dev, dev->name);
		atomic64_inc(&ctx->stats.iflag_seq_err);
		return false;
	}

	if (o_flags & TUNNEL_SEQ) {
		gre_mgr_warning("%p:%s oflag SEQ not supported\n", dev, dev->name);
		atomic64_inc(&ctx->stats.oflag_seq_err);
		return false;
	}

	return true;
}

/*
 * nss_ppe_gre_flags_check_v6()
 *	API to check if the v6 tunnel create flags are supported.
 *
 * There are a number of extended feature for GRE tunnels which are specified
 * when creating the tunnel.This API Checks if the flags are supported.
 */
static bool ppe_gre_mgr_flags_check_v6(struct net_device *dev, struct ip6_tnl *tun, enum ppe_drv_tun_cmn_ctx_type type)
{
	struct ppe_gre_mgr_ctx *ctx = global_ctx;

	if (!(tun->parms.flags & IP6_TNL_F_IGN_ENCAP_LIMIT)) {
		gre_mgr_warning("%p:%s Encap limit should be none", dev, dev->name);
		atomic64_inc(&ctx->stats.enc_lim_err);
		return false;
	}

	return ppe_gre_mgr_flags_check_cmn(dev, tun->parms.i_flags, tun->parms.o_flags, type);
}

/*
 * nss_ppe_gre_set_gre_flags()
 *     Set GRE Key, optional flags according to the config
 */
bool ppe_gre_mgr_set_gre_flags(struct ppe_drv_tun_cmn_ctx_gre *gre, uint16_t iflags, uint16_t oflags, uint32_t i_key, uint32_t o_key)
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
		gre_mgr_info("%p:ICSUM option enabled for GRE\n", gre);
	}

	if (oflags & TUNNEL_CSUM) {
		gre->flags |= PPE_DRV_TUN_CMN_CTX_GRE_ENCAP_CSUM;
		gre_mgr_info("%p:OCSUM option enabled for GRE\n", gre);
	}

	return true;
}

/*
 * nss_ppe_gre_ip4_dev_parse_param()
 *      Parse IPv4 gre arguments sent to PPE driver
 */
static bool ppe_gre_mgr_ip4_dev_parse_param(struct net_device *netdev, struct ppe_drv_tun_cmn_ctx *tun_hdr,
						enum ppe_drv_tun_cmn_ctx_type type)
{
	bool tun_cfg_ol_support;
	struct ip_tunnel *tunnel;
	struct ppe_drv_tun_cmn_ctx_l3 *l3 = &tun_hdr->l3;
	struct iphdr *iphdr;
	struct ppe_drv_tun_cmn_ctx_gre *gre = &tun_hdr->tun.gre;
	tunnel = (struct ip_tunnel *)netdev_priv(netdev);

	tun_cfg_ol_support = ppe_gre_mgr_flags_check_cmn(netdev, tunnel->parms.i_flags, tunnel->parms.o_flags, type);
	if (!tun_cfg_ol_support) {
		gre_mgr_warning("%p:Configured GRE extended header not supported\n", netdev);
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
	} else {
		l3->flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DF;
	}

	if (encap_ecn_mode <= PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_RFC4301_RFC6040_NORMAL_MODE) {
		l3->encap_ecn_mode = encap_ecn_mode;
	}

	if (decap_ecn_mode <= PPE_DRV_TUN_CMN_CTX_DECAP_ECN_RFC6040_MODE) {
		l3->decap_ecn_mode = decap_ecn_mode;
	}

	tun_hdr->type = type;

	return ppe_gre_mgr_set_gre_flags(gre, tunnel->parms.i_flags, tunnel->parms.o_flags, tunnel->parms.i_key, tunnel->parms.o_key);
}

/*
 * nss_ppe_gre_ip6_dev_parse_param()
 *      Parse IPv6 gre arguments sent to PPE driver
 */
static bool ppe_gre_mgr_ip6_dev_parse_param(struct net_device *netdev, struct ppe_drv_tun_cmn_ctx *tun_hdr,
						enum ppe_drv_tun_cmn_ctx_type type)
{
	struct ip6_tnl *tunnel;
	struct flowi6 *fl6;
	struct ppe_drv_tun_cmn_ctx_l3 *l3 = &tun_hdr->l3;
	bool tun_cfg_ol_support;
	struct ppe_drv_tun_cmn_ctx_gre *gre = &tun_hdr->tun.gre;
	tunnel = (struct ip6_tnl *)netdev_priv(netdev);

	tun_cfg_ol_support = ppe_gre_mgr_flags_check_v6(netdev, tunnel, type);
	if (!tun_cfg_ol_support) {
		gre_mgr_warning("%p:Configured GRE extended header not supported\n", netdev);
		return false;
	}

	/*
	 * Find the Tunnel device flow information
	 */
	fl6 = &tunnel->fl.u.ip6;
	gre_mgr_trace("%px: Tunnel param saddr: %pI6 daddr: %pI6\n", netdev, fl6->saddr.s6_addr32, fl6->daddr.s6_addr32);
	gre_mgr_trace("%px: Hop limit %d\n", netdev, tunnel->parms.hop_limit);
	gre_mgr_trace("%px: Tunnel param flag %x  fl6.flowlabel %x\n", netdev,  tunnel->parms.flags, fl6->flowlabel);

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

	if (ipv6_flow_label == PPE_DRV_TUN_CMN_CTX_FLOW_LABEL_FIX) {
		l3->flow_label_val = flowi6_get_flowlabel(fl6);
	}
	l3->flow_label = ipv6_flow_label;

	tun_hdr->type = type;

	return ppe_gre_mgr_set_gre_flags(gre, tunnel->parms.i_flags, tunnel->parms.o_flags, tunnel->parms.i_key, tunnel->parms.o_key);
}

/*
 * ppe_gre_mgr_dentry_deinit()
 *	Cleanup the debugfs tree.
 */
static void ppe_gre_mgr_dentry_deinit(struct ppe_gre_mgr_ctx *ppe_ctx)
{
	debugfs_remove_recursive(ppe_ctx->dentry);
	ppe_ctx->dentry = NULL;
}

/*
 * ppe_gre_mgr_dentry_init()
 *	Create gre tunnel statistics debugfs entry.
 */
static bool ppe_gre_mgr_dentry_init(struct ppe_gre_mgr_ctx *ppe_ctx)
{
	/*
	 * Initialize debugfs directory.
	 */
	struct dentry *parent;
	struct dentry *clients;

	parent = debugfs_lookup("qca-nss-ppe", NULL);
	if (!parent) {
		gre_mgr_warning("parent debugfs entry for qca-nss-ppe not present\n");
		return false;
	}

	clients = debugfs_lookup("clients", parent);
	if (!clients) {
		gre_mgr_warning("clients debugfs entry inside qca-nss-ppe not present\n");
		return false;
	}

	ppe_ctx->dentry = debugfs_create_dir("gre", clients);
	if (!ppe_ctx->dentry) {
		gre_mgr_warning("gre debugfs entry inside qca-nss-ppe/clients could not be created\n");
		return false;
	}

	clients = ppe_gre_mgr_stats_debugfs_init(ppe_ctx);
	if (!clients) {
		gre_mgr_warning("GRE Client debugfs create failed\n");
		debugfs_remove(ppe_ctx->dentry);
		return false;
	}

	return true;
}

enum ppe_drv_tun_cmn_ctx_type ppe_gre_mgr_gre_type_get(enum gre_mgr_cmn_ctx_type gre_tun_type)
{
	if (gre_tun_type == GRE_MGR_TUN_CMN_CTX_TYPE_GRETAP) {
		return PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP;
	}

	if (gre_tun_type == GRE_MGR_TUN_CMN_CTX_TYPE_GRETUN) {
		return PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN;
	}

	return PPE_DRV_TUN_CMN_CTX_TYPE_MAX;
}

/*
 * ppe_gre_mgr_netdev_register()
 * 	Handle NETDEV REGISTER event in PPE
 */
bool ppe_gre_mgr_netdev_register(struct net_device *netdev, enum gre_mgr_cmn_ctx_type gre_tun_type)
{
	struct ppe_gre_mgr_ctx *ppe_ctx = global_ctx;
	enum ppe_drv_tun_cmn_ctx_type type = 0;
	bool status;

	type = ppe_gre_mgr_gre_type_get(gre_tun_type);
	if (type == PPE_DRV_TUN_CMN_CTX_TYPE_MAX) {
		gre_mgr_warning("%px: Invalid GRE Tunnel type for dev: %s\n", netdev, netdev->name);
		return false;
	}

	status = ppe_tun_alloc(netdev, type);
	return status ? ppe_gre_mgr_stats_dentry_create(ppe_ctx, netdev) : false;
}

/*
 * ppe_gre_mgr_netdev_unregister()
 * 	Handle NETDEV UNREGISTER event in PPE
 */
void ppe_gre_mgr_netdev_unregister(struct net_device *netdev)
{
	struct ppe_gre_mgr_ctx *ppe_ctx = global_ctx;
	ppe_tun_free(netdev);
	ppe_gre_mgr_stats_dentry_free(ppe_ctx, netdev);
}

/*
 * ppe_gre_mgr_netdev_up()
 * 	Handle NETDEV UP event in PPE
 */
bool ppe_gre_mgr_netdev_up(struct net_device *netdev, enum gre_mgr_cmn_ctx_type gre_tun_type)
{
	struct ppe_gre_mgr_ctx *ppe_ctx = global_ctx;
	struct ppe_drv_tun_cmn_ctx *tun_hdr;
	struct ppe_tun_excp *tun_cb = NULL;
	enum ppe_drv_tun_cmn_ctx_type type = 0;
	bool status;

	gre_mgr_trace("%px: NETDEV_UP : name %s\n", netdev, netdev->name);

	type = ppe_gre_mgr_gre_type_get(gre_tun_type);
	if (type == PPE_DRV_TUN_CMN_CTX_TYPE_MAX) {
		gre_mgr_warning("%px: Invalid GRE Tunnel type for dev: %s\n", netdev, netdev->name);
		return false;
	}

	tun_hdr = kzalloc(sizeof(struct ppe_drv_tun_cmn_ctx), GFP_ATOMIC);
	if (!tun_hdr) {
		gre_mgr_warning("%px: memory allocation for tunnel %s failed\n", netdev, netdev->name);
		return false;
	}

	if (netif_is_ip6gretap(netdev) || (netdev->type == ARPHRD_IP6GRE)) {
		status = ppe_gre_mgr_ip6_dev_parse_param(netdev, tun_hdr, type);
	}
	else {
		status = ppe_gre_mgr_ip4_dev_parse_param(netdev, tun_hdr, type);
	}

	/*
	 * If we are not able to accelerate the outer flows in PPE.
	 * We can delete the tunnel VP as well since we dont support unidirectional flows.
	 */
	if (!status) {
		goto fail;
	}

	tun_cb = kzalloc(sizeof(struct ppe_tun_excp), GFP_ATOMIC);

	if (!tun_cb) {
		gre_mgr_warning("%px: memory allocation for tunnel callback failed for device %s\n", netdev, netdev->name);
		goto fail;
	}

	if (type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP) {
		tun_cb->src_excp_method = ppe_gre_mgr_gretap_src_exception;
	} else if (type == PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN) {
		tun_cb->src_excp_method = ppe_gre_mgr_gretun_src_exception;
	}

	tun_cb->stats_update_method = ppe_gre_mgr_dev_stats_update;

	if (!(ppe_tun_configure(netdev, tun_hdr, tun_cb))) {
		gre_mgr_warning("%px: Not able to create tunnel for dev: %s\n", netdev, netdev->name);
		kfree(tun_cb);
		goto fail;
	}

	kfree(tun_hdr);
	kfree(tun_cb);
	return true;

fail:
	kfree(tun_hdr);
	ppe_tun_free(netdev);
	ppe_gre_mgr_stats_dentry_free(ppe_ctx, netdev);
	return false;
}

/*
 * ppe_gre_mgr_netdev_down()
 * 	Handle NETDEV DOWN event in PPE
 */
void ppe_gre_mgr_netdev_down(struct net_device *netdev)
{
	gre_mgr_warning("%px: NETDEV_DOWN : name %s\n", netdev, netdev->name);
	ppe_tun_deconfigure(netdev);
}

/*
 * ppe_gre_mgr_netdev_changemtu()
 * 	Handle NETDEV MTU change event in PPE
 */
bool ppe_gre_mgr_netdev_changemtu(struct net_device *netdev)
{
	if (!ppe_tun_mtu_set(netdev, netdev->mtu)) {
		gre_mgr_warning("%px: Failed to update mtu for device %s\n", netdev, netdev->name);
		return false;
	}
	gre_mgr_warning("%px: NETDEV_CHANGEMTU : name %s\n", netdev, netdev->name);
	return true;
}

/*
 * ppe_gre_mgr_brleave()
 * 	Handle Bridge leave event in PPE
 * 	Valid only for GRETAP Tunnel
 */
bool ppe_gre_mgr_brleave(struct net_device *netdev)
{
	if (!ppe_tun_decap_disable(netdev)) {
		gre_mgr_warning("%p: Failed disabling decap at index %s", netdev, netdev->name);
		return false;
	}
	return true;
}

/*
 * ppe_gre_mgr_brjoin()
 * 	Handle Bridge Join event in PPE
 * 	Valid only for GRETUN Tunnel
 */
bool ppe_gre_mgr_brjoin(struct net_device *netdev)
{
	if (!ppe_tun_decap_enable(netdev)) {
		gre_mgr_warning("%px: Failed enabling encap at index %s", netdev, netdev->name);
		return false;
	}
	return true;
}

/*
 * ppe_gre_mgr_ctx_deinit()
 * 	Deinitialize the gre_ctx fields
 */
void ppe_gre_mgr_ctx_deinit(struct gre_mgr_cmn_ctx *gre_ctx)
{
	memset(gre_ctx, 0, sizeof(struct gre_mgr_cmn_ctx));
}

/*
 * ppe_gre_mgr_ctx_init()
 * 	Sets the PPE netdev handler with the GRE client
 */
void ppe_gre_mgr_ctx_init(struct gre_mgr_cmn_ctx *gre_ctx)
{
	gre_ctx->netdev_register = ppe_gre_mgr_netdev_register;
	gre_ctx->netdev_unregister = ppe_gre_mgr_netdev_unregister;
	gre_ctx->netdev_up = ppe_gre_mgr_netdev_up;
	gre_ctx->netdev_down = ppe_gre_mgr_netdev_down;
	gre_ctx->netdev_changemtu = ppe_gre_mgr_netdev_changemtu;
	gre_ctx->netdev_bridgeleave = ppe_gre_mgr_brleave;
	gre_ctx->netdev_bridgejoin = ppe_gre_mgr_brjoin;
}

/*
 * ppe_gre_mgr_init()
 *      PPE GRE Tunnel init function
 */
int ppe_gre_mgr_init(struct gre_mgr_cmn_ctx *gre_ctx)
{
	/*
	 * Allocate ppe_gre_mgr_ctx
	 */
	global_ctx = kzalloc(sizeof(struct ppe_gre_mgr_ctx), GFP_KERNEL);
	if (!global_ctx) {
		gre_mgr_warning("Failed to allocate global ppe mgr ctx\n");
		return 0;
	}

	struct ppe_gre_mgr_ctx *ppe_ctx = global_ctx;

	/*
	 * Create the debugfs directory for statistics.
	 */
	if (!ppe_gre_mgr_dentry_init(ppe_ctx)) {
		gre_mgr_trace("Failed to initialize debugfs\n");
		kfree(ppe_ctx);
		return 0;
	}

	if (encap_ecn_mode > PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_RFC4301_RFC6040_NORMAL_MODE) {
		ppe_gre_mgr_dentry_deinit(ppe_ctx);
		gre_mgr_warning("Invalid Encap ECN mode %u\n", encap_ecn_mode);
		kfree(ppe_ctx);
		return 0;
	}

	if (decap_ecn_mode > PPE_DRV_TUN_CMN_CTX_DECAP_ECN_RFC6040_MODE) {
		ppe_gre_mgr_dentry_deinit(ppe_ctx);
		gre_mgr_warning("Invalid Decap ECN mode %u\n", decap_ecn_mode);
		kfree(ppe_ctx);
		return 0;
	}

	if (ipv6_flow_label > PPE_DRV_TUN_CMN_CTX_FLOW_LABEL_COPY) {
		ppe_gre_mgr_dentry_deinit(ppe_ctx);
		gre_mgr_warning("Invalid flow label mode %u\n", ipv6_flow_label);
		kfree(ppe_ctx);
		return 0;
	}

	ppe_gre_mgr_ctx_init(gre_ctx);
	gre_mgr_trace("gre PPE driver registered\n");

	ppe_gre_mgr_minidump_log(ppe_ctx, sizeof(struct ppe_gre_mgr_ctx), "ppe_gre_mgr_ctx");

	return 1;
}

/*
 * ppe_gre_mgr_exit()
 * 	PPE GRE Tunnel exit function
 */
void ppe_gre_mgr_exit(struct gre_mgr_cmn_ctx *gre_ctx)
{
	struct ppe_gre_mgr_ctx *ppe_ctx = global_ctx;

	/*
	 * deactivate all GRE PPE instances.
	 */
	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP, false);
	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN, false);

	/*
	 * De-initialize debugfs.
	 */
	ppe_gre_mgr_ctx_deinit(gre_ctx);
	ppe_gre_mgr_dentry_deinit(ppe_ctx);

	ppe_gre_mgr_minidump_free(ppe_ctx, "ppe_gre_mgr_ctx");
	kfree(ppe_ctx);

	gre_mgr_info("gre module unloaded\n");
}
