/*
 * Copyright (c) 2023-2025, Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * @file netstandby_msg_if.h
 *	NSS Netlink common headers
 */
#ifndef __NETSTANDBY_MSG_IF_H
#define __NETSTANDBY_MSG_IF_H
#include <linux/version.h>

#ifdef RM_QCA_PROP
#include <linux/netstandby.h>
#endif

#define NETSTANDBY_ENTER_NSS_FLAG_ACL_ID 0x1
#define NETSTANDBY_ENTER_NSS_FLAG_ACL_TUPLE 0x2
#define NETSTANDBY_ENTER_NSS_FLAG_ACL_DEFAULT 0x4
#define NETSTANDBY_ENTER_NSS_FLAG_SWITCH_PORT_ID 0x8
#define NETSTANDBY_ENTER_NSS_FLAG_SWITCH_PORT_ALL 0x10
#define NETSTANDBY_ENTER_NSS_FLAG_SWITCH_PORT_NONE 0x20

#ifndef __KERNEL__
#define NR_CPUS 4
#endif /* Kernel */

#if defined(MHT_ETH_INTF)
#define NETSTANDBY_MAX_ETH_PORTS	2	/* Miami board */
#else
#define NETSTANDBY_MAX_ETH_PORTS	6	/* Alder board */
#endif /* MHT_ETH_INTF */
/*
 * We are reserving below netlink number to create a kernel netlink socket
 * These socket will be used for Kernel to APP and APP to APP communication
 */
#define NETSTANDBY_MSG_MAX_INTERFACES 16
#define	NETSTANDBY_MSG_IFNAME_MAX 128

/* Config file name is in following format: wlanX:<filepath> */
#define NETSTANDBY_MSG_CONF_FILENAME_MAX 256

#if defined(RM_QCA_PROP) && (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
#define NETSTANDBY_MSG_WIFI_INVALID_LINK_ID -1
#endif

#define NETSTANDBY_ACL_COUNT_MAX	8

/*
 * netstandby_trigger_rule
 *	Trigger rule
 */
struct netstandby_trigger_rule {
	uint32_t valid_flags;                   /**< Indicates which field to consider for trigger rule */
	uint32_t src_ip[4];                     /**< Source IP address */
	uint8_t smac[6];                        /**< Source MAC address */
	uint32_t dest_ip[4];                    /**< Destination IP address */
	uint8_t dmac[6];                        /**< Destination MAC address */
	int protocol;                           /**< Protocol */
};

/*
 * netstandby_nss_info()
 */
struct netstandby_nss_info {
	int acl_id[NETSTANDBY_ACL_COUNT_MAX];	/**< IDs of the custom ACL rules created using ppecfg */
	int acl_cnt;				/**< Number of ACL rules configured for standby entry */
	uint32_t flags;				/**< Flag to identify features supported */
	uint8_t switch_port_id;			/**< ID (1 to 4) of the port of a switch which is used for trigger (optional) */
};

#ifndef RM_QCA_PROP
struct netstandby_conf_file {
	char ifname[NETSTANDBY_MSG_IFNAME_MAX];
	char filename[NETSTANDBY_MSG_CONF_FILENAME_MAX];
	struct netstandby_conf_file *prev;
	struct netstandby_conf_file *next;
};
#endif

/*
 * netstandby_wifi_info()
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
struct netstandby_wifi_info {
	int link_id[NETSTANDBY_MSG_MAX_INTERFACES];
	int link_id_cnt;
};
#endif

/*
 * netstandby_enter_msg()
 */
struct netstandby_enter_msg {
	struct netstandby_nss_info nss_info;		/**< NSS related information for wakeup */
	struct netstandby_trigger_rule trigger_rule;		/**< Trigger rule */
	char designated_wakeup_intf[NETSTANDBY_MSG_MAX_INTERFACES][NETSTANDBY_MSG_IFNAME_MAX];	/**< List of designated interface names for trigger based wakeup */
	int iface_cnt;		/**< Number of designated wakeup interfaces configured */
#ifndef RM_QCA_PROP
	struct netstandby_conf_file *conf_file; /**< Linked list of config files */
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
	struct netstandby_wifi_info wifi_info;
#endif
};

/*
 * netstandby_exit_msg()
 */
struct netstandby_exit_msg {
	uint32_t reserved;	/**< Reserved */
};

/*
 * netstandby_init_msg()
 */
struct netstandby_init_msg {
	pid_t pid;			/**< PID of the user daemon process that runs the netstandy state machine */
#ifdef RM_QCA_PROP
	uint32_t nss_sampling_timer;	/**< NSS sampling timer value in seconds for sending the stats */
	char wan_intf[NETSTANDBY_MAX_ETH_PORTS][NETSTANDBY_MSG_IFNAME_MAX];	/* WAN iface for ignoring the neigh add/delete events
											   in telemetry stats */
	int wan_iface_cnt;	/**< Number of WAN ifaces */
#endif
};

/*
 * netstandby_rule
 *	Netstandby rule message
 */
struct netstandby_rule {
	/*
	 * Request
	 */
	union {
		struct netstandby_init_msg init;	/**< Init message */
		struct netstandby_enter_msg enter;	/**< Enter message */
		struct netstandby_exit_msg exit;	/**< Exit message */
	} msg;
};

#ifdef RM_QCA_PROP
/*
 * netstandby_lan_client_telemetry_type
 *	Lan client telemetry bucket
 */
enum netstandby_lan_client_telemetry_type {
	NETSTANDBY_LAN_CLIENT_PERIOD_12AM_TO_6AM = 0,	/**< Telemetry stats in 12AM TO 6AM slot */
	NETSTANDBY_LAN_CLIENT_PERIOD_6AM_TO_12PM,	/**< Telemetry stats in 6AM TO 12PM slot */
	NETSTANDBY_LAN_CLIENT_PERIOD_12PM_TO_6PM,	/**< Telemetry stats in 12PM TO 6PM slot */
	NETSTANDBY_LAN_CLIENT_PERIOD_6PM_TO_12AM,	/**< Telemetry stats in 6PM TO 12AM slot */
	NETSTANDBY_LAN_CLIENT_TELEMETRY_MAX,
};

/*
 * netstandby_lan_telemetry
 * 	LAN client connection event statistics periodically reported to ErP service in user space
 */
struct netstandby_lan_telemetry {
	uint32_t total_active_lan_clients;	/**< Total Number of new active lan clients */
	uint32_t lan_client_connect_events;	/**< Number of new clients connected */
	uint32_t lan_client_disconnect_events;	/**< Number of clients disconnected */
};
#endif /* RM_QCA_PROP */

/*
 * netstandby_erp_eth_stats
 * 	Eth stats to ERP service
 * 	TODO: Evaluate sending send the raw data to RM
 */
struct netstandby_erp_eth_stats {
	uint64_t tx_bytes_diff;	/** < Tx bytes increase from previous sampling period */
	uint64_t rx_bytes_diff;	/** < Rx bytes increase from previous sampling period */
	uint64_t tx_pkts_diff;	/** < Tx pkts increase from previous sampling period */
	uint64_t rx_pkts_diff;	/** < Rx pkts increase from previous sampling period */
	char dev_name[NETSTANDBY_MSG_IFNAME_MAX];	/** < Holds the dev name of eth iface */
};

/*
 * netstandby_erp_cpu_usage
 * 	CPU stats info to ERP service
 */
struct netstandby_erp_cpu_usage {
	uint64_t non_idle_time;	/** < Non idle time of CPU in nanosec during the NSS sampling period */
	uint64_t compute_period;	/** < Period (in nanosec) for which the non-idle time of the CPU is computed */
};

/*
 * netstandby_erp_eth_link_up_events
 * 	ERP link up events to ERP service
 */
struct netstandby_erp_eth_link_up_events {
	uint32_t prev_link_up;	/** < Number of links up in the prev iteration */
	uint32_t curr_link_up;	/** < Number of links up in the curr iteration */
};

/*
 * netstandby_erp_ct_data_flows
 *	Number of data flow entries found in conntrack table after filtering
 *	control path (network management) flows such as DHCP/DNS etc.
 */
struct netstandby_erp_ct_data_flows {
	uint32_t prev_ct_data_flow_cnt;	/** < Number of valid data flows in conntrack table in previos sampling period */
	uint32_t curr_ct_data_flow_cnt;	/** < Number of valid data flows in conntrack table in current sampling period */
};

/*
 * netstandby_erp_nss_telemetry
 * 	NSS statistics that are periodically collected and synced to user space for ErP idle state detection
 */
struct netstandby_erp_nss_telemetry {
	struct netstandby_erp_eth_stats ethstats[NETSTANDBY_MAX_ETH_PORTS];	/** < Structure for sending the eth iface stats */
	int num_of_ifaces;	/** < Number of eth ifaces present in the system */
	struct netstandby_erp_cpu_usage cpuutil_telemetry[NR_CPUS];	/** < Structure sending the CPU non idle time stats */
	struct netstandby_erp_eth_link_up_events eth_link_up;	/** < Number of up eth links */
	uint64_t num_of_nf_ct;	/** < Number of conntrack entries in netfilter conntrack table */
	struct netstandby_erp_ct_data_flows ct_data_flows;	/** < Number of important ct data flows in prev and curr iteration */
};

#endif /* __NETSTANDBY_MSG_IF_H */


