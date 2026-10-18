/*
 * netfn_capwap_tunid.c
 *	Network function's CAPWAP offload tunnel ID configuration.
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/if_arp.h>
#include <linux/bitmap.h>

#include "netfn_capwap.h"
#include "netfn_capwap_priv.h"
#include "netfn_capwap_hdr.h"
#include "netfn_capwap_tun.h"
#include "netfn_capwap_tunid.h"

/*
 * netfn_capwap_tunid_open()
 *	Start netdev queue.
 */
static int netfn_capwap_tunid_open(struct net_device *dev)
{
	netif_start_queue(dev);
	return 0;
}

/*
 * netfn_capwap_tunid_close()
 *	Stop netdev queue.
 */
static int netfn_capwap_tunid_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	return 0;
}

/*
 * netfn_capwap_tunid_get_tun_stats()
 *	Accumulates all child tunnel's error and drop stats.
 */

static void netfn_capwap_tunid_get_tun_stats(struct netfn_capwap_tunid *ncti)
{
	struct netfn_capwap_pkt_stats tunid_stats = {0};
	struct netfn_capwap_pkt_stats tun_stats = {0};
	struct netfn_capwap_tun *nct;
	struct net_device *tun_dev;
	int id = 0;

	rcu_read_lock_bh();
	for (id = 0; id < NETFN_CAPWAP_MAX_IDS; id++) {
		tun_dev = rcu_dereference(ncti->tunnels[id]);
		if (!tun_dev) {
			continue;
		}

		nct = netdev_priv(tun_dev);
		tun_stats = nct->stats;

		tunid_stats.tx_dropped += tun_stats.tx_dropped;
		tunid_stats.rx_dropped += tun_stats.rx_dropped;
		tunid_stats.tx_errors += tun_stats.tx_errors;
		tunid_stats.rx_errors += tun_stats.rx_errors;
	}

	rcu_read_unlock_bh();

	ncti->stats.tx_dropped = tunid_stats.tx_dropped;
	ncti->stats.rx_dropped = tunid_stats.rx_dropped;
	ncti->stats.tx_errors = tunid_stats.tx_errors;
	ncti->stats.rx_errors = tunid_stats.rx_errors;
}

/*
 * netfn_capwap_tunid_stats
 *	Get pdev stats.
 */
static void netfn_capwap_tunid_stats(struct net_device *dev, struct rtnl_link_stats64 *stats)
{
	struct netfn_capwap_tunid *ncti = netdev_priv(dev);
	struct netfn_capwap_pkt_stats *pkt_stats = &ncti->stats;

	stats->rx_packets = READ_ONCE(pkt_stats->rx_pkts);
	stats->rx_bytes = READ_ONCE(pkt_stats->rx_bytes);
	stats->tx_packets = READ_ONCE(pkt_stats->tx_pkts);
	stats->tx_bytes = READ_ONCE(pkt_stats->tx_bytes);

	/*
	 * Accumulate error and drop stats from child tunnels.
	 */
	netfn_capwap_tunid_get_tun_stats(ncti);

	stats->tx_dropped = READ_ONCE(pkt_stats->tx_dropped);
	stats->rx_dropped = READ_ONCE(pkt_stats->rx_dropped);
	stats->tx_errors = READ_ONCE(pkt_stats->tx_errors);
	stats->rx_errors = READ_ONCE(pkt_stats->rx_errors);
}

/*
 * netfn_capwap_tunid_rx_handler()
 *	Rx handler for tunid.
 */
static rx_handler_result_t netfn_capwap_tunid_rx_handler(struct sk_buff **pskb)
{
	struct netfn_capwap_pkt_stats *stats;
	struct netfn_capwap_tunid *ncti;
	rx_handler_func_t *rx_handler;
	struct sk_buff *skb = *pskb;
	struct net_device *dev;

	dev = rcu_dereference_bh(skb->dev->rx_handler_data);
	ncti = netdev_priv(dev);
	stats = &ncti->stats;

	NETFN_CAPWAP_TUN_STATS_INC(&stats->rx_pkts);
	NETFN_CAPWAP_TUN_STATS_ADD(&stats->rx_bytes, skb->len);

	/*
	 * Only set the dev if it's not exception packet.
	 */
	if (likely(!NETFN_CAPWAP_CB(skb)->exception)) {
		skb->dev = dev;
	}

	rx_handler = rcu_dereference(skb->dev->rx_handler);
	if (likely(rx_handler) && (rx_handler(&skb) == RX_HANDLER_CONSUMED)) {
		return RX_HANDLER_CONSUMED;
	}

	netif_receive_skb(skb);
	return RX_HANDLER_CONSUMED;
}

/*
 * netfn_capwap_tunid_tx()
 *	Start transmiting using pdev.
 */
static netdev_tx_t netfn_capwap_tunid_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct netfn_capwap_prehdr *pre = (struct netfn_capwap_prehdr *)skb->data;
	struct netfn_capwap_tunid *ncti = netdev_priv(dev);
	struct netfn_capwap_pkt_stats *stats = &ncti->stats;
	uint16_t tunid = pre->tunnel_id;
	struct net_device *tun_dev;

	/*
	 * Check if tunid is valid.
	 */
	if (tunid < 0 || tunid >= NETFN_CAPWAP_MAX_IDS) {
		pr_warn_ratelimited("%px: Invalid id(%d) allowed(0 to %d)\n", &global_nc, tunid, NETFN_CAPWAP_MAX_IDS - 1);
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	rcu_read_lock_bh();
	tun_dev = rcu_dereference(ncti->tunnels[pre->tunnel_id]);
	if (!tun_dev) {
		pr_warn_ratelimited("%px: No tun dev found corresponding to id(%d)\n", &global_nc, tunid);
		rcu_read_unlock_bh();
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	skb->dev = tun_dev;

	/*
	 * We are sending the packet to tunnel dev which is a controlled path
	 * so no need for dev_queue_xmit/dev_fast_xmit.
	 */
	if (tun_dev->netdev_ops->ndo_start_xmit(skb, tun_dev) != NETDEV_TX_OK) {
		dev_kfree_skb(skb);
		goto done;
	}

	NETFN_CAPWAP_TUN_STATS_INC(&stats->tx_pkts);
	NETFN_CAPWAP_TUN_STATS_ADD(&stats->tx_bytes, skb->len);
done:
	rcu_read_unlock_bh();
	return NETDEV_TX_OK;
}

/*
 * netfn_capwap_tunid_ops
 *	Tunnel id netdev operations.
 */
static const struct net_device_ops netfn_capwap_tunid_ops = {
	.ndo_open               = netfn_capwap_tunid_open,
	.ndo_stop               = netfn_capwap_tunid_close,
	.ndo_start_xmit         = netfn_capwap_tunid_tx,
	.ndo_set_mac_address    = eth_mac_addr,
	.ndo_change_mtu    	= dev_set_mtu,
	.ndo_get_stats64        = netfn_capwap_tunid_stats,
};

/*
 * netfn_capwap_tunid_destructor()
 *	Dummy dev descriptor
 */
static void netfn_capwap_tunid_destructor(struct net_device *dev)
{
	free_netdev(dev);
	return;
}

/*
 * netfn_capwap_tunid_setup()
 *	Tunid setup function.
 */
static void netfn_capwap_tunid_setup(struct net_device *dev)
{
	dev->addr_len = ETH_ALEN;
	dev->mtu = ETH_DATA_LEN;
	dev->needed_headroom = NETFN_CAPWAP_MAX_HDR_SZ;
	dev->needed_tailroom = NETFN_CAPWAP_MAX_HDR_SZ;
	dev->type = ARPHRD_VOID;
	dev->ethtool_ops = NULL;
	dev->header_ops = NULL;
	dev->netdev_ops = &netfn_capwap_tunid_ops;
	dev->priv_destructor = netfn_capwap_tunid_destructor;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
	memcpy(dev->dev_addr, "\x00\x00\x00\x00\x00\x00", dev->addr_len);
#else
	dev_addr_set(dev, "\x00\x00\x00\x00\x00\x00");
#endif
	memset(dev->broadcast, 0xff, dev->addr_len);
	memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);
}

/*
 * netfn_capwap_tunid_alloc()
 *	Allocates a new tunid.
 */
struct net_device *netfn_capwap_tunid_alloc(ssize_t pvt_sz)
{
	struct netfn_capwap_tunid *ncti;
	struct net_device *dev;
	int err;

	/*
	 * Allocate a new device for tunnel.
	 */
	dev = alloc_netdev(sizeof(*ncti) + pvt_sz, "nsscapwap%d", NET_NAME_ENUM, netfn_capwap_tunid_setup);
	if (!dev) {
		pr_warn("%p: failed to allocate capwap dummy device\n", &global_nc);
		return NULL;
	}

	ncti = netdev_priv(dev);
	ncti->dev = dev;
	memset(&ncti->tunnels, 0, sizeof(NETFN_CAPWAP_MAX_IDS));
	spin_lock_init(&ncti->lock);

	err = register_netdev(dev);
	if (err) {
		pr_warn("%px: register_netdev() failed with error(%d)\n", &global_nc, err);
		return NULL;
	}

	return dev;
}
EXPORT_SYMBOL(netfn_capwap_tunid_alloc);

/*
 * netfn_capwap_tunid_add()
 *	Adds tunnel with specific tun id.
 */
struct net_device *netfn_capwap_tunid_add(struct net_device *dev, uint8_t id, struct netfn_capwap_tun_cfg *cfg, struct netfn_tuple *tuple, ssize_t pvt_sz)
{
	struct netfn_capwap_tunid *ncti;
	struct net_device *tun_dev;

	/*
	 * Sanity check for input params
	 */
	BUG_ON(!dev);

	if (id < 0 || id >= NETFN_CAPWAP_MAX_IDS) {
		pr_warn("%px: Invalid id(%d) allowed(0 to %d)\n", &global_nc, id, NETFN_CAPWAP_MAX_IDS - 1);
		return NULL;
	}

	cfg->id = id;
	ncti = netdev_priv(dev);

	/*
	 * Check if id already in use
	 */
	if (test_and_set_bit(id, ncti->map)) {
		pr_warn("%px: Id(%d) already in use\n", ncti, id);
		return NULL;
	}

	tun_dev = netfn_capwap_tun_alloc(cfg, tuple, pvt_sz);
	if (!tun_dev) {
		clear_bit(id, ncti->map);
		pr_warn("%px: Unable to allocate tunnel for id(%d)\n", ncti, id);
		return NULL;
	}

	ncti = netdev_priv(dev);

	spin_lock_bh(&ncti->lock);
	rcu_assign_pointer(ncti->tunnels[id], tun_dev);
	spin_unlock_bh(&ncti->lock);

	rtnl_lock();
	dev_open(tun_dev, NULL);
	netdev_rx_handler_register(tun_dev, &netfn_capwap_tunid_rx_handler, dev);
	rtnl_unlock();

	return tun_dev;
}
EXPORT_SYMBOL(netfn_capwap_tunid_add);

/*
 * netfn_capwap_tunid_free()
 *	Frees tunnel corresponding to dev passed.
 *	We free the dev alone. We rely on the caller for
 *	freeing the corresponding tunnel devs.
 */
bool netfn_capwap_tunid_free(struct net_device *dev)
{
	struct netfn_capwap_tunid *ncti;

	/*
	 * Check if dev is null
	 */
	BUG_ON(!dev);

	ncti = netdev_priv(dev);

	/*
	 * Reject if we have active tunnels.
	 */
	if (!bitmap_empty(ncti->map, NETFN_CAPWAP_MAX_IDS)) {
		pr_warn("%px: Capwap tunnels are still active.\n", ncti);
		return false;
	}

	unregister_netdev(dev);
	return true;
}
EXPORT_SYMBOL(netfn_capwap_tunid_free);

/*
 * netfn_capwap_tunid_del()
 *	Delets tun_id specific tunnel.
 */
bool netfn_capwap_tunid_del(struct net_device *dev, uint8_t id)
{
	struct netfn_capwap_tunid *ncti;
	struct net_device *tun_dev;

	/*
	 * Sanity check for input params
	 */
	BUG_ON(!dev);

	if (id < 0 || id >= NETFN_CAPWAP_MAX_IDS) {
		pr_warn("%px: Invalid id(%d) allowed(0 to %d)\n", &global_nc, id, NETFN_CAPWAP_MAX_IDS - 1);
		return false;
	}

	ncti = netdev_priv(dev);

	spin_lock_bh(&ncti->lock);
	tun_dev = rcu_dereference_protected(ncti->tunnels[id], lock_is_held(&ncti->lock));
	rcu_assign_pointer(ncti->tunnels[id], NULL);
	clear_bit(id, ncti->map);
	spin_unlock_bh(&ncti->lock);

	if (!tun_dev) {
		pr_info("%px: Unable to delete tun(%d)\n", ncti, id);
		return false;
	}

	synchronize_rcu();
	netfn_capwap_tun_free(tun_dev);
	pr_info("%px: Successfully deleted tun(%d)\n", ncti, id);
	return true;
}
EXPORT_SYMBOL(netfn_capwap_tunid_del);

/*
 * netfn_capwap_tunid_bind()
 *	Binds tun_id netdev to next netdev
 */
bool netfn_capwap_tunid_bind(struct net_device *dev, uint8_t id, struct net_device *next)
{
	struct netfn_capwap_tunid *ncti;
	struct net_device *tun_dev;

	BUG_ON(!dev);
	BUG_ON(!next);

	if (id < 0 || id >= NETFN_CAPWAP_MAX_IDS) {
		pr_warn("%px: Invalid id(%d) allowed(0 to %d)\n", &global_nc, id, NETFN_CAPWAP_MAX_IDS - 1);
		return false;
	}

	ncti = netdev_priv(dev);

	rcu_read_lock_bh();
	tun_dev = rcu_dereference(ncti->tunnels[id]);
	if (!tun_dev) {
		pr_warn("%px: No tunnel found with id(%d)\n", ncti, id);
		goto error;
	}

	if (!netfn_capwap_tun_bind(tun_dev, next)) {
		pr_warn("%px: Unable to bind tun(%d).\n", ncti, id);
		goto error;
	}

	rcu_read_unlock_bh();
	return true;
error:
	rcu_read_unlock_bh();
	return false;
}
EXPORT_SYMBOL(netfn_capwap_tunid_bind);

/*
 * netfn_capwap_tunid_stats_get()
 *	Gets the tunnel ID stats.
 */
int netfn_capwap_tunid_stats_get(struct net_device *dev, uint8_t id, struct netfn_capwap_tun_stats *stats)
{
	struct netfn_capwap_tunid *ncti;
	struct net_device *tun_dev;

	/*
	 * Sanity check for input params
	 */
	BUG_ON(!dev);
	BUG_ON(!stats);

	if (id < 0 || id >= NETFN_CAPWAP_MAX_IDS) {
		pr_warn("%px: Invalid id(%d) allowed(0 to %d)\n", &global_nc, id, NETFN_CAPWAP_MAX_IDS - 1);
		return -EINVAL;
	}

	ncti = netdev_priv(dev);

	rcu_read_lock_bh();
	tun_dev = rcu_dereference(ncti->tunnels[id]);
	if (!tun_dev) {
		rcu_read_unlock_bh();
		pr_warn("%px: No tunnels found with id(%d)\n", ncti, id);
		return -EINVAL;
	}

	netfn_capwap_tun_stats_get(tun_dev, stats);
	rcu_read_unlock_bh();

	return 0;
}
EXPORT_SYMBOL(netfn_capwap_tunid_stats_get);

/*
 * netfn_capwap_tunid_pvt_get()
 *	Gets the private pointer
 */
void *netfn_capwap_tunid_pvt_get(struct net_device *dev)
{
	struct netfn_capwap_tunid *ncti = netdev_priv(dev);

	return ncti->pvt;
}
EXPORT_SYMBOL(netfn_capwap_tunid_pvt_get);
