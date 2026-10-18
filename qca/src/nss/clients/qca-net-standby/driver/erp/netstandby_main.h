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

#ifndef __NETSTANDBY_MAIN_H
#define __NETSTANDBY_MAIN_H

#include <uapi/linux/netstandby.h>

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 *  * If dynamic debug is enabled, use pr_debug.
 */
#define netstandby_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define netstandby_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define netstandby_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (NETSTANDBY_DEBUG_LEVEL < 2)
#define netstandby_warn(s, ...)
#else
#define netstandby_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (NETSTANDBY_DEBUG_LEVEL < 3)
#define netstandby_info(s, ...)
#else
#define netstandby_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (NETSTANDBY_DEBUG_LEVEL < 4)
#define netstandby_trace(s, ...)
#else
#define netstandby_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * debug message for module init and exit
 */
#define netstandby_info_always(s, ...) printk(KERN_INFO"%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)

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
#if defined(NETSTANDBY_WIFI_SS_ENABLE)
	NETSTANDBY_IFACE_TYPE_WIFI = 1,
#endif
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
	int acl_id;
	bool is_acl_valid;
	bool acl_id_register;
	struct sock *nl_sock;
	pid_t pid;
	netstandby_ptr_t sock_data;
};

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
};

void netstandby_trigger_work(struct work_struct *work);

/* Message send API(s) invoked by netlink layer */
enum netstandby_nl_ret netstandby_init_msg_send(struct netstandby_nl_msg *nl_msg);
enum netstandby_nl_ret netstandby_enter_msg_send(struct netstandby_nl_msg *nl_msg);
enum netstandby_nl_ret netstandby_exit_msg_send(struct netstandby_nl_msg *nl_msg);
enum netstandby_nl_ret netstandby_deinit_msg_send(struct netstandby_nl_msg *nl_msg);
#endif
