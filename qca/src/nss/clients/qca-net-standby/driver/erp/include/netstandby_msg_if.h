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
 * @file netstandby_msg_if.h
 *	NSS Netlink common headers
 */
#ifndef __NETSTANDBY_MSG_IF_H
#define __NETSTANDBY_MSG_IF_H

#define NETSTANDBY_ENTER_NSS_FLAG_ACL_ID 0x1
#define NETSTANDBY_ENTER_NSS_FLAG_ACL_TUPLE 0x2
#define NETSTANDBY_ENTER_NSS_FLAG_ACL_DEFAULT 0x4
#define NETSTANDBY_ENTER_NSS_FLAG_SWITCH_PORT_ID 0x8
#define NETSTANDBY_ENTER_NSS_FLAG_SWITCH_PORT_ALL 0x10
#define NETSTANDBY_ENTER_NSS_FLAG_SWITCH_PORT_NONE 0x20

/*
 * We are reserving below netlink number to create a kernel netlink socket
 * These socket will be used for Kernel to APP and APP to APP communication
 */
#define NETSTANDBY_MSG_MAX_INTERFACES 16
#define	NETSTANDBY_MSG_IFNAME_MAX 128

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
	int acl_id;		/**< ID of the custom ACL rule created using ppecfg */
	uint32_t flags;		/**< Flag to identify features supported */
	uint8_t switch_port_id;	/**< ID (1 to 4) of the port of a switch which is used for trigger (optional) */
};

/*
 * netstandby_enter_msg()
 */
struct netstandby_enter_msg {
	struct netstandby_nss_info nss_info;		/**< NSS related information for wakeup */
	struct netstandby_trigger_rule trigger_rule;		/**< Trigger rule */
	char designated_wakeup_intf[NETSTANDBY_MSG_MAX_INTERFACES][NETSTANDBY_MSG_IFNAME_MAX];	/**< List of designated interface names for trigger based wakeup */
	int iface_cnt;		/**< Number of designated wakeup interfaces configured */
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

#endif /* __NETSTANDBY_MSG_IF_H */


