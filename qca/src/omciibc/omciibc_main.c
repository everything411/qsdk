/******************************************************************
 *
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
 *
 ******************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/netfilter.h>
#include <linux/unistd.h>

#include <linux/delay.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/list.h>

#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/if_arp.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <net/switchdev.h>

#include "omciibc.h"

static char *omciibc_devname = "eth4";
module_param(omciibc_devname, charp, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(omciibc_devname, "the device for link real inf");

static unsigned int omciibc_vid = OMCIIBC_DEF_VID;
module_param(omciibc_vid, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(omciibc_vid, "vid for check ibc");

static unsigned int omciibc_pri = OMCIIBC_DEF_PBIT;
module_param(omciibc_pri, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(omciibc_pri, "priority for check ibc");

static unsigned int omciibc_ethtype = OMCIIBC_DEF_ETHTYPE;
module_param(omciibc_ethtype, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(omciibc_ethtype, "ethertype for check ibc");

static struct net_device *g_omci_dev;
static struct proc_dir_entry *omciibc_pde;

#ifndef OMCIIBC_SWHOOK_ENABLE
static struct omciibc_info_t __omciibc_info_g;
struct omciibc_info_t *omciibc_info_g = &__omciibc_info_g;
#endif

#define OMCIIBC_MACLEN        12

enum omciibc_state_chk_e {
	OMCI_CHECK_EVENT_NONE = 0,	// node not replied
	OMCI_CHECK_EVENT_DOWN_DONE,	// node missing
	OMCI_CHECK_EVENT_UP_DONE
};

enum omciibc_link_state_e {
	OMCI_LINK_DOWN_EVENT = 0,	// node not replied
	OMCI_LINK_UP_EVENT,		// node missing
	OMCI_LINK_NONE_EVENT
};

static unsigned int g_omci_check_state = OMCI_CHECK_EVENT_NONE;
static unsigned int g_omciibc_init_ready = 0;

//////////////////////////////////////////////////////////////
static void
print_mem(unsigned char *p, int pkt_len, int print_len)
{
	char line[128];
	char *s = line;
	int i;

	line[0] = 0;
	for (i = 0; i < print_len; i++) {
		char c = (pkt_len == 0 || i < pkt_len) ? ' ' : '-';

		if (i % 16 == 0)
			s += scnprintf(s, sizeof(line)-(s-line),"%04x", i);
		s += scnprintf(s, sizeof(line)-(s-line),"%c%02x", c, *p);
		p++;
		if (i % 8 == 7)
			s += scnprintf(s,sizeof(line)-(s-line), "%c", c);
		if (i % 16 == 15) {
			printk("%s\n", line);
			line[0] = 0;
			s = line;
		}
	}
	if (i % 16 != 0)
		printk("%s\n", line);

}

static int
omciibc_handle_frame(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	u16 vlan_id = 0;
	struct omciibc_priv *priv = NULL;

	/* Packets from dev_loopback_xmit() do not have L2 header, bail out */
	if (unlikely(skb == NULL)) {
		dbprintk("skb is NULL!!\r\n");
		return -1;
	}

	if (unlikely(skb->pkt_type == PACKET_LOOPBACK))
		return -1;

	if(!g_omciibc_init_ready)
		return -1;

	priv = (struct omciibc_priv *)netdev_priv(g_omci_dev);

	if (priv == NULL) {
		dbprintk("error priv data for omciibc\r\n");
		return -1;
	}

	if (skb->dev != priv->real_dev) {
		return -1;
	}

	if (!skb_vlan_tag_present(skb))
		return -1;

	vlan_id = skb_vlan_tag_get_id(skb);

	if (priv->debug_rx) {
		dbprintk("%s all: skb %p, len=%d, tx %lu, j=%lu, skbpro=0x%04x-0x%04x\n",
			 skb->dev->name, skb, skb->len, priv->stats.rx_packets, jiffies, ntohs(skb->protocol), vlan_id);
	}

	if (ntohs(skb->protocol) != omciibc_ethtype || vlan_id != omciibc_vid)
		return -1;

	if (priv->debug_rx) {
		dbprintk("%s: skb %p, len=%d, tx %lu, j=%lu, skbpro=0x%04x-0x%04x\n",
			 skb->dev->name, skb, skb->len, priv->stats.rx_packets, jiffies, ntohs(skb->protocol), vlan_id);
		if (priv->debug_rx >= 2)
			print_mem(skb->data, skb->len, skb->len);
	}
	///is omciibc_ethtype && omciibc_vid
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
	__vlan_hwaccel_clear_tag(skb);
#else
	skb->vlan_tci = 0;
#endif
	skb->dev = g_omci_dev;

	priv->stats.rx_packets++;
	priv->stats.rx_bytes += skb->len;
	if (priv->debug_rx) {
		dbprintk("%s all: skb %p, len=%d, tx %lu, j=%lu, skbpro=0x%04x-0x%04x\n",
			 skb->dev->name, skb, skb->len, priv->stats.rx_packets, jiffies, ntohs(skb->protocol), vlan_id);
	}

	return 0;
}

#ifndef OMCIIBC_SWHOOK_ENABLE
static int
omciibc_packet_recv(struct sk_buff *skb, struct net_device *dev, struct packet_type *ptype, struct net_device *orig_dev)
{
	// This handle hook was added by dev_all_pack that ptype is all
	// skb was be checked if skb is 0x88e4 and vid matched.
	// If yes,skb->dev will be chnaged as omciibc dev,
	// then go back to __netif_receive_skb_core flow.
	// otherwise keep original skb.
	// Finally , if callee is from deliver_skb ,users counter was increased by 1.
	// So need to do consume here.

	omciibc_handle_frame(&skb);
	// if last one handle , consume_skb will free skb
	// otherwise consume_skb will decrease skb->users.
	consume_skb(skb);

	return 0;
}

// omciibc_register_device_and_ethertype
static int
omciibc_register_device_and_ethertype(char *omciibc_devname, unsigned short omciibc_ethertype)
{
	struct net_device *omciibc_dev = NULL;

	// locate trex_dev
	if (omciibc_devname == NULL) {
		dbprintk("module cfg 'omciibc_devname' not specified\n");
		return -1;
	}

	if ((omciibc_dev = dev_get_byname(omciibc_devname)) == NULL) {
		dbprintk("dev %s doesnt exist\n", omciibc_devname);
		return -1;
	}
#if 0
	if ((omciibc_dev->flags & IFF_UP) == 0) {
		trex_util_os_dev_put(omciibc_dev);
		dbprintk("dev %s is not UP\n", omciibc_dev);
		return -1;
	}
#endif

	omciibc_info_g->omciibc_dev = omciibc_dev;

	omciibc_info_g->omciibc_packet_type.type = cpu_to_be16(ETH_P_ALL);

	omciibc_info_g->omciibc_packet_type.func = omciibc_packet_recv;
	omciibc_info_g->omciibc_packet_type.dev = omciibc_dev;
	// turn on ignore_outgoing , ignore tx path for PTYPE_ALL
	omciibc_info_g->omciibc_packet_type.ignore_outgoing = true;
	dev_add_pack(&omciibc_info_g->omciibc_packet_type);

#if 0
	dbprintk("\nomciibc register, dev=%s, type=0x%02x\n",
		 omciibc_info_g->omciibc_dev->name, ntohs(omciibc_info_g->omciibc_packet_type.type));
#endif

	return 0;
}

static void
omciibc_unregister_device_and_ethertype(void)
{
	if (omciibc_info_g->omciibc_dev != NULL) {
		dev_put(omciibc_info_g->omciibc_dev);
		omciibc_info_g->omciibc_dev = NULL;
	}

	if (omciibc_info_g->omciibc_packet_type.func != NULL) {
		dev_remove_pack(&omciibc_info_g->omciibc_packet_type);
		omciibc_info_g->omciibc_packet_type.func = NULL;
	}
}
#endif

#if 0
static struct sk_buff *
omciibc_vlan_tag_add(struct sk_buff *skb, unsigned short tag)
{
	struct vlan_ethhdr *veth;

	if (*(unsigned short *)(skb->data + 12) == __constant_htons(ETH_P_8021Q))
		return skb;

	if (skb_headroom(skb) < VLAN_HLEN) {
		struct sk_buff *sk_tmp = skb;
		skb = skb_realloc_headroom(sk_tmp, VLAN_HLEN);
		kfree_skb(sk_tmp);
		if (!skb) {
			dbprintk("vlan_add_tag: failed to realloc headroom\n");
			return NULL;
		}
	} else {
		skb = skb_unshare(skb, GFP_ATOMIC);
		if (!skb) {
			dbprintk("vlan_add_tag: failed to unshare skbuff\n");
			return NULL;
		}
	}

	veth = (struct vlan_ethhdr *)skb_push(skb, VLAN_HLEN);

	/* Move the mac addresses to the beginning of the new header. */
	memmove(skb->data, skb->data + VLAN_HLEN, 2 * VLAN_ETH_ALEN);

	/* first, the ethernet type */
	veth->h_vlan_proto = __constant_htons(ETH_P_8021Q);

	/* now, the tag */
	veth->h_vlan_TCI = htons(tag);

	return skb;
}
#endif

int
str2argv(char *str, char *argv[], int argv_size)
{
	static char buff[1024];
	char *p = buff;
	int i = 0;

	strlcpy(buff, str, sizeof(buff));
	while (p != NULL) {
		argv[i] = strsep(&p, " \t");
		if (strlen(argv[i]) > 0) {
			i++;
			if (i == argv_size - 1)
				break;
		}
	}
	argv[i] = 0;
	return i;
}

int
str2val(char *s)
{
	int val;
	if (s[0] == 'x') {
		sscanf(s + 1, "%x", &val);
	} else if (s[0] == '0' && s[1] == 'x') {
		sscanf(s + 2, "%x", &val);
	} else {
		sscanf(s, "%d", &val);
	}
	return val;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)

u16
omciibc_packet_pick_tx_queue(struct sk_buff * skb)
{
	struct net_device *dev = skb->dev;
	const struct net_device_ops *ops = dev->netdev_ops;
	int cpu = raw_smp_processor_id();
	u16 queue_index;

#ifdef CONFIG_XPS
	skb->sender_cpu = cpu + 1;
#endif
	skb_record_rx_queue(skb, cpu % dev->real_num_tx_queues);
	if (ops->ndo_select_queue) {
		queue_index = ops->ndo_select_queue(dev, skb, NULL);
		queue_index = netdev_cap_txqueue(dev, queue_index);
	} else {
		queue_index = netdev_pick_tx(dev, skb, NULL);
	}

	return queue_index;
}

int
omciibc_dev_direct_xmit(struct sk_buff *skb, u16 queue_id, unsigned int enable_chk, unsigned int silent_drop_if_qstop)
{
	struct net_device *dev = skb->dev;
	struct sk_buff *orig_skb = skb;
	struct netdev_queue *txq;
	int ret = NETDEV_TX_BUSY;
	bool again = false;
	int is_xmit_frozen = 0;

	if (enable_chk) {
		if (unlikely(!netif_running(dev) || !netif_carrier_ok(dev)))
			goto drop;
	}

	skb = validate_xmit_skb_list(skb, dev, &again);
	if(skb == NULL){
		dbprintk("XMIT skb is null\n");
		return NET_XMIT_DROP;
	}
	if (skb != orig_skb)
		goto drop;

	if (omciibc_pri <= OMCIIBC_INTPRI_MAX) {
		struct omciibc_priv *priv = netdev_priv(g_omci_dev);
		skb_set_int_pri(skb, (uint8_t)(omciibc_pri));
		if (priv->debug_tx) {
			u16 vlan_tci=0;
			__vlan_get_tag(skb, &vlan_tci);
			dbprintk("%s: vlan_tci=0x%x skb->int_pri=%d (cfg_int_pri=%d)\n",
				  priv->real_dev->name, vlan_tci, skb_get_int_pri(skb), omciibc_pri);
			if (priv->debug_tx >= 3)
				print_mem(skb->data, skb->len, skb->len);
		}
	}

	skb_set_queue_mapping(skb, queue_id);
	txq = skb_get_tx_queue(dev, skb);

	local_bh_disable();

	dev_xmit_recursion_inc();
	HARD_TX_LOCK(dev, txq, smp_processor_id());
	if (!netif_xmit_frozen_or_drv_stopped(txq)) {
		ret = netdev_start_xmit(skb, dev, txq, false);
	} else {
		is_xmit_frozen = 1;
	}
	HARD_TX_UNLOCK(dev, txq);
	dev_xmit_recursion_dec();

	local_bh_enable();

	if (!dev_xmit_complete(ret))
		kfree_skb(skb);
	if (silent_drop_if_qstop) {
		if (is_xmit_frozen)
			ret = NET_XMIT_SUCCESS;
	}
	return ret;

drop:
	dev->stats.tx_dropped++;
	kfree_skb_list(skb);
	return NET_XMIT_DROP;
}
#else
u16
omciibc_packet_pick_tx_queue(struct sk_buff * skb)
{
	return 0;
}

int
omciibc_dev_direct_xmit(struct sk_buff *skb, u16 queue_id, unsigned int enable_chk, unsigned int silent_drop_if_qstop)
{
	struct net_device *dev = skb->dev;
	struct sk_buff *orig_skb = skb;
	struct netdev_queue *txq;
	int ret = NETDEV_TX_BUSY;
	int is_xmit_frozen = 0;

	if (enable_chk) {
		if (unlikely(!netif_running(dev) || !netif_carrier_ok(dev)))
			goto drop;
	}

	txq = netdev_pick_tx(dev, skb, NULL);

	skb = validate_xmit_skb_list(skb, dev);
	if(skb == NULL){
		dbprintk("XMIT skb is null\n");
		return NET_XMIT_DROP;
	}
	if (skb != orig_skb)
		goto drop;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
	if (omciibc_pri <= OMCIIBC_INTPRI_MAX) {
		struct omciibc_priv *priv = netdev_priv(g_omci_dev);
		skb->int_pri = (uint8_t)(omciibc_pri);
		if (priv->debug_tx) {
			u16 vlan_tci=0;
			__vlan_get_tag(skb, &vlan_tci);
			dbprintk("%s: vlan_tci=0x%x skb->int_pri=%d (cfg_int_pri=%d)\n",
				  priv->real_dev->name, vlan_tci, skb_get_int_pri(skb), omciibc_pri);
			if (priv->debug_tx >= 3)
				print_mem(skb->data, skb->len, skb->len);
		}
	}
#endif

	local_bh_disable();

	__this_cpu_inc(xmit_recursion);
	HARD_TX_LOCK(dev, txq, smp_processor_id());
	if (!netif_xmit_frozen_or_drv_stopped(txq)) {
		ret = netdev_start_xmit(skb, dev, txq, false);
	} else {
		is_xmit_frozen = 1;
	}
	HARD_TX_UNLOCK(dev, txq);
	__this_cpu_dec(xmit_recursion);

	local_bh_enable();

	if (!dev_xmit_complete(ret))
		kfree_skb(skb);
	if (silent_drop_if_qstop) {
		if (is_xmit_frozen)
			ret = NET_XMIT_SUCCESS;
	}
	return ret;

drop:
	dev->stats.tx_dropped++;
	kfree_skb_list(skb);
	return NET_XMIT_DROP;
}

#endif
int
omciibc_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct omciibc_priv *priv = (struct omciibc_priv *)netdev_priv(dev);
	u16 vlan_tci=0;
	//struct vlan_ethhdr *veth = (struct vlan_ethhdr *)(skb->data);
	//u16 queue_id = 0;
	int skb_len;
	u16 *p16;

	if (skb == NULL)
		return OMCIIBC_NETDEV_TX_DROP;

	if(!g_omciibc_init_ready){
		dbprintk("omciibc_init not ready drop\n");
		dev_kfree_skb_any(skb);
		priv->stats.tx_dropped++;
		return OMCIIBC_NETDEV_TX_DROP;
	}

	if (dev == NULL) {	// should not happen, just in case other module calls hard_xmit directly without proper dev specfied, eg:dgasp module
		dev_kfree_skb_any(skb);
		priv->stats.tx_dropped++;
		return OMCIIBC_NETDEV_TX_DROP;
	}

	//check if omciibc ethtype
	p16 = skb->data + OMCIIBC_MACLEN;

	if (ntohs(*p16) != omciibc_ethtype ){
		if (priv->debug_tx)
			dbprintk("%s: skb %p, len=%d, tx_drop by ethtype err %lu, j=%lu\n",
				dev->name, skb, skb->len, priv->stats.tx_dropped, jiffies);
		if (priv->debug_tx >=2)
			print_mem(skb->data, skb->len, skb->len);
		priv->stats.tx_dropped++;
		dev_kfree_skb_any(skb);
		return OMCIIBC_NETDEV_TX_DROP;
	}

	vlan_tci = ((u16) omciibc_vid) | (OMCIIBC_DEF_PBIT << VLAN_PRIO_SHIFT);

	__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vlan_tci);

	if (priv->debug_tx) {
		dbprintk("%s: skb %p, len=%d, tx %lu, j=%lu, xid=0x%04x-0x%04x\n",
			 dev->name, skb, skb->len, priv->stats.tx_packets, jiffies, skb->vlan_proto, vlan_tci);
		if (priv->debug_tx >= 2)
			print_mem(skb->data, skb->len, skb->len);
	}

	skb_len = skb->len;
	skb->dev = priv->real_dev;

#if 0
	//dbprintk("%s: skb %p, len=%d, tx %lu, j=%lu, pack=0x%04x-0x%04x\n", 
	//              skb->dev->name, skb, skb->len, priv->stats.tx_packets, jiffies,  skb->vlan_proto, vlan_tci);

	dev_queue_xmit(skb);
#else

	{
		int ret = NETDEV_TX_BUSY;
		//dbprintk("%s: skb %p, len=%d, tx %lu, j=%lu, xid=%02x%02x\n", 
		//      skb->dev->name, skb, skb->len, priv->stats.tx_packets, jiffies, skb->data[0], skb->data[1]);
		skb_reset_mac_header(skb);
		if ((ret = omciibc_dev_direct_xmit(skb, omciibc_packet_pick_tx_queue(skb), priv->pass_chk, priv->silent_drop_if_qstop)) != NETDEV_TX_OK) {
			priv->stats.tx_dropped++;
			return OMCIIBC_NETDEV_TX_DROP;
		}
	}
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)
	dev->trans_start = jiffies;
#else
	netif_trans_update(dev);
#endif
	priv->stats.tx_packets++;
	priv->stats.tx_bytes += skb_len;
	return OMCIIBC_NETDEV_TX_OK;
}

int
omciibc_dev_state_check(struct net_device *dev, int updown)
{
	struct omciibc_priv *priv = (struct omciibc_priv *)netdev_priv(g_omci_dev);
	int ret = 0;

	if (priv->pass_chk)
		return -1;

	if (priv->debug_tx) {
		printk("omciibc in=%p ,cur=%p, inanme=%s, curname=%s, pass_chk=%d\n", 
			dev, priv->real_dev, dev->name, priv->real_dev->name, priv->pass_chk);
		printk("omciibc(%s) chk state=%d-%d\n", dev->name, g_omci_check_state, updown);
	}

	if (priv->real_dev != dev)
		return -1;

#if 1
	if (netif_carrier_ok(dev)) {
		ret = 0;
		g_omci_check_state = OMCI_CHECK_EVENT_UP_DONE;
	} else {
		switch (g_omci_check_state) {
		case OMCI_CHECK_EVENT_NONE:
			ret = -1;
			if (updown == OMCI_LINK_DOWN_EVENT)	//down
				g_omci_check_state = OMCI_CHECK_EVENT_DOWN_DONE;
			else if (updown == OMCI_LINK_UP_EVENT)
				g_omci_check_state = OMCI_CHECK_EVENT_UP_DONE;
			else
				ret = 0;
			break;	/*non --> one */
		case OMCI_CHECK_EVENT_DOWN_DONE:
			if (updown == OMCI_LINK_UP_EVENT) {	//up
				ret = -1;
				g_omci_check_state = OMCI_CHECK_EVENT_UP_DONE;
			} else if (updown == OMCI_LINK_DOWN_EVENT) {
				ret = 0;
				g_omci_check_state = OMCI_CHECK_EVENT_DOWN_DONE;
			} else {
				ret = 0;
			}
			break;
		case OMCI_CHECK_EVENT_UP_DONE:
			ret = 0;
			break;
		default:
			ret = -1;
			printk("omciibc(%s) chk unknow state=%d\n", dev->name, g_omci_check_state);
			break;
		}
	}
#else
	switch (g_omci_check_state) {
	case OMCI_CHECK_EVENT_NONE:
		ret = -1;
		if (updown == OMCI_LINK_DOWN_EVENT)	//down
			g_omci_check_state = OMCI_CHECK_EVENT_DOWN_DONE;
		else if (updown == OMCI_LINK_UP_EVENT)
			g_omci_check_state = OMCI_CHECK_EVENT_UP_DONE;
		else
			ret = 0;
		break;		/*non --> one */
	case OMCI_CHECK_EVENT_DOWN_DONE:
		if (updown == OMCI_LINK_UP_EVENT) {	//up
			ret = -1;
			g_omci_check_state = OMCI_CHECK_EVENT_UP_DONE;
		} else if (updown == OMCI_LINK_DOWN_EVENT) {
			ret = 0;
			g_omci_check_state = OMCI_CHECK_EVENT_DOWN_DONE;
		} else {
			ret = 0;
		}
		break;
	case OMCI_CHECK_EVENT_UP_DONE:
		ret = 0;
		break;
	default:
		ret = -1;
		printk("omciibc(%s) chk unknow state=%d\n", dev->name, g_omci_check_state);
		break;
	}
#endif

	if (priv->debug_tx) {
		printk("after omciibc(%s) chk state=%d-%d-%d\n", dev->name, g_omci_check_state, updown, ret);
	}

	return ret;
}

// misc function used by dev operation function /////////////////////////////////////////////////////////////
static void
omciibc_stop_rxtx(struct net_device *dev)
{
}

static void
omciibc_start_rxtx(struct net_device *dev)
{
}

static void
omciibc_set_macaddr(struct net_device *dev)
{
}

static int
omciibc_open(struct net_device *dev)
{
	try_module_get(THIS_MODULE);

	netif_start_queue(dev);

	return 0;
}

static int
omciibc_close(struct net_device *dev)
{
	netif_stop_queue(dev);

	module_put(THIS_MODULE);
	return 0;
}

static struct net_device_stats *
omciibc_get_stats(struct net_device *dev)
{
	struct omciibc_priv *priv = (struct omciibc_priv *)netdev_priv(dev);

	return &priv->stats;
}

static void
omciibc_set_rx_mode(struct net_device *dev)
{
}

static int
omciibc_change_mtu(struct net_device *dev, int new_mtu)
{
	return -EOPNOTSUPP;
}

static void
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
omciibc_tx_timeout(struct net_device *dev, unsigned int txqueue)
#else
omciibc_tx_timeout(struct net_device *dev)
#endif
{
	struct omciibc_priv *priv = (struct omciibc_priv *)netdev_priv(dev);

	// trigger an immediate transmit demand
	omciibc_stop_rxtx(dev);
	udelay(5);
	omciibc_start_rxtx(dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)
	dev->trans_start = jiffies;
#else
	netif_trans_update(dev);
#endif
	priv->stats.tx_errors++;
	if (priv->debug_tx)
		printk("%s: tx timeout! tx_errors=%lu\n", dev->name, priv->stats.tx_errors);

	netif_wake_queue(dev);
}

static int
omciibc_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	return -EOPNOTSUPP;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
static const struct net_device_ops netdev_ops = {
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_open = omciibc_open,
	.ndo_stop = omciibc_close,
	.ndo_start_xmit = omciibc_start_xmit,
	.ndo_get_stats = omciibc_get_stats,
	.ndo_set_rx_mode = omciibc_set_rx_mode,
	.ndo_tx_timeout = omciibc_tx_timeout,
	.ndo_change_mtu = omciibc_change_mtu,
	.ndo_do_ioctl = omciibc_ioctl,
};
#endif

static void *__init
omciibc_probe_one(char *omciibc_name, unsigned char *macaddr)
{
	struct net_device *dev;
	struct omciibc_priv *priv;

	// alloc_etherdev ensures aligned and zeroed private structures
	if ((dev = alloc_etherdev(sizeof(*priv))) == NULL) {
		dbprintk("%s: alloc_etherdev() failed.\n", OMCIIBC_IFP_NAME_WAN);
		return NULL;
	}
	// assign interface name
	dev->name[0] = 0;	/* ask system to assign name for this ethernet */
	strlcpy(dev->name, omciibc_name, IFNAMSIZ - 1);	// eg: "trex_ifomci%d"
	dev->name[IFNAMSIZ - 1] = 0;

	// assign mac addr
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,17,0)
	memcpy(dev->dev_addr, macaddr, 6);
#else
	eth_hw_addr_set(dev, macaddr);
#endif
	// set entries in the device structure
	ether_setup(dev);	// general defaults
	dev->base_addr = 0;
	dev->irq = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
	dev->netdev_ops = &netdev_ops;
#else
	dev->open = omciibc_open dev->stop = omciibc_close;
	dev->hard_start_xmit = omciibc_start_xmit;
	dev->get_stats = omciibc_get_stats;
	dev->do_ioctl = omciibc_ioctl;
	dev->change_mtu = omciibc_change_mtu;
	dev->set_multicast_list = omciibc_set_rx_mode;
	dev->tx_timeout = omciibc_tx_timeout;
#endif
	dev->watchdog_timeo = TX_TIMEOUT;
	dev->tx_queue_len = 0;
	dev->flags |= IFF_NOARP;
#ifdef NETIF_F_CSUM_MASK
	dev->features &= ~(NETIF_F_CSUM_MASK);	// linux 4 or later
#else
	dev->features &= ~(NETIF_F_ALL_CSUM);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	dev->header_ops = NULL;
#else
	dev->hard_header_cache = NULL;
#endif

	// set entries in priv stru
	priv = (struct omciibc_priv *)netdev_priv(dev);
	memset(priv, 0, sizeof(struct omciibc_priv));	// clear priv
	priv->debug_tx = 0;
	priv->debug_rx = 0;
	priv->pass_chk = 0;
	priv->silent_drop_if_qstop = 1;

	// add interface to linux
	if (register_netdev(dev)) {
		dbprintk("%s: register_netdev() failed!\n", OMCIIBC_IFP_NAME_WAN);
		kfree(dev);
		return NULL;
	}
	// disable hw port
	omciibc_stop_rxtx(dev);
	// config macaddr to hw
	omciibc_set_macaddr(dev);

	// printk("%s: registered_netdev() as %s.\n", TREX_IFOMCI_NAME, dev->name);
	return dev;
}

static int
__omciibc_cmd_process(char *command)
{
	int argc;
	char *argv[32];

	argc = str2argv(command, argv, 32);
	if (argc == 0)
		return 0;

	if (argc == 2 && strcmp(argv[0], "debug_rx") == 0) {
		int debug_rx = str2val(argv[1]) & 3;
		struct omciibc_priv *priv = netdev_priv(g_omci_dev);
		priv->debug_rx = debug_rx;
		return 1;
	} else if (argc == 2 && strcmp(argv[0], "debug_tx") == 0) {
		int debug_tx = str2val(argv[1]) & 3;
		struct omciibc_priv *priv = netdev_priv(g_omci_dev);
		priv->debug_tx = debug_tx;
		return 1;
	} else if (argc == 1 && strcmp(argv[0], "reset_counter") == 0) {
		struct omciibc_priv *priv = netdev_priv(g_omci_dev);
		memset(&priv->stats, 0, sizeof(struct net_device_stats));
		priv->last_rx_bytes = priv->last_tx_bytes = 0;
		priv->last_rx_packets = priv->last_tx_packets = 0;
		return 1;
	} else if (argc == 2 && strcmp(argv[0], "pass_chk") == 0) {
		int pass_chk = str2val(argv[1]) & 3;
		struct omciibc_priv *priv = netdev_priv(g_omci_dev);
		priv->pass_chk = pass_chk;
		return 1;
	} else if (argc == 2 && strcmp(argv[0], "silent_drop_if_qstop") == 0) {
		int value = str2val(argv[1]) & 1;
		struct omciibc_priv *priv = netdev_priv(g_omci_dev);
		priv->silent_drop_if_qstop = value;
		return 1;
	}

	printk("%s: '%s' cmd error\n", OMCIIBC_PROC_NAME_ROOT, command);
	printk("syntax:\n"
	       "reset_counter\n"
	       "debug_tx [0..3] (0:disable, 1:enable, 2:pktdump)\n"
	       "debug_rx [0..3] (0:disable, 1:enable, 2:pktdump)\n"
	       "pass_chk [0|1]  (0:disable, 1:enable)\n"
	       "silent_drop_if_qstop [0|1]\n");
	return 0;
}

static ssize_t
omciibc_proc_write(struct file *file, const char *buffer, size_t count, loff_t * data)
{
	static char cmd[1024];
	if (count > 0 && count < 1024) {
		if (copy_from_user(cmd, buffer, count))
			return -EFAULT;
		cmd[count - 1] = '\0';
		__omciibc_cmd_process(cmd);
	}
	return count;
}

static int
omciibc_proc_read(struct seq_file *seq, void *v)
{
	struct omciibc_priv *priv = netdev_priv(g_omci_dev);

	seq_printf(seq, "%s-%s(%c): debug_tx=%d, debug_rx=%d\n"
			"pass_chk=%d, state_chk=%d, silent_drop_if_qstop=%u\n",
		   priv->real_dev->name, g_omci_dev->name, (g_omci_dev->flags & IFF_UP) ? '+' : '-', 
		   priv->debug_tx, priv->debug_rx, 
		   priv->pass_chk, g_omci_check_state,
		   priv->silent_drop_if_qstop);
	seq_printf(seq, "rx byte:%lu pkt:%lu drop:%lu", priv->stats.rx_bytes, priv->stats.rx_packets, priv->stats.rx_dropped);

	seq_printf(seq, "\n");

	seq_printf(seq, "tx byte:%lu pkt:%lu drop:%lu", priv->stats.tx_bytes, priv->stats.tx_packets, priv->stats.tx_dropped);
	seq_printf(seq, "\n");
	seq_printf(seq, "omciibc version : V1.5");
	seq_printf(seq, "\n");

	return 0;
}

static int
omciibc_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, omciibc_proc_read, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static struct proc_ops
 omciibc_proc_fop = {
	.proc_open = omciibc_proc_open,
	.proc_write = omciibc_proc_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static struct file_operations
 omciibc_proc_fop = {
	.open = omciibc_proc_open,
	.write = omciibc_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int
omciibc_proc_init(void)
{
	omciibc_pde = proc_create(OMCIIBC_PROC_NAME_ROOT, S_IWUSR | S_IRUGO, NULL, &omciibc_proc_fop);
	if (!omciibc_pde)
		return -ENOMEM;

	return 0;
}

void
omciibc_proc_cleanup(void)
{
	remove_proc_entry(OMCIIBC_PROC_NAME_ROOT, NULL);
}

static void
omciibc_cleanup(void)
{
	struct omciibc_priv *priv;

#ifndef OMCIIBC_SWHOOK_ENABLE
	omciibc_unregister_device_and_ethertype();
#else
	RCU_INIT_POINTER(omciibc_net_recv, NULL);
	RCU_INIT_POINTER(omciibc_port_state_check, NULL);
#endif

	omciibc_proc_cleanup();

#if 0
	rtnl_lock();
	if (lowdev != NULL) {
		netdev_rx_handler_unregister(lowdev);
	}
	rtnl_unlock();
#endif

	priv = (struct omciibc_priv *)netdev_priv(g_omci_dev);
	dev_put(priv->real_dev);
	if (g_omci_dev) {
		omciibc_stop_rxtx(g_omci_dev);
		unregister_netdev(g_omci_dev);
		//kfree(trex_ifhost_dev_lan);
		free_netdev(g_omci_dev);
	}

	printk("unload %s module\n", OMCIIBC_IFP_NAME_WAN);
}

static int __init
omciibc_init(void)
{
	struct net_device *lowdev;
	unsigned char macaddr_default[6] = { 0 };
	struct omciibc_priv *priv;

	// init macaddr_default
	macaddr_default[2] = 0x95;	// 03 c1 -> omci
	macaddr_default[3] = 0x74;
	macaddr_default[5] = 0x04;

	// register trex_ifomci/iftrap interface
	//get_random_bytes(&macaddr_default[3], 3);     // randomize lower 3 bytes of macaddr
	g_omci_dev = omciibc_probe_one(OMCIIBC_IFP_NAME_WAN, macaddr_default);
	if (g_omci_dev == NULL) {
		return -ENODEV;
	}

	if ((lowdev = dev_get_byname(omciibc_devname)) == NULL) {
		printk("can't find lowdev %s\n", omciibc_devname);
		omciibc_stop_rxtx(g_omci_dev);
		unregister_netdev(g_omci_dev);
		//kfree(trex_ifhost_dev_lan);
		free_netdev(g_omci_dev);
		return -ENODEV;
	}

	dev_put(lowdev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,17,0)
	printk("%s dev flag=0x%x\r\n", lowdev->name, lowdev->priv_flags);
#else
	printk("%s dev flag=0x%llx\r\n", lowdev->name, lowdev->priv_flags);
#endif
	omciibc_proc_init();
	priv = (struct omciibc_priv *)netdev_priv(g_omci_dev);
	priv->real_dev = lowdev;
	dev_hold(lowdev);
#if 0
	rtnl_lock();
	lowdev->priv_flags &= ~IFF_NO_RX_HANDLER;
	lowdev->priv_flags |= IFF_L3MDEV_MASTER;

	err = netdev_rx_handler_register(lowdev, omciibc_handle_frame, NULL);
	rtnl_unlock();
	printk("after dev flag=0x%x\r\n", lowdev->priv_flags);
	if (err)
		printk("register %s rx handel func err=%d\n", OMCIIBC_IFP_NAME_WAN, err);
#endif

#ifndef OMCIIBC_SWHOOK_ENABLE

	memset(omciibc_info_g, 0x0, sizeof(struct omciibc_info_t));
	if (omciibc_register_device_and_ethertype(omciibc_devname, omciibc_ethtype) != 0) {
		printk("omciibc register ethertype pack fail!!!!\n");
		omciibc_proc_cleanup();
		dev_put(priv->real_dev);
		unregister_netdev(g_omci_dev);
		free_netdev(g_omci_dev);
		return -ENODEV;
	}
#else
	BUG_ON(omciibc_net_recv);
	RCU_INIT_POINTER(omciibc_net_recv, omciibc_handle_frame);

	BUG_ON(omciibc_port_state_check);
	RCU_INIT_POINTER(omciibc_port_state_check, omciibc_dev_state_check);
#endif

#ifdef OMCIIBC_SWHOOK_ENABLE
	printk("load %s module\n", OMCIIBC_IFP_NAME_WAN);
#else
	printk("load %s module without SWHOOK\n", OMCIIBC_IFP_NAME_WAN);
#endif
	g_omciibc_init_ready = 1;
	return 0;
}

module_init(omciibc_init);
module_exit(omciibc_cleanup);

MODULE_AUTHOR("TREX Software");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("omciibc ifnet driver");
