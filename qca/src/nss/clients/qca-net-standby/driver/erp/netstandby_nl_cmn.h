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
 * netstandby_nl_cmn.h
 *	Netstandby Netlink internal definitions
 */
#ifndef __NETSTANDBY_NL_CMN_H
#define __NETSTANDBY_NL_CMN_H

#define NETSTANDBY_NETLINK_INIT_FAMILY(family, pre_fn, post_fn) {	\
	.id = GENL_ID_GENERATE,	\
	.name = NETSTANDBY_NETLINK_##family##FAMILY,	\
	.hdrsize = 0,	\
	.version = NETSTANDBY_NETLINK_VER,	\
	.maxattr = NETSTANDBY_NETLINK_##family##_CMD_MAX,	\
	.netnsok = true,	\
	.pre_doit = pre_fn,	\
	.post_doit = post_fn,	\
}

#define NETSTANDBY_NETLINK_INIT_POLICY(cmd, ds) {	\
	.type = NLA_BINARY,	\
	.len = sizeof(struct ##ds),	\
}

typedef bool (*netstandby_nl_cmn_fn_t)(void);

/*
 * *************
 * Generic API(s)
 * *************
 */

/*
 * Copy NSS NL message buffer into a new SKB
 */
struct sk_buff *netstandby_nl_cmn_copy_msg(struct sk_buff *orig);

/*
 * Get NSS NL message pointer for a given command
int netstandby_nl_cmn_ucast_resp(struct sk_buff *skb);
 */
struct netstandby_nl_cmn *netstandby_nl_cmn_get_msg(struct genl_family *family, struct genl_info *info, uint16_t cmd);

/*
 * returns the start of netstandby_nl_cmn_ payload
 */
void *netstandby_nl_cmn_get_data(struct sk_buff *skb);

/*
 * unicast response to the user
 */
int netstandby_nl_cmn_ucast_resp(struct sk_buff *skb);

/*
 * unicast response to the user
 */
int netstandby_nl_cmn_ucast_resp_internal(struct sk_buff *skb, struct sock *sk, pid_t pid);

/*
 * psnl init/exit function
 */
void netstandby_nl_cmn_init(void);
void netstandby_nl_cmn_exit(void);

#endif /* __NETSTANDBY_NL_CMN_H */
