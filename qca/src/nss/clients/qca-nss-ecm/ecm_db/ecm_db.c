/*
 **************************************************************************
 * Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/icmp.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/pkt_sched.h>
#include <linux/string.h>
#include <linux/random.h>
#include <net/route.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <asm/uaccess.h>	/* for put_user */
#include <net/ipv6.h>
#include <net/ip6_route.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_bridge.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/ipv4/nf_conntrack_ipv4.h>
#include <net/netfilter/ipv4/nf_defrag_ipv4.h>

/*
 * Debug output levels
 * 0 = OFF
 * 1 = ASSERTS / ERRORS
 * 2 = 1 + WARN
 * 3 = 2 + INFO
 * 4 = 3 + TRACE
 */
#define DEBUG_LEVEL ECM_DB_DEBUG_LEVEL

#include "ecm_types.h"
#include "ecm_db_types.h"
#include "ecm_state.h"
#include "ecm_tracker.h"
#include "ecm_classifier.h"
#include "ecm_front_end_types.h"
#include "ecm_classifier_default.h"
#include "ecm_db.h"
#include "ecm_front_end_ipv4.h"
#ifdef ECM_IPV6_ENABLE
#include "ecm_front_end_ipv6.h"
#endif
#include "ecm_notifier_pvt.h"
#include "ecm_interface.h"

/*
 * Default path for sysctl
 */
#define ECM_DB_PATH "net/ecm/ecm_db"

/*
 * Locking of the database - concurrency control
 */
DEFINE_SPINLOCK(ecm_db_lock);					/* Protect the table from SMP access. */

/*
 * Enable/Disable IPV4 routing table events
 */
static int ecm_db_ipv4_route_enable = 1;

/*
 * Enable/Disable IPV6 routing table events
 */
#ifdef ECM_IPV6_ENABLE
static int ecm_db_ipv6_route_enable = 1;
#endif

/*
 * Debugfs dentry object.
 */
static struct dentry *ecm_db_dentry;

/*
 * Management thread control
 */
bool ecm_db_terminate_pending = false;			/* When true the user has requested termination */

/*
 * Random seed used during hash calculations
 */
uint32_t ecm_db_jhash_rnd __read_mostly;

/*
 * ecm_db_obj_dir_strings[]
 *      Common array that maps the object direction to a string
 */
char *ecm_db_obj_dir_strings[ECM_DB_OBJ_DIR_MAX] = {
        "FROM",
        "TO",
        "FROM_NAT",
        "TO_NAT"
};

/*
 * Global listener instance for DB events.
 */
static struct ecm_db_listener_instance *ecm_db_li;

#ifdef ECM_DB_PER_CLIENT_ROUTED_STATS_ENABLE
/*
 * ecm_db_per_client_routed_stats_state_write()
 *	Write out per client routed stats state
 */
int ecm_db_per_client_routed_stats_state_write(struct ecm_state_file_instance *sfi,
					       uint64_t from_data_routed, uint64_t to_data_routed,
					       uint64_t from_packet_routed, uint64_t to_packet_routed)
{
	int result;

	if ((result = ecm_state_prefix_add(sfi, "adv_stats"))) {
		return result;
	}

	if ((result = ecm_state_write(sfi, "from_data_routed", "%llu", from_data_routed))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "to_data_routed", "%llu", to_data_routed))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "from_packet_routed", "%llu", from_packet_routed))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "to_packet_routed", "%llu", to_packet_routed))) {
		return result;
	}

	return ecm_state_prefix_remove(sfi);
}
#endif

/*
 * Sysctl table header
 */
static struct ctl_table_header *ecm_db_ctl_table_header;

/*
 * ecm_db_adv_stats_state_write()
 *	Write out advanced stats state
 */
int ecm_db_adv_stats_state_write(struct ecm_state_file_instance *sfi,uint64_t from_data_total, uint64_t to_data_total,
				uint64_t from_packet_total, uint64_t to_packet_total, uint64_t from_data_total_dropped,
				uint64_t to_data_total_dropped, uint64_t from_packet_total_dropped, uint64_t to_packet_total_dropped)
{
	int result;

	if ((result = ecm_state_prefix_add(sfi, "adv_stats"))) {
		return result;
	}

	if ((result = ecm_state_write(sfi, "from_data_total", "%llu", from_data_total))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "to_data_total", "%llu", to_data_total))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "from_packet_total", "%llu", from_packet_total))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "to_packet_total", "%llu", to_packet_total))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "from_data_total_dropped", "%llu", from_data_total_dropped))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "to_data_total_dropped", "%llu", to_data_total_dropped))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "from_packet_total_dropped", "%llu", from_packet_total_dropped))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "to_packet_total_dropped", "%llu", to_packet_total_dropped))) {
		return result;
	}

	return ecm_state_prefix_remove(sfi);
}

/*
 * ecm_db_get_defunct_all()
 *	Reading this file returns the accumulated total of all objects
 */
static ssize_t ecm_db_get_defunct_all(struct file *file,
					char __user *user_buf,
					size_t sz, loff_t *ppos)
{
	int ret;
	int num;
	char *buf;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}

	/*
	 * Operate under our locks
	 */
	spin_lock_bh(&ecm_db_lock);
	num = _ecm_db_connection_count_get() + _ecm_db_mapping_count_get() + _ecm_db_host_count_get()
			+ _ecm_db_node_count_get() + _ecm_db_iface_count_get();
	spin_unlock_bh(&ecm_db_lock);

	ret = snprintf(buf, (ssize_t)PAGE_SIZE, "%d\n", num);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	ret = simple_read_from_buffer(user_buf, sz, ppos, buf, ret);
	kfree(buf);
	return ret;
}

/*
 * ecm_db_set_defunct_all()
 */
static ssize_t ecm_db_set_defunct_all(struct file *file,
					const char __user *user_buf,
					size_t sz, loff_t *ppos)
{
	ecm_db_connection_defunct_all();
	return sz;
}

/*
 * File operations for defunct_all.
 */
static struct file_operations ecm_db_defunct_all_fops = {
	.read = ecm_db_get_defunct_all,
	.write = ecm_db_set_defunct_all,
};

/*
 * ecm_db_ipv4_route_table_update_event()
 *	This is a call back for "routing table update event for IPv4".
 */
static int ecm_db_ipv4_route_table_update_event(struct notifier_block *nb,
					       unsigned long event,
					       void *ptr)
{
	DEBUG_TRACE("route table update event v4\n");

	/*
	 * Disable IPv4 frontend processing temporarily until defunct function call is completed.
	 */
	ecm_front_end_ipv4_stop_temp(1);

	ecm_db_connection_defunct_ip_version(4);

	/*
	 * Re-enable IPv4 frontend processing.
	 */
	ecm_front_end_ipv4_stop_temp(0);

	return NOTIFY_DONE;
}

static struct notifier_block ecm_db_iproute_table_update_nb = {
	.notifier_call = ecm_db_ipv4_route_table_update_event,
};

#ifdef ECM_IPV6_ENABLE
/*
 * ecm_db_ipv6_route_table_update_event()
 *	This is a call back for "routing table update event for IPv6".
 */
static int ecm_db_ipv6_route_table_update_event(struct notifier_block *nb,
					       unsigned long event,
					       void *ptr)
{
	struct fib6_config *cfg = (struct fib6_config *)ptr;
	struct ecm_db_connection_instance *ci;

	DEBUG_TRACE("route table update event v6\n");

	if ((event != RTM_DELROUTE) && (event != RTM_NEWROUTE)) {
		DEBUG_WARN("%px: Unhandled route table event: %lu\n", cfg, event);
		return NOTIFY_DONE;
	}

	/*
	 * If a default route is changed, fc_dst address is set to all zeros.
	 * In this case, we should defunct all the IPv6 flows.
	 */
	if (ipv6_addr_any(&cfg->fc_dst)) {
		DEBUG_TRACE("%px fc_dst (%pI6), default route is changed, defunct all IPv6 connections\n",
			    cfg, &cfg->fc_dst);
		ecm_db_connection_defunct_ip_version(6);
		return NOTIFY_DONE;
	}

	/*
	 * Disable IPv6 frontend processing temporarily until defunct function call is completed.
	 */
	ecm_front_end_ipv6_stop_temp(1);

	/*
	 * Iterate all connections
	 */
	ci = ecm_db_connections_get_and_ref_first();
	while (ci) {
		struct ecm_db_connection_instance *cin;
		struct in6_addr prefix_addr;
		struct in6_addr ecm_in6 = {0};
		ip_addr_t ecm_addr;
		struct ecm_db_iface_instance *interfaces[ECM_DB_IFACE_HEIRARCHY_MAX];
		int32_t if_first;
		struct net_device *ecm_dev;
		struct net_device *fc_dev;
		bool is_dest_ip_match = true;
		ecm_db_obj_dir_t obj_dir = ECM_DB_OBJ_DIR_TO;
		int fc_dst_len = cfg->fc_dst_len;

		if (ci->ip_version != 6) {
			goto next;
		}

		/*
		 * Get the ECM connection's destination IPv6 address.
		 */
		ecm_db_connection_address_get(ci, ECM_DB_OBJ_DIR_TO, ecm_addr);
		ECM_IP_ADDR_TO_NIN6_ADDR(ecm_in6, ecm_addr);

		/*
		 * Compute ECM connection's prefix destination address by masking it with the
		 * route config's destination address prefix length.
		 *
		 * Validate dst_len to be 0 <= len <= 128
		 */
		if (cfg->fc_dst_len > 128) {
			fc_dst_len = 128;
		} else if (cfg->fc_dst_len < 0){
			fc_dst_len = 0;
		}

		ipv6_addr_prefix(&prefix_addr, &ecm_in6, fc_dst_len);

		DEBUG_TRACE("dest addr prefix: %pI6 prefix_len: %d(cfg=%d) ecm_in6: %pI6\n",
				&prefix_addr, fc_dst_len, cfg->fc_dst_len, &ecm_in6);

		/*
		 * Compare the ECM connection's destination address prefix with the route config's
		 * destination address. If they are not equal, try with the ECM's source address prefix.
		 * Because ECM can create the connection in the reply direction, and in this case, ECM
		 * connection's source prefix IP address will match with the route config's dst IP address.
		 *
		 * If none of them match with the route config's destination address, this means that
		 * this connection is not related to this route change event.
		 * We should check with the next connection.
		 */
		if (ipv6_addr_cmp(&prefix_addr, &cfg->fc_dst)) {
			DEBUG_TRACE("dest addr prefix: %pI6 not equal to cfg->fc_dst: %pI6, go to next connection\n", &prefix_addr, &cfg->fc_dst);

			/*
			 * ECM's destination address didn't match.
			 * Get the ECM connection's source IPv6 address.
			 */
			ecm_db_connection_address_get(ci, ECM_DB_OBJ_DIR_FROM, ecm_addr);
			ECM_IP_ADDR_TO_NIN6_ADDR(ecm_in6, ecm_addr);

			/*
			 * Compute ECM connection's prefix source address by masking it with the
			 * route config's destination address prefix length.
			 */
			ipv6_addr_prefix(&prefix_addr, &ecm_in6, fc_dst_len);

			DEBUG_TRACE("src addr prefix: %pI6 prefix_len: %d(cfg=%d) ecm_in6: %pI6\n",
					&prefix_addr, fc_dst_len, cfg->fc_dst_len, &ecm_in6);

			if (ipv6_addr_cmp(&prefix_addr, &cfg->fc_dst)) {
				DEBUG_TRACE("src addr prefix: %pI6 not equal to cfg->fc_dst: %pI6, go to next connection\n", &prefix_addr, &cfg->fc_dst);
				goto next;
			}

			is_dest_ip_match = false;
			obj_dir = ECM_DB_OBJ_DIR_FROM;
		}

		DEBUG_TRACE("%px: ECM connection's %s address prefix: %pI6 equals to cfg->fc_dst: %pI6\n",
			    ci, is_dest_ip_match?"dest":"src", &prefix_addr, &cfg->fc_dst);

		/*
		 * If the event is a route delete event, comparing only the IP address is enough
		 * to defunct the connection.
		 */
		if (event == RTM_DELROUTE) {
			DEBUG_TRACE("%px: Route DELETE event, defunct the connection\n", ci);
			ecm_db_connection_make_defunct(ci);
			goto next;
		}

		DEBUG_TRACE("%px: Route ADD event\n", ci);

		/*
		 * If there is a route for this connection's source or destination IP address, we should
		 * compare the devices as well, because the IP address could remain the same, but
		 * the output device could be changed. So, the flows should take the new out device
		 * for their route.
		 */
		if_first = ecm_db_connection_interfaces_get_and_ref(ci, interfaces, obj_dir);
		if (if_first == ECM_DB_IFACE_HEIRARCHY_MAX) {
			DEBUG_WARN("%px: Failed to get %s interfaces list\n",
					ci, ecm_db_obj_dir_strings[obj_dir]);
			goto next;
		}

		/*
		 * Inner most interface has the IP address, so we should get that interface.
		 */
		ecm_dev = dev_get_by_index(&init_net,
				  ecm_db_iface_interface_identifier_get(interfaces[ECM_DB_IFACE_HEIRARCHY_MAX - 1]));
		if (!ecm_dev) {
			DEBUG_WARN("%px: unable to find ecm netdevice\n", ci);
			ecm_db_connection_interfaces_deref(interfaces, if_first);
			goto next;
		}
		ecm_db_connection_interfaces_deref(interfaces, if_first);

		fc_dev = dev_get_by_index(&init_net, cfg->fc_ifindex);
		if (!fc_dev) {
			DEBUG_WARN("%px: unable to find fib6_config netdevice\n", ci);
			dev_put(ecm_dev);
			goto next;
		}

		/*
		 * Compare the ECM connection's netdevice with the route change config's netdevice.
		 * If they are different, this means the new route effected the connection. So, defunct it.
		 */
		if (ecm_dev != fc_dev) {
			DEBUG_TRACE("%px: fib6_config dev: %s is different from ecm dev: %s, defunct the connection\n",
				    ci, fc_dev->name, ecm_dev->name);
			ecm_db_connection_make_defunct(ci);
		}
		dev_put(fc_dev);
		dev_put(ecm_dev);
next:
		cin = ecm_db_connection_get_and_ref_next(ci);
		ecm_db_connection_deref(ci);
		ci = cin;
	}

	/*
	 * Re-enable IPv6 frontend processing.
	 */
	ecm_front_end_ipv6_stop_temp(0);
	return NOTIFY_DONE;
}

static struct notifier_block ecm_db_ip6route_table_update_nb = {
	.notifier_call = ecm_db_ipv6_route_table_update_event,
};
#endif

/*
 * ecm_db_defunct_all_handler()
 * 	Proc handler to defunct ecm db
 */
static int ecm_db_defunct_all_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer, size_t *lenp, loff_t *ppos)
{
	/*
	 * Usage:
	 *
	 * To defunct ecm rules from ecm db
	 * echo 1 > /proc/sys/net/ecm/ecm_db/defunct_all
	 *
	 * To read status:
	 * cat /proc/sys/net/ecm/ecm_db/defunct_all
	 */

	char *buf;
	int ret;
	int num;
	int len;

	if (write) {
		ecm_db_connection_defunct_all();
		return *lenp;
	}

	buf = kzalloc(sizeof(int), GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}

	spin_lock_bh(&ecm_db_lock);
	num = _ecm_db_connection_count_get() + _ecm_db_mapping_count_get() + _ecm_db_host_count_get()
		 + _ecm_db_node_count_get() + _ecm_db_iface_count_get();
	spin_unlock_bh(&ecm_db_lock);

	len = scnprintf(buf, PAGE_SIZE, "%d\n", num);
	if (!len) {
		DEBUG_ERROR("Failed to read buf\n");
		kfree(buf);
		return -EINVAL;
	}

	ret = memory_read_from_buffer(buffer, *lenp, ppos, buf, len);
	*lenp = ret;
	kfree(buf);
	return 0;
}

/*
 * ecm_db_ipv4_route_handler()
 * 	Proc handler to disable IPV4 route table events
 */
static int ecm_db_ipv4_route_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer, size_t *lenp, loff_t *ppos)
{
	/*
	 * Usage:
	 *
	 * To disable IPV4 route table events
	 * echo 0 > /proc/sys/net/ecm/ecm_db/ipv4_route_handle
	 *
	 * To read status:
	 * cat /proc/sys/net/ecm/ecm_db/ipv4_route_handle
	 */

	int current_val;
	int ret;

	current_val = ecm_db_ipv4_route_enable;

	/*
	 * Write the variable with user input
	 */
	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		return ret;
	}

	if ((ecm_db_ipv4_route_enable != 0) && (ecm_db_ipv4_route_enable != 1)) {
		DEBUG_WARN("Invalid input. Valid values 0/1\n");
		ecm_db_ipv4_route_enable = current_val;
		return -EINVAL;
	}

	if (current_val == ecm_db_ipv4_route_enable) {
		DEBUG_WARN("No change in the current configuration.\n");
		return ret;
	}

	if (ecm_db_ipv4_route_enable) {
		ecm_front_end_ipv4_stop_temp(1);
		ret = ip_rt_register_notifier(&ecm_db_iproute_table_update_nb);
		if (ret) {
			DEBUG_ERROR("Could not register for the IPV4 routing events error:%d\n", ret);
		}
		ecm_front_end_ipv4_stop_temp(0);
		return ret;
	}

	ecm_front_end_ipv4_stop_temp(1);
	ret = ip_rt_unregister_notifier(&ecm_db_iproute_table_update_nb);
	if (ret) {
		DEBUG_ERROR("Could not unregister for the IPV4 routing events error:%d\n", ret);
	}
	ecm_front_end_ipv4_stop_temp(0);

	return ret;
}

/*
 * ecm_db_ipv6_route_handler()
 * 	Proc handler to disable IPV6 route table events
 */
#ifdef ECM_IPV6_ENABLE
static int ecm_db_ipv6_route_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer, size_t *lenp, loff_t *ppos)
{
	/*
	 * Usage:
	 *
	 * To disable IPV6 route table events
	 * echo 0 > /proc/sys/net/ecm/ecm_db/ipv6_route_handle
	 *
	 * To read status:
	 * cat /proc/sys/net/ecm/ecm_db/ipv6_route_handle
	 */

	int current_val;
	int ret;

	current_val = ecm_db_ipv6_route_enable;

	/*
	 * Write the variable with user input
	 */
	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		return ret;
	}

	if ((ecm_db_ipv6_route_enable != 0) && (ecm_db_ipv6_route_enable != 1)) {
		DEBUG_WARN("Invalid input. Valid values 0/1\n");
		ecm_db_ipv6_route_enable = current_val;
		return -EINVAL;
	}

	if (current_val == ecm_db_ipv6_route_enable) {
		DEBUG_WARN("No change in the current configuration.\n");
		return ret;
	}

	if (ecm_db_ipv6_route_enable) {
		ecm_front_end_ipv6_stop_temp(1);
		ret = rt6_register_notifier(&ecm_db_ip6route_table_update_nb);
		if (ret) {
			DEBUG_ERROR("Could not register for the IPV6 routing events error:%d\n", ret);
		}
		ecm_front_end_ipv6_stop_temp(0);
		return ret;
	}

	ecm_front_end_ipv6_stop_temp(1);
	ret = rt6_unregister_notifier(&ecm_db_ip6route_table_update_nb);
	if (ret) {
		DEBUG_ERROR("Could not unregister for the IPV6 routing events error:%d\n", ret);
	}
	ecm_front_end_ipv6_stop_temp(0);

	return ret;
}
#endif

static struct ctl_table ecm_db_ctl_table[] = {
	{
		.procname	= "defunct_all",
		.data		= NULL,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &ecm_db_defunct_all_handler,
	},
	{
		.procname	= "ipv4_route_handle",
		.data		= &ecm_db_ipv4_route_enable,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &ecm_db_ipv4_route_handler,
	},

#ifdef ECM_IPV6_ENABLE
	{
		.procname	= "ipv6_route_handle",
		.data		= &ecm_db_ipv6_route_enable,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &ecm_db_ipv6_route_handler,
	},
#endif
};

/*
 * ecm_db_init()
 */
int ecm_db_init(struct dentry *dentry)
{
	int ret = 0;

	DEBUG_INFO("ECM Module init\n");

	ecm_db_ctl_table_header = register_sysctl(ECM_DB_PATH, ecm_db_ctl_table);
	if (!ecm_db_ctl_table_header) {
		DEBUG_ERROR("Failed to create ecm db directory in sysctl\n");
		return -1;
	}

	if (!ecm_debugfs_create_dir("ecm_db", dentry, &ecm_db_dentry)) {
		DEBUG_ERROR("Failed to create ecm db directory in debugfs\n");
		unregister_sysctl_table(ecm_db_ctl_table_header);
		return -1;
	}

	/*
	 * Get a random seed for jhash()
	 */
	get_random_bytes(&ecm_db_jhash_rnd, sizeof(ecm_db_jhash_rnd));
	printk(KERN_INFO "ECM database jhash random seed: 0x%x\n", ecm_db_jhash_rnd);

	if (!ecm_db_connection_init(ecm_db_dentry)) {
		goto init_cleanup;
	}

	if (!ecm_db_host_init(ecm_db_dentry)) {
		goto init_cleanup_1;
	}

	if (!ecm_db_mapping_init(ecm_db_dentry)) {
		goto init_cleanup_2;
	}

	if (!ecm_db_node_init(ecm_db_dentry)) {
		goto init_cleanup_3;
	}

	ecm_db_iface_init(ecm_db_dentry);

	if (!ecm_debugfs_create_file("defunct_all", S_IRUGO | S_IWUSR, ecm_db_dentry,
					NULL, &ecm_db_defunct_all_fops)) {
		DEBUG_ERROR("Failed to create ecm db defunct_all file in debugfs\n");
		goto init_cleanup_4;
	}

	ecm_db_li = ecm_db_listener_alloc();
	if (!ecm_db_li) {
		DEBUG_ERROR("%px: Failed to allocate a listener instance\n", dentry);
		goto init_cleanup_4;
	}
	ecm_db_listener_add(ecm_db_li,
			NULL, /* ecm_notifier_iface_added */
			NULL, /* ecm_notifier_iface_removed */
			NULL, /* ecm_notifier_node_added */
			NULL, /* ecm_notifier_node_removed */
			NULL, /* ecm_notifier_host_added */
			NULL, /* ecm_notifier_host_removed */
			NULL, /* ecm_notifier_mapping_added */
			NULL, /* ecm_notifier_mapping_removed */
			ecm_notifier_connection_added,
			ecm_notifier_connection_removed,
			NULL, /* ecm_notifier_connection_final */
			NULL);

	/*
	 * Initialize the timer resources.
	 */
	ecm_db_timer_init();

	/*
	 * register for route table modification events
	 */
	if (ecm_db_ipv4_route_enable) {
		ret = ip_rt_register_notifier(&ecm_db_iproute_table_update_nb);
		if (ret) {
			DEBUG_ERROR("Could not register for the IPV4 routing events error:%d\n", ret);
		}
	}

#ifdef ECM_IPV6_ENABLE
	if (ecm_db_ipv6_route_enable) {
		ret = rt6_register_notifier(&ecm_db_ip6route_table_update_nb);
		if (ret) {
			DEBUG_ERROR("Could not register for the IPV6 routing events error:%d\n", ret);
		}
	}
#endif
	return ret;

init_cleanup_4:
	ecm_db_node_exit();
init_cleanup_3:
	ecm_db_mapping_exit();
init_cleanup_2:
	ecm_db_host_exit();
init_cleanup_1:
	ecm_db_connection_exit();
init_cleanup:
	ecm_debugfs_remove_recursive(ecm_db_dentry);
	unregister_sysctl_table(ecm_db_ctl_table_header);
	return -1;
}
EXPORT_SYMBOL(ecm_db_init);

/*
 * ecm_db_exit()
 */
void ecm_db_exit(void)
{
	DEBUG_INFO("ECM DB Module exit\n");

	int ret;

	spin_lock_bh(&ecm_db_lock);
	ecm_db_terminate_pending = true;
	spin_unlock_bh(&ecm_db_lock);

	/*
	 * unregister for route table update events only if registered.
	 */
	if (ecm_db_ipv4_route_enable) {
		ret = ip_rt_unregister_notifier(&ecm_db_iproute_table_update_nb);
		if (ret) {
			DEBUG_ERROR("Could not unregister for the IPV4 routing events error:%d\n", ret);
		}
	}

#ifdef ECM_IPV6_ENABLE
	if (ecm_db_ipv6_route_enable) {
		ret = rt6_unregister_notifier(&ecm_db_ip6route_table_update_nb);
		if (ret) {
			DEBUG_ERROR("Could not unregister for the IPV6 routing events error:%d\n", ret);
		}
	}
#endif
	ecm_db_connection_defunct_all();

	/*
	 * Clean-up the timer resources.
	 */
	ecm_db_timer_exit();

	if (ecm_db_li) {
		ecm_db_listener_deref(ecm_db_li);
		ecm_db_li = NULL;
	}

	/*
	 * Free the database.
	 */
	ecm_db_node_exit();
	ecm_db_mapping_exit();
	ecm_db_host_exit();
	ecm_db_connection_exit();

	/*
	 * Remove the debugfs files recursively.
	 */
	if (ecm_db_dentry) {
		ecm_debugfs_remove_recursive(ecm_db_dentry);
	}

	/*
	 * Unregister sysctl table header
	 */
	if (ecm_db_ctl_table_header) {
		unregister_sysctl_table(ecm_db_ctl_table_header);
	}
}
EXPORT_SYMBOL(ecm_db_exit);
