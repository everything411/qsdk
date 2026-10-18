/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include "sac.h"
#include "sac_feature.h"
#include "sac_telemetry.h"

struct sac_ctx sac_ctx_g;

static char *sac_stats_string[] = {
	"sac_enabled",
	"enable_rx_count",
	"disable_rx_count",
	"iface_config_rx",
	"mmap_offset",
	"invalid_l4_payload",
	"dns_parse_success",
	"dns_parse_fail",
	"dns_no_valid_ip4",
	"dns_no_valid_ip6",
	"dns_tx_success",
	"dns_tx_fail",
	"dns_tx_limit_reached",
	"dns_rx_ack",
	"dns_write_dis",
	"dns_unsupp_ans_type",
	"dns_ans_noip",
	"sni_tcp443_seen",
	"sni_parse_attempts",
	"sni_found",
	"sni_parse_fail",
	"sni_giveup",
	"sni_flow_cache_full",
	"sni_enqueue_ok",
	"sni_enqueue_fail",
};

/*
 * sac_stats_show()
 *	Called when reading the debugfs stats file
 */
static int sac_stats_show(struct seq_file *s, void *data)
{
	struct sac_ctx *ctx = (struct sac_ctx *)s->private;
	uint64_t *stats_shadow;
	int idx;

	if (!ctx) {
		DEBUG_ERROR("SAC Context not available.\n");
		return 0;
	}

	stats_shadow = (uint64_t *)kmalloc(sizeof(struct sac_stats), GFP_KERNEL);
	if (!stats_shadow) {
		DEBUG_ERROR("Unable to allocate memory for stats.\n");
		return -ENOMEM;
	}

	spin_lock_bh(&ctx->lock);
	memcpy(stats_shadow, &ctx->stats, sizeof(struct sac_stats));
	spin_unlock_bh(&ctx->lock);

	seq_printf(s, "\nSAC driver stats:\n");
	for (idx = 0; idx < (sizeof(struct sac_stats) / sizeof(uint64_t)); idx++) {
		seq_printf(s, "%s:  %llu\n", sac_stats_string[idx], stats_shadow[idx]);
	}

	kfree(stats_shadow);
	return 0;
}

/*
 * sac_stats_open()
 *	Called when opening the debugfs stats file.
 */
static int sac_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, sac_stats_show, inode->i_private);
}

/*
 * sac_stats_ops
 *	File operations for SAC stats.
 */
static const struct file_operations sac_stats_ops = {
	.owner     = THIS_MODULE,
	.open      = sac_stats_open,
	.read      = seq_read,
	.llseek    = seq_lseek,
	.release   = seq_release,
};

/*
 * sac_print_eth_header()
 *	Print ethernet header.
 */
static void __maybe_unused sac_print_eth_header(const struct ethhdr *eth)
{
	if (!eth) {
		DEBUG_ERROR("Ethernet header is NULL\n");
		return;
	}

	DEBUG_INFO("Ethernet Header:\n");
	DEBUG_INFO("  Destination MAC: %pM\n", eth->h_dest);
	DEBUG_INFO("  Source MAC     : %pM\n", eth->h_source);
	DEBUG_INFO("  Protocol       : 0x%04x\n", ntohs(eth->h_proto));
}

/*
 * sac_print_ipv4_header()
 *	Print ipv4 header.
 */
static void __maybe_unused sac_print_ipv4_header(const struct iphdr *iph)
{
	if (!iph) {
		DEBUG_ERROR("[IP_HDR] NULL IP header pointer\n");
		return;
	}

	DEBUG_INFO("[IP_HDR] Src IP: %pI4, Dst IP: %pI4, Proto: %u, Total Len: %u\n",
			&iph->saddr,
			&iph->daddr,
			iph->protocol,
			ntohs(iph->tot_len));
}

/*
 * sac_print_udp_header()
 *	Print udp header.
 */
static void __maybe_unused sac_print_udp_header(const struct udphdr *udph)
{
	if (!udph) {
		DEBUG_ERROR("[UDP_HDR] NULL UDP header pointer\n");
		return;
	}

	DEBUG_INFO("[UDP_HDR] Src Port: %u, Dst Port: %u, Length: %u\n",
			ntohs(udph->source),
			ntohs(udph->dest),
			ntohs(udph->len));
}

/*
 * sac_nf_ingress_hook()
 *	Netfilter ingress hook for DNS extraction.
 */
static unsigned int sac_nf_ingress_hook(void *priv, struct sk_buff *skb,
					const struct nf_hook_state *state)
{
	uint8_t ipv4_hdr_sz;
	uint16_t sport, dport, udp_len;
	struct ethhdr *eh;
	struct udphdr *uh;
	struct iphdr  *ih;

	if (!READ_ONCE(sac_ctx_g.enable)) {
		return NF_ACCEPT;
	}

	/*
	 * TODO: Check if we need to handle v6, or vlan or pppoe here.
	 */
	eh = (struct ethhdr *)eth_hdr(skb);
	if (!eh) {
		DEBUG_ERROR("NULL eth header pointer\n");
		return NF_ACCEPT;
	}

	if (eh->h_proto != htons(ETH_P_IP)) {
		return NF_ACCEPT;
	}

	if (!pskb_may_pull(skb, sizeof(struct iphdr))) {
		DEBUG_ERROR("Can't pull ip header\n");
		return NF_ACCEPT;
	}

	ih = (struct iphdr *)(skb->data);
	ipv4_hdr_sz = ih->ihl * 4;

	if (ih->ihl < 5) {
		return NF_ACCEPT;
	}

	/*
	 * TODO: Check if TCP + DNS is a real use case.
	 * Captures mostly show UDP + DNS, but RFC doesn't mandate UDP only.
	 */
	if (ih->protocol == IPPROTO_UDP) {
		if (!pskb_may_pull(skb, ipv4_hdr_sz + sizeof(struct udphdr))) {
			DEBUG_ERROR("Can't pull udp header\n");
			return NF_ACCEPT;
		}

		uh = (struct udphdr *)(skb->data + ipv4_hdr_sz);
		udp_len = ntohs(uh->len);

		if (!pskb_may_pull(skb, ipv4_hdr_sz + udp_len)) {
			DEBUG_ERROR("Can't pull udp payload\n");
			return NF_ACCEPT;
		}

		sport = ntohs(uh->source);
		dport = ntohs(uh->dest);

		if (sport == SAC_DNS_SPORT || dport == SAC_DNS_DPORT) {
			sac_feature_extract_dns(&sac_ctx_g, uh, false);
		}
	}

	return NF_ACCEPT;
}

/*
 * sac_register_nf_hook()
 *	Register netfilter hook for the netdevice.
 */
static int sac_register_nf_hook(struct net_device *dev, struct sac_ctx *ctx)
{
	struct sac_tracked_iface *tif = NULL;
	struct nf_hook_ops *ops = NULL;
	bool status = false;
	int idx = 0;

	tif = kzalloc(sizeof(struct sac_tracked_iface), GFP_KERNEL);
	if (!tif) {
		DEBUG_ERROR("Failed to allocate memory for tracking entry\n");
		return -ENOMEM;
	}

	tif->dev = dev;
	ops = &tif->nf_hook;

	/*
	 * Configure the netfilter hook ops and add to the hook.
	 */
	ops->hook = sac_nf_ingress_hook;
	ops->pf = NFPROTO_NETDEV;
	ops->hooknum = NF_NETDEV_INGRESS;

	/*
	 * TODO: Confirm if this priority works for all cases with
	 * qdisc folks.
	 */
	ops->priority = NF_IP_PRI_FIRST;
	ops->dev = dev;

	if (nf_register_net_hook(&init_net, ops)) {
		DEBUG_ERROR("Failed to register net hook\n");
		kfree(tif);
		return -EINVAL;
	}

	/*
	 * Find the available space and update it
	 */
	spin_lock_bh(&ctx->lock);
	for (idx = 0; idx < SAC_MAX_IFACE; idx++) {
		if (!ctx->tif[idx]) {
			ctx->tif[idx] = tif;
			ctx->num_devs++;
			status = true;
			break;
		}
	}

	spin_unlock_bh(&ctx->lock);

	/*
	 * Failed to find an empty slot.
	 * Although this should not happen as we have max check earlier.
	 */
	if (!status) {
		DEBUG_ERROR("Failed to find an empty slot, unregister hook\n");
		nf_unregister_net_hook(&init_net, ops);
		kfree(tif);
		return -EINVAL;
	}

	return 0;
}

/*
 * sac_add_tracked_iface()
 *	Add interface for tracking and register hook.
 */
static int sac_add_tracked_iface(const char *ifname)
{
	struct net_device *dev = NULL;
	int status = 0;
	int idx = 0;

	dev = dev_get_by_name(&init_net, ifname);
	if (!dev) {
		DEBUG_ERROR("Device not found %s\n", ifname);
		return -EINVAL;
	}

	spin_lock_bh(&sac_ctx_g.lock);
	if (sac_ctx_g.num_devs >= SAC_MAX_IFACE) {
		DEBUG_ERROR("Reached max limit\n");
		spin_unlock_bh(&sac_ctx_g.lock);
		status = -ENOSPC;
		goto done;
	}

	for (idx = 0; idx < SAC_MAX_IFACE; idx++) {
		if (sac_ctx_g.tif[idx] && sac_ctx_g.tif[idx]->dev == dev) {
			DEBUG_WARN("Already tracking this device %s\n", ifname);
			spin_unlock_bh(&sac_ctx_g.lock);
			status = -EEXIST;
			goto done;
		}
	}

	spin_unlock_bh(&sac_ctx_g.lock);

	/*
	 * Register hook for the net device.
	 */
	status = sac_register_nf_hook(dev, &sac_ctx_g);
	if (status) {
		goto done;
	}

	return status;

done:
	dev_put(dev);
	return status;
}

/*
 * sac_del_tracked_iface()
 *	Removes tracked interfaces and unregister netfilter hook.
 */
static void sac_del_tracked_iface(struct sac_ctx *ctx)
{
	struct sac_tracked_iface *tif0, *tif1;

	BUILD_BUG_ON(SAC_MAX_IFACE != 2);

	/*
	 * Snapshot both interface pointers under lock then clear the slots.
	 */
	spin_lock_bh(&ctx->lock);
	tif0 = ctx->tif[0];
	tif1 = ctx->tif[1];
	ctx->tif[0] = NULL;
	ctx->tif[1] = NULL;
	ctx->num_devs = 0;
	spin_unlock_bh(&ctx->lock);

	/*
	 * Unregister the hook for the interfaces.
	 */
	if (tif0) {
		DEBUG_INFO("Removing dev %s from tracked list\n", tif0->dev->name);
		nf_unregister_net_hook(&init_net, &tif0->nf_hook);
		dev_put(tif0->dev);
		kfree(tif0);
	}

	if (tif1) {
		DEBUG_INFO("Removing dev %s from tracked list\n", tif1->dev->name);
		nf_unregister_net_hook(&init_net, &tif1->nf_hook);
		dev_put(tif1->dev);
		kfree(tif1);
	}
}

/*
 * sac_handle_iface_config()
 *	Handler for interface config request.
 */
void sac_handle_iface_config(struct sac_iface_data *iface)
{
	struct sac_ctx *ctx = &sac_ctx_g;
	char *name;

	if (iface->count > SAC_MAX_IFACE) {
		DEBUG_WARN("SAC: Received unsupported number of interfaces %d\n", iface->count);
		return;
	}

	atomic64_inc(&ctx->stats.iface_cfg_rx);
	if (!READ_ONCE(ctx->enable)) {
		DEBUG_WARN("SAC is not enabled\n");
		return;
	}

	for (int count = 0; count < iface->count; count++) {
		name = iface->name[count];
		if (sac_add_tracked_iface(name)) {
			DEBUG_ERROR("SAC: failed to enable tracking for: %s\n", name);
		}

		DEBUG_TRACE("SAC: Successfully enabled tracking for: %s\n", name);
	}
}

/*
 * sac_disable()
 *	Handler to process disable request.
 */
void sac_disable(void)
{
	struct sac_ctx *ctx = &sac_ctx_g;

	spin_lock_bh(&ctx->lock);
	WRITE_ONCE(ctx->enable, false);
	atomic64_set(&ctx->stats.sac_enable, 0);
	atomic64_inc(&ctx->stats.disable_count);
	spin_unlock_bh(&ctx->lock);

	/*
	 * Remove the interfaces from tracking list.
	 */
	sac_del_tracked_iface(ctx);

	DEBUG_TRACE("SAC disabled.\n");
}

/*
 * sac_enable()
 *	Handler to process enable request.
 */
void sac_enable(void)
{
	struct sac_ctx *ctx = &sac_ctx_g;

	spin_lock_bh(&ctx->lock);
	WRITE_ONCE(ctx->enable, true);

	atomic64_set(&ctx->stats.sac_enable, 1);
	atomic64_inc(&ctx->stats.enable_count);
	spin_unlock_bh(&ctx->lock);

	DEBUG_TRACE("SAC enabled.\n");
}

/*
 * sac_init()
 *	Driver initialization function.
 */
static int __init sac_init(void)
{
	struct sac_ctx *ctx = &sac_ctx_g;

	spin_lock_init(&ctx->lock);
	ctx->enable = false;
	ctx->num_devs = 0;
	ctx->sni_enable = false;
	ctx->sni_max_attempts = SAC_FEATURE_SNI_DEFAULT_MAX_ATTEMPTS;

	BUILD_BUG_ON(ARRAY_SIZE(sac_stats_string) !=
		     sizeof(struct sac_stats) / sizeof(atomic64_t));

	/*
	 * Create debugfs entry.
	 */
	ctx->dir = debugfs_create_dir("qca-sac-drv", NULL);
	if (!ctx->dir) {
		DEBUG_ERROR("Failed to create debugfs dir for sac\n");
		return -ENOMEM;
	}

	/*
	 * Add stats file.
	 */
	if (!debugfs_create_file("stats", S_IRUGO, ctx->dir, ctx,
				 &sac_stats_ops)) {
		DEBUG_ERROR("Failed to create sac stats file\n");
		debugfs_remove_recursive(ctx->dir);
		return -ENOMEM;
	}

	if (sac_telemetry_init(ctx)) {
		DEBUG_ERROR("Failed to initialize telemetry context\n");
		debugfs_remove_recursive(ctx->dir);
		return -ENOMEM;
	}

	if (sac_feature_sni_init(&sac_feature_sni_ctx_g, ctx->dir)) {
		DEBUG_ERROR("Failed to initialize SNI context\n");
		sac_telemetry_deinit(ctx);
		debugfs_remove_recursive(ctx->dir);
		return -ENOMEM;
	}

	DEBUG_INFO("SAC Module Loaded %s\n", SAC_BUILD_ID);
	return 0;
}

/*
 * sac_exit()
 *	Driver exit function.
 */
static void __exit sac_exit(void)
{
	debugfs_remove_recursive(sac_ctx_g.dir);

	/*
	 * Unregister interface NF hooks.
	 */
	sac_disable();
	sac_feature_sni_deinit(&sac_feature_sni_ctx_g);
	sac_telemetry_deinit(&sac_ctx_g);
	DEBUG_INFO("SAC Module unloaded %s\n", SAC_BUILD_ID);
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Smart Application Classification Driver");

module_init(sac_init);
module_exit(sac_exit);
