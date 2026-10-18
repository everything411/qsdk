/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
*/
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/netfilter_ipv4.h>
#include <linux/uaccess.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/sysctl.h>
#include <net/net_namespace.h>

#include "sdc.h"

struct sdc_ctx sdc_ctx;

/*
 * Input String length for tracked interface commands.
 */
#define SDC_TRACKED_IF_STR_LEN 64

/*
 * Input String length for MAC database commands.
 */
#define SDC_MACDB_STR_LEN 64

/*
 * Input String for tracked interface commands.
 */
static char sdc_tracked_if_info[SDC_TRACKED_IF_STR_LEN];

/*
 * Input String for MAC database commands.
 */
static char sdc_macdb_info[SDC_MACDB_STR_LEN];

/*
 * Print Ethernet header.
 */
static void __maybe_unused sdc_print_eth_header(const struct ethhdr *eth)
{
	if (!eth) {
		DEBUG_ERR("Ethernet header is NULL\n");
		return;
	}

	DEBUG_INFO("Ethernet Header:\n");
	DEBUG_INFO("  Destination MAC: %pM\n", eth->h_dest);
	DEBUG_INFO("  Source MAC     : %pM\n", eth->h_source);
	DEBUG_INFO("  Protocol       : 0x%04x\n", ntohs(eth->h_proto));
}

/*
 * Print IP header.
 */
static void __maybe_unused sdc_print_ip_header(const struct iphdr *iph)
{
        if (!iph) {
                DEBUG_ERR("[IP_HDR][%s:%d] NULL IP header pointer\n", __func__, __LINE__);
                return;
        }

        DEBUG_INFO("[IP_HDR][%s:%d] Src IP: %pI4, Dst IP: %pI4, Proto: %u, Total Len: %u\n",
                __func__, __LINE__,
                &iph->saddr,
                &iph->daddr,
                iph->protocol,
                ntohs(iph->tot_len));
}

/*
 * Print UDP header.
 */
static void __maybe_unused sdc_print_udp_header(const struct udphdr *udph)
{
        if (!udph) {
                DEBUG_ERR("[UDP_HDR][%s:%d] NULL UDP header pointer\n", __func__, __LINE__);
                return;
        }

        DEBUG_INFO("[UDP_HDR][%s:%d] Src Port: %u, Dst Port: %u, Length: %u\n",
                __func__, __LINE__,
                ntohs(udph->source),
                ntohs(udph->dest),
                ntohs(udph->len));
}

/*
 * sdc_parse_eth_ip
 *	Parse and validate Ethernet + IPv4 headers from skb.
 *	Returns false if packet should be skipped (non-IPv4, pull failure).
 */
static bool sdc_parse_eth_ip(struct sk_buff *skb, struct ethhdr **eh,
			      struct iphdr **ih, uint8_t *ipv4_hdr_sz)
{
	*eh = (struct ethhdr *)eth_hdr(skb);
	if (!*eh || (*eh)->h_proto != htons(ETH_P_IP))
		return false;

	if (!pskb_may_pull(skb, sizeof(struct iphdr)))
		return false;

	/*
	 * pskb_may_pull() can reallocate skb->data (pskb_expand_head()),
	 * invalidating pointers captured before the call. Recompute eh
	 * against the current linear buffer before returning it.
	 */
	*eh = (struct ethhdr *)eth_hdr(skb);
	*ih = (struct iphdr *)(skb->data);
	*ipv4_hdr_sz = (*ih)->ihl * 4;
	return true;
}

/*
 * sdc_parse_udp
 *	Parse UDP header and fill pkt_info with L4 payload pointer and length.
 *	Returns false if payload cannot be pulled.
 */
static bool sdc_parse_udp(struct sk_buff *skb, struct iphdr *ih,
			   uint8_t ipv4_hdr_sz,
			   struct sdc_features_ext_pkt_info *info,
			   uint16_t *sport, uint16_t *dport)
{
	struct udphdr *uh;
	struct ethhdr *eh;
	uint16_t udp_len;

	if (!pskb_may_pull(skb, ipv4_hdr_sz + sizeof(struct udphdr))) {
		DEBUG_ERR("cant pull udp\n");
		return false;
	}

	uh      = (struct udphdr *)(skb->data + ipv4_hdr_sz);
	udp_len = ntohs(uh->len);

	if (!pskb_may_pull(skb, ipv4_hdr_sz + udp_len)) {
		DEBUG_ERR("cant pull udp payload\n");
		return false;
	}

	/*
	 * The pull above can reallocate skb->data; recompute both header
	 * pointers against the current linear buffer before use.
	 */
	uh = (struct udphdr *)(skb->data + ipv4_hdr_sz);
	eh = (struct ethhdr *)eth_hdr(skb);

	*sport           = ntohs(uh->source);
	*dport           = ntohs(uh->dest);
	info->smac       = eh->h_source;
	info->l4_payload = (uint8_t *)uh + sizeof(struct udphdr);
	info->l4_len     = udp_len - sizeof(struct udphdr);
	return true;
}

/*
 * Ingress Netfilter hook function registered in kernel.
 */
static unsigned int sdc_nf_ingress_hook(void *priv, struct sk_buff *skb,
					const struct nf_hook_state *state)
{
	struct sdc_features_ext_pkt_info info = {0};
	uint16_t sport, dport;
	uint8_t ipv4_hdr_sz;
	uint8_t tcp_hdr_sz;
	struct ethhdr *eh;
	struct iphdr *ih;

	if (!sdc_parse_eth_ip(skb, &eh, &ih, &ipv4_hdr_sz))
		return NF_ACCEPT;

	if (ih->protocol == IPPROTO_UDP) {
		if (!sdc_parse_udp(skb, ih, ipv4_hdr_sz,
				   &info, &sport, &dport))
			return NF_ACCEPT;

		if (sport == SDC_FEATURES_EXT_DHCP_SPORT || sport == SDC_FEATURES_EXT_DHCP_CPORT ||
		    dport == SDC_FEATURES_EXT_DHCP_SPORT || dport == SDC_FEATURES_EXT_DHCP_CPORT) {
			sdc_features_ext_dhcp(&sdc_ctx, skb, &info);
			return NF_ACCEPT;
		}

		/* TODO: Maybe check for multicast destination ip as well to be safe. */
		if (sport == SDC_FEATURES_EXT_MDNS_PORT || dport == SDC_FEATURES_EXT_MDNS_PORT) {
			sdc_features_ext_dns(&sdc_ctx, skb, &info, true);
			return NF_ACCEPT;
		}

		if (sport == SDC_FEATURES_EXT_DNS_PORT || dport == SDC_FEATURES_EXT_DNS_PORT) {
			sdc_features_ext_dns(&sdc_ctx, skb, &info, false);
			return NF_ACCEPT;
		}

		if (sport == SDC_FEATURES_EXT_SSDP_PORT || dport == SDC_FEATURES_EXT_SSDP_PORT) {
			sdc_features_ext_ssdp(&sdc_ctx, skb, &info);
			return NF_ACCEPT;
		}

		/* QUIC uses UDP port 443 — distinct from TLS which is TCP port 443. */
		if (sport == SDC_FEATURES_EXT_QUIC_PORT || dport == SDC_FEATURES_EXT_QUIC_PORT) {
			sdc_features_ext_quic(&sdc_ctx, skb, &info);
			return NF_ACCEPT;
		}

	} else if (ih->protocol == IPPROTO_TCP) {
		struct tcphdr *th;
		uint16_t tot_len;

		if (!pskb_may_pull(skb, ipv4_hdr_sz + sizeof(struct tcphdr)))
			return NF_ACCEPT;

		/*
		 * pskb_may_pull() above can reallocate skb->data; recompute
		 * ih/th against the current linear buffer before use.
		 */
		ih = (struct iphdr *)(skb->data);
		th = (struct tcphdr *)(skb->data + ipv4_hdr_sz);
		tot_len = ntohs(ih->tot_len);

		/* SYN/FIN/RST carry no TLS payload — skip before port check. */
		if (th->syn || th->fin || th->rst)
			return NF_ACCEPT;

		sport = ntohs(th->source);
		dport = ntohs(th->dest);

		/*
		 * TLS ports: 443 (HTTPS), 8443 (alt HTTPS), 8883 (MQTT/TLS),
		 * 5223 (Apple APNS), 5228 (Google FCM), 853 (DNS over TLS).
		 */
		static const uint16_t sdc_tls_ports[] = {
			443, 8443, 8883, 5223, 5228, 853, 0
		};
		for (int i = 0; sdc_tls_ports[i]; i++) {
			if (sport == sdc_tls_ports[i] || dport == sdc_tls_ports[i]) {
				tcp_hdr_sz  = th->doff * 4;
				info.l4_len = tot_len - ipv4_hdr_sz - tcp_hdr_sz;
				if (info.l4_len > 0 &&
				    pskb_may_pull(skb, ipv4_hdr_sz + tcp_hdr_sz + info.l4_len)) {
					/*
					 * This pull can also reallocate skb->data;
					 * recompute eh/th before deriving pointers
					 * so info references live memory.
					 */
					eh = (struct ethhdr *)eth_hdr(skb);
					th = (struct tcphdr *)(skb->data + ipv4_hdr_sz);
					info.smac       = eh->h_source;
					info.l4_payload = (uint8_t *)th + tcp_hdr_sz;
					sdc_features_ext_tls(&sdc_ctx, skb, &info);
				}
				break;
			}
		}
	}

	return NF_ACCEPT;
}

/*
 * Egress Netfilter hook function — captures server-to-client frames
 * (DHCP Offer/ACK/NAK and DNS responses) and updates per-device counters.
 * Device is looked up by chaddr (DHCP) or destination MAC (DNS).
 * Anomaly counters in sdc_ctx track lookup misses for observability.
 */
static unsigned int sdc_nf_egress_hook(void *priv, struct sk_buff *skb,
					const struct nf_hook_state *state)
{
	struct sdc_features_ext_pkt_info info = {0};
	uint16_t sport, dport;
	uint8_t ipv4_hdr_sz;
	struct ethhdr *eh;
	struct iphdr *ih;

	if (!sdc_parse_eth_ip(skb, &eh, &ih, &ipv4_hdr_sz))
		return NF_ACCEPT;

	if (ih->protocol != IPPROTO_UDP)
		return NF_ACCEPT;

	if (!sdc_parse_udp(skb, ih, ipv4_hdr_sz,
			   &info, &sport, &dport))
		return NF_ACCEPT;

	if (sport == SDC_FEATURES_EXT_DHCP_SPORT || dport == SDC_FEATURES_EXT_DHCP_CPORT) {
		sdc_features_ext_dhcp_egress(&sdc_ctx, skb, &info);
		return NF_ACCEPT;
	}

	if (sport == SDC_FEATURES_EXT_DNS_PORT) {
		/*
		 * sdc_parse_udp() above can reallocate skb->data via its
		 * internal pskb_may_pull() calls; recompute eh before
		 * dereferencing it.
		 */
		eh = (struct ethhdr *)eth_hdr(skb);
		sdc_features_ext_dns_egress(&sdc_ctx, skb, &info, eh->h_dest);
		return NF_ACCEPT;
	}

	return NF_ACCEPT;
}

/*
 * Function to add a new interface to track.
 */
int sdc_add_tracked_if_by_name(const char *ifname)
{
	struct sdc_tracked_if *tif = NULL;
	struct nf_hook_ops *ops = NULL;
	struct net_device *dev = NULL;
	int ret = 0;
	int i  = 0;

	dev = dev_get_by_name(&init_net, ifname);
	if (!dev) {
		DEBUG_ERR("Device not found %s\n", ifname);
		return -EINVAL;
	}

	if (sdc_ctx.num_devs >= SDC_MAX_IF) {
		DEBUG_ERR("Reached max limit\n");
		ret = -ENOSPC;
		goto done;
	}

	for (i = 0; i < SDC_MAX_IF; i++) {
		if (sdc_ctx.tif[i] && sdc_ctx.tif[i]->dev == dev) {
			ret = -EEXIST;
			DEBUG_ERR("Already tracking this device %s\n", ifname);
			goto done;
		}
	}

	/*
	 * Find the first free index and add the device.
	 */
	for (i = 0; i < SDC_MAX_IF; i++) {
		if (!sdc_ctx.tif[i]) {
			tif = kzalloc(sizeof(struct sdc_tracked_if), GFP_KERNEL);
			if (!tif) {
				DEBUG_ERR("Failed to allocate memory for tracking entry\n");
				ret = -ENOMEM;
				goto done;
			}

			tif->dev = dev;
			/*
			 * Configure the netfilter hook ops and add to the hook.
			 */
			ops = &tif->nf_ingress_hook;
			ops->hook = sdc_nf_ingress_hook;
			ops->pf = NFPROTO_NETDEV;
			ops->hooknum = NF_NETDEV_INGRESS;

			/*
			 * TODO:Confirm if this priority work for all the cases with
			 * qdisc folks.
			 */
			ops->priority = NF_IP_PRI_FIRST;
			ops->dev = dev;

			if (nf_register_net_hook(&init_net, ops)) {
				DEBUG_ERR("Failed to register net hook\n");
				kfree(tif);
				ret = -EINVAL;
				goto done;
			}

			ops = &tif->nf_egress_hook;
			ops->hook = sdc_nf_egress_hook;
			ops->pf = NFPROTO_NETDEV;
			ops->hooknum = NF_NETDEV_EGRESS;
			ops->priority = NF_IP_PRI_FIRST;
			ops->dev = dev;

			if (nf_register_net_hook(&init_net, ops)) {
				DEBUG_ERR("Failed to register egress net hook\n");
				nf_unregister_net_hook(&init_net, &tif->nf_ingress_hook);
				kfree(tif);
				ret = -EINVAL;
				goto done;
			}

			DEBUG_INFO("%px is the netdevice\n", tif->dev);
			/*
			 * Update the global entry.
			 */
			sdc_ctx.tif[i] = tif;
			sdc_ctx.num_devs++;

			return ret;
		}
	}
done:
	dev_put(dev);
	return ret;
}

/*
 * Function to delete a tracked interface.
 */
static void sdc_del_tracked_if(struct sdc_tracked_if *tif)
{
	if (!tif) {
		return;
	}

	nf_unregister_net_hook(&init_net, &tif->nf_ingress_hook);
	nf_unregister_net_hook(&init_net, &tif->nf_egress_hook);
	DEBUG_INFO("%px is the netdevice for delete\n", tif->dev);
	dev_put(tif->dev);
	kfree(tif);
}

/*
 * Function to delete a tracked interface via its name.
 */
void sdc_del_tracked_if_by_name(const char *ifname)
{
	struct sdc_tracked_if *tif = NULL;
	int i = 0;

	/*
	 * Check if we are tracking this device and unregister the hook.
	 */
	for (i = 0; i < SDC_MAX_IF; i++) {
		tif = sdc_ctx.tif[i];
		if (tif && (!strncmp(tif->dev->name, ifname, IFNAMSIZ))) {
			sdc_del_tracked_if(tif);
			sdc_ctx.tif[i] = NULL;
			sdc_ctx.num_devs--;
			DEBUG_INFO("Removed dev %s from tracked list\n", ifname);
			break;
		}
	}
}

/*
 * Function to clear all tracked interface.
 */
void sdc_clear_tracked_if(void)
{
	struct sdc_tracked_if *tif = NULL;
	int i = 0;

	for (i = 0; i < SDC_MAX_IF; i++) {
		tif = sdc_ctx.tif[i];
		if (tif) {
			sdc_del_tracked_if(tif);
			sdc_ctx.tif[i] = NULL;
			sdc_ctx.num_devs--;
		}
	}
}

/*
 * sdc_tracked_if_handler
 *	Sysctl handler for tracked interfaces.
 *
 * Read: Display all tracked interfaces
 * Write: Process commands (add/del/clear)
 */
static int sdc_tracked_if_handler(struct ctl_table *table, int write,
				  void __user *buffer, size_t *lenp, loff_t *ppos)
{
	struct sdc_tracked_if *tif = NULL;
	char *work_str;
	char *token;
	int ret;
	int i;

	/*
	 * Find the string, return an error if not found
	 */
	ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret) {
		return ret;
	}

	if (!write) {
		/*
		 * Handle read operation - display all tracked interfaces
		 */
		for (i = 0; i < SDC_MAX_IF; i++) {
			tif = sdc_ctx.tif[i];
			if (tif) {
				DEBUG_INFO("Tracked interface: %s\n", tif->dev->name);
			}
		}
		*lenp = 0;
		return ret;
	}

	/*
	 * Handle write operation - parse and execute command
	 */
	DEBUG_INFO("Input String: %s\n", sdc_tracked_if_info);

	work_str = sdc_tracked_if_info;
	token = strsep(&work_str, " ");

	if (!token) {
		DEBUG_ERR("Invalid input. Usage: echo 'add <ifname>' or 'del <ifname>' or 'clear' > /proc/sys/net/sdc/tracked_ifaces\n");
		memset(sdc_tracked_if_info, 0, sizeof(sdc_tracked_if_info));
		return -EINVAL;
	}

	if (!strncmp(token, "clear", 5)) {
		sdc_clear_tracked_if();
		DEBUG_INFO("Cleared all tracked interfaces\n");
	} else if (!strncmp(token, "add", 3)) {
		token = strsep(&work_str, " ");
		if (!token) {
			DEBUG_ERR("Missing interface name. Usage: echo 'add <ifname>' > /proc/sys/net/sdc/tracked_ifaces\n");
			memset(sdc_tracked_if_info, 0, sizeof(sdc_tracked_if_info));
			return -EINVAL;
		}

		ret = sdc_add_tracked_if_by_name(token);
		if (ret) {
			DEBUG_ERR("Failed to track interface %s. Error: %d\n", token, ret);
			memset(sdc_tracked_if_info, 0, sizeof(sdc_tracked_if_info));
			return ret;
		}
		DEBUG_INFO("Added interface %s to tracked list\n", token);
	} else if (!strncmp(token, "del", 3)) {
		token = strsep(&work_str, " ");
		if (!token) {
			DEBUG_ERR("Missing interface name. Usage: echo 'del <ifname>' > /proc/sys/net/sdc/tracked_ifaces\n");
			memset(sdc_tracked_if_info, 0, sizeof(sdc_tracked_if_info));
			return -EINVAL;
		}

		sdc_del_tracked_if_by_name(token);
		DEBUG_INFO("Deleted interface %s from tracked list\n", token);
	} else {
		DEBUG_ERR("Invalid command '%s'. Usage: echo 'add <ifname>' or 'del <ifname>' or 'clear' > /proc/sys/net/sdc/tracked_ifaces\n", token);
		memset(sdc_tracked_if_info, 0, sizeof(sdc_tracked_if_info));
		return -EINVAL;
	}

	memset(sdc_tracked_if_info, 0, sizeof(sdc_tracked_if_info));
	return ret;
}

/*
 * sdc_macdb_handler
 *	Sysctl handler for MAC database control.
 *
 * Read: Display all MAC addresses with their states
 * Write: Process commands (clear/del)
 */
static int sdc_macdb_handler(struct ctl_table *table, int write,
			      void __user *buffer, size_t *lenp, loff_t *ppos)
{
	struct sdc_macdb_ctx *ctx = &sdc_ctx.macdb_ctx;
	struct sdc_macdb_entry *entry;
	char *work_str;
	char *token;
	uint8_t mac[ETH_ALEN];
	int bkt;
	int count = 0;
	int ret;

	/*
	 * Find the string, return an error if not found
	 */
	ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret) {
		return ret;
	}

	if (!write) {
		/*
		 * Handle read operation - display all MAC entries
		 */
		spin_lock_bh(&sdc_ctx.lock);
		hash_for_each(ctx->htable, bkt, entry, hnode) {
			DEBUG_INFO("MAC: %pM  State: %s\n",
					entry->mac,
					entry->state == SDC_MACDB_MAC_STATE_INPROGRESS ?
					"INPROGRESS" : "COLLECTED");
			count++;
		}
		spin_unlock_bh(&sdc_ctx.lock);
		DEBUG_INFO("Total MAC entries: %d\n", count);
		*lenp = 0;
		return ret;
	}

	/*
	 * Handle write operation - parse and execute command
	 */
	DEBUG_INFO("Input String: %s\n", sdc_macdb_info);

	work_str = sdc_macdb_info;
	token = strsep(&work_str, " ");

	if (!token) {
		DEBUG_ERR("Invalid input. Usage: echo 'clear' or 'del <MAC>' > /proc/sys/net/sdc/macdb\n");
		memset(sdc_macdb_info, 0, sizeof(sdc_macdb_info));
		return -EINVAL;
	}

	if (!strncmp(token, "clear", 5)) {
		sdc_macdb_clear_collected(ctx);
		DEBUG_INFO("Cleared all COLLECTED MAC entries\n");
	} else if (!strncmp(token, "del", 3)) {
		token = strsep(&work_str, " ");
		if (!token) {
			DEBUG_ERR("Missing MAC address. Usage: echo 'del <MAC>' > /proc/sys/net/sdc/macdb\n");
			memset(sdc_macdb_info, 0, sizeof(sdc_macdb_info));
			return -EINVAL;
		}

		if (!mac_pton(token, mac)) {
			DEBUG_ERR("Invalid MAC address format: %s. Usage: echo 'del <MAC>' > /proc/sys/net/sdc/macdb\n", token);
			memset(sdc_macdb_info, 0, sizeof(sdc_macdb_info));
			return -EINVAL;
		}

		ret = sdc_macdb_del_by_mac(ctx, mac);
		if (ret) {
			DEBUG_ERR("Failed to delete MAC %pM. Error: %d\n", mac, ret);
			memset(sdc_macdb_info, 0, sizeof(sdc_macdb_info));
			return ret;
		}
		DEBUG_INFO("Deleted MAC %pM from database\n", mac);
	} else {
		DEBUG_ERR("Invalid command '%s'. Usage: echo 'clear' or 'del <MAC>' > /proc/sys/net/sdc/macdb\n", token);
		memset(sdc_macdb_info, 0, sizeof(sdc_macdb_info));
		return -EINVAL;
	}

	memset(sdc_macdb_info, 0, sizeof(sdc_macdb_info));
	return ret;
}

/*
 * Sysctl min/max bounds for runtime-configurable parameters.
 */
static int sdc_buf_size_min = 512;
static int sdc_buf_size_max = SDC_MACDB_BUF_SZ;
static int sdc_collection_timeout_min = 1;
static int sdc_collection_timeout_max = 300;

/*
 * Read-only sysctl handler for atomic_t counters.
 * Copies the current atomic value to a local int and returns it via proc_dointvec.
 */
static int sdc_proc_atomic_handler(struct ctl_table *table, int write,
				    void *buffer, size_t *lenp, loff_t *ppos)
{
	struct ctl_table tmp = *table;
	int val;

	if (write)
		return -EPERM;

	val = atomic_read((atomic_t *)table->data);
	tmp.data = &val;
	return proc_dointvec(&tmp, write, buffer, lenp, ppos);
}

/*
 * Sysctl table for SDC
 */
static struct ctl_table sdc_table[] = {
	{
		.procname	= "tracked_ifaces",
		.data		= &sdc_tracked_if_info,
		.maxlen		= sizeof(char) * SDC_TRACKED_IF_STR_LEN,
		.mode		= 0644,
		.proc_handler	= sdc_tracked_if_handler,
	},
	{
		.procname	= "macdb",
		.data		= &sdc_macdb_info,
		.maxlen		= sizeof(char) * SDC_MACDB_STR_LEN,
		.mode		= 0644,
		.proc_handler	= sdc_macdb_handler,
	},
	{
		.procname	= "buf_size",
		.data		= &sdc_ctx.buf_size,
		.maxlen		= sizeof(sdc_ctx.buf_size),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &sdc_buf_size_min,
		.extra2		= &sdc_buf_size_max,
	},
	{
		.procname	= "collection_timeout",
		.data		= &sdc_ctx.collection_timeout,
		.maxlen		= sizeof(sdc_ctx.collection_timeout),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &sdc_collection_timeout_min,
		.extra2		= &sdc_collection_timeout_max,
	},
	{
		.procname	= "drop_dhcp",
		.data		= &sdc_ctx.drop_dhcp,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= sdc_proc_atomic_handler,
	},
	{
		.procname	= "drop_dns",
		.data		= &sdc_ctx.drop_dns,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= sdc_proc_atomic_handler,
	},
	{
		.procname	= "drop_mdns",
		.data		= &sdc_ctx.drop_mdns,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= sdc_proc_atomic_handler,
	},
	{
		.procname	= "drop_ssdp",
		.data		= &sdc_ctx.drop_ssdp,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= sdc_proc_atomic_handler,
	},
	{
		.procname	= "drop_tls",
		.data		= &sdc_ctx.drop_tls,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= sdc_proc_atomic_handler,
	},
	{
		.procname	= "drop_quic",
		.data		= &sdc_ctx.drop_quic,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= sdc_proc_atomic_handler,
	},
	{
		.procname	= "egress_dhcp_miss",
		.data		= &sdc_ctx.egress_dhcp_miss,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= sdc_proc_atomic_handler,
	},
	{
		.procname	= "egress_dns_miss",
		.data		= &sdc_ctx.egress_dns_miss,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= sdc_proc_atomic_handler,
	},
	{
		.procname	= "tele_queue_drop",
		.data		= &sdc_ctx.tele_queue_drop,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= sdc_proc_atomic_handler,
	},
	{ }
};

/*
 * SDC init function
 */
static int __init sdc_init(void)
{
	spin_lock_init(&sdc_ctx.lock);
	sdc_ctx.num_devs = 0;
	sdc_ctx.buf_size = SDC_MACDB_BUF_SZ;
	sdc_ctx.collection_timeout = SDC_MACDB_TIMER_TIMEOUT_SECS;
	atomic_set(&sdc_ctx.egress_dhcp_miss, 0);
	atomic_set(&sdc_ctx.egress_dns_miss, 0);
	atomic_set(&sdc_ctx.tele_queue_drop, 0);
	atomic_set(&sdc_ctx.drop_dhcp, 0);
	atomic_set(&sdc_ctx.drop_dns, 0);
	atomic_set(&sdc_ctx.drop_mdns, 0);
	atomic_set(&sdc_ctx.drop_ssdp, 0);
	atomic_set(&sdc_ctx.drop_tls, 0);
	atomic_set(&sdc_ctx.drop_quic, 0);

	/*
	 * Register sysctl table at /proc/sys/net/sdc/
	 */
	sdc_ctx.sysctl_header = register_sysctl("net/sdc", sdc_table);
	if (!sdc_ctx.sysctl_header) {
		DEBUG_ERR("Failed to register sysctl table for SDC\n");
		return -ENOMEM;
	}

	if (sdc_telemetry_init(&sdc_ctx.tele_ctx)) {
		DEBUG_ERR("Failed to initialize telemetry context\n");
		unregister_sysctl_table(sdc_ctx.sysctl_header);
		return -ENOMEM;
	}

	sdc_macdb_init(&sdc_ctx.macdb_ctx);
	DEBUG_INFO("SDC Module Loaded %s\n", SDC_BUILD_ID);
	return 0;
}

/*
 * SDC exit function
 */
static void __exit sdc_exit(void)
{
	struct sdc_tracked_if *tif;

	/*
	 * Unregister sysctl table
	 */
	if (sdc_ctx.sysctl_header) {
		unregister_sysctl_table(sdc_ctx.sysctl_header);
	}

	for (int i = 0; i < SDC_MAX_IF; i++) {
		tif = sdc_ctx.tif[i];
		if (tif) {
			nf_unregister_net_hook(&init_net, &tif->nf_ingress_hook);
			nf_unregister_net_hook(&init_net, &tif->nf_egress_hook);
			dev_put(tif->dev);
		}
	}

	sdc_telemetry_deinit(&sdc_ctx.tele_ctx);
	sdc_macdb_deinit(&sdc_ctx.macdb_ctx);
	DEBUG_INFO("SDC Module unloaded %s\n", SDC_BUILD_ID);
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Smart Device Classification Driver");

module_init(sdc_init);
module_exit(sdc_exit);
