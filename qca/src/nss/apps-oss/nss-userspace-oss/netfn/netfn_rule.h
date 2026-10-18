/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __NETFN_RULE_H
#define __NETFN_RULE_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/socket.h>
#include <linux/errno.h>
#include <net/if.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/if_ether.h>

/* Generic Netlink header */
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/genl.h>

#include <jansson.h>
#include "netfn_hlos.h"
#define ETH_ALEN 6
#define IPV6_LEN 4

/*
 * netfn_cmds
 */
enum {
	NETFN_CMD_RULE_UNSPEC,		/* Must NOT use element 0 */
	NETFN_CMD_GENL,
	NETFN_CMD_MAX,
};

/*
 * netfn_rule_sk
 * Structure containing necessary information to communicate with netfn-auto module.
 */
struct netfn_rule_sk {
	struct nl_sock *sk;
	int family_id;
};

/*
 * netfn_rule_config()
 * API used to send the rule to netfn-auto kernel module
 */
int netfn_rule_config(struct netfn_rule_sk *rs, json_t *root, const char *family);

/*
 * netfn_rule_init()
 * 	netfn rule init
 */
struct netfn_rule_sk *netfn_rule_init(const char *);

/*
 * netfn_rule_deinit()
 * 	netfn rule deinit
 */
void netfn_rule_deinit(struct netfn_rule_sk *rs);

#endif
