/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/etherdevice.h>

/*
 * Debug output levels
 * 0 = OFF
 * 1 = ASSERTS / ERRORS
 * 2 = 1 + WARN
 * 3 = 2 + INFO
 * 4 = 3 + TRACE
 */
#define DEBUG_LEVEL 1

#include "ecm_types.h"
#include "ecm_db_types.h"
#include "ecm_state.h"
#include "ecm_tracker.h"
#include "ecm_classifier.h"
#include "ecm_db.h"
#include "ecm_sdx_stats.h"

/*
 * sysctl sdx_interface table header
 */
static struct ctl_table_header *ecm_sdx_sysctl_tbl_hdr;

/*
 * Listener for db events
 * Based on the listener events some actions are taken on the classifiers host db
 */
struct ecm_db_listener_instance *ecm_sdx_li;

/* ecm_sdx_iface_removed()
 *	Invoked when a iface is removed from the DB.
 */
static void ecm_sdx_iface_removed(void *arg, struct ecm_db_iface_instance *ii)
{
	DEBUG_INFO("iface_name=%s if_type=%d\n",ii->name, ii->type);
	if(ii->type == ECM_DB_IFACE_TYPE_RAWIP) {
		ecm_sdx_stats_update_all_host_stats();
	}
}

/*
 * ecm_sdx_host_removed()
 *	Invoked when a host is removed from the DB.
 */
static void ecm_sdx_host_removed(void *arg, struct ecm_db_host_instance *hi)
{
	if (hi->rx_routed_bytes[ECM_DB_IFACE_TYPE_RAWIP] != 0 ||
		hi->tx_routed_bytes[ECM_DB_IFACE_TYPE_RAWIP] != 0) {
		ecm_sdx_stats_update_host_stats(hi->address,
				hi->rx_routed_bytes[ECM_DB_IFACE_TYPE_RAWIP],
				hi->tx_routed_bytes[ECM_DB_IFACE_TYPE_RAWIP],
				true);
	}
}

/*
 * ecm_sdx_stats_handler()
 *	Proc handler function for stats read/write operation.
 */
static int ecm_sdx_stats_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer,
				size_t *lenp, loff_t *ppos)
{
	/*
	 * Usage: for read
	 *	Reset stats for all clients
	 *	echo RESET_ALL > /proc/sys/net/ecm/ecm_sdx_stats/stats
	 *
	 *	cat /proc/sys/net/ecm/ecm_sdx_stats/stats
	 */

	DEBUG_TRACE("perform write=%d operation on stats_node \n", write);
	if (!write) {
		DEBUG_TRACE("stats handler ppos=%lld, lenp=%zu", *ppos, *lenp);
		if(*ppos == 0) {
			DEBUG_TRACE("call update_all_host_stats\n");
			ecm_sdx_stats_update_all_host_stats();
		}
		return ecm_sdx_stats_handler_read(buffer, lenp, ppos);
	}

	return ecm_sdx_stats_handler_write(buffer, lenp);
}

/*
 * ecm_sdx_per_client_stats_handler()
 *	Proc handler function for packet_stats read/write operation.
 */
static int ecm_sdx_per_client_stats_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer,
					size_t *lenp, loff_t *ppos)
{
	/*
	 * Usage:
	 *	Get stats for particular ip
	 *	echo PER_IP_STATS ip_address > /proc/sys/net/ecm/ecm_sdx_stats/per_ip_stats
	 *	echo "PER_IP_STATS 192.168.226.124 " > /proc/sys/net/ecm/ecm_sdx_stats/per_ip_stats
	 *	echo "PER_IP_STATS 2405:e700:874:78e:6cb4:dcd2:71ec:a87 " > /proc/sys/net/ecm/ecm_sdx_stats/per_ip_stats
	 *
	 *	Delete host entry from cache
	 *	echo DELETE ip_address > /proc/sys/net/ecm/ecm_sdx_stats/per_ip_stats
	 *	echo "DELETE 192.168.226.124 " > /proc/sys/net/ecm/ecm_sdx_stats/per_ip_stats
	 *	echo "DELETE 2405:e700:874:78e:6cb4:dcd2:71ec:a87 " > /proc/sys/net/ecm/ecm_sdx_stats/per_ip_stats
	 *
	 *	Reset stats for particular ip
	 *	echo RESET ip_address > /proc/sys/net/ecm/ecm_sdx_stats/per_ip_stats
	 *	echo "RESET 192.168.226.124 " > /proc/sys/net/ecm/ecm_sdx_stats/per_ip_stats
	 *	echo "RESET 2405:e700:874:78e:6cb4:dcd2:71ec:a87 " > /proc/sys/net/ecm/ecm_sdx_stats/per_ip_stats
	 *
	 *	Dump all host stats to the console:
	 *	cat /proc/sys/net/ecm/ecm_sdx_stats/per_ip_stats
	 */

	DEBUG_TRACE("perform write=%d operation on per_client_node \n", write);
	if (!write) {
		if(*ppos == 0) {
			DEBUG_TRACE("call update_all_host_stats \n");
			ecm_sdx_stats_update_per_ip_stats();
		}
		return ecm_sdx_stats_per_client_handler_read(buffer, lenp, ppos);
	}

	return ecm_sdx_stats_per_client_handler_write(buffer, lenp);
}

/*
 * ecm_sdx_iface_type_handler()
 *	Proc handler function for packet_stats_interface read/write operation.
 */
static int ecm_sdx_iface_type_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer,
					size_t *lenp, loff_t *ppos)
{
	/*
	 * Usage:
	 *	Get stats for particular ip
	 *	echo "bh_type" interface_name > /proc/sys/net/ecm/ecm_sdx_stats/interface_type
	 *	echo "bh_type wwan " > /proc/sys/net/ecm/ecm_sdx_stats/interface_type
	 *	echo "bh_type NULL " > /proc/sys/net/ecm/ecm_sdx_stats/interface_type
	 *
	 *	read packet_stats_interface_type
	 *	cat /proc/sys/net/ecm/ecm_sdx_stats/interface_type
	 */

	DEBUG_TRACE("perform write=%d on iface_node \n", write);
	if (write) {
		return ecm_sdx_stats_interface_type_handler_write(buffer, lenp);
	}

	return ecm_sdx_stats_interface_type_handler_read(buffer, lenp, ppos);
}

/*
 * ecm_sdx_lan_prefix_handler()
 *	Proc handler function for lan_prefixes read/write operation.
 */
static int ecm_sdx_lan_prefix_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer,
					size_t *lenp, loff_t *ppos)
{
	/*
	 * Usage:
	 *	Get stats for particular ip
	 *	echo ip_type ip_addr mask > /proc/sys/net/ecm/ecm_sdx_stats/lan_prefixes
	 *	echo "ipv4 192.168.224.1 255.255.252.0 " > /proc/sys/net/ecm/ecm_sdx_stats/lan_prefixes
	 *	echo "ipv6 2405:e700:874:7d3::/64 " > /proc/sys/net/ecm/ecm_sdx_stats/lan_prefixes
	 *
	 *	Dump lan_subnets
	 *	cat /proc/sys/net/ecm/ecm_sdx_stats/lan_prefixes
	 */

	DEBUG_TRACE("perform write=%d operation on lan_perfixes node \n", write);
	if (write) {
		return ecm_sdx_stats_lan_prefix_handler_write(buffer, lenp);
	}

	return ecm_sdx_stats_lan_prefix_handler_read(buffer, lenp, ppos);
}

static struct ctl_table ecm_sdx_sysctl_tbl[] = {
	{
		.procname	= "stats",
		.data		= NULL,
		.maxlen		= sizeof(int),
		.mode		= 0666,
		.proc_handler	= &ecm_sdx_stats_handler,
	},
	{
		.procname	= "per_ip_stats",
		.data		= NULL,
		.maxlen		= sizeof(int),
		.mode		= 0666,
		.proc_handler	= &ecm_sdx_per_client_stats_handler,
	},
	{
		.procname	= "interface_type",
		.data		= NULL,
		.maxlen		= sizeof(int),
		.mode		= 0666,
		.proc_handler	= &ecm_sdx_iface_type_handler,
	},
	{
		.procname	= "lan_prefixes",
		.data		= NULL,
		.maxlen		= sizeof(int),
		.mode		= 0666,
		.proc_handler	= &ecm_sdx_lan_prefix_handler,
	},
};

/*
 * ecm_sdx_init()
 *	Init function for ecm_sdx_interface.
 */
int ecm_sdx_init(struct dentry *dentry)
{
	printk(KERN_INFO "ecm_sdx_init\n");

	/*
	 * Register sysctl table for packet_stats handling
	 */
	ecm_sdx_sysctl_tbl_hdr = register_sysctl("/net/ecm/ecm_sdx_stats",
				ecm_sdx_sysctl_tbl);
	if (!ecm_sdx_sysctl_tbl_hdr) {
		DEBUG_ERROR("Unable to register ecm_sdx_sysctl_tbl");
		return -EINVAL;
	}

	/*
	 * Allocate listener instance to listen for db events
	 */
	ecm_sdx_li = ecm_db_listener_alloc();
	if (!ecm_sdx_li) {
		DEBUG_ERROR("Failed to allocate listener\n");
		goto err_stats_li;
	}

	/*
	 * Add the listener into the database
	 */
	ecm_db_listener_add(ecm_sdx_li,
			NULL /* ecm_sdx_iface_added */,
			ecm_sdx_iface_removed,
			NULL /* ecm_sdx_node_added */,
			NULL /* ecm_sdx_node_removed */,
			NULL /* ecm_sdx_host_added */,
			ecm_sdx_host_removed,
			NULL /* ecm_sdx_mapping_added */,
			NULL /* ecm_sdx_mapping_removed */,
			NULL /* ecm_sdx_connection_added */,
			NULL /* ecm_sdx_connection_removed */,
			NULL /* ecm_sdx_listener_final */,
			ecm_sdx_li);

	if (ecm_sdx_stats_init(dentry)) {
		DEBUG_ERROR("ecm_sdx_stats_init failed \n");
		goto error_sdx_stats;
	}

	return 0;

error_sdx_stats:
	ecm_db_listener_deref(ecm_sdx_li);
err_stats_li:
	unregister_sysctl_table(ecm_sdx_sysctl_tbl_hdr);
	printk(KERN_INFO "ecm_sdx_init failed\n");
	return -1;
}

/*
 * ecm_sdx_exit()
 *	Exit function for ecm_sdx_interface.
 */
void ecm_sdx_exit(void)
{
	printk(KERN_INFO "ecm_sdx_exit\n");

	ecm_sdx_stats_exit();

	/*
	 * Release our ref to the listener.
	 * This will cause it to be unattached to the db listener list.
	 * NOTE: Our thread refs will be released on final callback when
	 * we know there will be no more callbacks to it.
	 */
	ecm_db_listener_deref(ecm_sdx_li);

	if(ecm_sdx_sysctl_tbl_hdr) {
		unregister_sysctl_table(ecm_sdx_sysctl_tbl_hdr);
	}
}
