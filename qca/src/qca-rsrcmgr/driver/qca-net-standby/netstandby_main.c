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
 * netstandby_main.c
 *	Main Handler
 */

#include <linux/kernel.h>
#include <linux/netstandby.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/if.h>
#include <linux/in.h>
#include <linux/netlink.h>
#include <linux/rcupdate.h>
#include <linux/etherdevice.h>
#include <linux/if_addr.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/if_vlan.h>
#include <linux/completion.h>
#include <linux/semaphore.h>
#include <linux/in.h>
#include <net/arp.h>
#include <net/genetlink.h>
#include <net/neighbour.h>
#include <net/net_namespace.h>
#include <net/route.h>
#include <net/sock.h>

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif


#include <netstandby_nl_if.h>
#include "netstandby_nss.h"
#include "netstandby_nl.h"
#include "netstandby_main.h"
#include "netstandby_nl_cmn.h"
#include <ppe_drv_port.h>

struct netstandby_gbl_ctx gbl_netstandby_ctx;

/*
 * netstandby_get_dev_type()
 *	Get device type
 */
static inline enum netstandby_iface_type netstandby_get_dev_type(char *name, struct net_device **return_dev)
{
	struct net_device *dev;
	int port_id;

	dev = dev_get_by_name(&init_net, name);
	if (!dev) {
		netstandby_warn("No valid dev for name %s\n", name);
		return NETSTANDBY_IFACE_TYPE_MAX;
	}

	*return_dev = dev;

	port_id = ppe_drv_port_num_from_dev(dev);

	if (port_id >=0 && port_id < PPE_DRV_PHYSICAL_MAX) {
		dev_put(dev);
		return NETSTANDBY_IFACE_TYPE_NSS;
	} else if (dev->ieee80211_ptr) {
		dev_put(dev);
#ifdef RM_QCA_PROP
		if (port_id < 0)
			return NETSTANDBY_IFACE_TYPE_MAX;
#endif
		return NETSTANDBY_IFACE_TYPE_WIFI;
	} else if (port_id < 0) {
		dev_put(dev);
		return NETSTANDBY_IFACE_TYPE_MAX;
	}

	dev_put(dev);
	return NETSTANDBY_IFACE_TYPE_PLATFORM;
}

/*
 * netstandby_send_completion_response()
 *	Send completion response
 */
void netstandby_send_completion_response(enum netstandby_notif_type ev_type, enum netstandby_subsystem_type type)
{
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;
	struct netstandby_system_info *info = &gbl_ctx->info[type];
	struct netstandby_nl_msg_info *ns_msg_info;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int msg_size;
	unsigned char *new_addr;

	ns_msg_info = kzalloc(sizeof(struct netstandby_nl_msg_info), GFP_KERNEL);
	if (!ns_msg_info) {
		netstandby_warn(KERN_ERR "Failed to allocate ns_msg_info\n");
		return;
	}

	msg_size = sizeof(struct netstandby_nl_msg_info);
	skb = nlmsg_new(msg_size, GFP_KERNEL);
	if (!skb) {
		kfree(ns_msg_info);
		netstandby_warn(KERN_ERR "Failed to allocate skb\n");
		return;
	}

	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, msg_size, 0);
	new_addr = (unsigned char *)nlmsg_data(nlh);

	ns_msg_info->event_type = NETSTANDBY_NL_KERNEL_EVENT;
	ns_msg_info->ns_msg.param.notif_type = ev_type;
	ns_msg_info->ns_msg.param.system_type = type;
	memcpy(new_addr, ns_msg_info, sizeof(struct netstandby_nl_msg_info));

	netstandby_nl_cmn_ucast_resp_internal(skb, info->nl_sock, info->pid);
	kfree(ns_msg_info);
}

/*
 * netstandby_trigger_completion_event()
 *	Send trigger completion event
 */
void netstandby_trigger_completion_event(void *app_data, struct netstandby_trigger_info *trigger_info)
{
	enum netstandby_subsystem_type type = trigger_info->system_type;
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;
	struct netstandby_system_info *info = &gbl_ctx->info[type];
	struct netstandby_nl_msg_info *ns_msg_info;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int msg_size;
	unsigned char *new_addr;

	if (!info->nl_sock) {
		netstandby_info_always("RM is stopped and so, trigger exit is not possible.\n"
				       "Reboot is the only option to exit standby mode\n");
		return;
	}

	ns_msg_info = kzalloc(sizeof(struct netstandby_nl_msg_info), GFP_KERNEL);
	if (!ns_msg_info) {
		netstandby_warn(KERN_ERR "Failed to allocate ns_msg_info\n");
		return;
	}

	msg_size = sizeof(struct netstandby_nl_msg_info);
	skb = nlmsg_new(msg_size, GFP_KERNEL);
	if (!skb) {
		kfree(ns_msg_info);
		netstandby_warn(KERN_ERR "Failed to allocate skb\n");
		return;
	}

	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, msg_size, 0);

	new_addr = (unsigned char *)nlmsg_data(nlh);
	ns_msg_info->event_type = NETSTANDBY_NL_KERNEL_EVENT;
	ns_msg_info->ns_msg.param.notif_type = NETSTANDBY_NOTIF_TRIGGER;
	ns_msg_info->ns_msg.param.system_type = type;
	memcpy(new_addr, ns_msg_info, sizeof(struct netstandby_nl_msg_info));

	netstandby_info("Sending Trigger completion to RM\n");
	netstandby_nl_cmn_ucast_resp_internal(skb, info->nl_sock, info->pid);
	kfree(ns_msg_info);
}

/*
 * netstandby_enter_completion_event()
 *	Enter standby completion event notification from subsystem
 */
void netstandby_enter_completion_event(void *app_data, struct netstandby_event_compl_info *event_info)
{
	enum netstandby_subsystem_type type = event_info->system_type;
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;
	struct netstandby_system_info *info = &gbl_ctx->info[type];
	struct netstandby_nl_msg_info *ns_msg_info;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int msg_size;
	unsigned char *new_addr;

	ns_msg_info = kzalloc(sizeof(struct netstandby_nl_msg_info), GFP_KERNEL);
	if (!ns_msg_info) {
		netstandby_warn(KERN_ERR "Failed to allocate ns_msg_info\n");
		return;
	}

	msg_size = sizeof(struct netstandby_nl_msg_info);
	skb = nlmsg_new(msg_size, GFP_KERNEL);
	if (!skb) {
		kfree(ns_msg_info);
		netstandby_warn(KERN_ERR "Failed to allocate skb\n");
		return;
	}

	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, msg_size, 0);
	new_addr = (unsigned char *)nlmsg_data(nlh);
	ns_msg_info->event_type = NETSTANDBY_NL_KERNEL_EVENT;
	ns_msg_info->ns_msg.param.notif_type = event_info->event_type;
	ns_msg_info->ns_msg.param.system_type = type;
	memcpy(new_addr, ns_msg_info, sizeof(struct netstandby_nl_msg_info));

	/*
	 * Mark the state as completed if we have received the completion from the last subsystem
	 */
	if (ns_msg_info->ns_msg.param.system_type == NETSTANDBY_SUBSYSTEM_TYPE_PLATFORM) {
		gbl_ctx->netstandby_state = NETSTANDBY_SYSTEM_ENTER_COMPLETED;
		gbl_ctx->netstandby_first_trigger = false;
	}

	netstandby_nl_cmn_ucast_resp_internal(skb, info->nl_sock, info->pid);
	kfree(ns_msg_info);
}

/*
 * netstandby_exit_completion_event()
 *	Enter standby completion event notification from subsystem
 */
void netstandby_exit_completion_event(void *app_data, struct netstandby_event_compl_info *event_info)
{
	enum netstandby_subsystem_type type = event_info->system_type;
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;
	struct netstandby_system_info *info = &gbl_ctx->info[type];
	struct netstandby_nl_msg_info *ns_msg_info;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int msg_size;
	unsigned char *new_addr;

	ns_msg_info = kzalloc(sizeof(struct netstandby_nl_msg_info), GFP_KERNEL);
	if (!ns_msg_info) {
		netstandby_warn(KERN_ERR "Failed to allocate ns_msg_info\n");
		return;
	}

	msg_size = sizeof(struct netstandby_nl_msg_info);
	skb = nlmsg_new(msg_size, GFP_KERNEL);
	if (!skb) {
		kfree(ns_msg_info);
		netstandby_warn(KERN_ERR "Failed to allocate skb\n");
		return;
	}

	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, msg_size, 0);

	new_addr = (unsigned char *)nlmsg_data(nlh);

	ns_msg_info->event_type = NETSTANDBY_NL_KERNEL_EVENT;
	ns_msg_info->ns_msg.param.notif_type = event_info->event_type;
	ns_msg_info->ns_msg.param.system_type = type;
	/*
	 * Restore state to INIT since exit is done
	 */
	gbl_ctx->netstandby_state = NETSTANDBY_SYSTEM_INIT_STATE;
	memcpy(new_addr, ns_msg_info, sizeof(struct netstandby_nl_msg_info));
	netstandby_nl_cmn_ucast_resp_internal(skb, info->nl_sock, info->pid);
	kfree(ns_msg_info);
}

#if defined(RM_QCA_PROP) && !defined(RM_QCA_256M_PROFILE)
/*
 * netstandby_dump_nss_telemetry
 * 	Dump the NSS telemetry
 */
void netstandby_dump_nss_telemetry(struct netstandby_erp_nss_telemetry *nss_telemetry)
{
	int i = 0;

	if (!nss_telemetry) {
		netstandby_warn(KERN_ERR "nss_telemetry is NULL\n");
		return;
	}

	netstandby_info("Dumping the NSS telemetry in netstandby driver\n");
	netstandby_info("Eth stats:\n");
	netstandby_info("Num of ifaces:%d\n", nss_telemetry->num_of_ifaces);
	for(i = 0; i < nss_telemetry->num_of_ifaces; i++) {
		netstandby_info("Dev:%s Tx bytes %llu Rx bytes %llu Tx pkts %llu Rx pkts %llu\n",
				nss_telemetry->ethstats[i].dev_name, nss_telemetry->ethstats[i].tx_bytes_diff,
				nss_telemetry->ethstats[i].rx_bytes_diff, nss_telemetry->ethstats[i].tx_pkts_diff,
				nss_telemetry->ethstats[i].rx_pkts_diff);
	}

	netstandby_info("CPU util stats:\n");
	for(i = 0; i < NR_CPUS; i++) {
		netstandby_info("CPU:%d non idle %llu compute %llu\n", i,
				nss_telemetry->cpuutil_telemetry[i].non_idle_time,
				nss_telemetry->cpuutil_telemetry[i].compute_period);
	}

	netstandby_info("New link up curr up dev %d prev up dev %d\n", nss_telemetry->eth_link_up.curr_link_up,
			nss_telemetry->eth_link_up.prev_link_up);

	netstandby_info("Curr imp ct %d Prev imp ct %d nf ct %llu\n", nss_telemetry->ct_data_flows.prev_ct_data_flow_cnt,
                        nss_telemetry->ct_data_flows.curr_ct_data_flow_cnt, nss_telemetry->num_of_nf_ct);
}

/*
 * netstandby_nss_telemetry_to_rm
 * 	Send the NSS telemetry to RM
 */
bool netstandby_nss_telemetry_to_rm(struct netstandby_erp_nss_telemetry *nss_telemetry)
{
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;
	struct netstandby_system_info *info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_NSS];
	struct netstandby_nl_msg_info *ns_msg_info;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int msg_size;
	unsigned char *new_addr;

	if (!info->nl_sock) {
		netstandby_warn(KERN_ERR "Sock is NULL\n");
		return false;
	}

	ns_msg_info = kzalloc(sizeof(struct netstandby_nl_msg_info), GFP_ATOMIC);
	if (!ns_msg_info) {
		netstandby_warn(KERN_ERR "Failed to allocate ns_msg_info\n");
		return false;
	}

	msg_size = sizeof(struct netstandby_nl_msg_info);
	skb = nlmsg_new(msg_size, GFP_ATOMIC);
	if (!skb) {
		kfree(ns_msg_info);
		netstandby_warn(KERN_ERR "Failed to allocate skb\n");
		return false;
	}

	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, msg_size, 0);

	new_addr = (unsigned char *)nlmsg_data(nlh);
	ns_msg_info->event_type = NETSTANDBY_NL_NSS_TELEMETRY;
	memcpy(&ns_msg_info->ns_msg.nss_telemetry, nss_telemetry, sizeof(struct netstandby_erp_nss_telemetry));
	memcpy(new_addr, ns_msg_info, sizeof(struct netstandby_nl_msg_info));

	netstandby_dump_nss_telemetry(nss_telemetry);
	netstandby_info("Sending NSS algo params to RM\n");
	netstandby_nl_cmn_ucast_resp_internal(skb, info->nl_sock, info->pid);
	kfree(ns_msg_info);
	return true;
}

#else
bool netstandby_nss_telemetry_to_rm(struct netstandby_erp_nss_telemetry *nss_telemetry)
{
	return false;
}

#endif /* RM_QCA_PROP && !RM_QCA_256M_PROFILE */

/*
 * netstandby_deinit_msg_send()
 */
enum netstandby_nl_ret netstandby_deinit_msg_send(struct netstandby_nl_msg *nl_msg)
{
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;
	struct netstandby_system_info *info;

	info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_NSS];

	if (!info->nl_sock) {
		netstandby_warn(KERN_ERR "Sock is not present\n");
		return NETSTANDBY_NL_RET_FAIL;
	}

#ifdef RM_QCA_PROP
	if (gbl_ctx->nss_sample.sampling_time_millisec) {
		cancel_delayed_work_sync(&gbl_ctx->nss_sample.sampling_work);
	}
#endif

	netlink_kernel_release(info->nl_sock);
	info->nl_sock = NULL;

	nl_msg->ret = NETSTANDBY_NL_RET_SUCCESS;
	return NETSTANDBY_NL_RET_SUCCESS;
}

/*
 * netstandby_init_msg_send()
 */
enum netstandby_nl_ret netstandby_init_msg_send(struct netstandby_nl_msg *nl_msg)
{
	struct netstandby_rule *rule = &nl_msg->rule;
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;
	struct netstandby_system_info *info;
	int status;
	int i = 0;
        struct netlink_kernel_cfg cfg = {0};
	struct sock *nl_sock = NULL;

	nl_sock = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
	if (!nl_sock) {
		netstandby_warn(KERN_ERR "Failed to create netlink socket\n");
		return NETSTANDBY_NL_RET_FAIL;
	}

	/*
	 * Register and exchange standby callbacks with NSS, Wi-Fi and platform drivers
	 */
	for (i = 0; i < NETSTANDBY_SUBSYSTEM_TYPE_MAX; i++) {
		if (i == NETSTANDBY_IFACE_TYPE_NSS) {
			info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_NSS];

			info->init_info.enter_cmp_cb = netstandby_enter_completion_event;
			info->init_info.exit_cmp_cb = netstandby_exit_completion_event;
			info->init_info.trigger_cb = netstandby_trigger_completion_event;
			info->type = NETSTANDBY_IFACE_TYPE_NSS;
			info->nl_sock = nl_sock;
			info->pid = rule->msg.init.pid;
			status = nss_dp_get_and_register_cb(&info->init_info);
			if (status != NETSTANDBY_SUCCESS) {
				netstandby_warn("NSS network callback register callback failed\n");
				return NETSTANDBY_NL_RET_FAIL;
			}

		}
#ifdef RM_QCA_PROP
		else if (i == NETSTANDBY_IFACE_TYPE_WIFI) {
			info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_WIFI];
			info->init_info.enter_cmp_cb = netstandby_enter_completion_event;
			info->init_info.exit_cmp_cb = netstandby_exit_completion_event;
			info->init_info.trigger_cb = netstandby_trigger_completion_event;
			info->type = NETSTANDBY_IFACE_TYPE_WIFI;
			info->nl_sock = nl_sock;
			info->pid = rule->msg.init.pid;

			status = netstandby_wifi_get_and_register_cb(&info->init_info);
			if (status != NETSTANDBY_SUCCESS) {
				netstandby_warn("WIFI network callback register callback failed\n");
				return NETSTANDBY_NL_RET_FAIL;
			}
		}
#endif

		else if (i == NETSTANDBY_IFACE_TYPE_PLATFORM) {
			info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_PLATFORM];
			info->init_info.enter_cmp_cb = netstandby_enter_completion_event;
			info->init_info.exit_cmp_cb = netstandby_exit_completion_event;
			info->init_info.trigger_cb = netstandby_trigger_completion_event;
			info->type = NETSTANDBY_IFACE_TYPE_PLATFORM;
			info->nl_sock = nl_sock;
			info->pid = rule->msg.init.pid;

			status = netstandby_platform_get_and_register_cb(&info->init_info);
			if (status != NETSTANDBY_SUCCESS) {
				netstandby_warn("Platform callback register callback failed\n");
				return NETSTANDBY_NL_RET_FAIL;
			}
		}
	}

#ifdef RM_QCA_PROP
	if (rule->msg.init.nss_sampling_timer)  {
		gbl_ctx->nss_sample.sampling_time_millisec = (SECS_TO_MILLI_SECS * rule->msg.init.nss_sampling_timer);
		schedule_delayed_work(&gbl_ctx->nss_sample.sampling_work,
				      msecs_to_jiffies(gbl_ctx->nss_sample.sampling_time_millisec));

		for(i = 0; i < rule->msg.init.wan_iface_cnt; i++) {
			memcpy(&gbl_ctx->wan_intf[i], &rule->msg.init.wan_intf[i], sizeof(gbl_ctx->wan_intf[i]));
		}

		gbl_ctx->wan_iface_cnt = rule->msg.init.wan_iface_cnt;
		netstandby_info("ERP service is enabled with sample timer as %d\n", rule->msg.init.nss_sampling_timer);
	}
#endif

	nl_msg->ret = NETSTANDBY_NL_RET_SUCCESS;
	return NETSTANDBY_NL_RET_SUCCESS;
}

/*
 * netstandby_enter_msg_send()
 */
enum netstandby_nl_ret netstandby_enter_msg_send(struct netstandby_nl_msg *nl_msg)
{
	struct netstandby_rule *rule = &nl_msg->rule;
	struct netstandby_enter_msg *msg = &rule->msg.enter;
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;
	struct netstandby_system_info *info = NULL;
	char *name;
	struct net_device *return_dev;
	int i;

	/*
	 * Check if system is already put into power down mode
	 */
	if (gbl_ctx->netstandby_state == NETSTANDBY_SYSTEM_ENTER_COMPLETED) {
		netstandby_warn("System already in standby mode\n");
		return NETSTANDBY_NL_RET_FAIL;
	}

	if (!gbl_ctx->iface_configured) {
		for (i = 0; i < msg->iface_cnt; i++) {
			enum netstandby_iface_type type;
			name = &msg->designated_wakeup_intf[i][0];

			type = netstandby_get_dev_type(name, &return_dev);

			switch (type) {
			case NETSTANDBY_IFACE_TYPE_NSS:
				info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_NSS];
				info->enter_info.dev[i] = return_dev;
				info->enter_info.iface_cnt++;
				break;

			case NETSTANDBY_IFACE_TYPE_WIFI:
#ifdef RM_QCA_PROP
				info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_WIFI];
				info->enter_info.dev[i] = return_dev;
				info->enter_info.iface_cnt++;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
				if (msg->wifi_info.link_id[i] == NETSTANDBY_MSG_WIFI_INVALID_LINK_ID) {
					netstandby_warn("link_id is not provided for iface=%s\n", info->enter_info.dev[i]->name);
					return NETSTANDBY_NL_RET_FAIL;
				}
				info->enter_info.wifi_info.link_id[i] =
					msg->wifi_info.link_id[i];
				info->enter_info.wifi_info.link_id_cnt++;
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0) */
#endif /* RM_QCA_PROP */
				break;
			case NETSTANDBY_IFACE_TYPE_PLATFORM:
				break;

			case NETSTANDBY_IFACE_TYPE_MAX:
			case NETSTANDBY_IFACE_TYPE_UNSUPPORTED:
				netstandby_warn("Enter failed due to invalid interface for type: %d\n", type);
				return NETSTANDBY_NL_RET_FAIL;
			}
		}

		gbl_ctx->iface_configured = true;
	}

	if (nl_msg->type == NETSTANDBY_SUBSYSTEM_TYPE_NSS) {
		info = &gbl_ctx->info[nl_msg->type];
		info->is_acl_valid = false;
		info->acl_id_register = false;
		info->acl_cnt = msg->nss_info.acl_cnt;

		/*
		 * For Miami Manhattan, we need to pass the user passed ID
		 */
		if ((msg->nss_info.flags & NETSTANDBY_ENTER_NSS_FLAG_SWITCH_PORT_ID) == NETSTANDBY_ENTER_NSS_FLAG_SWITCH_PORT_ID) {
			info->enter_info.nss_info.port_id = msg->nss_info.switch_port_id;
			info->enter_info.nss_info.flags |= NETSTANDBY_ENTER_NSS_FLAG_VALID_PORT_IDX;
		} else if ((msg->nss_info.flags & NETSTANDBY_ENTER_NSS_FLAG_SWITCH_PORT_ALL) == NETSTANDBY_ENTER_NSS_FLAG_SWITCH_PORT_ALL) {
			info->enter_info.nss_info.flags |= NETSTANDBY_ENTER_NSS_FLAG_VALID_PORT_ALL;
		} else if ((msg->nss_info.flags & NETSTANDBY_ENTER_NSS_FLAG_SWITCH_PORT_NONE) == NETSTANDBY_ENTER_NSS_FLAG_SWITCH_PORT_NONE) {
			info->enter_info.nss_info.flags |= NETSTANDBY_ENTER_NSS_FLAG_VALID_PORT_NONE;
		}

		/* Add default ACL rule if acl_rule is not provided by the user */
		for (i = 0; i < info->enter_info.iface_cnt; i++) {
			if ((msg->nss_info.flags & NETSTANDBY_ENTER_NSS_FLAG_ACL_DEFAULT) == NETSTANDBY_ENTER_NSS_FLAG_ACL_DEFAULT) {
				if (netstandby_acl_rule_create(&msg->trigger_rule, info->enter_info.dev[i], i, true)) {
					/* If any of the ACL rule create fails, destroy all ACL rules */
					while (i) {
						netstandby_acl_rule_destroy(info->acl_id[--i]);
					};
					return NETSTANDBY_NL_RET_FAIL;
				}
			} else if ((msg->nss_info.flags & NETSTANDBY_ENTER_NSS_FLAG_ACL_ID) == NETSTANDBY_ENTER_NSS_FLAG_ACL_ID) {
				if (i >= NETSTANDBY_ACL_COUNT_MAX) {
					netstandby_warn("acl limit exceeded\n");
					return NETSTANDBY_NL_RET_FAIL;
				}

				if (netstandby_acl_register(msg->nss_info.acl_id[i])) {
					/* If any of the ACL register fails, destroy all ACL rules */
					while (i) {
						netstandby_acl_unregister(info->acl_id[--i]);
					};
					return NETSTANDBY_NL_RET_FAIL;
				}

				info->acl_id_register = true;
				info->is_acl_valid = true;
				info->acl_id[i] = msg->nss_info.acl_id[i];
			} else if ((msg->nss_info.flags & NETSTANDBY_ENTER_NSS_FLAG_ACL_TUPLE) == NETSTANDBY_ENTER_NSS_FLAG_ACL_TUPLE) {
				if (netstandby_acl_rule_create(&msg->trigger_rule, info->enter_info.dev[i], i, false)) {
					/* If any of the ACL rule create fails, destroy all ACL rules */
					while (i) {
						netstandby_acl_rule_destroy(info->acl_id[--i]);
					};
					return NETSTANDBY_NL_RET_FAIL;

				}
			}
		}
	}
#ifdef RM_QCA_PROP
	else if (nl_msg->type == NETSTANDBY_SUBSYSTEM_TYPE_WIFI) {
		info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_WIFI];
	}
#endif
	else if (nl_msg->type == NETSTANDBY_SUBSYSTEM_TYPE_PLATFORM) {
		info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_PLATFORM];
	}

	if (info) {
		if (info->init_info.enter_cb) {
			if (info->init_info.enter_cb(info->init_info.app_data, &info->enter_info)) {
				netstandby_warn("%p: Entry standby failed for subsystem (%d)\n", gbl_ctx, nl_msg->type);
				nl_msg->ret = NETSTANDBY_NL_RET_FAIL;
				return NETSTANDBY_NL_RET_FAIL;
			}
		}

		/*
		 * If completion for enter standby not provided; then send dummy completion to
		 * daemon
		 */
		if (!info->init_info.enter_cmp_cb) {
			netstandby_send_completion_response(NETSTANDBY_NOTIF_ENTER_COMPLETE, nl_msg->type);
		}
	}

	nl_msg->ret = NETSTANDBY_NL_RET_SUCCESS;
	return NETSTANDBY_NL_RET_SUCCESS;
}

/*
 * netstandby_exit_msg_send()
 */
enum netstandby_nl_ret netstandby_exit_msg_send(struct netstandby_nl_msg *nl_msg)
{
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;
	struct netstandby_system_info *info = &gbl_ctx->info[nl_msg->type];
	struct netstandby_exit_info exit_info;

	/*
	 * Issue the exit command to the subsystem indicated in the message
	 */
	info = &gbl_ctx->info[nl_msg->type];

	/*
	 * Mark state as exit_in_progress, to control the number of exit trigger notifications to user space
	 */
	if (info->type == NETSTANDBY_SUBSYSTEM_TYPE_PLATFORM) {
		gbl_ctx->netstandby_state = NETSTANDBY_SYSTEM_EXIT_IN_PROGRESS;
	}

	if (info->init_info.exit_cb) {
		if (info->type == NETSTANDBY_SUBSYSTEM_TYPE_NSS) {
			/*
			 * Remove the wake up trigger if configured on the ethernet interface
			 */
			if (info->is_acl_valid) {
				if (info->acl_id_register) {
					int acl_count = info->acl_cnt;
					for (int i = 0; i < acl_count; i++) {
						netstandby_acl_unregister(info->acl_id[i]);
					}
				} else {
					for (int i = 0; i < info->enter_info.iface_cnt; i++) {
						netstandby_acl_rule_destroy(info->acl_id[i]);
					}
				}
			}

			gbl_ctx->iface_configured = false;
		}

		if (info->init_info.exit_cb(info->init_info.app_data, &exit_info)) {
			netstandby_warn("%p: Exit standby failed for subsystem (%d)\n", gbl_ctx, nl_msg->type);
			nl_msg->ret = NETSTANDBY_NL_RET_FAIL;
			return NETSTANDBY_NL_RET_FAIL;
		}
	}

	memset(&info->enter_info, 0, sizeof(info->enter_info));

	if (!info->init_info.exit_cmp_cb) {
		netstandby_send_completion_response(NETSTANDBY_NOTIF_EXIT_COMPLETE, nl_msg->type);
	}

	nl_msg->ret = NETSTANDBY_NL_RET_SUCCESS;
	return NETSTANDBY_NL_RET_SUCCESS;
}
