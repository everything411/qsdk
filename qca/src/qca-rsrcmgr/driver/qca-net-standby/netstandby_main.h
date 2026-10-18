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

#ifndef __NETSTANDBY_MAIN_H
#define __NETSTANDBY_MAIN_H

#include <uapi/linux/netstandby.h>
#ifdef RM_QCA_PROP
#include <nss_dp_api_if.h>
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 *  * If dynamic debug is enabled, use pr_debug.
 */
#define netstandby_warn(s, ...) pr_debug("[%d]:" s, __LINE__, ##__VA_ARGS__)
#define netstandby_info(s, ...) pr_debug("[%d]:" s, __LINE__, ##__VA_ARGS__)
#define netstandby_trace(s, ...) pr_debug("[%d]:" s, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (NETSTANDBY_DEBUG_LEVEL < 2)
#define netstandby_warn(s, ...)
#else
#define netstandby_warn(s, ...) pr_warn("[%d]:" s, __LINE__, ##__VA_ARGS__)
#endif

#if (NETSTANDBY_DEBUG_LEVEL < 3)
#define netstandby_info(s, ...)
#else
#define netstandby_info(s, ...) pr_notice("[%d]:" s, __LINE__, ##__VA_ARGS__)
#endif

#if (NETSTANDBY_DEBUG_LEVEL < 4)
#define netstandby_trace(s, ...)
#else
#define netstandby_trace(s, ...) pr_info("[%d]:" s, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * debug message for module init and exit
 */
#define netstandby_info_always(s, ...) printk(KERN_INFO"[%d]:" s, __LINE__, ##__VA_ARGS__)

#ifdef RM_QCA_PROP
#define SECS_TO_MILLI_SECS 1000
#endif

#define NETSTANDBY_ACL_COUNT_MAX	8

extern struct netstandby_gbl_ctx gbl_netstandby_ctx;

/*
 * netstandby_sytem_state
 */
enum netstandby_sytem_state {
	NETSTANDBY_SYSTEM_INIT_STATE = 0,
	NETSTANDBY_SYSTEM_ENTER_IN_PROGRESS,
	NETSTANDBY_SYSTEM_ENTER_COMPLETED,
	NETSTANDBY_SYSTEM_EXIT_IN_PROGRESS,
	NETSTANDBY_SYSTEM_EXIT_COMPLETED,
};

/*
 * netstandby_iface_type
 *	Interface type
 */
enum netstandby_iface_type {
	NETSTANDBY_IFACE_TYPE_NSS = 0,
	NETSTANDBY_IFACE_TYPE_WIFI = 1,
	NETSTANDBY_IFACE_TYPE_PLATFORM = 2,
	NETSTANDBY_IFACE_TYPE_UNSUPPORTED = 3,
	NETSTANDBY_IFACE_TYPE_MAX,
};

struct netstandby_interface {
	char *designated_wakeup_intf[MAX_INTERFACE];
	int iface_cnt;
};

/*
 * netstandby_system_info
 *	Network standby information
 */
struct netstandby_system_info {
	struct netstandby_reg_info init_info;
	struct netstandby_entry_info enter_info;
	int type;

	/*
	 * Socket information for event completion/exit completion/trigger completion
	 */
	int acl_id[NETSTANDBY_ACL_COUNT_MAX];
	int acl_cnt;
	bool is_acl_valid;
	bool acl_id_register;
	struct sock *nl_sock;
	pid_t pid;
	netstandby_ptr_t sock_data;
};

#ifdef RM_QCA_PROP
/*
 * netstandby_erp_eth_sample
 * 	Struct for maintaining Ethenert iface stats
 */
struct netstandby_erp_eth_sample {
	uint64_t cur_tx_bytes;	/** < Holds the curr Tx bytes of eth iface from dev stats */
	uint64_t cur_rx_bytes;	/** < Holds the curr Rx bytes of eth iface from dev stats */
	uint64_t prev_tx_bytes;	/** < Holds the prev Tx bytes of eth iface from dev stats */
	uint64_t prev_rx_bytes;	/** <Holds the prev Rx bytes of eth iface from dev stats */
	uint64_t cur_tx_packets;	/** < Holds the curr Tx pkts of eth iface from dev stats */
	uint64_t cur_rx_packets;	/** < Holds the curr Rx pkts of eth iface from dev stats */
	uint64_t prev_tx_packets;	/** < Holds the prev Tx pkts of eth iface from dev stats */
	uint64_t prev_rx_packets;	/** < Holds the prev Rx pkts of eth iface from dev stats */
};

/*
 * netstandby_erp_cpuutil_sample
 * 	Struct for maintaining CPU utilization stats
 */
struct netstandby_erp_cpuutil_sample {
	uint64_t current_idle_sec;	/** < Holds the curr idle sec of the each CPU */
	uint64_t previous_idle_sec;	/** < Holds the prev idle sec of the each CPU */
	uint64_t curr_time;	/** < Holds the curr time */
	uint64_t prev_time;	/** < Holds the prev time */
};

/*
 * netstandby_nss_erp_sample
 * 	Struct for maintaining NSS telemetry stats
 */
struct netstandby_nss_erp_sample {
	struct delayed_work sampling_work;	/** < Delayed work for collecting the NSS algo params */
	uint32_t sampling_time_millisec;	/** < NSS sampling time value in millsec */
	struct netstandby_erp_eth_sample curr_eth_sample[NSS_DP_MAX_INTERFACES-1];	/** < Holds the stats of all eth ifaces */
	struct netstandby_erp_cpuutil_sample curr_cpuutil_sample[NR_CPUS];	/** < Holds the non idle of each CPU in the sample time */
};
#endif

/*
 * netstandby_gbl_ctx
 *	Global context
 */
struct netstandby_gbl_ctx {
	struct netstandby_system_info info[NETSTANDBY_SUBSYSTEM_TYPE_MAX];
	struct netstandby_interface iface[NETSTANDBY_SUBSYSTEM_TYPE_MAX];
	struct delayed_work trigger_work;          /* free worker */
	enum netstandby_sytem_state netstandby_state;
	bool netstandby_first_trigger;
	bool iface_configured;
	bool cli_based_wakeup;
	bool is_acl_default;
#ifdef RM_QCA_PROP
	struct netstandby_nss_erp_sample nss_sample;	/** < Maintains the NSS sampling config & NSS algo params */
	struct nss_dp_eth_netdev_info ethinfo[NSS_DP_MAX_INTERFACES-1];	/** < Holds the devlist of NSS */
	char wan_intf[NETSTANDBY_MAX_ETH_PORTS][NETSTANDBY_MSG_IFNAME_MAX];	/** < WAN iface */
	int wan_iface_cnt;	/** < Number of WAN ifaces */
#endif
};

void netstandby_trigger_work(struct work_struct *work);
#ifdef RM_QCA_PROP
void netstandby_nss_sampling_work_cb(struct work_struct *work);
#endif

/* Message send API(s) invoked by netlink layer */
enum netstandby_nl_ret netstandby_init_msg_send(struct netstandby_nl_msg *nl_msg);
enum netstandby_nl_ret netstandby_enter_msg_send(struct netstandby_nl_msg *nl_msg);
enum netstandby_nl_ret netstandby_exit_msg_send(struct netstandby_nl_msg *nl_msg);
enum netstandby_nl_ret netstandby_deinit_msg_send(struct netstandby_nl_msg *nl_msg);
#ifdef RM_QCA_PROP
bool netstandby_nss_telemetry_to_rm(struct netstandby_erp_nss_telemetry *nss_telemetry);
#endif
#endif
