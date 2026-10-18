/*
 * netfn_capwap_tun.c
 *	Network function's CAPWAP offload tunnel configuration.
 *
 * Copyright (c) 2023-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/skbuff.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/lockdep.h>
#include <linux/rtnetlink.h>
#include <linux/debugfs.h>
#include <linux/ipv6.h>
#include <linux/bitmap.h>
#include <linux/if_arp.h>

#include <ppe_vp_public.h>
#include <ppe_drv.h>
#include <netfn_dtls.h>
#include <nss_dp_api_if.h>

#include "netfn_capwap.h"
#include "netfn_capwap_priv.h"
#include "netfn_capwap_hdr.h"
#include "netfn_capwap_tun.h"
#include "netfn_capwap_enc.h"
#include "netfn_capwap_dec.h"

#define NETFN_CAPWAP_MAX_STRLEN 25
#define NETFN_CAPWAP_TUN_STATS_MAX (sizeof(struct netfn_capwap_pkt_stats) / sizeof(uint64_t))


static void netfn_capwap_tun_enc(struct netfn_capwap_tun *nct, struct sk_buff *skb);
static void netfn_capwap_tun_rx_deliver(struct netfn_capwap_tun *nct, struct sk_buff_head *q_head);

/*
 * Tunnel statistics strings
 */
static int8_t *g_tun_stats_str[] = {
	"tx_pkts",
	"tx_bytes",
	"tx_errors",
	"tx_dropped",
	"rx_pkts",
	"rx_bytes",
	"rx_errors",
	"rx_dropped",
	"res1",
	"res2",
};

/*
 * netfn_capwap_tun_rx()
 *	Common rx handling for dev rx handler and VP handler.
 */
static void netfn_capwap_tun_rx(struct netfn_capwap_tun *nct, struct sk_buff *skb)
{
	struct netfn_capwap_dec_stats *stats = &nct->dec.stats;
	struct sk_buff_head q_head;

	skb_queue_head_init(&q_head);
	__skb_queue_head(&q_head, skb);

	/*
	 * Decapsulate and reassemble the skb
	 */
	if (unlikely(!netfn_capwap_dec_rx_skbs(&nct->dec, &q_head))) {
		NETFN_CAPWAP_TUN_STATS_ADD(&stats->err_dec_failure, skb_queue_len(&q_head));
		goto fail;
	}

	/*
	 * Deliver packet to rx handler.
	 */
	netfn_capwap_tun_rx_deliver(nct, &q_head);
	return;
fail:
	dev_kfree_skb_list_fast(&q_head);
	return;
}

/*
 * netfn_capwap_tun_rx_handler()
 *	Receive an SKB queueu for processing.
 */
static void netfn_capwap_tun_rx_handler(struct sk_buff *skb, void *data)
{
	struct net_device *dev = data;
	struct netfn_capwap_tun *nct;

	nct = netdev_priv(dev);

	netfn_capwap_tun_rx(nct, skb);
}

/*
 * netfn_capwap_tun_tx()
 *	Device transmit function
 */
static netdev_tx_t netfn_capwap_tun_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct netfn_capwap_tun *nct = netdev_priv(dev);
	unsigned int needed_hroom, needed_troom;
	struct netfn_capwap_enc_stats *stats;
	struct netfn_capwap_prehdr *phdr;
	bool expand_skb;

	stats = &nct->enc.stats;
	needed_hroom = dev->needed_headroom;
	needed_troom = dev->needed_tailroom;

	/*
	 * In case of non-linear SKB we would like to linearize
	 * skb_linearize() internally checks if pkt is non-linear.
	 */
	if (skb_linearize(skb)) {
		goto fail;
	}

	/*
	 * Unshare the SKB as we will be modifying it.
	 */
	if (unlikely(skb_shared(skb))) {
		skb = skb_unshare(skb, GFP_NOWAIT | __GFP_NOWARN);
		if (!skb) {
			pr_warn_ratelimited("%px: Failed to unshare\n", dev);
			goto fail;
		}
	}

	/*
	 * Check if we have sufficient headroom and tailroom.
	 */
	expand_skb = (skb_cloned(skb) || (skb_headroom(skb) < needed_hroom) || (skb_tailroom(skb) < needed_troom));
	if (expand_skb && pskb_expand_head(skb, needed_hroom, needed_troom, GFP_NOWAIT | __GFP_NOWARN)) {
		pr_warn_ratelimited("%px: Failed to expand SKB with headroom(%u) tailroom(%u)\n",
				dev, skb_headroom(skb), skb_tailroom(skb));
		NETFN_CAPWAP_TUN_STATS_INC(&stats->err_insufficient_hroom);
		goto fail;
	}

	NETFN_CAPWAP_CB(skb)->phdr = *(struct netfn_capwap_prehdr *)skb->data;
	phdr = &NETFN_CAPWAP_CB(skb)->phdr;
	__skb_pull(skb, sizeof(*phdr));

	/*
	 * Check for capwap version.
	 */
	if (unlikely(phdr->version != NETFN_CAPWAP_VERSION)) {
		pr_warn_ratelimited("%px: Version mismatch actual:%d, expected: %d.\n", skb,
				phdr->version, NETFN_CAPWAP_VERSION);
		NETFN_CAPWAP_TUN_STATS_INC(&stats->err_ver_mis);
		goto fail;
	}

	/*
	 * Check for dtls packets.
	 */
	if (unlikely(phdr->type & NETFN_CAPWAP_PKT_TYPE_DTLS)) {
		pr_warn_ratelimited("%px: We don't support DTLS packets encapsulation.\n", skb);
		NETFN_CAPWAP_TUN_STATS_INC(&stats->err_direct_dtls);
		goto fail;
	}

	/*
	 * Number of wireless sections shouldn't exceed max count.
	 */
	if (unlikely(phdr->nwireless > NETFN_CAPWAP_MAX_NWIRELESS)) {
		pr_warn_ratelimited("%px: Number of wireless sections %d exceeds max count(%d).\n",
				phdr, phdr->nwireless, NETFN_CAPWAP_MAX_NWIRELESS);
		NETFN_CAPWAP_TUN_STATS_INC(&stats->err_nwireless_len);
		goto fail;
	}

	netfn_capwap_tun_enc(nct, skb);
	return NETDEV_TX_OK;
fail:
	/*
	 * dev_kfree_skb has a NULL check inside.
	 */
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

/*
 * netfn_capwap_tun_open()
 *	Open the device for usage
 */
static int netfn_capwap_tun_open(struct net_device *dev)
{
	struct netfn_capwap_tun *nct = netdev_priv(dev);
	struct netfn_capwap_dec_ctx *ctx;
	int i;

	/*
	 * Open and close are protected by rtnl_lock()
	 */
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		pr_warn("%px: Unable to allocate memory for reassembly context.\n", nct);
		return -ENOMEM;
	}

	/*
	 * Initialize frags array.
	 */
	for (i = 0; i < NETFN_CAPWAP_TUN_MAX_REASM_WIN; i++) {
		netfn_capwap_frags_init(&ctx->reasm_table[i]);
	}

	ctx->dec = &nct->dec;
	rcu_assign_pointer(nct->dec.ctx, ctx);
	netif_start_queue(dev);
	return 0;
}

/*
 * netfn_capwap_tun_stop()
 *	Stop the device from usage
 */
static int netfn_capwap_tun_stop(struct net_device *dev)
{
	struct netfn_capwap_tun *nct = netdev_priv(dev);
	struct netfn_capwap_dec_ctx *ctx;
	int i;

	/*
	 * Stop the queue so that no additional SKBs queue in.
	 */
	netif_stop_queue(dev);

	/*
	 * There are open references to pool of SKBs sitting in reasm_table.
	 */
	ctx = rcu_dereference_protected(nct->dec.ctx, lockdep_rtnl_is_held());
	RCU_INIT_POINTER(nct->dec.ctx, NULL);
	synchronize_net();

	if (ctx->reasm_cache) {
		kfree_skb(ctx->reasm_cache);
	}

	for (i = 0; i < NETFN_CAPWAP_TUN_MAX_REASM_WIN; i++) {
		dev_kfree_skb_list_fast(&ctx->reasm_table[i].list);
	}

	kfree(ctx);
	return 0;
}

/*
 * netfn_capwap_tun_stats_update()
 *	Updates the tunnel statistics
 */
static inline void netfn_capwap_tun_stats_update(struct netfn_capwap_tun *nct)
{
	struct netfn_capwap_pkt_stats *stats = &nct->stats;

	/*
	 * Accumulate Rx/Tx errors and Rx/Tx drops.
	 */
	stats->rx_errors = netfn_capwap_dec_get_err_stats(&nct->dec);
	stats->tx_errors = netfn_capwap_enc_get_err_stats(&nct->enc);
	stats->rx_dropped = netfn_capwap_dec_get_drop_stats(&nct->dec);
	stats->tx_dropped = netfn_capwap_enc_get_drop_stats(&nct->enc);
}

/*
 * netfn_capwap_tun_dev_stats64()
 *	Get tunnel statistics
 */
static void netfn_capwap_tun_get_stats64(struct net_device *dev, struct rtnl_link_stats64 *dev_stats)
{
	struct netfn_capwap_tun *nct = netdev_priv(dev);
	struct netfn_capwap_pkt_stats *pkt_stats = &nct->stats;

	/*
	 * Update the tunnel stats with encap and decap
	 */
	netfn_capwap_tun_stats_update(nct);

	dev_stats->rx_packets = READ_ONCE(pkt_stats->rx_pkts);
	dev_stats->rx_bytes = READ_ONCE(pkt_stats->rx_bytes);
	dev_stats->tx_packets = READ_ONCE(pkt_stats->tx_pkts);
	dev_stats->tx_bytes = READ_ONCE(pkt_stats->tx_bytes);
	dev_stats->tx_dropped = READ_ONCE(pkt_stats->tx_dropped);
	dev_stats->rx_dropped = READ_ONCE(pkt_stats->rx_dropped);
	dev_stats->tx_errors = READ_ONCE(pkt_stats->tx_errors);
	dev_stats->rx_errors = READ_ONCE(pkt_stats->rx_errors);
}

/*
 * netfn_capwap_tun_mtu()
 *	Change tunnel MTU
 */
static int netfn_capwap_tun_mtu(struct net_device *dev, int mtu)
{
	struct netfn_capwap_tun *nct = netdev_priv(dev);
	struct net_device *next_dev = nct->next_dev;
	int next_mtu;

	/*
	 * Set nexthop MTU.
	 */
	__dev_set_mtu(next_dev, mtu);
	next_mtu = READ_ONCE(next_dev->mtu);

	/*
	 * Set maximum fragmenatation length for Encapsulation.
	 */
	netfn_capwap_enc_mtu_update(&nct->enc, next_mtu);

	/*
	 * Set our device MTU.
	 */
	WRITE_ONCE(dev->mtu, mtu);

	pr_info("MTU updated with flow mtu(%u)\n", mtu);
	return 0;
}

/*
 * capwap_tun_ops
 *	Capwap device operations.
 */
static const struct net_device_ops capwap_tun_ops = {
	.ndo_open = netfn_capwap_tun_open,
	.ndo_stop = netfn_capwap_tun_stop,
	.ndo_start_xmit = netfn_capwap_tun_tx,
	.ndo_get_stats64 = netfn_capwap_tun_get_stats64,
	.ndo_change_mtu = netfn_capwap_tun_mtu,
};

/*
 * netfn_capwap_tun_destructor()
 *	Tunnel dev destructor called for final free
 */
static void netfn_capwap_tun_destructor(struct net_device *dev)
{
	struct netfn_capwap_tun *nct = netdev_priv(dev);

	pr_info("%px: Freeing the tunnel(%s)\n", &global_nc, dev->name);

	debugfs_remove_recursive(nct->dentry);
	free_netdev(dev);
	return;
}

/*
 * netfn_capwap_tun_setup()
 *	Device setup function
 */
static void netfn_capwap_tun_setup(struct net_device *dev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
	uint8_t eth_addr[ETH_ALEN];
#endif
	dev->addr_len = ETH_ALEN;
	dev->mtu = ETH_DATA_LEN;
	dev->hard_header_len = NETFN_CAPWAP_MAX_HDR_SZ;
	dev->needed_headroom = NETFN_CAPWAP_MAX_HDR_SZ;
	dev->needed_tailroom = NETFN_CAPWAP_MAX_HDR_SZ;

	dev->type = ARPHRD_VOID;
	dev->ethtool_ops = NULL;
	dev->header_ops = NULL;
	dev->netdev_ops = &capwap_tun_ops;
	dev->priv_destructor = netfn_capwap_tun_destructor;

	/*
	 * Get the MAC address from the ethernet device
	 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
	random_ether_addr(dev->dev_addr);
#else
	eth_random_addr(eth_addr);
	dev_addr_set(dev, eth_addr);
#endif
	memset(dev->broadcast, 0xff, dev->addr_len);
	memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);

	return;
}

/*
 * netfn_capwap_tun_stats_clear()
 *      Show netfn capwap tun statistics.
 *
 * TODO: Add locks for writing to tunnel statistics
 */
static ssize_t netfn_capwap_tun_stats_clear(struct file *fp, const char __user *ubuf, size_t sz, loff_t *ppos)
{
	struct netfn_capwap_tun *nct = fp->private_data;
	uint32_t reset;

	if (kstrtou32_from_user(ubuf, sz, 0, &reset)) {
		return -EINVAL;
	}

	if (reset != 0) {
		return -EINVAL;
	}

	memset(&nct->stats, 0, sizeof(struct netfn_capwap_pkt_stats));

	return sz;
}

/*
 * netfn_capwap_tun_stats_show()
 *      Show netfn capwap tun statistics.
 */
static ssize_t netfn_capwap_tun_stats_show(struct file *fp, char __user *ubuf, size_t sz, loff_t *ppos)
{
	struct netfn_capwap_tun *nct = fp->private_data;
	uint64_t *stats = (uint64_t *)&nct->stats;
	size_t pkt_stats_sz, stats_sz;
	ssize_t bytes_read = 0;
	size_t size_wr = 0;
	char *buf;
	int i;

	pkt_stats_sz = sizeof(struct netfn_capwap_pkt_stats) / sizeof(uint64_t) * NETFN_CAPWAP_TUN_STATS_STRLEN;
	stats_sz = (NETFN_CAPWAP_TUN_STATS_WIDTH * NETFN_CAPWAP_TUN_STATS_STRLEN) + pkt_stats_sz;

	buf = vzalloc(stats_sz);
	if (!buf) {
		pr_warn("%px: Unable to allocate memory for stats.\n", fp);
		return -ENOMEM;
	}

	/*
	 * Update stats before printing.
	 */
	netfn_capwap_tun_stats_update(nct);

	/*
	 * Print pkt stats.
	 */
	BUILD_BUG_ON(ARRAY_SIZE(g_tun_stats_str) != NETFN_CAPWAP_TUN_STATS_MAX);
	size_wr += scnprintf(buf + size_wr, stats_sz - size_wr, "\n---------------[pkt stats start]-------------\n");

	for (i = 0; i < ARRAY_SIZE(g_tun_stats_str); i++, stats++) {
		size_t len = stats_sz - size_wr;
		char *start = buf + size_wr;

		size_wr += scnprintf(start, len, "%-*s\t\t = %llu\n", NETFN_CAPWAP_MAX_STRLEN, g_tun_stats_str[i], READ_ONCE(*stats));
	}

	size_wr += scnprintf(buf + size_wr, stats_sz - size_wr, "---------------[pkt stats end]--------------\n");

	bytes_read = simple_read_from_buffer(ubuf, sz, ppos, buf, size_wr);
	vfree(buf);
	return bytes_read;
}

/*
 * Capwap file operations.
 */
static const struct file_operations file_ops = {
	.open = simple_open,
	.llseek = default_llseek,
	.read = netfn_capwap_tun_stats_show,
	.write = netfn_capwap_tun_stats_clear
};

/*
 * netfn_capap_tun_vp_err()
 *	VP callback for outer exception.
 */
bool netfn_capwap_tun_vp_err(struct ppe_vp_cb_info *info, void *cb_data)
{
	struct netfn_capwap_tun *nct = cb_data;

	NETFN_CAPWAP_TUN_STATS_INC(&nct->enc.stats.err_dev_tx);
	dev_kfree_skb(info->skb);
	return true;
}

/*
 * netfn_capwap_tun_list_rx()
 *     Common rx handling for dev rx handler and VP handler.
 */
static void netfn_capwap_tun_list_rx(struct netfn_capwap_tun *nct, struct sk_buff_head *q_head)
{
	struct netfn_capwap_dec_stats *stats = &nct->dec.stats;

	/*
	 * Check if tunnel is up or not
	 */
	if (unlikely(!(nct->dev->flags & IFF_UP))) {
		NETFN_CAPWAP_TUN_STATS_ADD(&nct->stats.rx_dropped, skb_queue_len(q_head));
		goto fail;
	}

	/*
	 * Decapsulate and reassemble the skb
	 */
	if (unlikely(!netfn_capwap_dec_rx_skbs(&nct->dec, q_head))) {
		NETFN_CAPWAP_TUN_STATS_ADD(&stats->err_dec_failure, skb_queue_len(q_head));
		goto fail;
	}

	/*
	 * Deliver packet to rx handler.
	 */
	netfn_capwap_tun_rx_deliver(nct, q_head);
	return;
fail:
	dev_kfree_skb_list_fast(q_head);
	return;
}

/*
 * netfn_capwap_tun_vp_rx()
 *	VP callback for receiving packets.
 */
bool netfn_capwap_tun_vp_rx(struct ppe_vp_cb_info *info, void *cb_data)
{
	netfn_capwap_tun_rx(cb_data, info->skb);
	return true;
}

/*
 * netfn_capwap_tun_vp_list_rx()
 *     VP callback for receiving list of packets.
 */
bool netfn_capwap_tun_vp_list_rx(struct net_device *dev, struct sk_buff_head *q_head, void *cb_data)
{
	netfn_capwap_tun_list_rx(cb_data, q_head);
	return true;
}

/*
 * netfn_capwap_tun_vp_attach()
 *	Allocate VP corresponding to netdev
 */
static struct net_device *netfn_capwap_tun_vp_attach(struct net_device *dev)
{
	uint32_t queue_base, num_queues;
	struct ppe_vp_ai vpai = {0};
	struct net_device *vp_dev;

	/*
	 * Allocate VP.
	 */
	vpai.type = PPE_VP_TYPE_SW_L3;
	vpai.dst_cb = netfn_capwap_tun_vp_rx;
	vpai.dst_list_cb = netfn_capwap_tun_vp_list_rx;
	vpai.dst_cb_data = netdev_priv(dev);
	vpai.src_cb = netfn_capwap_tun_vp_err;
	vpai.src_cb_data = netdev_priv(dev);
	vpai.flags |= PPE_VP_FLAG_REDIR_ENABLE;

	/*
	 * Fetch the PPE queue base for the CAPWAP VP-feature ring from EDMA.
	 * This is SoC-specific and populated at EDMA init time.
	 */
	if (edma_cfg_get_vp_queues(EDMA_VP_FEAT_TYPE_CAPWAP, &queue_base, &num_queues)) {
		pr_warn("%p: EDMA VP-feature ring for CAPWAP not configured on this SoC\n", dev);
		return NULL;
	}

	pr_debug("%px: CAPWAP VP-feature ring: queue_base(%u) num_queues(%u)\n", dev, queue_base, num_queues);
	vpai.queue_num = queue_base;

	vp_dev = ppe_vp_alloc_dev(dev, &vpai);
	if (!vp_dev) {
		pr_warn("%p, Unable to create VP\n", dev);
		return NULL;
	}

	pr_info("%px: Tun(%s), vp(%s) alloc and bind suceess\n", dev, dev->name, vp_dev->name);
	return vp_dev;
}

/*
 * netfn_capwap_tun_vp_detach()
 *	Unbinds and frees the ppe-vp associated with dev.
 */
static void netfn_capwap_tun_vp_detach(struct net_device *dev)
{
	struct netfn_capwap_tun *nct = netdev_priv(dev);
	struct net_device *vp_dev;

	BUG_ON(nct->vp_dev != nct->next_dev);

	vp_dev = xchg(&nct->vp_dev, NULL);

	/*
	 * Free the vp dev
	 */
	ppe_vp_free_dev(vp_dev);
	pr_info("%px: Tun(%s), unbinded vp dev and freed\n", dev, dev->name);
	return;
}

/*
 * netfn_capwap_tun_enc()
 *	Encapsulate with CAPWAP Header
 */
void netfn_capwap_tun_enc(struct netfn_capwap_tun *nct, struct sk_buff *skb)
{
	struct netfn_capwap_pkt_stats *stats = &nct->stats;
	struct sk_buff_head q_frags;
	struct sk_buff *nskb;

	/*
	 * Encapsulates and transmits out.
	 */
	uint8_t *data = skb->data;
	uint32_t mtu = nct->enc.mtu;

	prefetch(data);
	if (likely(skb->len > mtu)) {
		prefetch_range(data + mtu, skb->len - mtu);
	}

	/*
	 * Encapsulate the packet.
	 */
	skb_queue_head_init(&q_frags);
	netfn_capwap_enc(&nct->enc, skb, &q_frags);

	/*
	 * Trasmit the packet out.
	 */
	skb_queue_walk_safe(&q_frags, skb, nskb) {
		struct net_device *dev = nct->next_dev;
		int len = skb->len;

		__skb_unlink(skb, &q_frags);
		skb->dev = dev;

		/*
		 * We are either sending it to VP or DTLS netdevice. Which is controlled path,
		 * and don't require unnecessary overhead of dev_queue_xmit()
		 */
		if (dev->netdev_ops->ndo_start_xmit(skb, dev) != NETDEV_TX_OK) {
			NETFN_CAPWAP_TUN_STATS_INC(&nct->enc.stats.err_dev_tx);
			dev_kfree_skb(skb);
			continue;
		}

		NETFN_CAPWAP_TUN_STATS_INC(&stats->tx_pkts);
		NETFN_CAPWAP_TUN_STATS_ADD(&stats->tx_bytes, len);
	}
}

/*
 * netfn_capwap_tun_rx_pri_deliver()
 *	Deliver Rx priority packet.
 */
void netfn_capwap_tun_rx_pri_deliver(struct netfn_capwap_tun *nct, struct sk_buff *skb)
{
	rx_handler_func_t *rx_handler;

	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);

	NETFN_CAPWAP_TUN_STATS_INC(&nct->stats.rx_pkts);
	NETFN_CAPWAP_TUN_STATS_ADD(&nct->stats.rx_bytes, skb->len);

	rx_handler = rcu_dereference(skb->dev->rx_handler);
	if (likely(rx_handler) && (rx_handler(&skb) == RX_HANDLER_CONSUMED)) {
		return;
	}

	netif_receive_skb(skb);
}

/*
 * netfn_capwap_tun_rx_deliver()
 *	Deliver Rx data packet.
 */
static void netfn_capwap_tun_rx_deliver(struct netfn_capwap_tun *nct, struct sk_buff_head *q_head)
{
	struct net_device *dev = nct->dev;
	struct sk_buff *skb, *next_skb;
	rx_handler_func_t *rx_handler;

	/*
	 * Walk the queue and pass onto required handler.
	 */
	skb_queue_walk_safe(q_head, skb, next_skb) {
		__skb_unlink(skb, q_head);
		skb_reset_mac_header(skb);
		skb_reset_network_header(skb);
		skb_reset_transport_header(skb);

		skb->dev = dev;
		skb->pkt_type = PACKET_HOST;
		skb->skb_iif = nct->dev->ifindex;

		NETFN_CAPWAP_TUN_STATS_INC(&nct->stats.rx_pkts);
		NETFN_CAPWAP_TUN_STATS_ADD(&nct->stats.rx_bytes, skb->len);

		rx_handler = rcu_dereference(skb->dev->rx_handler);
		if (likely(rx_handler) && (rx_handler(&skb) == RX_HANDLER_CONSUMED)) {
			continue;
		}

		netif_receive_skb(skb);
	}
}

/*
 * netfn_capwap_tun_alloc()
 *	Allocates a new tunnel.
 */
struct net_device *netfn_capwap_tun_alloc(struct netfn_capwap_tun_cfg *cfg, struct netfn_tuple *tuple, ssize_t pvt_sz)
{
	struct netfn_capwap *nc = &global_nc;
	struct netfn_capwap_tun *nct;
	struct net_device *dev;
	int err;

	/*
	 * Allocate a new device for tunnel.
	 */
	dev = alloc_netdev(sizeof(*nct) + pvt_sz, "capwap%d", NET_NAME_ENUM, netfn_capwap_tun_setup);
	if (!dev) {
		pr_warn("%p: failed to allocate capwap tunnel device\n", nc);
		return NULL;
	}

	nct = netdev_priv(dev);

	/*
	 * Tunnel init
	 */
	nct->nc = nc;
	nct->dev = dev;
	nct->id = cfg->id;
	nct->features = cfg->features;
	memset(&nct->stats, 0, sizeof(nct->stats));

	/*
	 * Register netdevice
	 * On failure destructor is invoked by linux.
	 */
	err = register_netdev(dev);
	if (err) {
		pr_warn("%p: failed to register capwap(%s) netdev with err(%d)\n", dev, dev->name, err);
		return NULL;
	}

	/*
	 * Create debug entry for tunnel.
	 */
	nct->dentry = debugfs_create_dir(dev->name, nc->dentry);
	if (!nct->dentry) {
		pr_warn("%p: failed to create debugfs entry for capwap(%s)\n", nct, dev->name);
		unregister_netdev(dev);
		return NULL;
	}

	if (!debugfs_create_file("stats", S_IRUGO, nct->dentry, nct, &file_ops)) {
		pr_warn("%p: Unable to create file for capwap(%s) stats.\n", nct, dev->name);
		unregister_netdev(dev);
		return NULL;
	}

	/*
	 * Add a VP for netdev.
	 */
	nct->vp_dev = netfn_capwap_tun_vp_attach(dev);
	if (!nct->vp_dev) {
		pr_warn("%px: Failed to allocate VP.\n", dev);
		unregister_netdev(dev);
		return NULL;
	}

	nct->next_dev = nct->vp_dev;

	/*
	 * Encap init.
	 */
	if (!netfn_capwap_enc_init(&nct->enc, &cfg->enc, tuple, nct)) {
		pr_warn("%px: Encapsulation init failed.\n", dev);
		goto fail;
	}

	/*
	 * Decap init.
	 */
	if (!netfn_capwap_dec_init(&nct->dec, &cfg->dec, nct)) {
		pr_warn("%px: Decapsulation init failed.\n", dev);
		goto fail;
	}

	pr_info("%px: Successfully allocated tunnel(%s)\n", nct, dev->name);
	return dev;
fail:
	netfn_capwap_tun_vp_detach(dev);
	unregister_netdev(dev);
	return NULL;
}
EXPORT_SYMBOL(netfn_capwap_tun_alloc);

/*
 * netfn_capwap_tun_free()
 *	Frees the tunnel associated with netdev.
 */
bool netfn_capwap_tun_free(struct net_device *dev)
{
	/*
	 * Check if dev is valid.
	 */
	BUG_ON(!dev);

	/*
	 * Free associated VP.
	 */
	netfn_capwap_tun_vp_detach(dev);

	pr_info("%px: Unregistering tunnel(%s).\n", &global_nc, dev->name);
	unregister_netdev(dev);

	return true;
}
EXPORT_SYMBOL(netfn_capwap_tun_free);

/*
 * netfn_capwap_tun_bind()
 *	Binds tunnel netdev to next dev.
 */
bool netfn_capwap_tun_bind(struct net_device *dev, struct net_device *next)
{
	struct netfn_capwap_enc *enc;
	struct net_device *old_next_dev;
	struct netfn_capwap_tun *nct;

	BUG_ON(!dev);
	BUG_ON(!next);

	nct = netdev_priv(dev);
	enc = &nct->enc;

	dev_hold(next);
	old_next_dev = xchg(&nct->next_dev, next);
	netfn_capwap_enc_mtu_update(&nct->enc, next->mtu);
	BUG_ON(old_next_dev != nct->vp_dev);
	netfn_dtls_register_data_cb(next, &netfn_capwap_tun_rx_handler, dev);

	pr_info("%px: Successfully binded dev(%s)-->next(%s)\n", nct, dev->name, next->name);
	return true;
}
EXPORT_SYMBOL(netfn_capwap_tun_bind);

/*
 * netfn_capwap_tun_unbind()
 *	Unbinds the tunnel from next hop.
 */
bool netfn_capwap_tun_unbind(struct net_device *dev)
{
	struct net_device *old_next_dev;
	struct netfn_capwap_tun *nct;

	BUG_ON(!dev);

	nct = netdev_priv(dev);

	old_next_dev = xchg(&nct->next_dev, nct->vp_dev);
	BUG_ON(old_next_dev == nct->vp_dev);

	netfn_dtls_unregister_data_cb(old_next_dev);
	dev_put(old_next_dev);

	pr_info("%px: Successfully unbinded(%s)\n", nct, dev->name);
	return true;
}
EXPORT_SYMBOL(netfn_capwap_tun_unbind);

/*
 * netfn_capwap_tun_enable_flow_db()
 *	Set the DB handle for flow-ID lookup
 */
void netfn_capwap_tun_enable_flow_db(struct net_device *dev, struct netfn_flow_cookie_db *db)
{
	struct netfn_capwap_tun *nct = netdev_priv(dev);

	/*
	 * Take the DB reference while loading the RCU protected handle
	 */
	rcu_assign_pointer(nct->db, netfn_flow_cookie_db_ref(db));
}
EXPORT_SYMBOL(netfn_capwap_tun_enable_flow_db);

/*
 * netfn_capwap_tun_disable_flow_db()
 *	Clear the DB handle for flow-ID lookup
 */
void netfn_capwap_tun_disable_flow_db(struct net_device *dev)
{
	struct netfn_capwap_tun *nct = netdev_priv(dev);
	struct netfn_flow_cookie_db *db;

	/*
	 * There are open references to
	 */
	db = rcu_dereference_protected(nct->db, lockdep_rtnl_is_held());
	if (db) {
		RCU_INIT_POINTER(nct->db, NULL);
		synchronize_net();
		netfn_flow_cookie_db_deref(db);
	}
}
EXPORT_SYMBOL(netfn_capwap_tun_disable_flow_db);

/*
 * netfn_capwap_tun_stats_get()
 *	Gets tunnel stats corresponding to dev.
 */
int netfn_capwap_tun_stats_get(struct net_device *dev, struct netfn_capwap_tun_stats *stats)
{
	struct netfn_capwap_pkt_stats *pkt_stats = &stats->pkts;
	struct netfn_capwap_tun *nct;

	BUG_ON(!dev);
	BUG_ON(!stats);

	nct = netdev_priv(dev);

	/*
	 * Update the tunnel stats with encap and decap
	 */
	netfn_capwap_tun_stats_update(nct);
	memcpy(pkt_stats, &nct->stats, sizeof(*pkt_stats));

	/*
	 * Read encap, decap stats.
	 */
	netfn_capwap_enc_stats_read(&nct->enc, stats);
	netfn_capwap_dec_stats_read(&nct->dec, stats);

	pr_info("%px: Stats get successful.\n", nct);
	return 0;
}
EXPORT_SYMBOL(netfn_capwap_tun_stats_get);

/*
 * netfn_capwap_tun_pvt_get()
 *	Returns the private pointer corresponding to dev.
 */
void *netfn_capwap_tun_pvt_get(struct net_device *dev)
{
	struct netfn_capwap_tun *nct = netdev_priv(dev);

	return nct->pvt;
}
EXPORT_SYMBOL(netfn_capwap_tun_pvt_get);

/*
 * netfn_capwap_tun_config_dtls()
 *	Enables/disables dtls feature for capwap
 */
void netfn_capwap_tun_config_dtls(struct net_device *dev, bool enable)
{
	struct netfn_capwap_tun *nct = netdev_priv(dev);

	/*
	 * Set enabled features for capwap if dtls is enabled
	 */
	if (enable) {
		nct->enc.features |= NETFN_CAPWAP_FEATURES_DTLS;
		nct->dec.features |= NETFN_CAPWAP_FEATURES_DTLS;
	} else {
		nct->enc.features &= ~NETFN_CAPWAP_FEATURES_DTLS;
		nct->dec.features &= ~NETFN_CAPWAP_FEATURES_DTLS;
	}
}
EXPORT_SYMBOL(netfn_capwap_tun_config_dtls);
