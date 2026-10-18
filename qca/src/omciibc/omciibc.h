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

#ifndef __OMCIIBC_H__
#define __OMCIIBC_H__

#define OMCIIBC_IFP_NAME_WAN		"omciibc"
#define OMCIIBC_DEF_DEV_NAME		"eth4"
#define OMCIIBC_DEF_VID			0x888
#define OMCIIBC_DEF_ETHTYPE		0x88e4
#define OMCIIBC_DEF_PBIT		0x7
#define	OMCIIBC_PROC_NAME_ROOT		"omciibc"
#define TX_TIMEOUT			(6*HZ)

#define OMCIIBC_NETDEV_TX_OK		0	// skb consumed by callee
#define OMCIIBC_NETDEV_TX_DROP		1	// skb consumed by callee, but dropped

#define OMCIIBC_PBIT_MAX		7
#define OMCIIBC_INTPRI_MAX		15


#ifndef VLAN_ETH_ALEN
#define VLAN_ETH_ALEN 6
#endif

static inline char *
purename(char *name)
{
	char *s;
	for (s = name + strlen(name) - 1; s >= name; s--)
		if (*s == '/')
			return s + 1;
	return name;
}

#define dbprintk(fmt, args...)		printk(KERN_ERR "%s:%d:%s(): " fmt, purename(__FILE__), __LINE__, __FUNCTION__, ##args)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#include <net/net_namespace.h>
#define dev_get_byname(ifname)		dev_get_by_name(&init_net, ifname)
#define dev_get_byindex(ifindex)	dev_get_by_index(&init_net, ifindex)
#else
#define dev_get_byname(ifname)		dev_get_by_name(ifname)
#define dev_get_byindex(ifindex)	dev_get_by_index(ifindex)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
#define netdev_priv(dev)	((dev)->priv)
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
#define netdev_mc_count(dev)	((dev)->mc_count)
#endif

struct omciibc_priv {
	struct net_device *real_dev;
	unsigned int debug_rx:2;
	unsigned int debug_tx:2;
	unsigned int pass_chk:2;
	unsigned int silent_drop_if_qstop:1;
	struct net_device_stats stats;
	unsigned long last_rx;	// jiffies, replace the one in dev->last_rx
	unsigned long last_rx_bytes;	// last_* used for rate calc in /proc/trex_ifomci
	unsigned long last_rx_packets;
	unsigned long last_tx_bytes;
	unsigned long last_tx_packets;
};

#ifndef OMCIIBC_SWHOOK_ENABLE
struct omciibc_info_t {
	// tx/rx hook with linux
	struct net_device *omciibc_dev;	// dev for ibc msg send
	struct packet_type omciibc_packet_type;	// datastru for dev_add_pack for ibc msg recv
};
#endif

extern int (*omciibc_net_recv) (struct sk_buff ** pskb);
extern int (*omciibc_port_state_check) (struct net_device * dev, int updown);

#endif
