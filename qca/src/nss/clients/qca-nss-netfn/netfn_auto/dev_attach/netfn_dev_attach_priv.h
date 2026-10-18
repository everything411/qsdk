/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NETFN_DEV_ATTACH_PRIV_H
#define __NETFN_DEV_ATTACH_PRIV_H

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/sysctl.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include "netfn_pkt_steer.h"
#include "netfn_capwap_types.h"

/*
 * athrs_fast_nat_recv
 *	External function pointer for fast NAT receive handler.
 */
extern int (*athrs_fast_nat_recv)(struct sk_buff *skb);

#define KEEPALIVE_MAGIC1 0xf3
#define KEEPALIVE_MAGIC2 0x10

/* skb allocation size for keepalive packets sent by netfn_dev_tx_capwap_keepalive() */
#define NETFN_KEEPALIVE_SKB_SIZE 1000

/*
 * netfn_ka_cfg
 *	Keepalive 5-tuple configuration, controlled via sysctl keepalive/ subtable.
 */
struct netfn_ka_cfg {
	char src_ip[16];		/* Source IPv4 address, dotted-decimal */
	char dst_ip[16];		/* Destination IPv4 address, dotted-decimal */
	uint32_t src_port;		/* UDP source port */
	uint32_t dst_port;		/* UDP destination port */
	int send;			/* Write 1 to trigger a keepalive send */
};

/*
 * netfn_dev_attach_priv
 *	Private context for the dev attach module.
 */
struct netfn_dev_attach_priv {
	struct net_device *lan_dev;		/* Ethernet device for LAN traffic */
	struct net_device *capwap_dev;		/* CAPWAP tunnel device */
	struct net_device *wan_dev;		/* WAN device for other traffic */
	struct net_device *test_dev;		/* Test device */
	struct net_device napi_dev;		/* Dummy net_device for NAPI */

	struct netfn_pkt_steer tx_steer;	/* Packet steering context */

	/*
	 * Sysctl configurable parameters
	 */
	char lan_dev_name[IFNAMSIZ];
	char capwap_dev_name[IFNAMSIZ];
	char wan_dev_name[IFNAMSIZ];
	uint32_t to_dev_cpu;
	uint32_t tx_napi_budget;
	uint32_t id;
	int attach;
	int debug;			/* Number of packets to dump */
	int capwap_drop;
	int fast_xmit;
	int steer_en;			/* Enable/disable packet steering */
	uint32_t ip_mask;
	uint32_t ip6_mask;

	struct netfn_capwap_prehdr prehdr;	/* Preheader configuration */

	/*
	 * Keepalive
	 */
	struct netfn_ka_cfg ka_cfg;		/* Keepalive 5-tuple configuration */

	/*
	 * Statistics
	 */
	uint64_t lan_rx_pkts;		/* Packets received on ethernet device */
	uint64_t capwap_rx_pkts;	/* Packets received on CAPWAP device */
	uint64_t tx_pkts;		/* Packets transmitted */
	uint64_t tx_keep_alive;		/* Keepalive packets transmitted */
	uint64_t rx_keep_alive;		/* Keepalive packets received */
	uint64_t dropped;
	uint64_t drop_qfull;
	uint64_t test_dev_tx_pkts;	/* Packets transmitted via test_dev */
	uint64_t test_dev_tx_bytes;	/* Bytes transmitted via test_dev */

	/*
	 * Debugfs and Sysctl
	 */
	struct dentry *dentry;
	struct ctl_table_header *sysctl_header;
	struct ctl_table_header *sysctl_prehdr_header;
	struct ctl_table_header *sysctl_ka_header;
};

#endif /* __NETFN_DEV_ATTACH_PRIV_H */
