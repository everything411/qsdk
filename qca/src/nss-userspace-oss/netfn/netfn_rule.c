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

#include "netfn_rule.h"
#include "netfn_parser.h"

/*
 * netfn_rule_sk_alloc()
 * 	create netlink socket based on the family.
 */
static struct netfn_rule_sk *netfn_rule_sk_alloc(const char *family)
{
	struct netfn_rule_sk *nrs = malloc(sizeof(struct netfn_rule_sk));
	if (!nrs) {
		netfn_log_error("Failed to allocate rule socket, family(%s)\n", family);
		return NULL;
	}

	nrs->sk = nl_socket_alloc();
	if(!nrs->sk) {
		netfn_log_error("%p:Failed to allocate NL_Sock, family(%s)\n", nrs, family);
		goto fail;
	}

	if (genl_connect(nrs->sk)) {
		netfn_log_error("%p:Failed to connect GENL, family(%s)\n", nrs, family);
		goto fail;
	}

	/* resolve the generic nl family id*/
	nrs->family_id = genl_ctrl_resolve(nrs->sk, family);
	if(nrs->family_id < 0){
		netfn_log_error("Failed to connect family(%s, %d)\n", family, nrs->family_id);
		goto fail2;
	}

	nl_socket_disable_auto_ack(nrs->sk);
	nl_socket_disable_seq_check(nrs->sk);

	return nrs;
fail2:
	nl_close(nrs->sk);
fail:
	nl_socket_free(nrs->sk);
	free(nrs);
	return NULL;
}

/*
 * netfn_rule_config()
 * 	Prepair and send netlink msg.
 */
int netfn_rule_config(struct netfn_rule_sk *nrs, json_t *root, const char *family)
{
	struct nl_msg *msg;
	int err = 0;

	/*
	 * Allocate netlink msg
	 */
	msg = nlmsg_alloc();
	if(!msg) {
		netfn_log_error("%p:Failed to allocate NL message, family(%s)\n", nrs, family);
		return -ENOMEM;
	}

	/*
	 * generic cmd for every family
	 */
	if(!genlmsg_put(msg, 0, 0, nrs->family_id, 0, 0, NETFN_CMD_GENL, 0)) {
		netfn_log_error("%pFailed to put GENL message, family(%s)\n", nrs, family);
		goto fail;
	}

	/*
	 * parse the json file
	 */
	if (!netfn_parse_json(root, msg)) {
		netfn_log_error("%p:Failed to parse JSON(%p), family(%s)\n", nrs, root, family);
		goto fail;
	}

	/*
	 * send netlink msg
	 */
	err = nl_send_sync(nrs->sk, msg);
	if (err < 0) {
		netfn_log_error("%pFailed to send nl message, family(%s) with error(%d)\n", nrs, family, err);
		goto fail;
	}

	err = nl_recvmsgs_default(nrs->sk);
	if (err) {
		netfn_log_error("%pnl_recvmsgs_default returned with error (%d), family(%s)\n", nrs, err, family);
		goto fail;
	}

	netfn_log_info("%p:Rule is configured for family(%s)\n", nrs, family);
	return 0;
fail:
	nlmsg_free(msg);
	return -EINVAL;
}

/*
 * netfn_rule_init()
 * 	netlink socket rule init.
 */
struct netfn_rule_sk * netfn_rule_init(const char *family)
{
	return(netfn_rule_sk_alloc(family));
}

/*
 * netfn_rule_deinit()
 * 	netlink socket deinit.
 */
void netfn_rule_deinit(struct netfn_rule_sk *nrs)
{
	nl_close(nrs->sk);
	nl_socket_free(nrs->sk);
	free(nrs);
}
