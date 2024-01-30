/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/if.h>
#include <linux/rculist.h>
#include <linux/netdevice.h>
#include <linux/netdevice.h>
#include <linux/debugfs.h>
#include <net/addrconf.h>
#include <net/dst_cache.h>
#include <net/route.h>
#include <net/ip.h>
#include <net/ip6_route.h>

#include "eip_ipsec_priv.h"


/*
 * eip_ipsec_dev_get_summary_stats()
 *	Update the summary stats.
 */
static void eip_ipsec_dev_get_summary_stats(struct eip_ipsec_dev *eid,
		struct eip_ipsec_dev_stats *stats)
{
	int words;
	int cpu;
	int i;

	words = (sizeof(*stats) / sizeof(uint64_t));
	memset(stats, 0, sizeof(*stats));

	/*
	 * All statistics are 64bit. So we can just iterate by words.
	 */
	for_each_possible_cpu(cpu) {
		const struct eip_ipsec_dev_stats *sp = per_cpu_ptr(eid->stats_pcpu, cpu);
		uint64_t *stats_ptr = (uint64_t *)stats;
		uint64_t *sp_ptr = (uint64_t *)sp;

		for (i = 0; i < words; i++, stats_ptr++, sp_ptr++)
			*stats_ptr += *sp_ptr;
	}
}

/*
 * eip_ipsec_dev_read()
 *	Read device statistics.
 */
static ssize_t eip_ipsec_dev_read(struct file *fp, char __user *ubuf, size_t sz, loff_t *ppos)
{
	struct eip_ipsec_dev *eid = fp->private_data;
	struct eip_ipsec_dev_stats stats;
	uint8_t sa_count;
	ssize_t len = 0;
	ssize_t max_len;
	char *buf;

	eip_ipsec_dev_get_summary_stats(eid, &stats);
	sa_count = stats.sa_added - stats.sa_removed;

	/*
	 * We need to calculate required string buffer for stats, else full stats may not be captured.
	 */
	max_len = (sizeof(struct eip_ipsec_dev_stats) / sizeof(uint64_t)) * EIP_IPSEC_MAX_STR_LEN; /* Members */
	max_len += EIP_IPSEC_MAX_STR_LEN; /* Encap heading */
	max_len += EIP_IPSEC_MAX_STR_LEN; /* Decap heading */
	max_len += (EIP_IPSEC_MAX_STR_LEN * sa_count); /* SA header */
	max_len += ((sizeof(struct eip_ipsec_sa_stats) / sizeof(uint64_t)) * EIP_IPSEC_MAX_STR_LEN * sa_count) ; /* SA Members */

	buf = vzalloc(max_len);
	if (!buf) {
		pr_warn("%px: failed to allocate print buffer (%zu)", eid, max_len);
		return 0;
	}


	/*
	 * Create Stats strings.
	 */
	len = snprintf(buf, max_len, "SA Added: %llu\n", stats.sa_added);
	len += snprintf(buf + len, max_len - len, "SA Removed: %llu\n", stats.sa_removed);
	len += snprintf(buf + len, max_len - len, "Device Encapsulation Statistics:\n");
	len += snprintf(buf + len, max_len - len, "\tTx packets: %llu\n", stats.tx_pkts);
	len += snprintf(buf + len, max_len - len, "\tTx bytes: %llu\n", stats.tx_bytes);
	len += snprintf(buf + len, max_len - len, "\tTx VP Exception: %llu\n", stats.tx_vp_exp);
	len += snprintf(buf + len, max_len - len, "\tTx host: %llu\n", stats.tx_host);
	len += snprintf(buf + len, max_len - len, "\tTx Error: %llu\n", stats.tx_fail);
	len += snprintf(buf + len, max_len - len, "\tTx Fail SA: %llu\n", stats.tx_fail_sa);
	len += snprintf(buf + len, max_len - len, "\tTx Fail SA (VP Exception): %llu\n", stats.tx_fail_vp_sa);

	len += snprintf(buf + len, max_len - len, "Device Decapsulation Statistics:\n");
	len += snprintf(buf + len, max_len - len, "\tRx packets: %llu\n", stats.rx_pkts);
	len += snprintf(buf + len, max_len - len, "\tRx bytes: %llu\n", stats.rx_bytes);
	len += snprintf(buf + len, max_len - len, "\tRx host: %llu\n", stats.rx_host);
	len += snprintf(buf + len, max_len - len, "\tRx Error: %llu\n", stats.rx_fail);
	len += snprintf(buf + len, max_len - len, "\tRx Fail SA: %llu\n", stats.rx_fail_sa);

	/*
	 * Encap SA stats read.
	 */
	len += eip_ipsec_sa_read(eid->ndev, true, buf + len, max_len - len);

	/*
	 * Decap SA stats read.
	 */
	len += eip_ipsec_sa_read(eid->ndev, false, buf + len, max_len - len);

	len = simple_read_from_buffer(ubuf, sz, ppos, buf, len);
	vfree(buf);

	return len;
}

/*
 * eip_ipsec_dev_open()
 *	Netdevice open handler.
 */
static int eip_ipsec_dev_open(struct net_device *ndev)
{
	netif_start_queue(ndev);
	return 0;
}

/*
 * eip_ipsec_dev_stop()
 *	Netdevice stop handler.
 */
static int eip_ipsec_dev_stop(struct net_device *ndev)
{
	netif_stop_queue(ndev);
	return 0;
}

/*
 * eip_ipsec_dev_stats64()
 *	Handler to fetch netdevice rtnl statistics.
 */
static void eip_ipsec_dev_stats64(struct net_device *ndev, struct rtnl_link_stats64 *rtnl_stats)
{
	struct eip_ipsec_dev *eid = netdev_priv(ndev);
	struct eip_ipsec_dev_stats stats = {0};

	memset(rtnl_stats, 0, sizeof(*rtnl_stats));
	eip_ipsec_dev_get_summary_stats(eid, &stats);

	rtnl_stats->tx_packets = stats.tx_pkts;
	rtnl_stats->tx_bytes = stats.tx_bytes;
	rtnl_stats->tx_dropped = stats.tx_fail;
	rtnl_stats->rx_packets = stats.rx_pkts;
	rtnl_stats->rx_bytes = stats.rx_bytes;
	rtnl_stats->rx_dropped = stats.rx_fail;
}

/*
 * eip_ipsec_dev_mtu()
 *	Update device MTU.
 */
static int eip_ipsec_dev_mtu(struct net_device *ndev, int mtu)
{
	ndev->mtu = mtu;
	return 0;
}

/*
 * eip_ipsec_dev_enc_err()
 *	Encapsulation error completion callback.
 */
void eip_ipsec_dev_enc_err(void *app_data, eip_req_t req, int err)
{
	struct eip_ipsec_sa *sa = eip_ipsec_sa_ref_unless_zero((struct eip_ipsec_sa *)app_data);
	struct sk_buff *skb = eip_req2skb(req);
	struct eip_ipsec_dev_stats *dev_stats;
	struct eip_ipsec_sa_stats *sa_stats;
	struct eip_ipsec_dev *eid;

	pr_debug("%px: Encapsulation failed with err(%d)\n", sa, err);

	if (unlikely(!sa)) {
		pr_debug("%px: Failed to take reference on SA\n", skb);
		consume_skb(skb);
		return;
	}

	eid = sa->eid;
	dev_stats = this_cpu_ptr(eid->stats_pcpu);
	dev_stats->tx_fail++;

	/*
	 * Update SA statistics.
	 */
	sa_stats = this_cpu_ptr(sa->stats_pcpu);
	sa_stats->rx_pkts++;
	sa_stats->rx_bytes += skb->len;
	sa_stats->fail_transform++;

	eip_ipsec_sa_deref(sa);
	skb_dump(KERN_DEBUG, skb, false);
	consume_skb(skb);
}

/*
 * eip_ipsec_dev_enc_done_v6()
 *	Encapsulation completion callback.
 */
void eip_ipsec_dev_enc_done_v6(void *app_data, eip_req_t req)
{
	struct eip_ipsec_sa *sa = eip_ipsec_sa_ref_unless_zero((struct eip_ipsec_sa *)app_data);
	struct sk_buff *skb = eip_req2skb(req);
	struct eip_ipsec_dev_stats *dev_stats;
	struct eip_ipsec_sa_stats *sa_stats;
	struct eip_ipsec_dev *eid;
	struct dst_entry *dst;

	if (unlikely(!sa)) {
		pr_debug("%px: Failed to take reference on SA\n", sa);
		consume_skb(skb);
		return;
	}

	eid = sa->eid;
	dev_stats = this_cpu_ptr(eid->stats_pcpu);
	sa_stats = this_cpu_ptr(sa->stats_pcpu);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);

	/*
	 * Update SA statistics.
	 */
	sa_stats->rx_pkts++;
	sa_stats->rx_bytes += skb->len;

	dst = dst_cache_get(&sa->dst_cache);
	if (unlikely(!dst)) {
		struct ipv6hdr *ip6h = ipv6_hdr(skb);
		struct flowi6 fl6;

		memset(&fl6, 0, sizeof(struct flowi6));
		memcpy(&fl6.daddr, &ip6h->daddr, sizeof(fl6.daddr));
		memcpy(&fl6.saddr, &ip6h->saddr, sizeof(fl6.saddr));

		dst = ip6_route_output(&init_net, NULL, &fl6);
		if (IS_ERR(dst)) {
			sa_stats->fail_route++;
			goto consume;
		}

		sa_stats->dst_cache_miss++;
		dst_cache_set_ip6(&sa->dst_cache, dst, &fl6.saddr);
	}

	/*
	 * Drop existing dst and set new.
	 */
	skb_dst_drop(skb);
	skb_dst_set(skb, dst);

	/*
	 * Reset General SKB fields for further processing.
	 */
	skb->protocol = htons(ETH_P_IPV6);
	skb->skb_iif = sa->ndev->ifindex;
	skb->ip_summed = CHECKSUM_COMPLETE;

	memset(IP6CB(skb), 0, sizeof(*IP6CB(skb)));
	IP6CB(skb)->flags |= IP6SKB_XFRM_TRANSFORMED;

	/*
	 * Update device statistics.
	 */
	dev_stats->tx_host++;

	ip6_local_out(&init_net, NULL, skb);
	eip_ipsec_sa_deref(sa);
	return;

consume:
	dev_stats->tx_fail++;
	eip_ipsec_sa_deref(sa);
	skb_dump(KERN_DEBUG, skb, false);
	consume_skb(skb);
}

/*
 * eip_ipsec_enc_done_v4()
 *	Encapsulation completion callback.
 */
void eip_ipsec_dev_enc_done_v4(void *app_data, eip_req_t req)
{
	struct eip_ipsec_sa *sa = eip_ipsec_sa_ref_unless_zero((struct eip_ipsec_sa *)app_data);
	struct sk_buff *skb = eip_req2skb(req);
	struct eip_ipsec_dev_stats *dev_stats;
	struct eip_ipsec_sa_stats *sa_stats;
	struct eip_ipsec_dev *eid;
	struct dst_entry *dst;

	if (unlikely(!sa)) {
		pr_debug("%px: Failed to take reference on SA\n", sa);
		consume_skb(skb);
		return;
	}

	eid = sa->eid;
	dev_stats = this_cpu_ptr(eid->stats_pcpu);
	sa_stats = this_cpu_ptr(sa->stats_pcpu);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);

	/*
	 * Update SA statistics.
	 */
	sa_stats->rx_pkts++;
	sa_stats->rx_bytes += skb->len;

	dst = dst_cache_get(&sa->dst_cache);
	if (unlikely(!dst)) {
		struct rtable *rt;

		rt = ip_route_output(&init_net, ip_hdr(skb)->daddr, ip_hdr(skb)->saddr, 0, 0);
		if (IS_ERR(rt)) {
			sa_stats->fail_route++;
			goto consume;
		}

		dst = &rt->dst;
		sa_stats->dst_cache_miss++;
		dst_cache_set_ip4(&sa->dst_cache, dst, ip_hdr(skb)->saddr);
	}

	/*
	 * Drop existing dst and set new.
	 */
	skb_dst_drop(skb);
	nf_reset_ct(skb);
	skb_dst_set(skb, dst);

	/*
	 * Reset General SKB fields for further processing.
	 */
	skb->protocol = htons(ETH_P_IP);
	skb->skb_iif = sa->ndev->ifindex;
	skb->ip_summed = CHECKSUM_COMPLETE;

	memset(IPCB(skb), 0, sizeof(*IPCB(skb)));
	IPCB(skb)->flags |= IPSKB_XFRM_TRANSFORMED;

	/*
	 * Update device statistics.
	 */
	dev_stats->tx_host++;

	ip_local_out(&init_net, NULL, skb);
	eip_ipsec_sa_deref(sa);
	return;

consume:
	dev_stats->tx_fail++;
	eip_ipsec_sa_deref(sa);
	skb_dump(KERN_DEBUG, skb, false);
	consume_skb(skb);
}

#if defined(EIP_IPSEC_HYBRID)

/*
 * eip_ipsec_dev_vp_dummy()
 *	VP callback after PPE lookup.
 */
bool eip_ipsec_dev_vp_dummy(struct net_device *ndev, struct sk_buff *skb, void *cb_data)
{
	pr_warn("%px: Destination VP exception; Drop SKB\n", ndev);
	consume_skb(skb);
	return true;
}

/*
 * eip_ipsec_dev_vp_cb()
 *	VP callback for outer exception.
 */
bool eip_ipsec_dev_vp_cb(struct net_device *ndev, struct sk_buff *skb, void *cb_data)
{
	struct eip_ipsec_dev_stats *dev_stats;
	struct eip_ipsec_dev *eid = cb_data;
	struct eip_ipsec_sa *sa;

	dev_stats = this_cpu_ptr(eid->stats_pcpu);

	/*
	 * First SA in the device encapsulation head is always selected.
	 */
	sa = eip_ipsec_sa_ref_get_encap(ndev);
	if (!sa) {
		pr_debug("%px: Failed to find a valid SA for encapsulation\n", ndev);
		dev_stats->tx_fail_vp_sa++;
		consume_skb(skb);
		return true;
	}

	dev_stats->tx_vp_exp++;
	sa->cb(sa, skb);

	eip_ipsec_sa_deref(sa);
	return true;
}

/*
 * eip_ipsec_dev_vp_alloc()
 *	Allocate VP for IPsec netdevice.
 */
static ppe_vp_num_t eip_ipsec_dev_alloc_vp(struct net_device *ndev)
{
	struct ppe_drv_iface *iface;
	struct ppe_vp_ai vpai = {0};
	ppe_vp_num_t vp_num;

	/*
	 * Allocate new VP
	 */
	vpai.type = PPE_VP_TYPE_SW_L3;
	vpai.dst_cb = eip_ipsec_dev_vp_dummy;
	vpai.dst_cb_data = netdev_priv(ndev);
	vpai.src_cb = eip_ipsec_dev_vp_cb;
	vpai.src_cb_data = netdev_priv(ndev);
	vpai.queue_num = 0;

	vp_num = ppe_vp_alloc(ndev, &vpai);
	if (vp_num < 0) {
		pr_debug("%px: Failed to allocate VP, status(%d).\n", ndev, vpai.status);
		return -1;
	}

	iface = ppe_drv_iface_get_by_dev(ndev);
	if (!iface) {
		ppe_vp_free(vp_num);
		pr_debug("%px: Failed to find iface, status(%d).\n", ndev, vpai.status);
		return -1;
	}

	if (ppe_drv_iface_eip_set(iface, PPE_DRV_EIP_SERVICE_IIPSEC, PPE_DRV_EIP_FEATURE_MTU_DISABLE) != PPE_DRV_RET_SUCCESS) {
		ppe_vp_free(vp_num);
		pr_debug("%px: Failed set feature on iface)\n", iface);
		return -1;
	}

	return vp_num;
}

#endif

/*
 * eip_ipsec_dev_tx()
 *	Encapsulates plaintext packet.
 */
static netdev_tx_t eip_ipsec_dev_tx(struct sk_buff *skb, struct net_device *ndev)
{
	struct eip_ipsec_dev *eid = netdev_priv(ndev);
	struct eip_ipsec_dev_stats *dev_stats;
	struct eip_ipsec_sa_stats *sa_stats;
	struct eip_ipsec_sa *sa;
	unsigned int len;
	bool expand_skb;
	int error;

	dev_stats = this_cpu_ptr(eid->stats_pcpu);
	len = skb->len;

	/*
	 * Unshare the SKB as we will be modifying it.
	 */
	if (unlikely(skb_shared(skb))) {
		skb = skb_unshare(skb, GFP_NOWAIT | __GFP_NOWARN);
		if (!skb) {
			dev_stats->tx_fail++;
			return NETDEV_TX_OK;
		}
	}

	/*
	 * First SA in the device encapsulation head is always selected.
	 */
	sa = eip_ipsec_sa_ref_get_encap(ndev);
	if (!sa) {
		pr_debug("%px: Failed to find a valid SA for encapsulation\n", ndev);
		dev_stats->tx_fail_sa++;
		goto sa_fail;
	}

	sa_stats = this_cpu_ptr(sa->stats_pcpu);

	expand_skb = (skb_cloned(skb) || (skb_headroom(skb) < sa->head_room) || (skb_tailroom(skb) < sa->tail_room));
	if (expand_skb && pskb_expand_head(skb, sa->head_room, sa->tail_room, GFP_NOWAIT | __GFP_NOWARN)) {
		pr_debug("%px: Failed to expand SKB with headroom(%u) tailroom(%u)\n",
				ndev, skb_headroom(skb), skb_tailroom(skb));
		sa_stats->fail_expand++;
		goto fail;
	}

	error = eip_tr_ipsec_enc(sa->tr, skb);
	if (unlikely(error < 0)) {
		/*
		 * TODO: We need to reschedule packet during congestion.
		 */
		sa_stats->fail_enqueue++;
		goto fail;
	}

	/*
	 * Update statistics.
	 * TODO: Move this to done and Update this using VP callback.
	 */
	sa_stats->tx_pkts++;
	sa_stats->tx_bytes += len;
	dev_stats->tx_pkts++;
	dev_stats->tx_bytes += len;

	eip_ipsec_sa_deref(sa);
	return NETDEV_TX_OK;

fail:
	eip_ipsec_sa_deref(sa);
sa_fail:
	dev_stats->tx_fail++;
	consume_skb(skb);
	return NETDEV_TX_OK;
}

/*
 * eip_ipsec_dev_free()
 *	Free netdevice memory.
 */
static void eip_ipsec_dev_free(struct net_device *ndev)
{
	struct eip_ipsec_drv *drv = &eip_ipsec_drv_g;
	struct eip_ipsec_dev *eid = netdev_priv(ndev);

	/*
	 * We take netdevice reference for each entry in list.
	 * So, List must be empty at this point.
	 */
	WARN_ON(!list_empty(&eid->enc_sa));
	WARN_ON(!list_empty(&eid->dec_sa));

	if (eid->stats_pcpu) {
		free_percpu(eid->stats_pcpu);
	}

	if (eid->dentry) {
		debugfs_remove_recursive(eid->dentry);
	}

	free_netdev(ndev);
	eip_ipsec_drv_deref(drv);
	pr_info("%px: IPsec device freed\n", ndev);
}

/*
 * IPsec device callbacks.
 */
static const struct net_device_ops ipsec_dev_ops = {
	.ndo_open = eip_ipsec_dev_open,
	.ndo_stop = eip_ipsec_dev_stop,
	.ndo_start_xmit = eip_ipsec_dev_tx,
	.ndo_get_stats64 = eip_ipsec_dev_stats64,
	.ndo_change_mtu = eip_ipsec_dev_mtu,
};

/*
 * IPsec stats callback.
 */
const struct file_operations eip_ipsec_dev_file_ops = {
	.open = simple_open,
	.llseek = default_llseek,
	.read = eip_ipsec_dev_read,
};

/*
 * eip_ipsec_dev_setup()
 *	Setup ipsec connection device.
 */
static void eip_ipsec_dev_setup(struct net_device *ndev)
{
	ndev->addr_len = ETH_ALEN;
	ndev->mtu = ETH_DATA_LEN - EIP_IPSEC_DEV_MAX_HDR_LEN;

	ndev->hard_header_len = EIP_IPSEC_DEV_MAX_HDR_LEN;
	ndev->needed_headroom = EIP_IPSEC_DEV_MAX_HEADROOM; /* TODO: DMA extends towards tail; What is right way? */
	ndev->needed_tailroom = EIP_IPSEC_DEV_MAX_TAILROOM;
	ndev->type = EIP_IPSEC_DEV_ARPHRD;
	ndev->ethtool_ops = NULL; /* TODO:  Can we use ethtool interface for device statistics */
	ndev->header_ops = NULL;
	ndev->netdev_ops = &ipsec_dev_ops;
	ndev->priv_destructor = eip_ipsec_dev_free;

	/*
	 * Assign random ethernet address.
	 */
	random_ether_addr(ndev->dev_addr);
	memset(ndev->broadcast, 0xff, ndev->addr_len);
	memcpy(ndev->perm_addr, ndev->dev_addr, ndev->addr_len);
}

/*
 * eip_ipsec_dev_get_by_id()
 *	Fetch the net_device from the db for the given ID.
 */
struct net_device *eip_ipsec_dev_get_by_id(int64_t devid)
{
	struct eip_ipsec_drv *drv = &eip_ipsec_drv_g;
	struct eip_ipsec_dev *eid;

	spin_lock_bh(&drv->lock);
	list_for_each_entry(eid, &drv->dev_head, node) {
		if ((eid->devid == devid) && ((!list_empty(&eid->enc_sa)) || (!list_empty(&eid->dec_sa)))) {
			spin_unlock_bh(&drv->lock);
			return eid->ndev;
		}
	}

	spin_unlock_bh(&drv->lock);
	return NULL;
}

/*
 * eip_ipsec_dev_unlink_id()
 *	Delete the net_device entry from the db.
 */
void eip_ipsec_dev_unlink_id(struct net_device *ndev, int64_t devid)
{
	struct eip_ipsec_drv *drv = &eip_ipsec_drv_g;
	struct eip_ipsec_dev *eid = netdev_priv(ndev);

	/*
	 * devid should be >=0 prior to
	 * entering this API.
	 */
	BUG_ON(eid->devid < 0);

	spin_lock_bh(&drv->lock);
	list_del(&eid->node);
	dev_put(ndev);
	eid->devid = -1;
	spin_unlock_bh(&drv->lock);
}

/*
 * eip_ipsec_dev_link_id()
 *	Add the net_device entry into the db.
 */
void eip_ipsec_dev_link_id(struct net_device *ndev, int64_t devid)
{
	struct eip_ipsec_drv *drv = &eip_ipsec_drv_g;
	struct eip_ipsec_dev *eid = netdev_priv(ndev);

	/*
	 * devid should be -1 prior to
	 * entering this API.
	 */
	BUG_ON(eid->devid >= 0);

	spin_lock_bh(&drv->lock);
	eid->devid = devid;
	list_add(&eid->node, &drv->dev_head);
	dev_hold(ndev);
	spin_unlock_bh(&drv->lock);
}

/*
 * eip_ipsec_dev_sa_exist()
 *	Checks whether encap/decap SA's are currently mapped to a given net_device.
 */
bool eip_ipsec_dev_sa_exist(struct net_device *ndev)
{
	struct eip_ipsec_dev *eid = netdev_priv(ndev);
	struct eip_ipsec_drv *drv = &eip_ipsec_drv_g;

	spin_lock_bh(&drv->lock);
	if ((!list_empty(&eid->enc_sa)) || (!list_empty(&eid->dec_sa))) {
		spin_unlock_bh(&drv->lock);
		return true;
	}

	spin_unlock_bh(&drv->lock);
	return false;
}

/*
 * eip_ipsec_dev_del()
 *	Delete IPsec device associated with the netdevice.
 */
void eip_ipsec_dev_del(struct net_device *ndev)
{
	struct eip_ipsec_dev *eid = netdev_priv(ndev);

	/*
	 * the given net_device should be unlinked from the db
	 * before deletion.
	 */
	BUG_ON(eid->devid >= 0);

	/*
	 * User is require to remove SA first.
	 * TODO: Can we flush all?
	 */
	if (!list_empty(&eid->enc_sa) || !list_empty(&eid->dec_sa)) {
		pr_warn("%px: Device is being deleted with active SA\n", ndev);
	}

#if defined(EIP_IPSEC_HYBRID)
	if (eid->vp_num >= 0)
		ppe_vp_free(eid->vp_num);
#endif

	/*
	 * Bring down the device and unregister from linux.
	 */
	if (rtnl_is_locked()) {
		dev_close(ndev);
		unregister_netdevice(ndev);
	} else {
		rtnl_lock();
		dev_close(ndev);
		unregister_netdevice(ndev);
		rtnl_unlock();
	}
}
EXPORT_SYMBOL(eip_ipsec_dev_del);

/*
 * eip_ipsec_dev_add()
 *	Create a IPsec device for a new connection.
 */
struct net_device *eip_ipsec_dev_add(void)
{
	struct eip_ipsec_drv *drv = &eip_ipsec_drv_g;
	struct eip_ipsec_dev *eid;
	struct net_device *ndev;
	bool rlock = false;
	int status;

	/*
	 * Netdevice allocation.
	 */
	ndev = alloc_netdev(sizeof(*eid), "ipsectun%d", NET_NAME_ENUM, eip_ipsec_dev_setup);
	if (!ndev) {
		pr_err("%px: Failed to allocate IPsec device\n", drv);
		return NULL;
	}

	/*
	 * Initialize device private structure.
	 */
	eid = netdev_priv(ndev);
	eid->ndev = ndev;
	eid->devid = -1;

	INIT_LIST_HEAD(&eid->enc_sa);
	INIT_LIST_HEAD(&eid->dec_sa);

	/*
	 * dereference: ndev->priv_destructor
	 */
	eip_ipsec_drv_ref(drv);

	eid->stats_pcpu = alloc_percpu_gfp(struct eip_ipsec_dev_stats, GFP_KERNEL | __GFP_ZERO);
	if (!eid->stats_pcpu) {
		pr_err("%px: Failed to allocate stats memory for encap\n", ndev);
		ndev->priv_destructor(ndev);
		return NULL;
	}

	if (!rtnl_is_locked()) {
		rtnl_lock();
		rlock = true;
	}

	/*
	 * Register netdevice with kernel.
	 * kernels invoke the destructor upon failure
	 */
	status = register_netdevice(ndev);
	if (status < 0) {
		pr_err("%px: Failed to register netdevce, error(%d)\n", ndev, status);
		return NULL;
	}

	/*
	 * Set netdevice to UP state.
	 */
	status = dev_open(ndev, NULL);
	if (status < 0) {
		pr_err("%px: Failed to Open netdevce, error(%d)\n", ndev, status);
		unregister_netdevice(ndev);
		if (rlock)
			rtnl_unlock();

		return NULL;
	}

	if (rlock)
		rtnl_unlock();

#if defined(EIP_IPSEC_HYBRID)
	/*
	 * Device will be only used after SA addition; So there wont be any race condition.
	 * Allocate VP. On Failure IPsec outer flow will be via SFE.
	 * TODO: After Encapsulation, Packet will exception from PPE and IPsec callback has to be called.
	 * shell we create default VP for handling those cases?
	 */
	eid->vp_num = eip_ipsec_dev_alloc_vp(ndev);
	if (eid->vp_num < 0) {
		pr_warn("%px: Failed to allocate VP(%d).\n", ndev, eid->vp_num);
	}
#endif

	eid->dentry = debugfs_create_file(ndev->name, S_IRUGO, drv->dentry, eid, &eip_ipsec_dev_file_ops);
	return ndev;
}
EXPORT_SYMBOL(eip_ipsec_dev_add);
