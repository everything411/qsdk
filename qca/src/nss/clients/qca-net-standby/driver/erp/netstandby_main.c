/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
	if (port_id < 0) {
		dev_put(dev);
		return NETSTANDBY_IFACE_TYPE_MAX;
	}

	if (port_id < PPE_DRV_PHYSICAL_MAX) {
		dev_put(dev);
		return NETSTANDBY_IFACE_TYPE_NSS;
	} else if (dev->ieee80211_ptr) {
#if defined(NETSTANDBY_WIFI_SS_ENABLE)
		dev_put(dev);
		return NETSTANDBY_IFACE_TYPE_WIFI;
#else
		/* For open profile, wifi is not supported */
		return NETSTANDBY_IFACE_TYPE_UNSUPPORTED;
#endif
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
		netstandby_warn(KERN_ERR "Failed to allocate skb\n");
		return;
	}

	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, msg_size, 0);
	new_addr = (unsigned char *)nlmsg_data(nlh);

	ns_msg_info->event_type = NETSTANDBY_NL_KERNEL_EVENT;
	ns_msg_info->ns_msg.param.event_type = ev_type;
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
		netstandby_warn(KERN_ERR "Failed to allocate skb\n");
		return;
	}

	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, msg_size, 0);

	new_addr = (unsigned char *)nlmsg_data(nlh);
	ns_msg_info->event_type = NETSTANDBY_NL_KERNEL_EVENT;
	ns_msg_info->ns_msg.param.event_type = NETSTANDBY_NOTIF_TRIGGER;
	ns_msg_info->ns_msg.param.system_type = type;
	memcpy(new_addr, ns_msg_info, sizeof(struct netstandby_nl_msg_info));

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
		netstandby_warn(KERN_ERR "Failed to allocate skb\n");
		return;
	}

	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, msg_size, 0);
	new_addr = (unsigned char *)nlmsg_data(nlh);
	ns_msg_info->event_type = NETSTANDBY_NL_KERNEL_EVENT;
	ns_msg_info->ns_msg.param.event_type = event_info->event_type;
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
		netstandby_warn(KERN_ERR "Failed to allocate skb\n");
		return;
	}

	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, msg_size, 0);

	new_addr = (unsigned char *)nlmsg_data(nlh);

	ns_msg_info->event_type = NETSTANDBY_NL_KERNEL_EVENT;
	ns_msg_info->ns_msg.param.event_type = event_info->event_type;
	ns_msg_info->ns_msg.param.system_type = type;
	/*
	 * Restore state to INIT since exit is done
	 */
	gbl_ctx->netstandby_state = NETSTANDBY_SYSTEM_INIT_STATE;
	memcpy(new_addr, ns_msg_info, sizeof(struct netstandby_nl_msg_info));
	netstandby_nl_cmn_ucast_resp_internal(skb, info->nl_sock, info->pid);
	kfree(ns_msg_info);
}

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
#if defined(NETSTANDBY_WIFI_SS_ENABLE)
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
	struct netstandby_system_info *info;
	char *name;
	struct net_device *return_dev;
	int i;

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

#if defined(NETSTANDBY_WIFI_SS_ENABLE)
			case NETSTANDBY_IFACE_TYPE_WIFI:
				info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_WIFI];
				info->enter_info.dev[i] = return_dev;
				info->enter_info.iface_cnt++;
				break;
#endif
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
				if (netstandby_acl_rule_create(&msg->trigger_rule, info->enter_info.dev[i], true)) {
					return NETSTANDBY_NL_RET_FAIL;
				}
			} else if ((msg->nss_info.flags & NETSTANDBY_ENTER_NSS_FLAG_ACL_ID) == NETSTANDBY_ENTER_NSS_FLAG_ACL_ID) {
				if (netstandby_acl_register(msg->nss_info.acl_id)) {
					return NETSTANDBY_NL_RET_FAIL;
				}

				info->acl_id_register = true;
				info->is_acl_valid = true;
				info->acl_id = msg->nss_info.acl_id;
			} else if ((msg->nss_info.flags & NETSTANDBY_ENTER_NSS_FLAG_ACL_TUPLE) == NETSTANDBY_ENTER_NSS_FLAG_ACL_TUPLE) {
				if (netstandby_acl_rule_create(&msg->trigger_rule, info->enter_info.dev[i], false)) {
					return NETSTANDBY_NL_RET_FAIL;
				}
			}
		}
	}
#if defined(NETSTANDBY_WIFI_SS_ENABLE)
	else if (nl_msg->type == NETSTANDBY_SUBSYSTEM_TYPE_WIFI) {
		info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_WIFI];
	}
#endif
	else if (nl_msg->type == NETSTANDBY_SUBSYSTEM_TYPE_PLATFORM) {
		info = &gbl_ctx->info[NETSTANDBY_SUBSYSTEM_TYPE_PLATFORM];
	}

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
					netstandby_acl_unregister(info->acl_id);
				} else {
					netstandby_acl_rule_destroy(info->acl_id);
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
