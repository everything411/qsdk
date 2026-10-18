/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/module.h>
#include <net/genetlink.h>
#include <net/sock.h>

#include "nss_dscpstats.h"
#include "nss_dscpstats_nl.h"
#include "exports/nss_dscpstats_nl_public.h"

/*
 * prototypes
 */
static int nss_dscpstats_nl_ops_enable(struct sk_buff *skb, struct genl_info *info);
static int nss_dscpstats_nl_ops_disable(struct sk_buff *skb, struct genl_info *info);
static int nss_dscpstats_nl_ops_reset(struct sk_buff *skb, struct genl_info *info);
static int nss_dscpstats_nl_ops_read_dump(struct sk_buff *skb, struct netlink_callback *cb);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_dscpstats_ops[] = {
	{.cmd = NSS_DSCPSTATS_ENABLE_MSG, .doit = nss_dscpstats_nl_ops_enable},   /* enable msg*/
	{.cmd = NSS_DSCPSTATS_DISABLE_MSG, .doit = nss_dscpstats_nl_ops_disable,},   /* disable msg*/
	{.cmd = NSS_DSCPSTATS_RESET_MSG, .doit = nss_dscpstats_nl_ops_reset,}, /* reset counters */
	{.cmd = NSS_DSCPSTATS_READ_MSG, .dumpit = nss_dscpstats_nl_ops_read_dump}, /* read statistics */
};

/*
 * dscp stats family definition
 */
static struct genl_family nss_dscpstats_family = {
	.name = NSS_DSCPSTATS_FAMILY,
	.hdrsize = sizeof(struct nss_dscpstats_nl_rule),
	.version = NSS_DSCPSTATS_VER,
	.maxattr = NSS_DSCPSTATS_MAX_MSG_TYPES,
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_dscpstats_ops,
	.n_ops = ARRAY_SIZE(nss_dscpstats_ops),
};

/*
 * nss_dscpstats_nl_ops_enable()
 *	enable message handler
 */
static int nss_dscpstats_nl_ops_enable(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_dscpstats_nl_rule *enable_rule;
	struct net *net = sock_net(skb->sk);
	struct sk_buff *skb_out;
	int ret;

	enable_rule = genl_info_userhdr(info);

	nss_dscpstats_trace("%s:  WAN %s dscp %llx\n", __func__, enable_rule->conf_msg.name,
			     enable_rule->conf_msg.dscp_mask);

	ret = nss_dscpstats_db_enable_dscp(enable_rule->conf_msg.dscp_mask, enable_rule->conf_msg.name);

	skb_out = skb_copy(skb, GFP_KERNEL);
	if (!skb_out) {
		nss_dscpstats_info("failed to enable alloc reply message\n");
		return 0;
	}

	enable_rule = genlmsg_data(NLMSG_DATA(skb_out->data));
	enable_rule->ret = ret;

	genlmsg_end(skb_out, enable_rule);
	genlmsg_unicast(net, skb_out, info->snd_portid);

	return 0;
}

/*
 * nss_dscpstats_nl_ops_disable()
 *	disable message handler
 */
static int nss_dscpstats_nl_ops_disable(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_dscpstats_nl_rule *disable_rule;
	struct net *net = sock_net(skb->sk);
	struct sk_buff *skb_out;
	int ret;

	disable_rule = genl_info_userhdr(info);

	nss_dscpstats_trace("%s:  WAN %s dscp %llx\n", __func__, disable_rule->conf_msg.name,
			     disable_rule->conf_msg.dscp_mask);

	ret = nss_dscpstats_db_disable_dscp(disable_rule->conf_msg.dscp_mask, disable_rule->conf_msg.name);

	skb_out = skb_copy(skb, GFP_KERNEL);
	if (!skb_out) {
		nss_dscpstats_info("failed to alloc sidable reply message\n");
		return 0;
	}

	disable_rule = genlmsg_data(NLMSG_DATA(skb_out->data));
	disable_rule->ret = ret;
	genlmsg_end(skb_out, disable_rule);
	genlmsg_unicast(net, skb_out, info->snd_portid);

	return 0;
}

/*
 * nss_dscpstats_nl_ops_reset()
 *	reset message handler
 */
static int nss_dscpstats_nl_ops_reset(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_dscpstats_nl_rule *reset_rule;
	struct net *net = sock_net(skb->sk);
	struct sk_buff *skb_out;
	int ret = 0;

	nss_dscpstats_trace("%s\n", __func__);

	reset_rule = genl_info_userhdr(info);

	nss_dscpstats_db_reset();

	skb_out = skb_copy(skb, GFP_KERNEL);
	if (!skb_out) {
		nss_dscpstats_info("failed to alloc reset reply message\n");
		return 0;
	}

	reset_rule = genlmsg_data(NLMSG_DATA(skb_out->data));
	reset_rule->ret = ret;
	genlmsg_end(skb_out, reset_rule);
	genlmsg_unicast(net, skb_out, info->snd_portid);

	return 0;
}

/*
 * nss_dscpstats_nl_ops_read_dump()
 *	read message handler
 */
static int nss_dscpstats_nl_ops_read_dump(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct nss_dscpstats_nl_read_msg *read_stats;
	struct nss_dscpstats_nl_read_ctx *read_ctx = (struct nss_dscpstats_nl_read_ctx *)cb->ctx;
	int hash_index = 0;
	int num_entry;

	nss_dscpstats_trace();

	/*
	 * since this function is called as part of NLM_F_DUMP, it is possible that tot_read_req
	 * could be incremented multiple times for single read netlink request.
	 */
	nss_dscpstats_ctx.common_stats.tot_read_req++;

	read_stats = genlmsg_put(skb, NETLINK_CB(cb->skb).portid,
					cb->nlh->nlmsg_seq,
					&nss_dscpstats_family, NLM_F_MULTI, NSS_DSCPSTATS_READ_MSG);
	if (!read_stats) {
		nss_dscpstats_warn("nlmsg put failed\n");
		return -ENOMEM;
	}

	read_stats->num_clients = 0;
	read_stats->more = false;

	num_entry = skb_tailroom(skb) / sizeof(struct nss_dscpstats_nl_stats_client);
	if (!num_entry) {
		genlmsg_cancel(skb, read_stats);
		return -ENOMEM;
	}

	spin_lock(&nss_dscpstats_ctx.lock);
	for (hash_index = read_ctx->hash_index; hash_index < nss_dscpstats_ctx.hash_size; hash_index++) {
		struct hlist_head *mac_list = &nss_dscpstats_ctx.mac_hash_table[hash_index];
		struct nss_dscpstats_mac_node *mac_obj;

		hlist_for_each_entry(mac_obj, mac_list, mac_node) {
			struct hlist_head *dscp_list = &mac_obj->dscp_list;
			struct nss_dscpstats_dscp_node *dscp_obj;
			uint32_t rx_bytes, tx_bytes;

			if (read_ctx->mac_obj && (read_ctx->mac_obj != mac_obj)) {
				continue;
			}
			read_ctx->mac_obj = NULL;

			hlist_for_each_entry(dscp_obj, dscp_list, dscp_node) {
				struct nss_dscpstats_nl_stats_client *client;

				if (read_ctx->dscp_obj && (read_ctx->dscp_obj != dscp_obj)) {
					continue;
				}
				read_ctx->dscp_obj = NULL;
				rx_bytes = dscp_obj->rxBytes;
				tx_bytes = dscp_obj->txBytes;
				if (tx_bytes || rx_bytes) {
					if (read_stats->num_clients < num_entry) {
						client = &read_stats->clients[read_stats->num_clients];
						client->rxBytes = rx_bytes;
						client->txBytes = tx_bytes;
						client->dscp = dscp_obj->dscp;
						memcpy(client->mac, &mac_obj->mac_addr, 6);
						read_stats->num_clients += 1;

						dscp_obj->rxBytes -= rx_bytes;
						dscp_obj->txBytes -= tx_bytes;
						continue;
					}

					/*
					 * the node with traffic bytes will not be freed
					 * so saving the node info to context.
					 */
					read_stats->more = true;
					read_ctx->dscp_obj = dscp_obj;
					read_ctx->mac_obj = mac_obj;
					goto done;
				}
			}
		}
	}

done:
	spin_unlock(&nss_dscpstats_ctx.lock);

	/*
	 * if there is no clients then we have to send one
	 * message to listener that no clients to read. after
	 * sending this message this function is again invoked
	 * since in previous iteration we returned non zero value
	 * so to indicate that there is nothing to send return zero
	 */
	if (!read_stats->num_clients && read_ctx->iter) {
		genlmsg_cancel(skb, read_stats);
		return 0;
	}

	read_ctx->iter += 1;
	read_ctx->hash_index = hash_index;
	skb_put(skb, read_stats->num_clients * sizeof(struct nss_dscpstats_nl_stats_client));
	genlmsg_end(skb, read_stats);
	return skb->len;
}

/*
 * nss_dscpstats_nl_init()
 *	register NETLINK ops with the family
 */
int nss_dscpstats_nl_init(void)
{
	int error;

	error = genl_register_family(&nss_dscpstats_family);
	if (error != 0) {
		nss_dscpstats_info("Error: unable to register client dscp stats family\n");
		return -1;
	}

	nss_dscpstats_info("netlink ops client dscp stats registered successfully");
	return 0;
}

/*
 * nss_dscpstats_nl_exit()
 *	unregister NETLINK ops with the family
 */
void nss_dscpstats_nl_exit(void)
{
	int error;

	error = genl_unregister_family(&nss_dscpstats_family);
	if (error != 0) {
		nss_dscpstats_info("Error: unable to unregister client dscp stats family\n");
	}
}
