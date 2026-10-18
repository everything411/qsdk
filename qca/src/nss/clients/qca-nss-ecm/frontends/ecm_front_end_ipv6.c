/*
 **************************************************************************
 * Copyright (c) 2014-2017, 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/inet.h>
#include <linux/etherdevice.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/ip6_route.h>
#include <net/route.h>

/*
 * Debug output levels
 * 0 = OFF
 * 1 = ASSERTS / ERRORS
 * 2 = 1 + WARN
 * 3 = 2 + INFO
 * 4 = 3 + TRACE
 */
#define DEBUG_LEVEL ECM_FRONT_END_IPV6_DEBUG_LEVEL

#include "ecm_types.h"
#include "ecm_db_types.h"
#include "ecm_state.h"
#include "ecm_tracker.h"
#include "ecm_classifier.h"
#include "ecm_front_end_types.h"
#include "ecm_tracker_datagram.h"
#include "ecm_tracker_udp.h"
#include "ecm_tracker_tcp.h"
#include "ecm_db.h"
#include "ecm_front_end_ipv6.h"
#include "ecm_interface.h"
#include "ecm_ipv6.h"
#include"ecm_stats_v6.h"

/*
 * Default path for sysctl
 */
#define ECM_FRONT_END_IPV6_PATH "net/ecm"

/*
 * Sysctl table header
 */
static struct ctl_table_header *ecm_front_end_ipv6_ctl_table_header;

/*
 * General operational control
 */
extern int ecm_front_end_ipv6_stopped;	/* When non-zero further traffic will not be processed */

/*
 * Temporary operational control
 */
atomic_t ecm_front_end_ipv6_stopped_temp = ATOMIC_INIT(0);	/* When non-zero further traffic/process will not be processed where it is checked */

/*
 * ecm_front_end_ipv6_interface_construct_ip_addr_set()
 *	Sets the IP addresses.
 *
 * Sets the ip address fields of the ecm_front_end_interface_construct_instance
 * with the given ip addresses.
 */
static void ecm_front_end_ipv6_interface_construct_ip_addr_set(struct ecm_front_end_interface_construct_instance *efeici,
							ip_addr_t from_mac_lookup, ip_addr_t to_mac_lookup,
							ip_addr_t from_nat_mac_lookup, ip_addr_t to_nat_mac_lookup)
{
	ECM_IP_ADDR_COPY(efeici->from_mac_lookup_ip_addr, from_mac_lookup);
	ECM_IP_ADDR_COPY(efeici->to_mac_lookup_ip_addr, to_mac_lookup);
	ECM_IP_ADDR_COPY(efeici->from_nat_mac_lookup_ip_addr, from_nat_mac_lookup);
	ECM_IP_ADDR_COPY(efeici->to_nat_mac_lookup_ip_addr, to_nat_mac_lookup);
}

/*
 * ecm_front_end_ipv6_interface_construct_netdev_set()
 *	Sets the net devices.
 *
 * Sets the net device fields of the ecm_front_end_interface_construct_instance
 * with the given net devices.
 */
static void ecm_front_end_ipv6_interface_construct_netdev_set(struct ecm_front_end_interface_construct_instance *efeici,
							struct net_device *from, struct net_device *from_other,
							struct net_device *to, struct net_device *to_other,
							struct net_device *from_nat, struct net_device *from_nat_other,
							struct net_device *to_nat, struct net_device *to_nat_other)
{
	efeici->from_dev = from;
	efeici->from_other_dev = from_other;
	efeici->to_dev = to;
	efeici->to_other_dev = to_other;
	efeici->from_nat_dev = from_nat;
	efeici->from_nat_other_dev = from_nat_other;
	efeici->to_nat_dev = to_nat;
	efeici->to_nat_other_dev = to_nat_other;
}

/*
 * ecm_front_end_ipv6_stop_handler()
 * 	Proc handler to enable or disable ipv6 frontend
 */
static int ecm_front_end_ipv6_stop_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer, size_t *lenp, loff_t *ppos)
{
	/*
	 * Usage:
	 *
	 * Enable IPv6 frontend
	 * echo 1 > /proc/sys/net/ecm/front_end_ipv6_stop
	 *
	 * Disable IPv6 frontend
	 * echo 0 > /proc/sys/net/ecm/front_end_ipv6_stop
	 *
	 * To read status
	 * cat /proc/sys/net/ecm/front_end_ipv6_stop
	 */
	int ret;
	int current_val;

	/*
	 * Write the value with user input
	 */
	current_val = ecm_front_end_ipv6_stopped;
	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		/*
		 * Return if failure or read operation
		 */
		return ret;
	}

	if ((ecm_front_end_ipv6_stopped != 0) && (ecm_front_end_ipv6_stopped != 1)) {
		DEBUG_ERROR("Invalid input, valid input 0/1\n");
		ecm_front_end_ipv6_stopped = current_val;
		return -EINVAL;
	}

	return ret;
}

static struct ctl_table ecm_front_end_ipv6_ctl_table[] = {
	{
		.procname	= "front_end_ipv6_stop",
		.data		= &ecm_front_end_ipv6_stopped,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &ecm_front_end_ipv6_stop_handler,
	},
};

/*
 * ecm_front_end_ipv6_interface_construct_netdev_put()
 *	Release the references of the net devices.
 */
void ecm_front_end_ipv6_interface_construct_netdev_put(struct ecm_front_end_interface_construct_instance *efeici)
{
	dev_put(efeici->from_dev);
	dev_put(efeici->from_other_dev);
	dev_put(efeici->to_dev);
	dev_put(efeici->to_other_dev);
	dev_put(efeici->from_nat_dev);
	dev_put(efeici->from_nat_other_dev);
	dev_put(efeici->to_nat_dev);
	dev_put(efeici->to_nat_other_dev);
}

/*
 * ecm_front_end_ipv6_interface_construct_netdev_hold()
 *	Holds the references of the netdevices.
 */
void ecm_front_end_ipv6_interface_construct_netdev_hold(struct ecm_front_end_interface_construct_instance *efeici)
{
	dev_hold(efeici->from_dev);
	dev_hold(efeici->from_other_dev);
	dev_hold(efeici->to_dev);
	dev_hold(efeici->to_other_dev);
	dev_hold(efeici->from_nat_dev);
	dev_hold(efeici->from_nat_other_dev);
	dev_hold(efeici->to_nat_dev);
	dev_hold(efeici->to_nat_other_dev);
}

/*
 * ecm_front_end_ipv6_interface_construct_set_and_hold()
 *	Sets the IPv6 ECM front end interface construct instance,
 *	and holds the net devices.
 */
bool ecm_front_end_ipv6_interface_construct_set_and_hold(struct sk_buff *skb, ecm_tracker_sender_type_t sender, ecm_db_direction_t ecm_dir, bool is_routed,
							struct net_device *in_dev, struct net_device *out_dev,
							ip_addr_t ip_src_addr, ip_addr_t ip_src_addr_nat,
							ip_addr_t ip_dest_addr, ip_addr_t ip_dest_addr_nat,
							struct ecm_front_end_interface_construct_instance *efeici)
{
	struct dst_entry *dst = skb_dst(skb);
	struct rt6_info *rt = (struct rt6_info *)dst;
	struct net_device *rt_iif_dev = NULL;
	ip_addr_t rt_dst_addr;
	struct net_device *from = NULL;
	struct net_device *from_other = NULL;
	struct net_device *to = NULL;
	struct net_device *to_other = NULL;
	struct net_device *from_nat = NULL;
	struct net_device *from_nat_other = NULL;
	struct net_device *to_nat = NULL;
	struct net_device *to_nat_other = NULL;
	struct net_device *dst_dev = NULL;
	struct net_device *master = NULL;
	ip_addr_t from_mac_lookup;
	ip_addr_t to_mac_lookup;
	ip_addr_t from_nat_mac_lookup;
	ip_addr_t to_nat_mac_lookup;
	bool gateway = false;
	bool dst_dev_override = false;
	struct in6_addr nat_dev_saddr = {0};
	struct in6_addr nat_dev_daddr = {0};
	ip_addr_t ip_nat_dev_saddr;

	/*
	 * Set the rt_dst_addr with the destination IP address by default.
	 */
	ECM_IP_ADDR_COPY(rt_dst_addr, ip_dest_addr);

	if (!is_routed) {
		/*
		 * Bridged
		 */
		from = in_dev;
		from_other = in_dev;
		to = out_dev;
		to_other = out_dev;

		ECM_IP_ADDR_COPY(from_mac_lookup, ip_src_addr);
		ECM_IP_ADDR_COPY(to_mac_lookup, ip_dest_addr);
		ECM_IP_ADDR_COPY(from_nat_mac_lookup, ip_src_addr);
	} else {
		/*
		 * Routed
		 */
		if (!rt) {
			DEBUG_WARN("rt6_info is NULL\n");
			return false;
		}

		/*
		 * If the flow is routed, extract the route information from the skb.
		 * Print the extracted information for debug purpose.
		 */
		rt_iif_dev = dev_get_by_index(&init_net, skb->skb_iif);
		if (!rt_iif_dev) {
			DEBUG_WARN("No rt_iif dev\n");
			return false;
		}

		/*
		 * For Hairpin NAT flows, (rt_iif_dev) will be a Bridged Port, and it's Master dev
		 * will the bridge interface.
		 * We need to use the master netdevice for heirarchy creation.
		 */
		if (rt_iif_dev->priv_flags & IFF_BRIDGE_PORT) {
			rcu_read_lock();
			master = netdev_master_upper_dev_get_rcu(rt_iif_dev);
			rcu_read_unlock();

			if (master) {
				dev_put(rt_iif_dev);
				rt_iif_dev = master;
				dev_hold(rt_iif_dev);
			}
		}

		dst_dev = dst->dev;

#ifdef ECM_XFRM_ENABLE
		/*
		 * If the dst is an xfrm dst, then override the dst_dev.
		*/
		if (dst_xfrm(dst)) {
			int32_t if_type;
			struct net_device *xfrm_dst_dev = ecm_interface_get_and_hold_ipsec_tun_netdev(NULL, skb, &if_type);
			/*
			 * If we reach here and are unable to find the tunnel netdevice,
			 * then return failure.
			 */
			if (!xfrm_dst_dev) {
				if (rt_iif_dev) {
					dev_put(rt_iif_dev);
				}
				return false;
			}

			dst_dev = xfrm_dst_dev;
			dst_dev_override = true;
		}

#endif
		DEBUG_TRACE("in_dev: %s\n", in_dev->name);
		DEBUG_TRACE("out_dev: %s\n", out_dev->name);
		DEBUG_TRACE("dst->dev: %s dst_dev: %s\n", dst_dev->name, dst_dev->name);
		DEBUG_INFO("rt_dst_addr: " ECM_IP_ADDR_OCTAL_FMT "\n", ECM_IP_ADDR_TO_OCTAL(rt_dst_addr));
		DEBUG_TRACE("rt_iif_dev: %s\n", rt_iif_dev->name);
		DEBUG_TRACE("%px: rt6i_dst.addr: %pi6\n", rt, &rt->rt6i_dst.addr);
		DEBUG_TRACE("%px: rt6i_src.addr: %pi6\n", rt, &rt->rt6i_src.addr);
		DEBUG_TRACE("%px: rt6i_gateway: %pi6\n", rt, &rt->rt6i_gateway);
		DEBUG_TRACE("%px: rt6i_idev: %s\n", rt, rt->rt6i_idev->dev->name);
		DEBUG_TRACE("%px: skb->dev: %s\n", rt, skb->dev->name);

		DEBUG_INFO("ip_src_addr: " ECM_IP_ADDR_OCTAL_FMT "\n", ECM_IP_ADDR_TO_OCTAL(ip_src_addr));
		DEBUG_INFO("ip_src_addr_nat: " ECM_IP_ADDR_OCTAL_FMT "\n", ECM_IP_ADDR_TO_OCTAL(ip_src_addr_nat));
		DEBUG_INFO("ip_dest_addr: " ECM_IP_ADDR_OCTAL_FMT "\n", ECM_IP_ADDR_TO_OCTAL(ip_dest_addr));
		DEBUG_INFO("ip_dest_addr_nat: " ECM_IP_ADDR_OCTAL_FMT "\n", ECM_IP_ADDR_TO_OCTAL(ip_dest_addr_nat));


		/*
		 * If the destination host is behind a gateway, use the gateway address as destination
		 * for routed connections.
		 */
		if (!ECM_IP_ADDR_MATCH(rt->rt6i_dst.addr.in6_u.u6_addr32, rt->rt6i_gateway.in6_u.u6_addr32) || (rt->rt6i_flags & RTF_GATEWAY)) {
			if (!ECM_IP_ADDR_IS_NULL(rt->rt6i_gateway.in6_u.u6_addr32)) {
				ECM_NIN6_ADDR_TO_IP_ADDR(rt_dst_addr, rt->rt6i_gateway);
				gateway = true;
			}
		}

		from = rt_iif_dev;
		from_other = dst_dev;
		to = dst_dev;
		to_other = rt_iif_dev;

		ECM_IP_ADDR_COPY(from_mac_lookup, ip_src_addr);
		ECM_IP_ADDR_COPY(to_mac_lookup, rt_dst_addr);
	}

	/*
	 * Based on the flow and connection direction, set the NAT'd net devices.
	 * The above IP address settings are valid for each flow and connection direction case.
	 */
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		if (ecm_dir == ECM_DB_DIRECTION_EGRESS_NAT) {
			from_nat = dst_dev;
			from_nat_other = rt_iif_dev;
			to_nat = dst_dev;
			to_nat_other = rt_iif_dev;

			/*
			 * ip_src_addr_nat could be dummy address, we will get ipv6 address of out_dev
			 * and map it to from_nat_mac_lookup
			 */
			ECM_IP_ADDR_TO_NIN6_ADDR(nat_dev_daddr, ip_dest_addr);
			ipv6_dev_get_saddr(dev_net(out_dev), out_dev, &nat_dev_daddr, 0, &nat_dev_saddr);
			ECM_NIN6_ADDR_TO_IP_ADDR(ip_nat_dev_saddr, nat_dev_saddr);
			ECM_IP_ADDR_COPY(from_nat_mac_lookup, ip_nat_dev_saddr);
			ECM_IP_ADDR_COPY(to_nat_mac_lookup, ip_dest_addr_nat);
		} else if (ecm_dir == ECM_DB_DIRECTION_NON_NAT) {
			from_nat = rt_iif_dev;
			from_nat_other = dst_dev;
			to_nat = dst_dev;
			to_nat_other = rt_iif_dev;
			ECM_IP_ADDR_COPY(from_nat_mac_lookup, ip_src_addr_nat);
			ECM_IP_ADDR_COPY(to_nat_mac_lookup, ip_dest_addr_nat);
		} else if (ecm_dir == ECM_DB_DIRECTION_INGRESS_NAT) {
			from_nat = rt_iif_dev;
			from_nat_other = dst_dev;
			to_nat = rt_iif_dev;
			to_nat_other = dst_dev;

			/*
			 * ip_dst_addr_nat could be dummy address, we will get ipv6 address of in_dev
			 * and map it to to_nat_mac_lookup
			 */
			ECM_IP_ADDR_TO_NIN6_ADDR(nat_dev_daddr, ip_src_addr);
			ipv6_dev_get_saddr(dev_net(in_dev), in_dev, &nat_dev_daddr, 0, &nat_dev_saddr);
			ECM_NIN6_ADDR_TO_IP_ADDR(ip_nat_dev_saddr, nat_dev_saddr);
			ECM_IP_ADDR_COPY(from_nat_mac_lookup, ip_src_addr_nat);
			ECM_IP_ADDR_COPY(to_nat_mac_lookup, ip_nat_dev_saddr);
		} else if (ecm_dir == ECM_DB_DIRECTION_BRIDGED) {
			from_nat = in_dev;
			from_nat_other = in_dev;
			to_nat = out_dev;
			to_nat_other = out_dev;
			ECM_IP_ADDR_COPY(from_nat_mac_lookup, ip_src_addr_nat);
			ECM_IP_ADDR_COPY(to_nat_mac_lookup, ip_dest_addr_nat);
		} else if (ecm_dir == ECM_DB_DIRECTION_HAIRPIN_NAT) {
			from_nat = out_dev;
			from_nat_other = out_dev;
			to_nat = out_dev;
			to_nat_other = out_dev;

			ECM_IP_ADDR_TO_NIN6_ADDR(nat_dev_daddr, ip_dest_addr);
			ipv6_dev_get_saddr(dev_net(out_dev), out_dev, &nat_dev_daddr, 0, &nat_dev_saddr);
			ECM_NIN6_ADDR_TO_IP_ADDR(ip_nat_dev_saddr, nat_dev_saddr);
			ECM_IP_ADDR_COPY(from_nat_mac_lookup, ip_nat_dev_saddr);
			ECM_IP_ADDR_COPY(to_nat_mac_lookup, ip_nat_dev_saddr);
		} else {
			DEBUG_ASSERT(false, "Unhandled ecm_dir: %d\n", ecm_dir);
		}
	} else {
		if (ecm_dir == ECM_DB_DIRECTION_EGRESS_NAT) {
			from_nat = rt_iif_dev;
			from_nat_other = dst_dev;
			to_nat = rt_iif_dev;
			to_nat_other = dst_dev;

			/*
			 * ip_dst_addr_nat could be dummy address, we will get ipv6 address of in_dev
			 * and map it to to_nat_mac_lookup
			 */
			ECM_IP_ADDR_TO_NIN6_ADDR(nat_dev_daddr, ip_src_addr);
			ipv6_dev_get_saddr(dev_net(in_dev), in_dev, &nat_dev_daddr, 0, &nat_dev_saddr);
			ECM_NIN6_ADDR_TO_IP_ADDR(ip_nat_dev_saddr, nat_dev_saddr);
			ECM_IP_ADDR_COPY(from_nat_mac_lookup, ip_src_addr_nat);
			ECM_IP_ADDR_COPY(to_nat_mac_lookup, ip_nat_dev_saddr);
		} else if (ecm_dir == ECM_DB_DIRECTION_NON_NAT) {
			from_nat = rt_iif_dev;
			from_nat_other = dst_dev;
			to_nat = dst_dev;
			to_nat_other = rt_iif_dev;
			ECM_IP_ADDR_COPY(from_nat_mac_lookup, ip_src_addr_nat);
			ECM_IP_ADDR_COPY(to_nat_mac_lookup, ip_dest_addr_nat);
		} else if (ecm_dir == ECM_DB_DIRECTION_INGRESS_NAT) {
			from_nat = dst_dev;
			from_nat_other = rt_iif_dev;
			to_nat = dst_dev;
			to_nat_other = rt_iif_dev;

			/*
			 * ip_src_addr_nat could be dummy address, we will get ipv6 address of out_dev
			 * and map it to from_nat_mac_lookup
			 */
			ECM_IP_ADDR_TO_NIN6_ADDR(nat_dev_daddr, ip_dest_addr);
			ipv6_dev_get_saddr(dev_net(out_dev), out_dev, &nat_dev_daddr, 0, &nat_dev_saddr);
			ECM_NIN6_ADDR_TO_IP_ADDR(ip_nat_dev_saddr, nat_dev_saddr);
			ECM_IP_ADDR_COPY(from_nat_mac_lookup, ip_nat_dev_saddr);
			ECM_IP_ADDR_COPY(to_nat_mac_lookup, ip_dest_addr_nat);
		} else if (ecm_dir == ECM_DB_DIRECTION_BRIDGED) {
			from_nat = in_dev;
			from_nat_other = in_dev;
			to_nat = out_dev;
			to_nat_other = out_dev;
			ECM_IP_ADDR_COPY(from_nat_mac_lookup, ip_src_addr_nat);
			ECM_IP_ADDR_COPY(to_nat_mac_lookup, ip_dest_addr_nat);
		} else if (ecm_dir == ECM_DB_DIRECTION_HAIRPIN_NAT) {
			from_nat = out_dev;
			from_nat_other = out_dev;
			to_nat = out_dev;
			to_nat_other = out_dev;

			ECM_IP_ADDR_TO_NIN6_ADDR(nat_dev_daddr, ip_dest_addr);
			ipv6_dev_get_saddr(dev_net(out_dev), out_dev, &nat_dev_daddr, 0, &nat_dev_saddr);
			ECM_NIN6_ADDR_TO_IP_ADDR(ip_nat_dev_saddr, nat_dev_saddr);
			ECM_IP_ADDR_COPY(from_nat_mac_lookup, ip_nat_dev_saddr);
			ECM_IP_ADDR_COPY(to_nat_mac_lookup, ip_nat_dev_saddr);
		} else {
			DEBUG_ASSERT(false, "Unhandled ecm_dir: %d\n", ecm_dir);
		}
	}

	/*
	 * If we have a gateway IP address we should use it for the
	 * to_nat_mac_lookup IP address.
	 * Note that in hairpin NAT the destination IP address and the destination
	 * NAT IP addresses are different than each other. Because of this we cannot
	 * use the rt_dst_addr for to_nat_mac_lookup as well. In a normal routing
	 * traffic they are equal.
	 */
	if (gateway) {
		ECM_IP_ADDR_COPY(to_nat_mac_lookup, rt_dst_addr);
	}

	ecm_front_end_ipv6_interface_construct_netdev_set(efeici, from, from_other,
								to, to_other,
								from_nat, from_nat_other,
								to_nat, to_nat_other);

	ecm_front_end_ipv6_interface_construct_netdev_hold(efeici);

	ecm_front_end_ipv6_interface_construct_ip_addr_set(efeici, from_mac_lookup, to_mac_lookup,
								from_nat_mac_lookup, to_nat_mac_lookup);

	if (dst_dev_override) {
		dev_put(dst_dev);
	}

	/*
	 * Release the iff_dev which was hold by the dev_get_by_index() call.
	 */
	if (rt_iif_dev) {
		dev_put(rt_iif_dev);
	}

	DEBUG_INFO("from_mac_lookup: " ECM_IP_ADDR_OCTAL_FMT "\n", ECM_IP_ADDR_TO_OCTAL(from_mac_lookup));
	DEBUG_INFO("to_mac_lookup: " ECM_IP_ADDR_OCTAL_FMT "\n", ECM_IP_ADDR_TO_OCTAL(to_mac_lookup));
	DEBUG_INFO("from_nat_mac_lookup: " ECM_IP_ADDR_OCTAL_FMT "\n", ECM_IP_ADDR_TO_OCTAL(from_nat_mac_lookup));
	DEBUG_INFO("to_nat_mac_lookup: " ECM_IP_ADDR_OCTAL_FMT "\n", ECM_IP_ADDR_TO_OCTAL(to_nat_mac_lookup));

	return true;
}

/*
 * ecm_front_end_ipv6_stop_temp()
 */
void ecm_front_end_ipv6_stop_temp(int num)
{
	if (num) {
		atomic_inc(&ecm_front_end_ipv6_stopped_temp);
	} else {
		if (atomic_read(&ecm_front_end_ipv6_stopped_temp) > 0) {
			atomic_dec(&ecm_front_end_ipv6_stopped_temp);
		}
	}
}

/*
 * ecm_front_end_ipv6_init()
 */
int ecm_front_end_ipv6_init(struct dentry *dentry)
{
	struct dentry *ecm_stats_dentry;

	ecm_front_end_ipv6_ctl_table_header = register_sysctl(ECM_FRONT_END_IPV6_PATH, ecm_front_end_ipv6_ctl_table);
	if (!ecm_front_end_ipv6_ctl_table_header) {
		DEBUG_ERROR("Failed to create sysctl node for front end ipv6\n");
		return -1;
	}

	ecm_debugfs_create_u32("front_end_ipv6_stop", S_IRUGO | S_IWUSR, dentry, (u32 *)&ecm_front_end_ipv6_stopped);

	if (!ecm_debugfs_lookup("stats", dentry, &ecm_stats_dentry)) {
		DEBUG_ERROR("Stats dentry not created\n");
		goto init_cleanup;
	}

	if (ecm_stats_v6_debugfs_init(ecm_stats_dentry)) {
		DEBUG_ERROR("Failed to create v6 stats file in ecm\n");
		/*
		 * Debugfs cleanup will be taken care by the calling function.
		 */
		goto init_cleanup;
	}

	return ecm_ipv6_init(dentry);

init_cleanup:

	unregister_sysctl_table(ecm_front_end_ipv6_ctl_table_header);
	return -1;
}

/*
 * ecm_front_end_ipv6_exit()
 */
void ecm_front_end_ipv6_exit(void)
{
	ecm_ipv6_exit();

	/*
	 * Unregister sysctl table header
	 */
	if (ecm_front_end_ipv6_ctl_table_header) {
		unregister_sysctl_table(ecm_front_end_ipv6_ctl_table_header);
	}
}
