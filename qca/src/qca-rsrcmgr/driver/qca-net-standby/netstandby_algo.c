/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

/*
 * netstandby_algo.c
 *	NSS algorithm
 */

#include <linux/kernel.h>
#include <linux/netstandby.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <net/genetlink.h>
#include <net/sock.h>
#include <netstandby_nl_if.h>
#include <netstandby_msg_if.h>
#include "netstandby_main.h"
#include "netstandby_nl_cmn.h"
#include <net/netfilter/nf_conntrack.h>
#include <linux/tick.h>
#include <linux/kernel_stat.h>

#if defined(NSS_TELEMETRY_SUPPORT)
#define NETSTANDBY_DNS_PORT	53
#define NETSTANDBY_MCAST_IPV4_ALLHOST_ADDR   0xe0000001 /* 224.0.0.1 */
#define NETSTANDBY_MCAST_IPV4_SSDP_ADDR	0xeffffffa /* 239.255.255.250 */
#define NETSTANDBY_MCAST_IPV4_DNS_ADDR	0xeffffffb /* 239.255.255.251 */
#define NETSTANDBY_MCAST_IPV6_ALLNODES_ADDR	0x2000100	/* ff02:0000:0000:0000:0000:0000:0001:0002 */
#define NETSTANDBY_MCAST_IPV6_NEIGHDIS_ADDR	0xfb000000	/* ff02:0000:0000:0000:0000:0000:0000:00fb */

int netstandby_nss_telemetry_retry = 0;
struct netstandby_erp_nss_telemetry nss_telemetry;
#define NETSTANDBY_MAX_NSS_TELEMETRY_RETRY	3

/*
 * netstandby_get_idle_time
 * 	Get the idle time of CPU
 */
static u64 netstandby_get_idle_time(struct kernel_cpustat *kcs, int cpu)
{
        u64 idle, idle_usecs = -1ULL;

        if (cpu_online(cpu))
                idle_usecs = get_cpu_idle_time_us(cpu, NULL);

        if (idle_usecs == -1ULL)
                /* !NO_HZ or cpu offline so we can rely on cpustat.idle */
                idle = kcs->cpustat[CPUTIME_IDLE];
        else
                idle = idle_usecs * NSEC_PER_USEC;

        return idle;
}

/*
 * netstandby_is_flow_not_important
 * 	Returns true if we find that the flow could be a data path flow,
 * 	after filtering out routine network management flows.
 */
bool netstandby_is_flow_not_important(struct nf_conn *ct)
{
	struct nf_conntrack_tuple *orig_tuple = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	int sport, dport;

	sport = ntohs(orig_tuple->src.u.all);
	dport = ntohs(orig_tuple->dst.u.all);

	/*
	 * Ignore loopback, broadcast flows. We also ignore multicast flows that are created in
	 * conntrack table for periodic network management
	 */
	if (ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.l3num == NFPROTO_IPV4) {
		if (IN_LOOPBACK(ntohl(orig_tuple->src.u3.ip))) {
			return true;
		} else if (IN_MULTICAST(ntohl(orig_tuple->dst.u3.ip))) {
			netstandby_info("Not imp multi IPv4 dst=%pI4\n", &orig_tuple->dst.u3.ip);
			if (ntohl(orig_tuple->dst.u3.ip) == NETSTANDBY_MCAST_IPV4_ALLHOST_ADDR) {
				return true;
			} else if (ntohl(orig_tuple->dst.u3.ip) == NETSTANDBY_MCAST_IPV4_SSDP_ADDR) {
				return true;
			} else if (ntohl(orig_tuple->dst.u3.ip) == NETSTANDBY_MCAST_IPV4_DNS_ADDR) {
				return true;
			}
		} else if (orig_tuple->dst.u3.ip == INADDR_BROADCAST) {
			return true;
		}
	}

	if (ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.l3num == NFPROTO_IPV6) {
		if (ipv6_addr_is_multicast(&orig_tuple->dst.u3.in6)) {
			netstandby_info("Not imp multicast IPV6 dst=%pI6\n", orig_tuple->dst.u3.ip6);
			if (orig_tuple->dst.u3.in6.s6_addr32[3] == NETSTANDBY_MCAST_IPV6_ALLNODES_ADDR) {
				return true;
			} else if (orig_tuple->dst.u3.in6.s6_addr32[3] == NETSTANDBY_MCAST_IPV6_NEIGHDIS_ADDR) {
				return true;
			}
		} else if (ipv6_addr_loopback(&orig_tuple->src.u3.in6)) {
			return true;
		}
	}

	/*
	 * Ignore DNS flows
	 */
	if (dport == NETSTANDBY_DNS_PORT) {
		return true;
	}

	if (ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.l3num == NFPROTO_IPV4) {
		netstandby_info("IPV4 imp flow : src=%pI4, dst=%pI4, proto=%u status %ld l3 num %d sport %d dport %d\n",
				&orig_tuple->src.u3.ip,
				&orig_tuple->dst.u3.ip,
				orig_tuple->dst.protonum, ct->status, orig_tuple->src.l3num,
				ntohs(orig_tuple->src.u.all), ntohs(orig_tuple->dst.u.all));
	}

	if (ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.l3num == NFPROTO_IPV6) {
		netstandby_info("IPV6 imp flow: src=%pI6, dst=%pI6, proto=%u status %ld l3 num %d sport %d dport %d\n",
				orig_tuple->src.u3.ip6,
				orig_tuple->dst.u3.ip6,
				orig_tuple->dst.protonum, ct->status, orig_tuple->src.l3num,
				ntohs(orig_tuple->src.u.all), ntohs(orig_tuple->dst.u.all));

	}

	return false;
}

/*
 * netstandby_nss_sampling_timer_cb
 *	Timer to collect the stats of NSS and report it to RM
 */
void netstandby_nss_sampling_work_cb(struct work_struct *work)
{
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;
	struct net_device *dev;
	uint8_t i = 0;
	int bucket = 0;
#if defined(MHT_ETH_INTF)
	int port_id = 0;
#endif
	struct nf_conn *ct;
	struct nf_conntrack_tuple_hash *h;
	struct hlist_nulls_node *n;
	struct kernel_cpustat *kcs;
	const struct rtnl_link_stats64 *dev_stats;
	struct rtnl_link_stats64 temp;
	struct netstandby_erp_cpu_usage *cpuutil_telemetry;
	struct netstandby_erp_eth_sample *curr_eth_sample;
	struct netstandby_erp_cpuutil_sample *curr_cpuutil_sample;
	struct netstandby_erp_eth_stats *eth_telemetry;

	nss_dp_get_eth_info(gbl_ctx->ethinfo, NSS_DP_MAX_INTERFACES - 1);

	/*
	 * Rate analysis
	 * Compute the difference between prev and curr bytes/pkts
	 */
	for (i = 0; i < NSS_DP_MAX_INTERFACES - 1; i++) {
		dev = (struct net_device *)gbl_ctx->ethinfo[i].netdev;
		if (!dev) {
			netstandby_info("dev is NULL i %d\n", i);
			continue;
		}

		dev_stats = dev_get_stats(dev, &temp);
		curr_eth_sample = &gbl_ctx->nss_sample.curr_eth_sample[i];
		eth_telemetry = &nss_telemetry.ethstats[i];

		curr_eth_sample->cur_tx_bytes = dev_stats->tx_bytes;
		curr_eth_sample->cur_rx_bytes = dev_stats->rx_bytes;
		curr_eth_sample->cur_tx_packets = dev_stats->tx_packets;
		curr_eth_sample->cur_rx_packets = dev_stats->rx_packets;

		eth_telemetry->tx_bytes_diff = curr_eth_sample->cur_tx_bytes - curr_eth_sample->prev_tx_bytes;
		eth_telemetry->rx_bytes_diff = curr_eth_sample->cur_rx_bytes - curr_eth_sample->prev_rx_bytes;
		eth_telemetry->tx_pkts_diff = curr_eth_sample->cur_tx_packets - curr_eth_sample->prev_tx_packets;
		eth_telemetry->rx_pkts_diff = curr_eth_sample->cur_rx_packets - curr_eth_sample->prev_rx_packets;

		curr_eth_sample->prev_tx_bytes = curr_eth_sample->cur_tx_bytes;
		curr_eth_sample->prev_rx_bytes = curr_eth_sample->cur_rx_bytes;
		curr_eth_sample->prev_tx_packets = curr_eth_sample->cur_tx_packets;
		curr_eth_sample->prev_rx_packets = curr_eth_sample->cur_rx_packets;

		memcpy(&nss_telemetry.ethstats[i].dev_name, dev->name, sizeof(nss_telemetry.ethstats[i].dev_name));
		netstandby_info("Dev:%s Tx bytes %llu Rx bytes %llu Tx pkts %llu Rx pkts %llu\n",
				nss_telemetry.ethstats[i].dev_name, nss_telemetry.ethstats[i].tx_bytes_diff,
				nss_telemetry.ethstats[i].rx_bytes_diff, nss_telemetry.ethstats[i].tx_pkts_diff,
				nss_telemetry.ethstats[i].rx_pkts_diff);
	}
	nss_telemetry.num_of_ifaces = i;
	netstandby_info("Number of ifaces %d\n", nss_telemetry.num_of_ifaces);

	/*
	 * CPU utilization analysis
	 * 	Per CPU utilization logic
	 */
	for(i = 0; i < NR_CPUS; i++) {
		kcs = &kcpustat_cpu(i);
		curr_cpuutil_sample = &gbl_ctx->nss_sample.curr_cpuutil_sample[i];
		cpuutil_telemetry = &nss_telemetry.cpuutil_telemetry[i];

		curr_cpuutil_sample->curr_time = ktime_get_real_ns();
		curr_cpuutil_sample->current_idle_sec = netstandby_get_idle_time(kcs, i);

		/*
		 * Calculate the non idle time and period for which the non idle is computed
		 */
		cpuutil_telemetry->non_idle_time = (curr_cpuutil_sample->curr_time - curr_cpuutil_sample->prev_time) -
			(curr_cpuutil_sample->current_idle_sec - curr_cpuutil_sample->previous_idle_sec);
		cpuutil_telemetry->compute_period = (curr_cpuutil_sample->curr_time - curr_cpuutil_sample->prev_time);

		netstandby_info("CPU:%d prev time %llu cur time %llu prev idle %llu "
				"curr idle %llu non idle %llu compute %llu\n", i,
				gbl_ctx->nss_sample.curr_cpuutil_sample[i].prev_time,
				gbl_ctx->nss_sample.curr_cpuutil_sample[i].curr_time,
				gbl_ctx->nss_sample.curr_cpuutil_sample[i].previous_idle_sec,
				gbl_ctx->nss_sample.curr_cpuutil_sample[i].current_idle_sec,
				nss_telemetry.cpuutil_telemetry[i].non_idle_time,
				nss_telemetry.cpuutil_telemetry[i].compute_period);

		curr_cpuutil_sample->prev_time = curr_cpuutil_sample->curr_time;
		curr_cpuutil_sample->previous_idle_sec = curr_cpuutil_sample->current_idle_sec;
	}

	/*
	 * Iface up analysis
	 */
	nss_telemetry.eth_link_up.prev_link_up = nss_telemetry.eth_link_up.curr_link_up;
	nss_telemetry.eth_link_up.curr_link_up = 0;

	for (i = 0; i < NSS_DP_MAX_INTERFACES-1; i++) {
		dev = (struct net_device *)gbl_ctx->ethinfo[i].netdev;

		/*
		 * Checks whether the iface is up or not
		 */
		if (netif_oper_up(dev)) {
			netstandby_info("dev name %s switch connected %d\n", dev->name,
					gbl_ctx->ethinfo[i].switch_connected);
#if defined(MHT_ETH_INTF)
			if (gbl_ctx->ethinfo[i].switch_connected) {
				for (port_id = 0; port_id < MAX_MHT_PORTS; port_id++) {
					netstandby_info("PortID: %d status %d\n", port_id+1,
							gbl_ctx->ethinfo[i].mht_port_status[port_id]);
					if (gbl_ctx->ethinfo[i].mht_port_status[port_id]) {
						nss_telemetry.eth_link_up.curr_link_up++;
					}
				}
			} else {
				nss_telemetry.eth_link_up.curr_link_up++;
			}
#else
			nss_telemetry.eth_link_up.curr_link_up++;
#endif
		}
	}

	netstandby_info("New link up curr up dev %d prev up dev %d\n", nss_telemetry.eth_link_up.curr_link_up,
			nss_telemetry.eth_link_up.prev_link_up);

	/*
	 * Level#4 analysis
	 * Iteration of nf conntrach
	 * If the number of conn track is more than max, then NSS is in non idle state
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
	/* nf_conntrack_count() gives the total number of nf conntrack entries */
	nss_telemetry.num_of_nf_ct = nf_conntrack_count(&init_net);
#else
	/* init_net.ct.count gives the total number of nf conntrack entries */
	nss_telemetry.num_of_nf_ct = atomic_read(&init_net.ct.count);;
#endif
	nss_telemetry.ct_data_flows.prev_ct_data_flow_cnt = nss_telemetry.ct_data_flows.curr_ct_data_flow_cnt;
	nss_telemetry.ct_data_flows.curr_ct_data_flow_cnt = 0;
	for (bucket = 0; bucket < nf_conntrack_htable_size; bucket++) {

		/*
		 * Loop through each entry in the bucket
		 */
		hlist_nulls_for_each_entry(h, n, &nf_conntrack_hash[bucket],
				hnnode) {

			/*
			 * Check the CT for IP_CT_DIR_ORIGINAL dir alone
			 */
			if (NF_CT_DIRECTION(h)) {
				continue;
			}
			ct = nf_ct_tuplehash_to_ctrack(h);
			if (netstandby_is_flow_not_important(ct)) {
				continue;
			}
			nss_telemetry.ct_data_flows.curr_ct_data_flow_cnt++;
		}
	}

	netstandby_info("Curr imp ct %d Prev imp ct %d nf ct %llu\n", nss_telemetry.ct_data_flows.prev_ct_data_flow_cnt,
			nss_telemetry.ct_data_flows.curr_ct_data_flow_cnt, nss_telemetry.num_of_nf_ct);

	/*
	 * Send the eth telemetry to RM
	 */
	if (!netstandby_nss_telemetry_to_rm(&nss_telemetry)) {
		netstandby_nss_telemetry_retry++;
	}

	if (netstandby_nss_telemetry_retry < NETSTANDBY_MAX_NSS_TELEMETRY_RETRY) {
		schedule_delayed_work(&gbl_ctx->nss_sample.sampling_work,
				msecs_to_jiffies(gbl_ctx->nss_sample.sampling_time_millisec));
	} else {
		netstandby_warn("Eth Telemetry to ERP service is failed for 3 times and so, samples will not sent"
				"again to ERP un till standby is loaded again\n");
	}
}
#endif /* defined(NSS_TELEMETRY_SUPPORT) */
