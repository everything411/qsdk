/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/sysctl.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/ipv6.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/inet.h>
#include <linux/cpumask.h>
#include <asm/cacheflush.h>
#include <net/protocol.h>
#include <net/checksum.h>
#include <net/ip.h>

#include "netfn_capwap_types.h"
#include "netfn_dev_attach_priv.h"

/*
 * Global static context for the module.
 */
static struct netfn_dev_attach_priv nda_g;

/*
 * Forward declarations
 */
static int netfn_dev_attach(struct ctl_table *ctl, int write, void __user *buffer, size_t *lenp, loff_t *ppos);
static int netfn_dev_tx_capwap_keepalive(struct ctl_table *ctl, int write, void __user *buffer, size_t *lenp, loff_t *ppos);
static int netfn_dev_attach_cfg(struct ctl_table *ctl, int write, void __user *buffer, size_t *lenp, loff_t *ppos);
static void netfn_dev_attach_setup(struct net_device *dev);

/*
 * netfn_dev_ops
 *	Netdevice ops for the test device.
 */
static netdev_tx_t netfn_dev_xmit(struct sk_buff *skb, struct net_device *dev);
static void netfn_dev_get_stats64(struct net_device *dev, struct rtnl_link_stats64 *stats);

static const struct net_device_ops netfn_dev_ops = {
	.ndo_start_xmit		= netfn_dev_xmit,
	.ndo_get_stats64	= netfn_dev_get_stats64,
};

/*
 * netfn_prehdr_table
 */
static struct ctl_table netfn_prehdr_table[] = {
	{
		.procname		= "version",
		.data			= &nda_g.prehdr.version,
		.maxlen			= sizeof(uint32_t),
		.mode			= 0644,
		.proc_handler		= proc_douintvec,
	},
	{
		.procname		= "rid",
		.data			= &nda_g.prehdr.rid,
		.maxlen			= sizeof(uint32_t),
		.mode			= 0644,
		.proc_handler		= proc_douintvec,
	},
	{
		.procname		= "dscp",
		.data			= &nda_g.prehdr.dscp,
		.maxlen			= sizeof(uint32_t),
		.mode			= 0644,
		.proc_handler		= proc_douintvec,
	},
	{
		.procname		= "vlan_pcp",
		.data			= &nda_g.prehdr.vlan_pcp,
		.maxlen			= sizeof(uint32_t),
		.mode			= 0644,
		.proc_handler		= proc_douintvec,
	},
	{
		.procname		= "type",
		.data			= &nda_g.prehdr.type,
		.maxlen			= sizeof(uint32_t),
		.mode			= 0644,
		.proc_handler		= proc_douintvec,
	},
	{
		.procname		= "nwireless",
		.data			= &nda_g.prehdr.nwireless,
		.maxlen			= sizeof(uint32_t),
		.mode			= 0644,
		.proc_handler		= proc_douintvec,
	},
	{
		.procname		= "wireless_qos",
		.data			= &nda_g.prehdr.wireless_qos,
		.maxlen			= sizeof(uint32_t),
		.mode			= 0644,
		.proc_handler		= proc_douintvec,
	},
	{
		.procname		= "outer_sgt",
		.data			= &nda_g.prehdr.outer_sgt,
		.maxlen			= sizeof(uint32_t),
		.mode			= 0644,
		.proc_handler		= proc_douintvec,
	},
	{
		.procname		= "inner_sgt",
		.data			= &nda_g.prehdr.inner_sgt,
		.maxlen			= sizeof(uint32_t),
		.mode			= 0644,
		.proc_handler		= proc_douintvec,
	},
	{
		.procname		= "vapid",
		.data			= &nda_g.prehdr.vapid,
		.maxlen			= sizeof(uint32_t),
		.mode			= 0644,
		.proc_handler		= proc_douintvec,
	},
	{ }
};

/*
 * netfn_dev_table
 */
static struct ctl_table netfn_dev_table[] = {
	{
		.procname               = "lan_dev",
		.data                   = nda_g.lan_dev_name,
		.maxlen                 = IFNAMSIZ,
		.mode                   = 0644,
		.proc_handler           = &netfn_dev_attach_cfg,
	},
	{
		.procname               = "capwap_dev",
		.data                   = nda_g.capwap_dev_name,
		.maxlen                 = IFNAMSIZ,
		.mode                   = 0644,
		.proc_handler           = &netfn_dev_attach_cfg,
	},
	{
		.procname               = "wan_dev",
		.data                   = nda_g.wan_dev_name,
		.maxlen                 = IFNAMSIZ,
		.mode                   = 0644,
		.proc_handler           = &netfn_dev_attach_cfg,
	},
	{
		.procname               = "to_dev_cpu",
		.data                   = &nda_g.to_dev_cpu,
		.maxlen                 = sizeof(uint32_t),
		.mode                   = 0644,
		.proc_handler           = proc_douintvec,
	},
	{
		.procname               = "tx_napi_budget",
		.data                   = &nda_g.tx_napi_budget,
		.maxlen                 = sizeof(uint32_t),
		.mode                   = 0644,
		.proc_handler           = proc_douintvec,
	},
	{
		.procname               = "id",
		.data                   = &nda_g.id,
		.maxlen                 = sizeof(uint32_t),
		.mode                   = 0644,
		.proc_handler           = proc_douintvec,
	},
	{
		.procname               = "attach",
		.data                   = &nda_g.attach,
		.maxlen                 = sizeof(int),
		.mode                   = 0644,
		.proc_handler           = &netfn_dev_attach,
	},
	{
		.procname               = "debug",
		.data                   = &nda_g.debug,
		.maxlen                 = sizeof(int),
		.mode                   = 0644,
		.proc_handler           = proc_dointvec,
	},
	{
		.procname               = "capwap_drop",
		.data                   = &nda_g.capwap_drop,
		.maxlen                 = sizeof(int),
		.mode                   = 0644,
		.proc_handler           = proc_dointvec,
	},
	{
		.procname               = "fast_xmit",
		.data                   = &nda_g.fast_xmit,
		.maxlen                 = sizeof(int),
		.mode                   = 0644,
		.proc_handler           = proc_dointvec,
	},
	{
		.procname               = "steer_en",
		.data                   = &nda_g.steer_en,
		.maxlen                 = sizeof(int),
		.mode                   = 0644,
		.proc_handler           = proc_dointvec,
	},
	{
		.procname               = "ip_mask",
		.data                   = &nda_g.ip_mask,
		.maxlen                 = sizeof(uint32_t),
		.mode                   = 0644,
		.proc_handler           = proc_douintvec,
	},
	{
		.procname               = "ip6_mask",
		.data                   = &nda_g.ip6_mask,
		.maxlen                 = sizeof(uint32_t),
		.mode                   = 0644,
		.proc_handler           = proc_douintvec,
	},
	{ }
};

/*
 * netfn_ka_table
 */
static struct ctl_table netfn_ka_table[] = {
	{
		.procname		= "src_ip",
		.data			= nda_g.ka_cfg.src_ip,
		.maxlen			= sizeof(nda_g.ka_cfg.src_ip),
		.mode			= 0644,
		.proc_handler		= proc_dostring,
	},
	{
		.procname		= "dst_ip",
		.data			= nda_g.ka_cfg.dst_ip,
		.maxlen			= sizeof(nda_g.ka_cfg.dst_ip),
		.mode			= 0644,
		.proc_handler		= proc_dostring,
	},
	{
		.procname		= "src_port",
		.data			= &nda_g.ka_cfg.src_port,
		.maxlen			= sizeof(uint32_t),
		.mode			= 0644,
		.proc_handler		= proc_douintvec,
	},
	{
		.procname		= "dst_port",
		.data			= &nda_g.ka_cfg.dst_port,
		.maxlen			= sizeof(uint32_t),
		.mode			= 0644,
		.proc_handler		= proc_douintvec,
	},
	{
		.procname		= "send",
		.data			= &nda_g.ka_cfg.send,
		.maxlen			= sizeof(int),
		.mode			= 0644,
		.proc_handler		= netfn_dev_tx_capwap_keepalive,
	},
	{ }
};

/*
 * netfn_dev_attach_stats_show()
 *	Seq file show function for dev attach stats.
 */
static int netfn_dev_attach_stats_show(struct seq_file *m, void *v)
{
	struct netfn_dev_attach_priv *nda = &nda_g;
	struct netfn_pkt_steer_stats stats = {0};

	if (nda->steer_en) {
		netfn_pkt_steer_get_stats(&nda->tx_steer, &stats);
	}

	seq_printf(m, "\n------------[Dev attach stats start]----------\n");
	seq_printf(m, "%-25s = %llu\n", "lan_rx", READ_ONCE(nda->lan_rx_pkts));
	seq_printf(m, "%-25s = %llu\n", "capwap_rx", READ_ONCE(nda->capwap_rx_pkts));
	seq_printf(m, "%-25s = %llu\n", "tx_pkts", READ_ONCE(nda->tx_pkts));
	seq_printf(m, "%-25s = %llu\n", "tx_keep_alive", READ_ONCE(nda->tx_keep_alive));
	seq_printf(m, "%-25s = %llu\n", "rx_keep_alive", READ_ONCE(nda->rx_keep_alive));
	seq_printf(m, "%-25s = %llu\n", "dropped", READ_ONCE(nda->dropped));
	seq_printf(m, "%-25s = %llu\n", "drop_qfull", READ_ONCE(nda->drop_qfull));
	seq_printf(m, "%-25s = %llu\n", "fifo_full", READ_ONCE(stats.fifo_full));
	seq_printf(m, "%-25s = %llu\n", "fifo_empty", READ_ONCE(stats.fifo_empty));
	seq_printf(m, "%-25s = %llu\n", "fifo_enq", READ_ONCE(stats.fifo_enq));
	seq_printf(m, "%-25s = %llu\n", "fifo_dq", READ_ONCE(stats.fifo_dq));
	seq_printf(m, "%-25s = %llu\n", "test_dev_tx_pkts", READ_ONCE(nda->test_dev_tx_pkts));
	seq_printf(m, "%-25s = %llu\n", "test_dev_tx_bytes", READ_ONCE(nda->test_dev_tx_bytes));
	seq_printf(m, "------------[Dev attach stats end]-----------\n");

	return 0;
}

/*
 * netfn_dev_attach_stats_open()
 *	Opens stats file.
 */
static int netfn_dev_attach_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, netfn_dev_attach_stats_show, inode->i_private);
}

/*
 * file_ops
 *	File operations for the stats file.
 */
static void __phdr_dump(struct sk_buff *skb)
{
	struct netfn_capwap_prehdr *pre = (struct netfn_capwap_prehdr *)skb->data;

	pr_info("version\t\t = %u\nrid\t\t = %u\ndscp\t\t = %d\nvlan_pcp\t\t = %d\ntype\t\t = %d\nnwireless\t = %d\nnwireless_qos\t = %hhx\nouter_sgt\t = %d\ninner_sgt\t = %d\nvapid\t\t = %d\n",
			pre->version, pre->rid, pre->dscp, pre->vlan_pcp, pre->type, pre->nwireless, pre->wireless_qos, pre->outer_sgt, pre->inner_sgt, pre->vapid);
	pr_info("wl_info\t\t = {%hhx, %hhx, %hhx, %hhx, %hhx, %hhx, %hhx, %hhx}\n",
			pre->wl_info[0], pre->wl_info[1], pre->wl_info[2], pre->wl_info[3],
			pre->wl_info[4], pre->wl_info[5], pre->wl_info[6], pre->wl_info[7]);
	pr_info("tunnel_id\t\t = %d\n", pre->tunnel_id);
}

static const struct file_operations file_ops = {
	.open = netfn_dev_attach_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * __attach_phdr()
 *	Attaches preheader to skb. Returns 0 on success, -ENOMEM on failure.
 */
static int __attach_phdr(struct netfn_dev_attach_priv *nda, struct sk_buff *skb, uint32_t tunnel_id)
{
	unsigned int needed_hroom = skb->dev->needed_headroom;
	unsigned int needed_troom = skb->dev->needed_tailroom;
	struct netfn_capwap_prehdr *pre = &nda->prehdr;
	bool expand_skb = false;

	pre->tunnel_id = tunnel_id;

	expand_skb = (skb_cloned(skb) || (skb_headroom(skb) < needed_hroom) || (skb_tailroom(skb) < needed_troom));
	if (expand_skb && pskb_expand_head(skb, needed_hroom, needed_troom, GFP_NOWAIT | __GFP_NOWARN)) {
		pr_warn_ratelimited("%p: Failed to expand SKB with headroom(%u) tailroom(%u)\n",
				skb->dev, skb_headroom(skb), skb_tailroom(skb));
		return -ENOMEM;
	}

	memcpy(__skb_push(skb, sizeof(*pre)), pre, sizeof(*pre));
	return 0;
}

/*
 * __steer()
 *	Transmits a packet, either via steering or directly.
 */
static void __steer(struct netfn_dev_attach_priv *nda, struct sk_buff *skb)
{
	WRITE_ONCE(nda->tx_pkts, nda->tx_pkts + 1);

	if (nda->steer_en) {
		if (!netfn_pkt_steer_send(&nda->tx_steer, skb, nda->to_dev_cpu)) {
			WRITE_ONCE(nda->drop_qfull, nda->drop_qfull + 1);
			dev_kfree_skb_any(skb);
		}
		return;
	}

	if (nda->fast_xmit) {
		skb->fast_xmit = 1;
		skb->fast_recycled = 1;
		dmac_clean_range_no_dsb((void *)skb->data, (void *)(skb->data + skb->len));
		dev_fast_xmit(skb, skb->dev, netif_skb_features(skb));
		return;
	}

	if (unlikely(nda->debug > 0)) {
		skb_dump(KERN_INFO, skb, false);
		nda->debug--;
	}

	dev_queue_xmit(skb);
}

/*
 * __encap_and_send()
 *	Encapsulate and send a packet.
 */
static void __encap_and_send(struct netfn_dev_attach_priv *nda, struct sk_buff *skb)
{
	uint32_t tunnel_id = nda->id;

	if (!nda->capwap_dev) {
		pr_warn_ratelimited("%p: CAPWAP device not attached, dropping packet\n", nda);
		WRITE_ONCE(nda->dropped, nda->dropped + 1);
		dev_kfree_skb_any(skb);
		return;
	}

	skb->dev = nda->capwap_dev;
	if (__attach_phdr(nda, skb, tunnel_id)) {
		WRITE_ONCE(nda->dropped, nda->dropped + 1);
		dev_kfree_skb_any(skb);
		return;
	}

	__steer(nda, skb);
}

/*
 * netfn_dev_xmit()
 *	Transmit a packet from the test device.
 */
static netdev_tx_t netfn_dev_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct netfn_dev_attach_priv *nda = &nda_g;
	unsigned int len = skb->len;

	WRITE_ONCE(nda->test_dev_tx_pkts, nda->test_dev_tx_pkts + 1);
	WRITE_ONCE(nda->test_dev_tx_bytes, nda->test_dev_tx_bytes + len);

	skb->dev = dev;
	__encap_and_send(nda, skb);
	return NETDEV_TX_OK;
}

/*
 * netfn_dev_get_stats64()
 *	Returns stats for the test netdevice.
 */
static void netfn_dev_get_stats64(struct net_device *dev, struct rtnl_link_stats64 *stats)
{
	struct netfn_dev_attach_priv *nda = &nda_g;

	stats->tx_packets = READ_ONCE(nda->test_dev_tx_pkts);
	stats->tx_bytes   = READ_ONCE(nda->test_dev_tx_bytes);
}

/*
 * netfn_dev_lan_handler()
 *	Handler for LAN/WLAN device to receive inner packet(s).
 */
static int netfn_dev_lan_handler(struct sk_buff *skb)
{
	struct netfn_dev_attach_priv *nda = &nda_g;
	struct net_device *lan_dev;
	struct ipv6hdr *ip6h;
	struct iphdr *iph;

	rcu_read_lock();
	lan_dev = rcu_dereference(nda->lan_dev);
	if (!lan_dev || skb->dev != lan_dev) {
		rcu_read_unlock();
		return 0;
	}

	pr_debug("%p : Received packet on (%s) handler netfn_dev_lan_handler ( nda->lan_dev %s)\n", skb, skb->dev->name, lan_dev->name);
	rcu_read_unlock();

	if (unlikely(nda->debug > 0)) {
		pr_info("%p : Received packet on (%s) netfn_dev_lan_handler\n", skb, skb->dev->name);
		skb_dump(KERN_INFO, skb, false);
		nda->debug--;
	}

	WRITE_ONCE(nda->lan_rx_pkts, nda->lan_rx_pkts + 1);

	switch (ntohs(skb->protocol)) {
	case ETH_P_IP:
		iph = ip_hdr(skb);

		/*
		 * Example match: 192.168.1.0/24
		 */
		if ((iph->daddr & htonl(nda->ip_mask)) != htonl(nda->ip_mask)) {
			return 0;
		}

		break;

	case ETH_P_IPV6:
		ip6h = ipv6_hdr(skb);

		/*
		 * Example match: 2001::/32
		 */
		if ((ip6h->daddr.s6_addr32[0] & htonl(nda->ip6_mask)) != htonl(nda->ip6_mask)) {
			return 0;
		}

		break;

	default:
		return 0;
	}

	if (unlikely(skb_headroom(skb) < ETH_HLEN)) {
		pr_warn_ratelimited("%p: Insufficient headroom for ETH header\n", nda);
		WRITE_ONCE(nda->dropped, nda->dropped + 1);
		dev_kfree_skb_any(skb);
		return 1;
	}

	skb_push(skb, ETH_HLEN);
	__encap_and_send(nda, skb);

	return 1;
}

/*
 * netfn_dev_capwap_handler()
 *	Handler for CAPWAP.
 */
static rx_handler_result_t netfn_dev_capwap_handler(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct netfn_dev_attach_priv *nda = rcu_dereference_bh(skb->dev->rx_handler_data);

	WRITE_ONCE(nda->capwap_rx_pkts, nda->capwap_rx_pkts + 1);

	if (unlikely(nda->debug > 0)) {
		pr_info("%p : Received packet on (%s) netfn_dev_capwap_handler\n", skb, skb->dev->name);
		pr_info("Received Meta Data:-\n");
		__phdr_dump(skb);
		skb_dump(KERN_INFO, skb, false);
		nda->debug--;
	}

	skb_pull(skb, sizeof(struct netfn_capwap_prehdr));

	if (nda->capwap_drop) {
		WRITE_ONCE(nda->dropped, nda->dropped + 1);
		pr_debug("%p : packet has been dropped\n", skb);
		dev_kfree_skb_any(skb);
		return RX_HANDLER_CONSUMED;
	}

	skb->dev = rcu_dereference_bh(nda->lan_dev);
	if (unlikely(!skb->dev)) {
		WRITE_ONCE(nda->dropped, nda->dropped + 1);
		dev_kfree_skb_any(skb);
		return RX_HANDLER_CONSUMED;
	}

	__steer(nda, skb);

	return RX_HANDLER_CONSUMED;
}

/*
 * netfn_dev_except_handler()
 *	Handler for exceptioned packet.
 */
static rx_handler_result_t netfn_dev_except_handler(struct sk_buff **pskb)
{
	struct netfn_dev_attach_priv *nda = &nda_g;
	struct sk_buff *skb = *pskb;

	if (skb->len >= 2 && (skb->data[skb->len - 1] == KEEPALIVE_MAGIC2 && skb->data[skb->len - 2] == KEEPALIVE_MAGIC1)) {
		WRITE_ONCE(nda->rx_keep_alive, nda->rx_keep_alive + 1);
		skb_dump(KERN_INFO, skb, 0);
		dev_kfree_skb_any(skb);
		return RX_HANDLER_CONSUMED;
	}
	return RX_HANDLER_PASS;
}

/*
 * __ps_cb()
 *	Callback for processing steered packets
 */
static void __ps_cb(struct netfn_pkt_steer *tx_steer, struct sk_buff_head *q_head)
{
	struct netfn_dev_attach_priv *nda = container_of(tx_steer, struct netfn_dev_attach_priv, tx_steer);
	struct sk_buff *skb, *next_skb;

	skb_queue_walk_safe(q_head, skb, next_skb) {
		__skb_unlink(skb, q_head);

		pr_debug("%p : Sending packet out on (%s) after steer\n", skb, skb->dev->name);
		if (nda->fast_xmit) {
			skb->fast_xmit = 1;
			skb->fast_recycled = 1;
			dmac_clean_range_no_dsb((void *)skb->data, (void *)(skb->data + skb->len));
			dev_fast_xmit(skb, skb->dev, netif_skb_features(skb));
			continue;
		}

		dev_queue_xmit(skb);
	}
}

/*
 * __add_steer()
 *	Steers the packet from one core to other
 */
static int __add_steer(struct netfn_dev_attach_priv *nda)
{
	cpumask_t tx_cpus = CPU_MASK_ALL, rx_cpus = CPU_MASK_ALL;
	struct netfn_pkt_steer_info info = {0};

	if (!nda->steer_en) {
		return 0;
	}

	info.cb = __ps_cb;
	info.budget = 64;
	info.weight = 1;
	info.dev = &nda->napi_dev;
	info.sender_mask = tx_cpus;
	info.receiver_mask = rx_cpus;
	info.fifo_len = 8192;
	netfn_pkt_steer_init(&nda->tx_steer, &info);

	if (!netfn_pkt_steer_enable(&nda->tx_steer)) {
		pr_warn("%p: Unable to enable tx data pkt steering.\n", nda);
		netfn_pkt_steer_deinit(&nda->tx_steer);
		return -ENOMEM;
	}

	return 0;
}

/*
 * __del_steer()
 *	Removes the packet pkt steer
 */
static void __del_steer(struct netfn_dev_attach_priv *nda)
{
	if (!nda->steer_en) {
		return;
	}
	netfn_pkt_steer_disable(&nda->tx_steer);
	netfn_pkt_steer_deinit(&nda->tx_steer);
}

/*
 * __detach_devices()
 *	Detaches handlers from network devices.
 */
static void __detach_devices(struct netfn_dev_attach_priv *nda)
{
	RCU_INIT_POINTER(athrs_fast_nat_recv, NULL);
	synchronize_rcu();

	rtnl_lock();
	if (nda->capwap_dev) {
		netdev_rx_handler_unregister(nda->capwap_dev);
	}
	if (nda->wan_dev) {
		netdev_rx_handler_unregister(nda->wan_dev);
	}
	rtnl_unlock();

	if (nda->capwap_dev) {
		dev_put(nda->capwap_dev);
		nda->capwap_dev = NULL;
	}
	if (nda->lan_dev) {
		dev_put(nda->lan_dev);
		nda->lan_dev = NULL;
	}
	if (nda->wan_dev) {
		dev_put(nda->wan_dev);
		nda->wan_dev = NULL;
	}
}

/*
 * __attach_devices()
 *	Attaches handlers to network devices.
 */
static int __attach_devices(struct netfn_dev_attach_priv *nda)
{
	int ret;

	if (nda->lan_dev) {
		pr_warn("%p: Already attached\n", nda);
		return -EBUSY;
	}

	nda->lan_dev = dev_get_by_name(&init_net, nda->lan_dev_name);
	if (!nda->lan_dev) {
		pr_warn("%p: Could not find lan_dev: %s\n", nda, nda->lan_dev_name);
		return -ENODEV;
	}

	nda->capwap_dev = dev_get_by_name(&init_net, nda->capwap_dev_name);
	if (!nda->capwap_dev) {
		pr_warn("%p: Could not find capwap_dev: %s\n", nda, nda->capwap_dev_name);
		ret = -ENODEV;
		goto put_lan;
	}

	nda->wan_dev = dev_get_by_name(&init_net, nda->wan_dev_name);
	if (!nda->wan_dev) {
		pr_warn("%p: Could not find wan_dev: %s\n", nda, nda->wan_dev_name);
		ret = -ENODEV;
		goto put_capwap;
	}

	if (athrs_fast_nat_recv) {
		pr_warn("%p: Overriding existing athrs_fast_nat_recv handler\n", nda);
	}
	RCU_INIT_POINTER(athrs_fast_nat_recv, netfn_dev_lan_handler);

	rtnl_lock();

	ret = netdev_rx_handler_register(nda->capwap_dev, &netfn_dev_capwap_handler, nda);
	if (ret) {
		pr_warn("%p: Failed to register rx handler for dev(%s)\n", nda, nda->capwap_dev->name);
		goto unwind1;
	}

	ret = netdev_rx_handler_register(nda->wan_dev, &netfn_dev_except_handler, nda);
	if (ret) {
		pr_warn("%p: Failed to register rx handler for dev(%s)\n", nda, nda->wan_dev->name);
		goto unwind2;
	}

	rtnl_unlock();
	return 0;
unwind2:
	netdev_rx_handler_unregister(nda->capwap_dev);
unwind1:
	rtnl_unlock();
	RCU_INIT_POINTER(athrs_fast_nat_recv, NULL);
	dev_put(nda->wan_dev);
	nda->wan_dev = NULL;
put_capwap:
	dev_put(nda->capwap_dev);
	nda->capwap_dev = NULL;
put_lan:
	dev_put(nda->lan_dev);
	nda->lan_dev = NULL;
	return ret;
}

/*
 * netfn_dev_attach()
 *	Adds a new dev ctx
 */
static int netfn_dev_attach(struct ctl_table *ctl, int write, void __user *buffer, size_t *lenp, loff_t *ppos)
{
	struct netfn_dev_attach_priv *nda = &nda_g;
	int ret;

	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	switch(nda->attach) {
	case 0:
		__detach_devices(nda);
		break;
	case 1:
		__attach_devices(nda);
		break;
	default:
		pr_warn("%p: Invalid value for attach. Expected 0 or 1.\n", nda);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * netfn_dev_tx_capwap_keepalive()
 *	trigger new Keep alive packet
 */
static int netfn_dev_tx_capwap_keepalive(struct ctl_table *ctl, int write, void __user *buffer, size_t *lenp, loff_t *ppos)
{
	static const uint8_t capwap[] = {0x00, 0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00};
	static const uint8_t data[] = {0x00, 0x16, 0x00, 0x23,
		0x00, 0x10, 0x00, 0x15, 0x60, 0xc9, 0xd1, 0xe2, 0x00, 0x14, 0x78, 0x11, 0x22, 0x98,
		0x25, 0x31, KEEPALIVE_MAGIC1, KEEPALIVE_MAGIC2};
	unsigned int udp_len = sizeof(struct udphdr) + sizeof(capwap) + sizeof(data);
	unsigned int ip_len = sizeof(struct iphdr) + udp_len;
	struct netfn_dev_attach_priv *nda = &nda_g;
	struct netfn_ka_cfg *cfg = &nda->ka_cfg;
	struct netfn_capwap_prehdr *pre;
	struct udphdr *udph;
	struct iphdr *iph;
	struct sk_buff *skb;
	struct net_device *dev;
	int ret;

	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	dev = nda->capwap_dev;
	if (!dev) {
		pr_warn("%p: CAPWAP device not attached\n", nda);
		return -ENODEV;
	}

	/*
	 * Send CAPWAP encap keepalive packet (to be bypassed).
	 */
	skb = dev_alloc_skb(NETFN_KEEPALIVE_SKB_SIZE);
	if (!skb) {
		return -ENOMEM;
	}

	skb->dev = dev;
	skb_reserve(skb, 512);

	iph = (struct iphdr *)skb_put(skb, sizeof(*iph));
	iph->version	= 4;
	iph->ihl	= 5;
	iph->tos	= 0x50;
	iph->tot_len	= htons(ip_len);
	iph->id		= htons(1);
	iph->frag_off	= htons(IP_DF);
	iph->ttl	= IPDEFTTL;
	iph->protocol	= IPPROTO_UDP;
	iph->saddr	= in_aton(cfg->src_ip);
	iph->daddr	= in_aton(cfg->dst_ip);
	iph->check	= 0;
	iph->check	= ip_fast_csum(iph, iph->ihl);

	udph = (struct udphdr *)skb_put(skb, sizeof(*udph));
	udph->source	= htons((uint16_t)cfg->src_port);
	udph->dest	= htons((uint16_t)cfg->dst_port);
	udph->len	= htons(udp_len);
	udph->check	= 0;

	memcpy(__skb_put(skb, sizeof(capwap)), capwap, sizeof(capwap));
	memcpy(__skb_put(skb, sizeof(data)), data, sizeof(data));

	if (__attach_phdr(nda, skb, nda->id)) {
		WRITE_ONCE(nda->dropped, nda->dropped + 1);
		dev_kfree_skb_any(skb);
		goto send_enc_keepalive;
	}

	pre = (struct netfn_capwap_prehdr *)skb->data;
	pre->type |= NETFN_CAPWAP_PKT_TYPE_BYPASS_KEEPALIVE;

	ret = dev_queue_xmit(skb);
	if (ret != NETDEV_TX_OK) {
		pr_warn("%p: Transmission failure for bypass keepalive\n", nda);
		WRITE_ONCE(nda->dropped, nda->dropped + 1);
		dev_kfree_skb_any(skb);
	} else {
		pr_info("%p: Sent keepalive with PKT_TYPE_BYPASS_KEEPALIVE\n", nda);
		WRITE_ONCE(nda->tx_keep_alive, nda->tx_keep_alive + 1);
	}

	/*
	 * Send packet with just the keepalive bit set.
	 */
send_enc_keepalive:
	skb = dev_alloc_skb(NETFN_KEEPALIVE_SKB_SIZE);
	if (!skb) {
		return -ENOMEM;
	}

	skb->dev = dev;
	skb_reserve(skb, 512);
	memcpy(__skb_put(skb, sizeof(data)), data, sizeof(data));

	if (__attach_phdr(nda, skb, nda->id)) {
		WRITE_ONCE(nda->dropped, nda->dropped + 1);
		dev_kfree_skb_any(skb);
		return 0;
	}

	pre = (struct netfn_capwap_prehdr *)skb->data;
	pre->type = NETFN_CAPWAP_PKT_TYPE_ENC_KEEPALIVE;

	ret = dev_queue_xmit(skb);
	if (ret != NETDEV_TX_OK) {
		pr_warn("%p: Transmission failure for encap keepalive\n", nda);
		WRITE_ONCE(nda->dropped, nda->dropped + 1);
		dev_kfree_skb_any(skb);
	} else {
		pr_info("%p: Sent keepalive with PKT_TYPE_ENC_KEEPALIVE\n", nda);
		WRITE_ONCE(nda->tx_keep_alive, nda->tx_keep_alive + 1);
	}

	return 0;
}

/*
 *	Sysctl handler for string parameters.
 */
static int netfn_dev_attach_cfg(struct ctl_table *ctl, int write, void __user *buffer, size_t *lenp, loff_t *ppos)
{
	return proc_dostring(ctl, write, buffer, lenp, ppos);
}

/*
 * netfn_dev_attach_setup()
 *	Setup function for test netdevice.
 */
static void netfn_dev_attach_setup(struct net_device *dev)
{
	uint8_t eth_addr[ETH_ALEN];

	ether_setup(dev);
	dev->netdev_ops = &netfn_dev_ops;
	eth_random_addr(eth_addr);
	dev_addr_set(dev, eth_addr);
}

/*
 * netfn_dev_attach_init()
 *	module init
 */
static int __init netfn_dev_attach_init(void)
{
	int ret;

	memset(&nda_g, 0, sizeof(nda_g));
	init_dummy_netdev(&nda_g.napi_dev);
	nda_g.steer_en = 1; /* Enable steering by default */

	/*
	 * Initialize preheader config to default values
	 */
	nda_g.prehdr.version = 3;
	nda_g.prehdr.rid = 10;
	nda_g.prehdr.dscp = 20;
	nda_g.prehdr.vlan_pcp = 13;
	nda_g.prehdr.type = (NETFN_CAPWAP_PKT_TYPE_DATA | NETFN_CAPWAP_PKT_TYPE_8023 | NETFN_CAPWAP_PKT_TYPE_WINFO);
	nda_g.prehdr.nwireless = 1;
	nda_g.prehdr.wireless_qos = 0x0110;
	nda_g.prehdr.outer_sgt = 10;
	nda_g.prehdr.inner_sgt = 0x2468;
	nda_g.prehdr.vapid = 0x3579;

	/*
	 * Initialize keepalive 5-tuple defaults.
	 */
	strscpy(nda_g.ka_cfg.src_ip, "192.168.2.2", sizeof(nda_g.ka_cfg.src_ip));
	strscpy(nda_g.ka_cfg.dst_ip, "192.168.2.1", sizeof(nda_g.ka_cfg.dst_ip));
	nda_g.ka_cfg.src_port = 5247;
	nda_g.ka_cfg.dst_port = 5003;

	nda_g.test_dev = alloc_netdev(0, "test_dev", NET_NAME_UNKNOWN, netfn_dev_attach_setup);
	if (!nda_g.test_dev) {
		pr_warn("%p: Failed to allocate test device\n", &nda_g);
		return -ENOMEM;
	}

	ret = register_netdev(nda_g.test_dev);
	if (ret) {
		pr_warn("%p: Failed to register test device\n", &nda_g);
		goto free_dev;
	}

	/*
	 * Create debugfs entry.
	 */
	nda_g.dentry = debugfs_create_dir("qca-netfn-dev-attach", NULL);
	if (IS_ERR_OR_NULL(nda_g.dentry)) {
		pr_warn("%p: Unable to create debugfs directory for NETFN dev attach\n", &nda_g);
		ret = -ENOMEM;
		goto unwind_netdev;
	}

	debugfs_create_file("stats", 0444, nda_g.dentry, &nda_g, &file_ops);

	nda_g.sysctl_header = register_sysctl("dev/qca_netfn_dev_attach", netfn_dev_table);
	if (!nda_g.sysctl_header) {
		pr_warn("%p: Failed to register sysctl table\n", &nda_g);
		ret = -EINVAL;
		goto unwind_debugfs;
	}

	nda_g.sysctl_prehdr_header = register_sysctl("dev/qca_netfn_dev_attach/prehdr", netfn_prehdr_table);
	if (!nda_g.sysctl_prehdr_header) {
		pr_warn("%p: Failed to register sysctl prehdr table\n", &nda_g);
		ret = -EINVAL;
		goto unwind_header;
	}

	nda_g.sysctl_ka_header = register_sysctl("dev/qca_netfn_dev_attach/keepalive", netfn_ka_table);
	if (!nda_g.sysctl_ka_header) {
		pr_warn("%p: Failed to register sysctl keepalive table\n", &nda_g);
		ret = -EINVAL;
		goto unwind_prehdr;
	}

	ret = __add_steer(&nda_g);
	if (ret) {
		pr_warn("%p: Failed to add packet steering\n", &nda_g);
		goto unwind_ka;
	}

	pr_info("NETFN Dev attach module loaded\n");
	return 0;

unwind_ka:
	unregister_sysctl_table(nda_g.sysctl_ka_header);
unwind_prehdr:
	unregister_sysctl_table(nda_g.sysctl_prehdr_header);
unwind_header:
	unregister_sysctl_table(nda_g.sysctl_header);
unwind_debugfs:
	debugfs_remove_recursive(nda_g.dentry);
unwind_netdev:
	unregister_netdev(nda_g.test_dev);
free_dev:
	free_netdev(nda_g.test_dev);
	return ret;
}

/*
 * netfn_dev_attach_exit()
 *	module exit
 */
static void __exit netfn_dev_attach_exit(void)
{
	__detach_devices(&nda_g);
	__del_steer(&nda_g);

	unregister_netdev(nda_g.test_dev);
	free_netdev(nda_g.test_dev);

	if (nda_g.sysctl_ka_header) {
		unregister_sysctl_table(nda_g.sysctl_ka_header);
	}

	if (nda_g.sysctl_prehdr_header) {
		unregister_sysctl_table(nda_g.sysctl_prehdr_header);
	}

	if (nda_g.sysctl_header) {
		unregister_sysctl_table(nda_g.sysctl_header);
	}

	if (nda_g.dentry) {
		debugfs_remove_recursive(nda_g.dentry);
	}

	pr_info("NETFN Dev attach module unloaded\n");
}

module_init(netfn_dev_attach_init);
module_exit(netfn_dev_attach_exit);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NetFN Device Attach Test Module");
