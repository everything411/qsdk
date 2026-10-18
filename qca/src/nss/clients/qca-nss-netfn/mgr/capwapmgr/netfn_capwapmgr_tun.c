/*
 * netfn_capwapmgr.c
 *	Network function's CAPWAP manager tunnel config APIs.
 *
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

#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/ipv6.h>
#include <linux/timer.h>
#include <linux/seq_file.h>
#include <linux/rtnetlink.h>

#include <ppe_drv.h>
#include <netfn_capwapmgr.h>
#include "netfn_capwapmgr_priv.h"
#include "netfn_capwapmgr_tun.h"

/*
 * netfn_capwapmgr_tun_stats_sync
 *	Stats sync timer callback.
 */
static void netfn_capwapmgr_tun_stats_sync(struct timer_list *t)
{
	struct netfn_capwapmgr_tun_ctx *ctx = from_timer(ctx, t, stats_sync);
	unsigned long next_timer_jiffies;

	struct netfn_flowmgr_flow_conn_stats stats = {0};
	struct netfn_flowmgr_conn_stats *conn_stats = NULL;
	struct netfn_capwapmgr_flow_stats *tun_stats = &ctx->stats;

	/*
	 * Copy the tuple information.
	 * Flow manager uses original tuple information
	 * to lookup per connection stats.
	 */
	memcpy(&stats.tuple, &ctx->cfg.tuple, sizeof(netfn_tuple_t));

	/*
	 * Get the stats associated with the tuple information from the flowmgr.
	 */
	netfn_flowmgr_get_conn_stats(&stats, NETFN_FLOWMGR_ACCEL_MODE_PPE);

	/*
	 * Update the per tunnel stats counters.
	 */
	conn_stats = &stats.conn_stats;
	tun_stats->flow_tx_pkts += conn_stats->org_tx_pkt_count;
	tun_stats->flow_tx_bytes += conn_stats->org_tx_byte_count;
	tun_stats->flow_rx_pkts += conn_stats->org_rx_pkt_count;
	tun_stats->flow_rx_bytes += conn_stats->org_rx_byte_count;

	tun_stats->return_tx_pkts += conn_stats->reply_tx_pkt_count;
	tun_stats->return_tx_bytes += conn_stats->reply_tx_byte_count;
	tun_stats->return_rx_pkts += conn_stats->reply_rx_pkt_count;
	tun_stats->return_rx_bytes += conn_stats->reply_rx_byte_count;

	/*
	 * If tunnel free is not in progress, re-attach the timer.
	 */
	if (atomic64_read(&ctx->timer_resched)) {
		next_timer_jiffies = jiffies + msecs_to_jiffies(NETFN_CAPWAPMGR_STATS_SYNC_FREQ);
		mod_timer(&ctx->stats_sync, next_timer_jiffies);
	}
}

/*
 * netfn_capwapmgr_tun_stats_print()
 *	API to print tunnel stats
 */
static int netfn_capwapmgr_tun_stats_print(struct seq_file *sf, void *ptr)
{
	struct netfn_capwapmgr_tun_ctx *ctx = (struct netfn_capwapmgr_tun_ctx *)sf->private;
	struct netfn_capwapmgr_flow_stats *flow_stats = NULL;
	struct netfn_capwap_tun_stats stats = {0};
	struct netfn_capwap_pkt_stats *pkt_stats;
	struct netfn_tuple_5tuple *ftuple = NULL;
	uint8_t ip_version;
	int ret = 0;

	ip_version = ctx->cfg.tuple.ip_version;
	ftuple = &ctx->cfg.tuple.tuples.tuple_5;

	/*
	 * Get the stats associated with the capwap device from the offload engine.
	 */
	ret = netfn_capwap_tun_stats_get(ctx->capwap_dev, &stats);
	if (ret) {
		netfn_capwapmgr_warn("Failed to get capwap offload stats with error %d\n", ret);
		return 0;
	}

	seq_puts(sf, "\n Tunnel Flow rule \n");
		seq_printf(sf, "\tIP Version:%d\n", ip_version);
		seq_printf(sf, "\tSource Port:0x%d\n", ntohs(ftuple->l4_src_ident));
		seq_printf(sf, "\tDestination Port:0x%d\n", ntohs(ftuple->l4_dest_ident));
	if (ip_version == NETFN_FLOWMGR_TUPLE_IP_VERSION_V4) {
		seq_printf(sf, "\tSource IP:%pI4n\n", &ftuple->src_ip.ip4.s_addr);
		seq_printf(sf, "\tDestination IP:%pI4n\n", &ftuple->dest_ip.ip4.s_addr);
	} else {
		seq_printf(sf, "\tSource IP:%pI6n\n", ftuple->src_ip.ip6.s6_addr32);
		seq_printf(sf, "\tDestination IP:%pI6n\n", ftuple->dest_ip.ip6.s6_addr32);
	}

	seq_puts(sf, "\n CAPWAP offload stats \n");

	pkt_stats = &stats.pkts;
	seq_printf(sf, "\t[%s]: %llu\n", "Tx Packets", pkt_stats->tx_pkts);
	seq_printf(sf, "\t[%s]: %llu\n", "Tx Bytes", pkt_stats->tx_bytes);
	seq_printf(sf, "\t[%s]: %llu\n", "Tx error", pkt_stats->tx_errors);
	seq_printf(sf, "\t[%s]: %llu\n", "Tx dropped", pkt_stats->tx_dropped);
	seq_printf(sf, "\t[%s]: %llu\n", "Rx Packets", pkt_stats->rx_pkts);
	seq_printf(sf, "\t[%s]: %llu\n", "Rx Bytes", pkt_stats->rx_bytes);
	seq_printf(sf, "\t[%s]: %llu\n", "Rx error", pkt_stats->rx_errors);
	seq_printf(sf, "\t[%s]: %llu\n", "Rx dropped", pkt_stats->rx_dropped);
	seq_puts(sf, "\n");

	seq_puts(sf, "\n Flow stats \n");

	flow_stats = &ctx->stats;
	seq_printf(sf, "\n Wan Dev: %s \n", ctx->cfg.flow.out_dev->name);
	seq_printf(sf, "\t[%s]: %llu\n", "Tx Packets", flow_stats->flow_tx_pkts);
	seq_printf(sf, "\t[%s]: %llu\n", "Tx Bytes", flow_stats->flow_tx_bytes);
	seq_printf(sf, "\t[%s]: %llu\n", "Rx packets", flow_stats->flow_rx_pkts);
	seq_printf(sf, "\t[%s]: %llu\n", "Rx Bytes", flow_stats->flow_rx_bytes);

	seq_puts(sf, "\n");

	if (ctx->dtls_dev) {
		seq_printf(sf, "\n DTLS Dev: %s \n", ctx->dtls_dev->name);
	} else {
		seq_printf(sf, "\n CAPWAP Dev: %s \n", ctx->capwap_dev->name);
	}

	seq_printf(sf, "\t[%s]: %llu\n", "Tx Packets", flow_stats->return_tx_pkts);
	seq_printf(sf, "\t[%s]: %llu\n", "Tx Bytes", flow_stats->return_tx_bytes);
	seq_printf(sf, "\t[%s]: %llu\n", "Rx packets", flow_stats->return_rx_pkts);
	seq_printf(sf, "\t[%s]: %llu\n", "Rx Bytes", flow_stats->return_rx_bytes);

	seq_puts(sf, "\n\n");
	return 0;
}

/*
 * netfn_capwapmgr_tun_stats_write()
 *	Clear netfn_capwapmgr_tun statistics.
 *
 * TODO: Add locks around context stats
 */
static ssize_t netfn_capwapmgr_tun_stats_write(struct file *filep, const char __user *buffer, size_t count, loff_t *ppos)
{
	struct seq_file *seq = filep->private_data;
	struct netfn_capwapmgr_tun_ctx *ctx = (struct netfn_capwapmgr_tun_ctx *)seq->private;
	uint32_t reset;

	if (kstrtou32_from_user(buffer, count, 0, &reset)) {
		return -EINVAL;
	}

	if (reset != 0) {
		return -EINVAL;
	}

	memset(&ctx->stats, 0, sizeof(struct netfn_capwapmgr_flow_stats));

	return count;
}

/*
 * netfn_capwapmgr_tun_stats_open
 *	Open file handler for tunnel statistics debugfs.
 */
static int netfn_capwapmgr_tun_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, netfn_capwapmgr_tun_stats_print, inode->i_private);
}

/*
 * netfn_capwapmgr_tun_stats_ops
 *	File operations for tunnel specific statistics
 */
const struct file_operations netfn_capwapmgr_tun_stats_ops = {
	.open = netfn_capwapmgr_tun_stats_open,
	.read = seq_read,
	.write = netfn_capwapmgr_tun_stats_write,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * netfn_capwapmgr_tun_get_reply
 *	Get reply directiction tuple
 */
static void netfn_capwapmgr_tun_get_reply(netfn_tuple_t *orig, netfn_tuple_t *reply)
{
	struct netfn_tuple_5tuple *orig_t5, *reply_t5;
	BUG_ON(orig == reply);

	orig_t5 = &orig->tuples.tuple_5;
	reply_t5 = &reply->tuples.tuple_5;

	reply->ip_version = orig->ip_version;
	reply->tuple_type = orig->tuple_type;
	reply_t5->protocol = orig_t5->protocol;

	if (orig->ip_version == 4) {
		reply_t5->src_ip.ip4.s_addr = orig_t5->dest_ip.ip4.s_addr;
		reply_t5->dest_ip.ip4.s_addr = orig_t5->src_ip.ip4.s_addr;
	} else {
		/*
		 * Get Source IP in reply direction.
		 */
		reply_t5->src_ip.ip6.s6_addr32[0] = orig_t5->dest_ip.ip6.s6_addr32[0];
		reply_t5->src_ip.ip6.s6_addr32[1] = orig_t5->dest_ip.ip6.s6_addr32[1];
		reply_t5->src_ip.ip6.s6_addr32[2] = orig_t5->dest_ip.ip6.s6_addr32[2];
		reply_t5->src_ip.ip6.s6_addr32[3] = orig_t5->dest_ip.ip6.s6_addr32[3];

		/*
		 * Get Destination IP in reply direction.
		 */
		reply_t5->dest_ip.ip6.s6_addr32[0] = orig_t5->src_ip.ip6.s6_addr32[0];
		reply_t5->dest_ip.ip6.s6_addr32[1] = orig_t5->src_ip.ip6.s6_addr32[1];
		reply_t5->dest_ip.ip6.s6_addr32[2] = orig_t5->src_ip.ip6.s6_addr32[2];
		reply_t5->dest_ip.ip6.s6_addr32[3] = orig_t5->src_ip.ip6.s6_addr32[3];
	}

	reply_t5->l4_src_ident = orig_t5->l4_dest_ident;
	reply_t5->l4_dest_ident = orig_t5->l4_src_ident;
}

/*
 * netfn_capwapmgr_flow_rule_create
 *	Create flow rule.
 *
 * This API creates flow rule based on the tunnel configuration.
 */
static netfn_capwapmgr_ret_t netfn_capwapmgr_flow_rule_create(struct netfn_capwapmgr_tun_cfg *cfg)
{
	struct netfn_flowmgr_create_rule original = {0};
	struct netfn_flowmgr_create_rule reply = {0};
	struct netfn_flowmgr_flow_info *flow = &cfg->flow;
	netfn_flowmgr_ret_t netfn_status = NETFN_FLOWMGR_RET_SUCCESS;
	uint32_t ext_valid_flags = cfg->ext_cfg.ext_valid_flags;

	/*
	 * Original flow rule will route the UL capwap packets from CAPWAP netdevice to WAN netdevice.
	 * Reply flow rule will route the packets from WAN netdevice to CAPWAP netdevice.
	 */

	/*
	 * Get original and reply directions tuple information from tunnel configuration.
	 */
	memcpy(&original.tuple, &cfg->tuple, sizeof(netfn_tuple_t));

	netfn_capwapmgr_tun_get_reply(&original.tuple, &reply.tuple);

	/*
	 * If tunnel config has VLAN enabled, update flow rule accordingly.
	 */
	if (ext_valid_flags & NETFN_CAPWAPMGR_EXT_VALID_VLAN) {
		struct netfn_flowmgr_vlan_rule *vlan = &cfg->ext_cfg.vlan;
		original.rule_info.rule_valid_flags |= NETFN_FLOWMGR_VALID_RULE_FLAG_VLAN;
		memcpy(&original.rule_info.vlan_rule, vlan, sizeof(struct netfn_flowmgr_vlan_rule));

		/*
		 * For the reply direction, update the ingress VLAN tags.
		 */
		reply.rule_info.rule_valid_flags |= NETFN_FLOWMGR_VALID_RULE_FLAG_VLAN;
		reply.rule_info.vlan_rule.inner.ingress_vlan_tag = vlan->inner.egress_vlan_tag;
		reply.rule_info.vlan_rule.inner.egress_vlan_tag = vlan->inner.ingress_vlan_tag;
		reply.rule_info.vlan_rule.outer.ingress_vlan_tag = vlan->outer.egress_vlan_tag;
		reply.rule_info.vlan_rule.outer.egress_vlan_tag = vlan->outer.ingress_vlan_tag;
	}

	/*
	 * If tunnel config has PPPoE enalbed, update flow rule accordingly.
	 * We only update the original direction's rule in this case.
	 */
	if (ext_valid_flags & NETFN_CAPWAPMGR_EXT_VALID_PPPOE) {
		original.rule_info.rule_valid_flags |= NETFN_FLOWMGR_VALID_RULE_FLAG_PPPOE;
		memcpy(&original.rule_info.pppoe_rule, &cfg->ext_cfg.pppoe, sizeof(struct netfn_flowmgr_pppoe_rule));
	}

	/*
	 * Update flow rules for both the directions.
	 */
	memcpy(&original.flow_info, flow, sizeof(struct netfn_flowmgr_flow_info));

	reply.flow_info.in_dev = flow->out_dev;
	reply.flow_info.out_dev = flow->in_dev;
	reply.flow_info.top_indev = flow->top_outdev;
	reply.flow_info.top_outdev = flow->top_indev;
	reply.flow_info.flow_mtu = flow->flow_mtu;

	/*
	 * Update the mac address for the reply flow rule.
	 */
	memcpy(reply.flow_info.flow_src_mac, flow->flow_dest_mac, ETH_ALEN);
	memcpy(reply.flow_info.flow_dest_mac, flow->flow_src_mac, ETH_ALEN);
	/*
	 * Mark the flow type as VP for both original and reply direction flows.
	 */
	original.flow_flags |= NETFN_FLOWMGR_FLOW_FLAG_VP_FLOW;
	reply.flow_flags |= NETFN_FLOWMGR_FLOW_FLAG_VP_FLOW;

	/*
	 * For the DL direction, use NOEDIT flag.
	 * This is to ensure that the vlan header is not stripped of by PPE,
	 * as we need to read the vlan pcp value and update the metaheader.
	 */
	reply.rule_info.rule_valid_flags |= NETFN_FLOWMGR_VALID_RULE_FLAG_NOEDIT_RULE;

	/*
	 * Create flow rule.
	 */
	netfn_status =	netfn_flowmgr_rule_accel(&original, &reply, NETFN_FLOWMGR_ACCEL_MODE_PPE);
	if (NETFN_FLOWMGR_GET_NETFN_ERROR_CODE(netfn_status) != NETFN_FLOWMGR_RET_SUCCESS) {
		netfn_capwapmgr_warn("%px: Failed to push flow rule flow_mgr_error %d,ae_error %d",
				cfg,
				NETFN_FLOWMGR_GET_NETFN_ERROR_CODE(netfn_status),
				NETFN_FLOWMGR_GET_AE_ERROR_CODE(netfn_status));
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_FLOW_RULE_CREATE]);
		return NETFN_CAPWAPMGR_ERROR_FLOW_RULE_CREATE;
	}

	return NETFN_CAPWAPMGR_SUCCESS;
}

/*
 * netfn_capwapmgr_flow_rule_destroy.
 *	Destroy flow rule assocaited with the tunnel.
 */
static netfn_capwapmgr_ret_t netfn_capwapmgr_flow_rule_destroy(struct netfn_capwapmgr_tun_cfg *cfg)
{
	struct netfn_flowmgr_destroy_rule original = {0};
	struct netfn_flowmgr_destroy_rule reply = {0};
	netfn_flowmgr_ret_t netfn_status = NETFN_FLOWMGR_RET_SUCCESS;

	/*
	 * Get original and reply directions tuple information from tunnel configuration.
	 */
	memcpy(&original.tuple, &cfg->tuple, sizeof(netfn_tuple_t));

	netfn_capwapmgr_tun_get_reply(&original.tuple, &reply.tuple);

	/*
	 * Destroy flow rule.
	 *
	 * We check for PPE error code as well, since theres a posibility that PPE flushed the
	 * flow rule because of some error packets. And in this case we should continue to
	 * destroy the tunnel.
	 */
	netfn_status = netfn_flowmgr_rule_decel(&original, &reply, NETFN_FLOWMGR_ACCEL_MODE_PPE);
	if ((NETFN_FLOWMGR_GET_NETFN_ERROR_CODE(netfn_status) != NETFN_FLOWMGR_RET_SUCCESS) &&
		(NETFN_FLOWMGR_GET_AE_ERROR_CODE(netfn_status) != PPE_DRV_RET_FAILURE_DESTROY_NO_CONN)) {
		netfn_capwapmgr_warn("%px: Failed to destroy flow rule flow_mgr_error %d,ae_error %d",
				cfg,
				NETFN_FLOWMGR_GET_NETFN_ERROR_CODE(netfn_status),
				NETFN_FLOWMGR_GET_AE_ERROR_CODE(netfn_status));
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_FLOW_RULE_DESTROY]);
		return NETFN_CAPWAPMGR_ERROR_FLOW_RULE_DESTROY;
	}

	return NETFN_CAPWAPMGR_SUCCESS;
}

/*
 * netfn_capwapmgr_flow_cookie_add
 *	Add flow cookie to flow db.
 */
static netfn_capwapmgr_ret_t netfn_capwapmgr_flow_cookie_add(struct netfn_capwapmgr_tun_ctx *ctx, struct netfn_capwapmgr_fc_info *fci)
{
	/*
	 * Account for the Flow Cookie Add operation here.
	 * The Function Call ultimately hooks into the Flow Cookie Module
	 * to perform the desired DB operation.
	 */

	if (netfn_flow_cookie_db_add(ctx->db, &fci->tuple, &fci->nfc) != 0) {
		atomic64_inc(&ctx->mgr->stats.error_stats[NETFN_CAPWAPMGR_ERROR_FLOW_COOKIE_ADD]);
		return NETFN_CAPWAPMGR_ERROR_FLOW_COOKIE_ADD;
	}

	/*
	 * If, this is the first entry then enable the flow cookie lookup
	 * in CAPWAP decapsulation path
	 */
	if (++ctx->flow_count == 1) {
		netfn_capwap_tun_enable_flow_db(ctx->capwap_dev, ctx->db);
	}

	return NETFN_CAPWAPMGR_SUCCESS;
}

/*
 * netfn_capwapmgr_flow_cookie_del
 *	Delete flow cookie from flow db.
 */
static netfn_capwapmgr_ret_t netfn_capwapmgr_flow_cookie_del(struct netfn_capwapmgr_tun_ctx *ctx, struct netfn_capwapmgr_fc_info *fci)
{
	/*
	 * Account for the Flow Cookie Delete operation here.
	 * The Function Call ultimately hooks into the Flow Cookie Module
	 * to perform the desired DB operation.
	 */
	if (netfn_flow_cookie_db_del(ctx->db, &fci->tuple) != 0) {
		atomic64_inc(&ctx->mgr->stats.error_stats[NETFN_CAPWAPMGR_ERROR_FLOW_COOKIE_DEL]);
		return NETFN_CAPWAPMGR_ERROR_FLOW_COOKIE_DEL;
	}

	/*
	 * If, this is the last entry then disable the flow cookie lookup in
	 * decapsulation path
	 */
	if (--ctx->flow_count == 0) {
		netfn_capwap_tun_disable_flow_db(ctx->capwap_dev);
	}

	return NETFN_CAPWAPMGR_SUCCESS;
}

/*
 * netfn_capwapmgr_tun_update_dest_mac
 *	Update destination mac address.
 */
static netfn_capwapmgr_ret_t netfn_capwapmgr_tun_update_dest_mac(struct netfn_capwapmgr_tun_ctx *ctx, uint8_t *dest_mac)
{
	netfn_capwapmgr_ret_t status;
	long bit_pos;

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_FLOW_CONFIGURED) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		status = netfn_capwapmgr_flow_rule_destroy(&ctx->cfg);
		if (status != NETFN_CAPWAPMGR_SUCCESS) {
			netfn_capwapmgr_warn("%px: Failed to delete flow rule when updating destination mac address\n", ctx);
			return status;
		}
	}

	/*
	 * Update destination mac address.
	 * Flow rule will be in UL direction.
	 */
	memcpy(ctx->cfg.flow.flow_dest_mac, dest_mac, ETH_ALEN);

	status = netfn_capwapmgr_flow_rule_create(&ctx->cfg);
	if (status != NETFN_CAPWAPMGR_SUCCESS) {
		netfn_capwapmgr_warn("%px:Failed to re-create flow rule with new destination mac address\n", ctx);
		return status;
	}

	__set_bit(bit_pos, ctx->state);
	return status;
}

/*
 * netfn_capwapmgr_tun_update_mtu
 *	Update path MTU.
 */
static netfn_capwapmgr_ret_t netfn_capwapmgr_tun_update_mtu(struct netfn_capwapmgr_tun_ctx *ctx, uint32_t mtu)
{
	netfn_capwapmgr_ret_t status;
	long bit_pos;

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_FLOW_CONFIGURED) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		status = netfn_capwapmgr_flow_rule_destroy(&ctx->cfg);
		if (status != NETFN_CAPWAPMGR_SUCCESS) {
			netfn_capwapmgr_warn("%px: Failed to delete flow rule when updating destination mac address\n", ctx);
			return status;
		}
	}

	ctx->cfg.flow.flow_mtu = mtu;

	status = netfn_capwapmgr_flow_rule_create(&ctx->cfg);
	if (status != NETFN_CAPWAPMGR_SUCCESS) {
		netfn_capwapmgr_warn("%px:Failed to re-create flow rule with new destination mac address\n", ctx);
	}

	__set_bit(bit_pos, ctx->state);

	/*
	 * Upate capwap dev's MTU.
	 * This internally handles updating dtls devices MTU as well
	 * when DTLS is enabled.
	 */
	rtnl_lock();
	__dev_set_mtu(ctx->capwap_dev, mtu);
	rtnl_unlock();
	return status;
}

/*
 * netfn_capwapmgr_tun_update_src_interface
 *	Update source interface.
 */
static netfn_capwapmgr_ret_t netfn_capwapmgr_tun_update_src_interface(struct netfn_capwapmgr_tun_ctx *ctx, struct net_device *dev)
{
	netfn_capwapmgr_ret_t status;
	long bit_pos;

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_FLOW_CONFIGURED) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		status = netfn_capwapmgr_flow_rule_destroy(&ctx->cfg);
		if (status != NETFN_CAPWAPMGR_SUCCESS) {
			netfn_capwapmgr_warn("%px: Failed to delete flow rule when updating destination mac address\n", ctx);
			return status;
		}
	}

	/*
	 * Flow rule is in uplink direction (CAPWAP to WAN dev).
	 */
	ctx->cfg.flow.out_dev = dev;
	ctx->cfg.flow.top_outdev = dev;

	status = netfn_capwapmgr_flow_rule_create(&ctx->cfg);
	if (status != NETFN_CAPWAPMGR_SUCCESS) {
		netfn_capwapmgr_warn("%px:Failed to re-create flow rule with new src interface\n", ctx);
	}

	__set_bit(bit_pos, ctx->state);
	return status;
}

/*
 * netfn_capwapmgr_tun_dtls_enable
 *	Update dtls configuration
 *
 * This API is called to Add DTLS after the CAPWAP tunnel is created.
 */
static netfn_capwapmgr_ret_t netfn_capwapmgr_tun_dtls_enable(struct netfn_capwapmgr_tun_ctx *ctx, struct netfn_dtls_cfg *enc, struct netfn_dtls_cfg *dec)
{
	netfn_capwapmgr_ret_t status = NETFN_CAPWAPMGR_SUCCESS;
	netfn_tuple_t dec_tuple = {0};
	long bit_pos;
	int ret = 0;

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_BIND) - 1;
	if (test_bit(bit_pos, ctx->state)) {
		netfn_capwapmgr_warn("%px: DTLS already enabled\n", ctx);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_DTLS_ENABLED]);
		return NETFN_CAPWAPMGR_ERROR_DTLS_ENABLED;
	}

	ctx->dtls_dev = netfn_dtls_tun_alloc(0);
	if (!ctx->dtls_dev) {
		netfn_capwapmgr_warn("%p:Failed to allocate dtls tunnel in DTLS offload engine\n", ctx);
		status = NETFN_CAPWAPMGR_ERROR_DTLS_ALLOC;
		goto fail;
	}

	dev_hold(ctx->dtls_dev);
	netfn_capwapmgr_tun_get_reply(&ctx->cfg.tuple, &dec_tuple);

	/*
	 * Create Encap and decap sessions.
	 */
	ret = netfn_dtls_tun_session_add(ctx->dtls_dev, enc, &ctx->cfg.tuple);
	if (ret) {
		netfn_capwapmgr_warn("Failed to add Encap dtls session %d\n", ret);
		status = NETFN_CAPWAPMGR_ERROR_DTLS_ENCAP_SESSION_ADD;
		goto fail;
	}

	ret = netfn_dtls_tun_session_add(ctx->dtls_dev, dec, &dec_tuple);
	if (ret) {
		netfn_capwapmgr_warn("Failed to add Decap dtls session %d\n", ret);
		netfn_dtls_tun_session_del(ctx->dtls_dev, true, enc->epoch);
		status = NETFN_CAPWAPMGR_ERROR_DTLS_DECAP_SESSION_ADD;
		goto fail;
	}

	rtnl_lock();
	__dev_set_mtu(ctx->dtls_dev, ctx->cfg.flow.flow_mtu);
	rtnl_unlock();

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_CONFIGURED) - 1;
	__set_bit(bit_pos, ctx->state);

	if (!netfn_capwap_tun_bind(ctx->capwap_dev, ctx->dtls_dev)) {
		netfn_capwapmgr_warn("%px: Failed to bind DTLS netdevice\n", ctx);
		status = NETFN_CAPWAPMGR_ERROR_DTLS_BIND;
		goto fail;
	}

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_BIND) - 1;
	__set_bit(bit_pos, ctx->state);

	/*
	 * Delete the flow rule.
	 */
	status = netfn_capwapmgr_flow_rule_destroy(&ctx->cfg);
	if (status != NETFN_CAPWAPMGR_SUCCESS) {
		netfn_capwapmgr_warn("%px: Failed to delete flow rule when enabling dtls\n", ctx);
		goto fail;
	}

	/*
	 * Flow rule is in UL direction.
	 * Update in_dev as dtls_dev.
	 */
	ctx->cfg.flow.in_dev = ctx->dtls_dev;
	ctx->cfg.flow.top_indev = ctx->dtls_dev;
	status = netfn_capwapmgr_flow_rule_create(&ctx->cfg);
	if (status != NETFN_CAPWAPMGR_SUCCESS) {
		netfn_capwapmgr_warn("%px:Failed to re-create flow rule when enabling dtls\n", ctx);
		goto fail;
	}

	netfn_capwap_tun_config_dtls(ctx->capwap_dev, true);
	memcpy(&ctx->cfg.ext_cfg.enc, enc, sizeof(struct netfn_dtls_cfg));
	memcpy(&ctx->cfg.ext_cfg.dec, dec, sizeof(struct netfn_dtls_cfg));

	return status;

fail:

	/*
	 * Unwind the DTLS interface unbind
	 */
	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_BIND) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		netfn_capwap_tun_unbind(ctx->capwap_dev);
	}

	/*
	 * Unwind the DTLS configuration
	 */
	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_CONFIGURED) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		netfn_dtls_tun_session_del(ctx->dtls_dev, false, dec->epoch);
		netfn_dtls_tun_session_del(ctx->dtls_dev, true, enc->epoch);
	}

	/*
	 * Free the DTLS device
	 */
	if (ctx->dtls_dev) {
		dev_put(ctx->dtls_dev);
		netfn_dtls_tun_free(ctx->dtls_dev);
	}

	atomic64_inc(&g_mgr.stats.error_stats[status]);
	return status;
}

/*
 * netfn_capwapmgr_tun_dtls_disable
 *	Update dtls configuration
 *
 * This API is called to remove DTLS after the CAPWAP+DTLS tunnel is created.
 */
static netfn_capwapmgr_ret_t netfn_capwapmgr_tun_dtls_disable(struct netfn_capwapmgr_tun_ctx *ctx, struct netfn_dtls_cfg *enc, struct netfn_dtls_cfg *dec)
{
	netfn_capwapmgr_ret_t status = NETFN_CAPWAPMGR_SUCCESS;
	long bit_pos;

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_BIND) - 1;
	if (!test_bit(bit_pos, ctx->state)) {
		netfn_capwapmgr_warn("%px: DTLS not enabled\n", ctx);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_DTLS_TUN_NOT_CONFIGURED]);
		return NETFN_CAPWAPMGR_ERROR_DTLS_TUN_NOT_CONFIGURED;
	}

	/*
	 * Unwind the DTLS interface unbind
	 */
	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_BIND) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		netfn_capwap_tun_unbind(ctx->capwap_dev);
	}

	/*
	 * Unwind the DTLS configuration
	 */
	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_CONFIGURED) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		netfn_dtls_tun_session_del(ctx->dtls_dev, false, ctx->cfg.ext_cfg.dec.epoch);
		netfn_dtls_tun_session_del(ctx->dtls_dev, true, ctx->cfg.ext_cfg.enc.epoch);
	}

	/*
	 * Free the DTLS device
	 */
	if (ctx->dtls_dev) {
		dev_put(ctx->dtls_dev);
		netfn_dtls_tun_free(ctx->dtls_dev);
		ctx->dtls_dev = NULL;
	}

	/*
	 * Delete the flow rule.
	 */
	status = netfn_capwapmgr_flow_rule_destroy(&ctx->cfg);
	if (status != NETFN_CAPWAPMGR_SUCCESS) {
		netfn_capwapmgr_warn("%px: Failed to delete flow rule when enabling dtls\n", ctx);
		return status;
	}

	/*
	 * Re-create flow rule with capwap dev as the in dev.
	 */
	ctx->cfg.flow.in_dev = ctx->capwap_dev;
	ctx->cfg.flow.top_indev = ctx->capwap_dev;
	status = netfn_capwapmgr_flow_rule_create(&ctx->cfg);
	if (status != NETFN_CAPWAPMGR_SUCCESS) {
		netfn_capwapmgr_warn("%px:Failed to re-create flow rule when enabling dtls\n", ctx);
	}

	netfn_capwap_tun_config_dtls(ctx->capwap_dev, false);
	return status;
}

/*
 * netfn_capwapmgr_tun_update_dtls_session
 *	DTLS Session update
 */
static netfn_capwapmgr_ret_t netfn_capwapmgr_tun_update_dtls_session(struct netfn_capwapmgr_tun_ctx *ctx, struct netfn_dtls_cfg *dtls_cfg, bool encap)
{
	netfn_tuple_t dec = {0};
	long bit_pos;
	int ret;

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_BIND) - 1;
	if (!test_bit(bit_pos, ctx->state)) {
		netfn_capwapmgr_warn("%px:Cannot add DTLS session without creating the DTLS tunnel\n", ctx);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_DTLS_TUN_NOT_CONFIGURED]);
		return NETFN_CAPWAPMGR_ERROR_DTLS_TUN_NOT_CONFIGURED;
	}

	if (encap) {
		dtls_cfg->flags = ctx->cfg.ext_cfg.enc.flags;
		dtls_cfg->version = ctx->cfg.ext_cfg.enc.version;
		dtls_cfg->df = ctx->cfg.ext_cfg.enc.df;
		dtls_cfg->tos = ctx->cfg.ext_cfg.enc.tos;
		dtls_cfg->hop_limit = ctx->cfg.ext_cfg.enc.hop_limit;

		ret = netfn_dtls_tun_session_add(ctx->dtls_dev, dtls_cfg, &ctx->cfg.tuple);
		if (ret) {
			netfn_capwapmgr_warn("Failed to add Encap dtls session %d\n", ret);
			atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_DTLS_ENCAP_SESSION_ADD]);
			return NETFN_CAPWAPMGR_ERROR_DTLS_ENCAP_SESSION_ADD;
		}
		memcpy(&ctx->cfg.ext_cfg.enc, dtls_cfg, sizeof(struct netfn_dtls_cfg));
	} else {
		dtls_cfg->flags = ctx->cfg.ext_cfg.dec.flags;
		dtls_cfg->version = ctx->cfg.ext_cfg.dec.version;

		netfn_capwapmgr_tun_get_reply(&ctx->cfg.tuple, &dec);
		ret = netfn_dtls_tun_session_add(ctx->dtls_dev, dtls_cfg, &dec);
		if (ret) {
			netfn_capwapmgr_warn("Failed to add Decap dtls session %d\n", ret);
			atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_DTLS_DECAP_SESSION_ADD]);
			return NETFN_CAPWAPMGR_ERROR_DTLS_DECAP_SESSION_ADD;
		}
		memcpy(&ctx->cfg.ext_cfg.dec, dtls_cfg, sizeof(struct netfn_dtls_cfg));
	}

	return NETFN_CAPWAPMGR_SUCCESS;
}

/*
 * netfn_capwapmgr_tun_dtls_session_switch
 *	Switch DTLS session.
 */
static netfn_capwapmgr_ret_t netfn_capwapmgr_tun_dtls_session_switch(struct netfn_capwapmgr_tun_ctx *ctx, bool encap)
{
	long bit_pos;

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_BIND) - 1;
	if (!test_bit(bit_pos, ctx->state)) {
		netfn_capwapmgr_warn("%px:Cannot switch DTLS session without creating the DTLS tunnel\n", ctx);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_DTLS_SESSION_SWITCH]);
		return NETFN_CAPWAPMGR_ERROR_DTLS_SESSION_SWITCH;
	}

	netfn_dtls_tun_session_switch(ctx->dtls_dev, encap);
	return NETFN_CAPWAPMGR_SUCCESS;
}

/*
 * netfn_capwapmgr_tun_init_cfg
 *	Validate tunnel configuration.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tun_init_cfg(struct netfn_capwapmgr_tun_cfg *cfg)
{
	netfn_tuple_t *tuple;
	uint32_t valid_ext;
	uint8_t protocol;

	/*
	 * We start by assuming this a pure CAPWAP flow. Then switch the tuple if
	 * extended features are enabled.
	 */
	tuple = &cfg->tuple;
	valid_ext = cfg->ext_cfg.ext_valid_flags;
	if (!cfg->flow.out_dev) {
		netfn_capwapmgr_warn("%px: NULL Wan netdevice\n", cfg);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_NULL_WAN_NDEV]);
		return NETFN_CAPWAPMGR_ERROR_NULL_WAN_NDEV;
	}

	if ((valid_ext & NETFN_CAPWAPMGR_EXT_VALID_VLAN) || (valid_ext & NETFN_CAPWAPMGR_EXT_VALID_PPPOE)) {
		if (!cfg->flow.top_outdev) {
			netfn_capwapmgr_warn("%px: NULL top netdevice\n", cfg);
			atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_NULL_TOP_NDEV]);
			return NETFN_CAPWAPMGR_ERROR_NULL_TOP_NDEV;
		}
	}

	if (tuple->tuple_type != NETFN_TUPLE_5TUPLE) {
		netfn_capwapmgr_warn("%px:Unsupported tuple type %d\n", cfg, tuple->tuple_type);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_UNSUPPORTED_TUPLE_TYPE]);
		return NETFN_CAPWAPMGR_ERROR_UNSUPPORTED_TUPLE_TYPE;
	}

	protocol = tuple->tuples.tuple_5.protocol;
	if((protocol != IPPROTO_UDP) && (protocol != IPPROTO_UDPLITE)) {
		netfn_capwapmgr_warn("%px:Unsupported l4 protocol %d\n", cfg, protocol);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_UNSUPPORTED_L4_PROTO]);
		return NETFN_CAPWAPMGR_ERROR_UNSUPPORTED_L4_PROTO;
	}

	/*
	 * We only support dtls tunnel creation with both encap and decap sessions
	 * since its DTLS + CAPWAP
	 */
	if (valid_ext & NETFN_CAPWAPMGR_EXT_VALID_DTLS) {
		if (!(valid_ext & NETFN_CAPWAPMGR_EXT_VALID_DTLS_ENC)) {
			netfn_capwapmgr_warn("%px: Unidirectional DTLS session not supported(%x) \n", cfg, valid_ext);
			atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_DTLS_CFG]);
			return NETFN_CAPWAPMGR_ERROR_DTLS_CFG;
		}

		if (!(valid_ext & NETFN_CAPWAPMGR_EXT_VALID_DTLS_DEC)) {
			netfn_capwapmgr_warn("%px: Unidirectional DTLS session not supported(%x) \n", cfg, valid_ext);
			atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_DTLS_CFG]);
			return NETFN_CAPWAPMGR_ERROR_DTLS_CFG;
		}
	}

	if (cfg->capwap.dec.max_frags > NETFN_CAPWAP_MAX_FRAGS) {
		netfn_capwapmgr_warn("%px: Invalid max fragments: %d, max: %d\n",
					cfg, cfg->capwap.dec.max_frags, NETFN_CAPWAP_MAX_FRAGS);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_CAPWAP_CFG]);
		return NETFN_CAPWAPMGR_ERROR_CAPWAP_CFG;
	}

	if (cfg->capwap.dec.max_frags == 0) {
		cfg->capwap.dec.max_frags = NETFN_CAPWAP_MAX_FRAGS;
	}

	if (cfg->capwap.dec.max_payload_sz > NETFN_CAPWAP_MAX_BUF_SZ) {
		netfn_capwapmgr_warn("%px: Invalid max payload size: %d, max: %d\n", cfg,
					cfg->capwap.dec.max_payload_sz, NETFN_CAPWAP_MAX_BUF_SZ);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_CAPWAP_CFG]);
		return NETFN_CAPWAPMGR_ERROR_CAPWAP_CFG;
	}

	if (cfg->capwap.dec.max_payload_sz == 0) {
		cfg->capwap.dec.max_payload_sz = NETFN_CAPWAP_MAX_BUF_SZ;
	}

	/*
	 * Update CAPWAP rule to specify any optional l2 rules enabled that might
	 * affect the MTU.
	 *
	 * TODO: When capwap offload exposes netfn_capwap_types.h
	 * Update the flags exposed by capwap offload instead.
	 */
	valid_ext = cfg->ext_cfg.ext_valid_flags;
	if ( valid_ext & NETFN_CAPWAPMGR_EXT_VALID_VLAN) {
		cfg->capwap.enc.flags |= NETFN_CAPWAPMGR_EXT_VALID_VLAN;
	}

	if (valid_ext & NETFN_CAPWAPMGR_EXT_VALID_PPPOE) {
		cfg->capwap.enc.flags |= NETFN_CAPWAPMGR_EXT_VALID_PPPOE;
	}

	return NETFN_CAPWAPMGR_SUCCESS;
}

/*
 * netfn_capwapmgr_tun_init()
 *	Common handling for creating Default and Tunid tunnels
 */
bool netfn_capwapmgr_tun_init(struct netfn_capwapmgr_tun_ctx *ctx, struct netfn_capwapmgr_tun_cfg *cfg)
{
	struct net_device *capwap_dev = ctx->capwap_dev;
	struct netfn_capwapmgr *mgr = &g_mgr;
	struct timer_list *timer = NULL;
	struct netfn_flow_cookie_db *db;
	netfn_tuple_t dec_tuple = {0};
	long bit_pos;
	int ret = 0;

	/*
	 * Flow rule is for the UL direction (CAPWAP netdevice to WAN netdevice).
	 */
	cfg->flow.in_dev = capwap_dev;
	cfg->flow.top_indev = capwap_dev;

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_CAPWAP_CONFIGURED) - 1;
	__set_bit(bit_pos, ctx->state);

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_ENABLED) - 1;
	__set_bit(bit_pos, ctx->state);

	/*
	 * Instantiate the Flow Cookie Data Base here.
	 */
	db = netfn_flow_cookie_db_alloc(NETFN_CAPWAPMGR_FLOW_COOKIE_HASHTABLE_SIZE);
	if (!db) {
		atomic64_inc(&mgr->stats.error_stats[NETFN_CAPWAPMGR_ERROR_FLOW_COOKIE_DB_ALLOC]);
		netfn_capwapmgr_warn("%p: Failed to alloc flow_db instance", ctx);
		return false;
	}

	ctx->db = netfn_flow_cookie_db_ref(db);

	/*
	 * If DTLS is enabled on this tunnel, configure DTLS offload accordingly.
	 * We onyl check for encap flag as in tunnel config validation,
	 * It is made sure that both encap and decap sessions are enabled.
	 */
	if (!(cfg->ext_cfg.ext_valid_flags & NETFN_CAPWAPMGR_EXT_VALID_DTLS_ENC))
		goto skip_dtls;

	ctx->dtls_dev = netfn_dtls_tun_alloc(0);
	if (!ctx->dtls_dev) {
		netfn_capwapmgr_warn("%p:Failed to allocate dtls tunnel in DTLS offload engine\n", capwap_dev);
		goto fail;
	}

	dev_hold(ctx->dtls_dev);
	netfn_capwapmgr_tun_get_reply(&cfg->tuple, &dec_tuple);

	/*
	 * When DTLS is enabled, the in dev for flow rule will be the dtls dev.
	 */
	cfg->flow.in_dev = ctx->dtls_dev;
	cfg->flow.top_indev = ctx->dtls_dev;

	/*
	 * Create Encap and decap sessions.
	 */
	ret = netfn_dtls_tun_session_add(ctx->dtls_dev, &cfg->ext_cfg.enc, &cfg->tuple);
	if (ret) {
		netfn_capwapmgr_warn("Failed to add Encap dtls session %d\n", ret);
		goto fail;
	}

	ret = netfn_dtls_tun_session_add(ctx->dtls_dev, &cfg->ext_cfg.dec, &dec_tuple);
	if (ret) {
		netfn_capwapmgr_warn("Failed to add Decap dtls session %d\n", ret);
		netfn_dtls_tun_session_del(ctx->dtls_dev, true, cfg->ext_cfg.enc.epoch);
		goto fail;
	}

	rtnl_lock();
	__dev_set_mtu(ctx->dtls_dev, cfg->flow.flow_mtu);
	rtnl_unlock();

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_CONFIGURED) - 1;
	__set_bit(bit_pos, ctx->state);

	if (!netfn_capwap_tun_bind(ctx->capwap_dev, ctx->dtls_dev)) {
		netfn_capwapmgr_warn("%px: Failed to bind DTLS netdevice\n", cfg);
		goto fail;
	}

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_BIND) - 1;
	__set_bit(bit_pos, ctx->state);

skip_dtls:

	/*
	 * Configure flow rule.
	 *
	 * If Top Out Dev is not passed to us,
	 * It means WAN interface does not have an interface
	 * heirarchy.
	 */
	if (!cfg->flow.top_outdev) {
		cfg->flow.top_outdev = cfg->flow.out_dev;
	}

	if (netfn_capwapmgr_flow_rule_create(cfg) != NETFN_CAPWAPMGR_SUCCESS) {
		netfn_capwapmgr_warn("%px:Failed to configure flow rule\n", cfg);
		goto fail;

	}

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_FLOW_CONFIGURED) - 1;
	__set_bit(bit_pos, ctx->state);

	/*
	 * Create debugfs stats entry for the tunnel.
	 */
	ctx->dentry = debugfs_create_file(capwap_dev->name,S_IRUGO,mgr->dentry,ctx,&netfn_capwapmgr_tun_stats_ops);
	if (!ctx->dentry) {
		netfn_capwapmgr_warn("%px: Failed to create stats file for the tunnel\n", cfg);
	}

	ctx->mgr = mgr;

	/*
	 * Setup the timer to sync stats from flowmgr.
	 * Attach the timer on the current core.
	 */
	atomic64_set(&ctx->timer_resched, 1);
	timer = &ctx->stats_sync;
	timer_setup(timer, netfn_capwapmgr_tun_stats_sync, TIMER_PINNED);
	timer->expires = jiffies + msecs_to_jiffies(NETFN_CAPWAPMGR_STATS_SYNC_FREQ);
	add_timer_on(timer, smp_processor_id());

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_TIMER_CONFIGURED) - 1;
	__set_bit(bit_pos, ctx->state);

	ctx->flow_count = 0;

	/*
	 * Keep a copy of tunnel configuration in tunnel context.
	 */
	memcpy(&ctx->cfg, cfg, sizeof(struct netfn_capwapmgr_tun_cfg));
	return true;
fail:
	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_FLOW_CONFIGURED) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		netfn_capwapmgr_flow_rule_destroy(cfg);
	}

	/*
	 * Unwind the DTLS interface unbind
	 */
	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_BIND) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		netfn_capwap_tun_unbind(ctx->capwap_dev);
	}

	/*
	 * Unwind the DTLS configuration
	 */
	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_CONFIGURED) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		netfn_dtls_tun_session_del(ctx->dtls_dev, false, cfg->ext_cfg.dec.epoch);
		netfn_dtls_tun_session_del(ctx->dtls_dev, true, cfg->ext_cfg.enc.epoch);
	}

	/*
	 * Free the DTLS device
	 */
	if (ctx->dtls_dev) {
		dev_put(ctx->dtls_dev);
		netfn_dtls_tun_free(ctx->dtls_dev);
	}

	/*
	 * Check if the flow_db has entries then Drop the reference taken in the alloc
	 */
	if (ctx->flow_count) {
		pr_warn("%p: Active flows present in flow_db (%d)\n", ctx, ctx->flow_count);
		netfn_capwap_tun_disable_flow_db(ctx->capwap_dev);

		/*
		 * TODO: purge entries in case flow_count is non-zero
		 */
		return false;
	}

	netfn_flow_cookie_db_deref(ctx->db);
	ctx->db = NULL;
	return false;
}

/*
 * netfn_capwapmgr_tun_deinit
 *	De-initialize the tunnel.
 */
bool netfn_capwapmgr_tun_deinit(struct netfn_capwapmgr_tun_ctx *ctx)
{
	netfn_capwapmgr_ret_t status = NETFN_CAPWAPMGR_SUCCESS;
	long bit_pos;

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_ENABLED) - 1;
	if (test_bit(bit_pos, ctx->state)) {
		netfn_capwapmgr_warn("%px: Cannot deinitialize the tunnel in enabled state\n", ctx);
		return false;
	}

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DEINIT_DONE) - 1;
	if (test_bit(bit_pos, ctx->state)) {
		return true;
	}

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DEINIT_IN_PROGRESS) - 1;
	if (__test_and_set_bit(bit_pos, ctx->state)) {
		netfn_capwapmgr_warn("%px: Tunnel deinit in progress \n", ctx);
		return false;
	}

	/*
	 * Remove stats debugfs associated with the tunnel.
	 */
	if(ctx->dentry) {
		debugfs_remove(ctx->dentry);
		ctx->dentry = NULL;
	}

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_TIMER_CONFIGURED) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		/*
		 * Delete the stats sync timer.
		 *
		 * del_timer_sync is used to make sure that we block till
		 * the ongoing timer is done executing. As we will be freeing the
		 * tunnel context after this.
		 */
		atomic64_set(&ctx->timer_resched, 0);
		del_timer_sync(&ctx->stats_sync);
	}

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_FLOW_CONFIGURED) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		/*
		 * Destroy flow rule.
		 */
		status = netfn_capwapmgr_flow_rule_destroy(&ctx->cfg);
		if (status != NETFN_CAPWAPMGR_SUCCESS) {
			netfn_capwapmgr_warn("%px:Failed to destroy flow rule\n", ctx);
			__set_bit(bit_pos, ctx->state);
			goto fail;
		}
	}

	/*
	 * Unbind DTLS dev if bound.
	 */
	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_BIND) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		if(!netfn_capwap_tun_unbind(ctx->capwap_dev)) {
			netfn_capwapmgr_warn("%px: Failed to unbind dtls node\n", ctx);
			__set_bit(bit_pos, ctx->state);
			goto fail;
		}
	}

	/*
	 * Destroy DTLS sessions.
	 */
	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DTLS_CONFIGURED) - 1;
	if (__test_and_clear_bit(bit_pos, ctx->state)) {
		netfn_dtls_tun_session_del(ctx->dtls_dev, false, ctx->cfg.ext_cfg.dec.epoch);
		netfn_dtls_tun_session_del(ctx->dtls_dev, true, ctx->cfg.ext_cfg.enc.epoch);

		/*
		 * Free the DTLS net device.
		 */
		dev_put(ctx->dtls_dev);
		netfn_dtls_tun_free(ctx->dtls_dev);
	}

	/*
	 * Check if the flow_db has entries then Drop the reference taken in the alloc
	 */
	if (ctx->flow_count) {
		pr_warn("%p: Active flows present in flow_db (%d)\n", ctx, ctx->flow_count);
		netfn_capwap_tun_disable_flow_db(ctx->capwap_dev);

		/*
		 * TODO: purge entries in case flow_count is non-zero
		 */
		return false;
	}

	netfn_flow_cookie_db_deref(ctx->db);
	ctx->db = NULL;

	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DEINIT_DONE) - 1;
	__set_bit(bit_pos, ctx->state);
	return true;
fail:
	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_DEINIT_IN_PROGRESS) - 1;
	__clear_bit(bit_pos, ctx->state);
	return false;
}

/*
 * netfn_capwapmgr_tun_alloc()
 *	Allocate a CAPWAP tunnel.
 */
struct net_device *netfn_capwapmgr_tun_alloc(struct netfn_capwapmgr_tun_cfg *cfg)
{
	struct netfn_capwapmgr_tun_ctx *ctx = NULL;
	struct net_device *capwap_dev = NULL;
	struct netfn_capwapmgr *mgr = &g_mgr;

	if (netfn_capwapmgr_tun_init_cfg(cfg) != NETFN_CAPWAPMGR_SUCCESS) {
		netfn_capwapmgr_warn("%px: Failed to valiate the configuration\n", cfg);
		return NULL;
	}

	/*
	 * Create dev tunnel.
	 */
	capwap_dev = netfn_capwap_tun_alloc(&cfg->capwap, &cfg->tuple, sizeof(struct netfn_capwapmgr_tun_ctx));
	if (!capwap_dev) {
		netfn_capwapmgr_warn("%px:Failed to create capwap tunnel\n", cfg);
		return NULL;
	}

	/*
	 * Update CAPWAP dev associated with the tunnel.
	 */
	dev_hold(capwap_dev);
	ctx = netfn_capwap_tun_pvt_get(capwap_dev);
	ctx->capwap_dev = capwap_dev;

	if (!netfn_capwapmgr_tun_init(ctx, cfg)) {
		netfn_capwapmgr_warn("%px:Failed to initialize the capwap tunnel\n", cfg);
		dev_put(capwap_dev);
		netfn_capwap_tun_free(capwap_dev);
		return NULL;

	}

	/*
	 * deref:netfn_capwapmgr_tun_free.
	 */
	netfn_capwapmgr_ref(mgr);
	atomic_inc(&mgr->stats.tun_dev_alloc);
	return capwap_dev;
}
EXPORT_SYMBOL(netfn_capwapmgr_tun_alloc);

/*
 * netfn_capwapmgr_tun_free()
 *	Free the CAPWAP tunnel.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tun_free(struct net_device *dev)
{
	struct netfn_capwapmgr *mgr = &g_mgr;
	struct netfn_capwapmgr_tun_ctx *ctx = NULL;

	ctx = netfn_capwap_tun_pvt_get(dev);

	/*
	 * Deinit tunnel.
	 */
	if (netfn_capwapmgr_tun_deinit(ctx)) {
		netfn_capwapmgr_warn("%px: Failed to de-init the tunnel\n", dev);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUN_DEINIT]);
		return NETFN_CAPWAPMGR_ERROR_TUN_DEINIT;
	}

	/*
	 * Free capwap tunnel.
	 */
	if (netfn_capwap_tun_free(ctx->capwap_dev)) {
		netfn_capwapmgr_warn("%px: Failed to free capwap tun \n", dev);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUN_FREE]);
		return NETFN_CAPWAPMGR_ERROR_TUN_FREE;
	}

	/*
	 * ref:netfn_capwapmgr_tun_alloc.
	 */
	netfn_capwapmgr_deref(mgr);
	atomic_inc(&mgr->stats.tun_dev_free);
	return NETFN_CAPWAPMGR_SUCCESS;
}
EXPORT_SYMBOL(netfn_capwapmgr_tun_free);

/*
 * netfn_capwapmgr_tun_update()
 *	Update tunnel configuration.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tun_update(struct net_device *dev, struct netfn_capwapmgr_tun_update *cfg)
{
	netfn_capwapmgr_ret_t status = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_ctx *ctx = NULL;
	long bit_pos;

	ctx = netfn_capwap_tun_pvt_get(dev);

	/*
	 * If Tunnel is enabled, we do not support updating the configuration.
	 */
	bit_pos = __builtin_ffs(NETFN_CAPWAPMGR_TUN_STATE_ENABLED) - 1;
	if (test_bit(bit_pos, ctx->state)) {
		netfn_capwapmgr_warn("%px: Updating Tunnel configuration not supported when tunnel is enabled\n", dev);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_TUN_ENABLED]);
		return NETFN_CAPWAPMGR_ERROR_TUN_ENABLED;
	}

	switch (cfg->type) {
		/*
		 * Update version is to support legacy API and its a no-op.
		 */
	case NETFN_CAPWAPMGR_UPDATE_CAPWAP_VER:
		return status;

	case NETFN_CAPWAPMGR_UPDATE_DEST_MAC:
		return netfn_capwapmgr_tun_update_dest_mac(ctx, cfg->update_cfg.dest_mac);

	case NETFN_CAPWAPMGR_UPDATE_MTU:
		return netfn_capwapmgr_tun_update_mtu(ctx, cfg->update_cfg.mtu);

	case NETFN_CAPWAPMGR_UPDATE_SRC_INTERFACE:
		return netfn_capwapmgr_tun_update_src_interface(ctx, cfg->update_cfg.dev);

	case NETFN_CAPWAPMGR_UPDATE_DTLS_ENABLE:
		return netfn_capwapmgr_tun_dtls_enable(ctx, &cfg->update_cfg.dtls.enc, &cfg->update_cfg.dtls.dec);

	case NETFN_CAPWAPMGR_UPDATE_DTLS_DISABLE:
		return netfn_capwapmgr_tun_dtls_disable(ctx, &cfg->update_cfg.dtls.enc, &cfg->update_cfg.dtls.dec);

	case NETFN_CAPWAPMGR_UPDATE_DTLS_ENCAP_SESSION:
		return netfn_capwapmgr_tun_update_dtls_session(ctx, &cfg->update_cfg.dtls.enc, true);

	case NETFN_CAPWAPMGR_UPDATE_DTLS_DECAP_SESSION:
		return netfn_capwapmgr_tun_update_dtls_session(ctx, &cfg->update_cfg.dtls.dec, false);

	case NETFN_CAPWAPMGR_ADD_NETFN_FLOW_COOKIE:
		return netfn_capwapmgr_flow_cookie_add(ctx, &cfg->update_cfg.fci);

	case NETFN_CAPWAPMGR_DEL_NETFN_FLOW_COOKIE:
		return netfn_capwapmgr_flow_cookie_del(ctx, &cfg->update_cfg.fci);

	case NETFN_CAPWAPMGR_DTLS_ENCAP_SESSION_SWITCH:
		return netfn_capwapmgr_tun_dtls_session_switch(ctx, true);

	case NETFN_CAPWAPMGR_DTLS_DECAP_SESSION_SWITCH:
		return netfn_capwapmgr_tun_dtls_session_switch(ctx, false);

	default:
		netfn_capwapmgr_warn("%px: Unknown cfg update type received", dev);
		return NETFN_CAPWAPMGR_ERROR_INVALID_CFG;
	}
}
EXPORT_SYMBOL(netfn_capwapmgr_tun_update);

/*
 * netfn_capwapmgr_tun_get_dtls_dev()
 *	Get the DTLS dev associated with the tunnel.
 */
struct net_device *netfn_capwapmgr_tun_get_dtls_dev(struct net_device *dev)
{
	struct netfn_capwapmgr_tun_ctx *ctx = netfn_capwap_tun_pvt_get(dev);
	return ctx->dtls_dev;
}
EXPORT_SYMBOL(netfn_capwapmgr_tun_get_dtls_dev);

/*
 * netfn_capwapmgr_tun_get_stats()
 *	Get stats associated with capwap tunnel
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tun_get_stats(struct net_device *dev, struct netfn_capwapmgr_tun_stats *stats)
{
	struct netfn_capwapmgr_tun_ctx *ctx = netfn_capwap_tun_pvt_get(dev);

	/*
	 * Copy flow stats.
	 */
	memcpy(&stats->flow, &ctx->stats, sizeof(struct netfn_capwapmgr_flow_stats));

	/*
	 * Copy capwap stats.
	 * TODO: Use error specific return codes.
	 */
	if (netfn_capwap_tun_stats_get(dev, &stats->capwap)) {
		netfn_capwapmgr_warn("%px: Failed to capwap offload stats\n", dev);
		atomic64_inc(&g_mgr.stats.error_stats[NETFN_CAPWAPMGR_ERROR_STATS_GET]);
		return NETFN_CAPWAPMGR_ERROR_STATS_GET;
	}

	return NETFN_CAPWAPMGR_SUCCESS;
}
EXPORT_SYMBOL(netfn_capwapmgr_tun_get_stats);
