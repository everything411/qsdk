/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_bridge.h>

#include "ppe_mcast_mgr.h"
#include <nss_ppenl_base.h>

/*
 * Define MDBA_RTA macro if not available in kernel headers
 */
#ifndef MDBA_RTA
#define MDBA_RTA(r) ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct br_mdb_entry))))
#endif

/*
 * Global socket file descriptor for netlink
 */
int ppe_mcast_mgr_netlink_socket;

/*
 * ppe_mcast_mgr_parse_rtattr()
 *	Custom implementation of parse_rtattr function
 */
static void ppe_mcast_mgr_parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
	memset(tb, 0, sizeof(struct rtattr *) * (max + 1));

	while (RTA_OK(rta, len)) {
		if (rta->rta_type <= max) {
			tb[rta->rta_type] = rta;
		}
		rta = RTA_NEXT(rta, len);
	}
}

/*
 * ppe_mcast_mgr_netlink_msg_process()
 *	Process a netlink message
 */
static int ppe_mcast_mgr_netlink_msg_process(struct nlmsghdr *hdr) {
	struct br_port_msg *bpm;
	struct br_mdb_entry *entry = NULL;
	struct rtattr *tb[MDBA_MAX + 1];
	struct rtattr *mdb_tb[MDBA_MDB_MAX + 1];
	struct rtattr *entry_tb[MDBA_MDB_ENTRY_MAX + 1];
	struct rtattr *eattr_tb[MDBA_MDB_EATTR_MAX + 1];
	struct nss_ppenl_mcast_req nl_msg = {0};
	char ifname[IFNAMSIZ];
	char ip_str[INET_ADDRSTRLEN];
	char ip6_str[INET6_ADDRSTRLEN];
	void *src;
	int len;
	int error;

	/*
	 * Process only Add and Delete events
	 */
	if ((hdr->nlmsg_type != RTM_NEWMDB) && (hdr->nlmsg_type != RTM_DELMDB)) {
		return 0;
	}

	bpm = NLMSG_DATA(hdr);
	len = hdr->nlmsg_len;

	/*
	 * Check message length.
	 */
	if (len < NLMSG_LENGTH(sizeof(*bpm))) {
		ppe_mcast_mgr_log_error("Invalid MDB message length: %d, expected at least %zu\n",
				len, NLMSG_LENGTH(sizeof(*bpm)));
		return 0;
	}

	/*
	 * Parse top-level attributes
	 */
	ppe_mcast_mgr_parse_rtattr(tb, MDBA_MAX, (struct rtattr *)(bpm + 1),
			hdr->nlmsg_len - NLMSG_LENGTH(sizeof(*bpm)));

	/*
	 * Ignore messages without MDBA_MDB attribute (e.g., bridge link config changes)
	 * These are valid messages but not MDB entries, so just skip them
	 */
	if (!tb[MDBA_MDB]) {
		return 0;
	}

	/*
	 * Parse MDBA_MDB to get MDBA_MDB_ENTRY
	 */
	ppe_mcast_mgr_parse_rtattr(mdb_tb, MDBA_MDB_MAX, RTA_DATA(tb[MDBA_MDB]), RTA_PAYLOAD(tb[MDBA_MDB]));

	if (!mdb_tb[MDBA_MDB_ENTRY]) {
		ppe_mcast_mgr_log_error("MDBA_MDB_ENTRY attribute not found in message type %d\n",
				hdr->nlmsg_type);
		return 0;
	}

	/*
	 * Parse MDBA_MDB_ENTRY to get MDBA_MDB_ENTRY_INFO
	 */
	ppe_mcast_mgr_parse_rtattr(entry_tb, MDBA_MDB_ENTRY_MAX, RTA_DATA(mdb_tb[MDBA_MDB_ENTRY]),
			RTA_PAYLOAD(mdb_tb[MDBA_MDB_ENTRY]));

	if (!entry_tb[MDBA_MDB_ENTRY_INFO]) {
		ppe_mcast_mgr_log_error("MDBA_MDB_ENTRY_INFO attribute not found in message type %d\n",
				hdr->nlmsg_type);
		return 0;
	}

	/*
	 * Extract br_mdb_entry from MDBA_MDB_ENTRY_INFO
	 */
	entry = RTA_DATA(entry_tb[MDBA_MDB_ENTRY_INFO]);
	int entry_len = RTA_PAYLOAD(entry_tb[MDBA_MDB_ENTRY_INFO]);

	if (entry_len < sizeof(*entry)) {
		ppe_mcast_mgr_log_error("Invalid MDB entry length: %d, expected at least %zu\n",
				entry_len, sizeof(*entry));
		return 0;
	}

	if (hdr->nlmsg_type == RTM_NEWMDB) {
		ppe_mcast_mgr_log_trace("New MDB entry event detected:\n");
		nss_ppenl_mcast_init_req(&nl_msg, NSS_PPE_MCAST_CREATE_ENTRY);
	} else {
		ppe_mcast_mgr_log_trace("Deleted MDB entry event detected:\n");
		nss_ppenl_mcast_init_req(&nl_msg, NSS_PPE_MCAST_DELETE_ENTRY);
	}

	/*
	 * Parse MDB entry and fill netlink message info
	 */
	if (entry) {

		if (entry->ifindex) {
			if (if_indextoname(entry->ifindex, ifname) == NULL) {
				ppe_mcast_mgr_log_warn("Failed to get interface name for index %d\n", entry->ifindex);
				snprintf(ifname, sizeof(ifname), "if%d", entry->ifindex);
			}
			memcpy(&nl_msg.mc_entry.dev, ifname, sizeof(ifname));
			ppe_mcast_mgr_log_trace("\tInterface: %s (Index %d)\n", ifname, entry->ifindex);
		}

		if (ntohs(entry->addr.proto) == ETH_P_IP) {
			inet_ntop(AF_INET, &entry->addr.u.ip4, ip_str, sizeof(ip_str));
			nl_msg.mc_entry.is_v4 = true;
			nl_msg.mc_entry.gip.v4 = entry->addr.u.ip4;
			ppe_mcast_mgr_log_trace("\tMulticast Group: %s\n", ip_str);
		} else if (ntohs(entry->addr.proto) == ETH_P_IPV6) {
			inet_ntop(AF_INET6, &entry->addr.u.ip6, ip6_str, sizeof(ip6_str));
			memcpy(nl_msg.mc_entry.gip.v6, entry->addr.u.ip6.s6_addr32, sizeof(nl_msg.mc_entry.gip.v6));
			ppe_mcast_mgr_log_trace("\tMulticast Group: %s\n", ip6_str);
		}

		if (entry->vid) {
			nl_msg.mc_entry.vlan_enabled = true;
			nl_msg.mc_entry.vlan_id = entry->vid;
			ppe_mcast_mgr_log_trace("\tVLAN ID: %u\n", entry->vid);
		}

		/*
		 * Parse extended attributes that follow the br_mdb_entry structure
		 */
		if (entry_len > sizeof(*entry)) {
			ppe_mcast_mgr_parse_rtattr(eattr_tb, MDBA_MDB_EATTR_MAX,
				(struct rtattr *)((char *)entry + NLMSG_ALIGN(sizeof(*entry))),
				entry_len - NLMSG_ALIGN(sizeof(*entry)));

			if (eattr_tb[MDBA_MDB_EATTR_SOURCE]) {
				src = RTA_DATA(eattr_tb[MDBA_MDB_EATTR_SOURCE]);
				nl_msg.mc_entry.sip_enabled = true;

				if (ntohs(entry->addr.proto) == ETH_P_IP) {
					inet_ntop(AF_INET, src, ip_str, sizeof(ip_str));
					nl_msg.mc_entry.sip.v4 = *(__be32 *)src;
					ppe_mcast_mgr_log_trace("\tSource IP: %s\n", ip_str);
				} else if (ntohs(entry->addr.proto) == ETH_P_IPV6) {
					inet_ntop(AF_INET6, src, ip6_str, sizeof(ip6_str));
					memcpy(nl_msg.mc_entry.sip.v6, src, sizeof(nl_msg.mc_entry.sip.v6));
					ppe_mcast_mgr_log_trace("\tSource IP: %s\n", ip6_str);
				}
			}
		}
	}

	/*
	 * Send message
	 */
	error = nss_ppenl_mcast_send_req(&nl_msg);
	if (error < 0) {
		ppe_mcast_mgr_log_warn("Unable to send message for %s operation, error: %d\n",
			(hdr->nlmsg_type == RTM_NEWMDB) ? "add" : "delete", error);
	}

	return 0;
}

/*
 * Signal handler flag for graceful shutdown
 */
static volatile sig_atomic_t keep_running = 1;

/*
 * ppe_mcast_mgr_signal_handler()
 *	Handle shutdown signals gracefully
 */
void ppe_mcast_mgr_signal_handler(int signum) {
	ppe_mcast_mgr_log_info("Received signal %d, shutting down...\n", signum);
	keep_running = 0;
}

/*
 * ppe_mcast_mgr_netlink_msg_setup()
 *	Setup netlink socket for MDB events
 */
static int ppe_mcast_mgr_netlink_msg_setup(void) {
	struct sockaddr_nl local;
	int group = RTNLGRP_MDB;
	int ret;

	/* Create netlink socket */
	ppe_mcast_mgr_netlink_socket = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (ppe_mcast_mgr_netlink_socket < 0) {
		ppe_mcast_mgr_log_error("socket: %s\n", strerror(errno));
		return -1;
	}

	/* Setup local address */
	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_pid = getpid();

	/* Bind socket */
	if (bind(ppe_mcast_mgr_netlink_socket, (struct sockaddr *)&local, sizeof(local)) < 0) {
		ppe_mcast_mgr_log_error("socket bind: %s\n", strerror(errno));
		close(ppe_mcast_mgr_netlink_socket);
		return -1;
	}

	/* Join multicast group for MDB events */
	ret = setsockopt(ppe_mcast_mgr_netlink_socket, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, &group, sizeof(group));
	if (ret < 0) {
		ppe_mcast_mgr_log_error("socket setsockopt: %s\n", strerror(errno));
		close(ppe_mcast_mgr_netlink_socket);
		return -1;
	}

	return 0;
}

/*
 * ppe_mcast_mgr_netlink_msg_rcv()
 *	Receive and process netlink messages
 */
static int ppe_mcast_mgr_netlink_msg_rcv(void) {
	struct sockaddr_nl kernel;
	struct msghdr msg;
	struct iovec iov;
	char buffer[8192];
	struct nlmsghdr *nlh;
	int ret;

	/* Setup receive buffer */
	memset(&msg, 0, sizeof(msg));
	memset(&kernel, 0, sizeof(kernel));
	memset(&iov, 0, sizeof(iov));

	iov.iov_base = buffer;
	iov.iov_len = sizeof(buffer);

	kernel.nl_family = AF_NETLINK;

	msg.msg_name = &kernel;
	msg.msg_namelen = sizeof(kernel);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	/* Receive message */
	ret = recvmsg(ppe_mcast_mgr_netlink_socket, &msg, 0);
	if (ret < 0) {
		if (errno == EINTR) {
			/* Interrupted by signal, not an error */
			return 0;
		}
		ppe_mcast_mgr_log_error("recvmsg error: %s\n", strerror(errno));
		return -1;
	}

	/* Process message */
	for (nlh = (struct nlmsghdr *)buffer; NLMSG_OK(nlh, ret); nlh = NLMSG_NEXT(nlh, ret)) {
		if (nlh->nlmsg_type == NLMSG_DONE) {
			break;
		}

		if (nlh->nlmsg_type == NLMSG_ERROR) {
			struct nlmsgerr *err = (struct nlmsgerr *)NLMSG_DATA(nlh);
			if (err->error != 0) {
				ppe_mcast_mgr_log_error("Netlink error: %s\n", strerror(-err->error));
			}
			continue;
		}

		/* Process valid messages */
		ppe_mcast_mgr_netlink_msg_process(nlh);
	}

	return 0;
}

/*
 * main()
 *	main function for listening to MDB events
 */
int main(int argc, char **argv) {
	/*
	 * Setup signal handlers for graceful shutdown
	 */
	signal(SIGINT, ppe_mcast_mgr_signal_handler);
	signal(SIGTERM, ppe_mcast_mgr_signal_handler);

	/*
	 * Setup netlink socket for MDB events
	 */
	if (ppe_mcast_mgr_netlink_msg_setup() < 0) {
		ppe_mcast_mgr_log_error("Failed to setup netlink socket\n");
		return -1;
	}

	ppe_mcast_mgr_log_info("Successfully opened Netlink socket. Listening for MDB events...\n");

	/*
	 * Continuously listen to MDB events
	 */
	while (keep_running) {
		if (ppe_mcast_mgr_netlink_msg_rcv() < 0) {
			ppe_mcast_mgr_log_error("Error receiving netlink message\n");
			break;
		}
	}

	close(ppe_mcast_mgr_netlink_socket);
	ppe_mcast_mgr_log_info("Shutdown complete.\n");
	return 0;
}
