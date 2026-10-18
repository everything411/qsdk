/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/vmalloc.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/etherdevice.h>
#ifdef PPE_DRV_DDRQ_ENABLE
#include <fal/fal_qm.h>
#endif
#include "ppe_drv.h"
#ifdef PPE_TUNNEL_ENABLE
#include "tun/ppe_drv_tun_v4.h"
#include "tun/ppe_drv_tun_v6.h"
#endif

/*
 * ppe_drv_cc_v4_tuple_fill()
 *	Fill an IPv4 5-tuple from the dissected flow keys.
 */
static void ppe_drv_cc_v4_tuple_fill(struct flow_keys *keys, struct ppe_drv_v4_5tuple *tuple)
{
	tuple->flow_ip = ntohl(keys->addrs.v4addrs.src);
	tuple->return_ip = ntohl(keys->addrs.v4addrs.dst);
	tuple->protocol = keys->basic.ip_proto;

	/*
	 * PPE support only TCP, UDP or UDP_LITE for 5-tuple
	 */
	if ((keys->basic.ip_proto == IPPROTO_TCP)
			|| (keys->basic.ip_proto == IPPROTO_UDP)
			|| (keys->basic.ip_proto == IPPROTO_UDPLITE)) {

		tuple->flow_ident = ntohs(keys->ports.src);
		tuple->return_ident = ntohs(keys->ports.dst);
	}
}

/*
 * ppe_drv_cc_v6_tuple_fill()
 *	Fill an IPv6 5-tuple from the dissected flow keys.
 */
static void ppe_drv_cc_v6_tuple_fill(struct flow_keys *keys, struct ppe_drv_v6_5tuple *tuple)
{
	PPE_DRV_IN6_TO_IPV6(tuple->flow_ip, keys->addrs.v6addrs.src);
	PPE_DRV_IN6_TO_IPV6(tuple->return_ip, keys->addrs.v6addrs.dst);
	tuple->protocol = keys->basic.ip_proto;

	/*
	 * PPE support only TCP, UDP or UDP_LITE for 5-tuple
	 */
	if ((keys->basic.ip_proto == IPPROTO_TCP)
			|| (keys->basic.ip_proto == IPPROTO_UDP)
			|| (keys->basic.ip_proto == IPPROTO_UDPLITE)) {

		tuple->flow_ident = ntohs(keys->ports.src);
		tuple->return_ident = ntohs(keys->ports.dst);
	}
}

#ifdef PPE_TUNNEL_ENABLE
/*
 * ppe_drv_cc_tun_process_v4()
 *	Deactivate and flush the IPv4 tunnel connection matching the given flow keys.
 */
static ppe_drv_ret_t ppe_drv_cc_tun_process_v4(struct flow_keys *keys)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_v4_rule_destroy destroy_v4 = {0};
	bool is_flush = true;
	ppe_drv_ret_t ret;

	ppe_drv_cc_v4_tuple_fill(keys, &destroy_v4.tuple);

	/*
	 * Exceptions for regular flows also reach here, so first check quietly if this
	 * tuple belongs to a tunnel connection. This avoids polluting the tunnel
	 * statistics and logs for every non tunnel exception.
	 */
	spin_lock_bh(&p->lock);
	if (!ppe_drv_v4_conn_tun_conn_get(&destroy_v4.tuple)) {
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_FAILURE_DESTROY_NO_CONN;
	}
	spin_unlock_bh(&p->lock);

	ppe_drv_stats_inc(&p->stats.gen_stats.v4_tun_flush_req);

	/*
	 * Notify tunnel deletion with reason flush
	 */
	ret = ppe_drv_v4_tun_del_ce_notify(&destroy_v4, is_flush);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_drv_stats_inc(&p->stats.gen_stats.v4_tun_flush_fail);
	}

	return ret;
}

/*
 * ppe_drv_cc_tun_process_v6()
 *	Deactivate and flush the IPv6 tunnel connection matching the given flow keys.
 */
static ppe_drv_ret_t ppe_drv_cc_tun_process_v6(struct flow_keys *keys)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_v6_rule_destroy destroy_v6 = {0};
	bool is_flush = true;
	ppe_drv_ret_t ret;

	ppe_drv_cc_v6_tuple_fill(keys, &destroy_v6.tuple);

	/*
	 * Exceptions for regular flows also reach here, so first check quietly if this
	 * tuple belongs to a tunnel connection. This avoids polluting the tunnel
	 * statistics and logs for every non tunnel exception.
	 */
	spin_lock_bh(&p->lock);
	if (!ppe_drv_v6_conn_tun_conn_get(&destroy_v6.tuple)) {
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_FAILURE_DESTROY_NO_CONN;
	}
	spin_unlock_bh(&p->lock);

	ppe_drv_stats_inc(&p->stats.gen_stats.v6_tun_flush_req);

	/*
	 * Notify tunnel deletion with reason flush
	 */
	ret = ppe_drv_v6_tun_del_ce_notify(&destroy_v6, is_flush);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_drv_stats_inc(&p->stats.gen_stats.v6_tun_flush_fail);
	}

	return ret;
}
#endif

/*
 * ppe_drv_cc_process_v4()
 *	Try flush for a given flow key.
 */
static void ppe_drv_cc_process_v4(ppe_drv_cc_t cc, struct flow_keys *keys, struct net_device *dev)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_flow *flow;
	struct ppe_drv_v4_conn_flow *pcf;
	struct ppe_drv_v4_conn_flow *pcr;
	struct ppe_drv_v4_5tuple v4_5tuple = {0};
	struct ppe_drv_v4_conn_sync *cns;
	struct ppe_drv_v4_conn *cn = NULL;

	ppe_drv_cc_v4_tuple_fill(keys, &v4_5tuple);

	/*
	 * Get the flow table entry and try to destroy.
	 */
	spin_lock_bh(&p->lock);
	flow = ppe_drv_flow_v4_get_by_dev(&v4_5tuple, dev);
	if (!flow) {
		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.v4_flush_conn_not_found);
		ppe_drv_info("%p: flow entry not found for cpu_code: %d", keys, cc);
		return;
	}

	pcf = flow->pcf.v4;
	cn = pcf->conn;
	pcr = (pcf == &cn->pcf) ? &cn->pcr : &cn->pcf;

	if (ppe_drv_v4_conn_flow_flags_check(pcf, PPE_DRV_V4_CONN_FLAG_FLOW_RFS_PPE_ASSIST)) {
		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.v4_flush_skip_conn_rfs);
		ppe_drv_info("%p: connection flush called for ppe_rfs cpu_code: %d", keys, cc);
		return;
	}

	if (ppe_drv_v4_conn_flow_flags_check(pcr, PPE_DRV_V4_CONN_FLAG_FLOW_RFS_PPE_ASSIST)) {
		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.v4_flush_skip_conn_rfs);
		ppe_drv_info("%p: connection flush called for ppe_rfs cpu_code: %d", keys, cc);
		return;
	}

	if (ppe_drv_v4_flush(pcf->conn) != PPE_DRV_RET_SUCCESS) {
		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.v4_flush_conn_not_found);
		ppe_drv_info("%p: connection flush failed for cpu_code: %d", keys, cc);
		return;
	}

	/*
	 * Capture remaining stats.
	 */
	cns = ppe_drv_v4_conn_stats_alloc();
	if (cns) {
		ppe_drv_v4_conn_sync_one(ppe_drv_v4_conn_flow_conn_get(pcf), cns, PPE_DRV_STATS_SYNC_REASON_FLUSH);
	}

	spin_unlock_bh(&p->lock);

	/*
	 * Sync stats with CM
	 */
	if (cns) {
		ppe_drv_v4_conn_stats_sync_invoke_cb(cns);
		ppe_drv_v4_conn_stats_free(cns);
	}

	ppe_drv_v4_conn_free(ppe_drv_v4_conn_flow_conn_get(pcf));
}

/*
 * ppe_drv_cc_process_v6()
 *	Try flush for a given flow key.
 */
static void ppe_drv_cc_process_v6(ppe_drv_cc_t cc, struct flow_keys *keys, struct net_device *dev)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_flow *flow;
	struct ppe_drv_v6_conn_flow *pcf;
	struct ppe_drv_v6_conn_flow *pcr;
	struct ppe_drv_v6_5tuple v6_5tuple = {0};
	struct ppe_drv_v6_conn_sync *cns;
	struct ppe_drv_v6_conn *cn = NULL;

	ppe_drv_cc_v6_tuple_fill(keys, &v6_5tuple);

	/*
	 * Get the flow table entry and try to destroy.
	 */
	spin_lock_bh(&p->lock);
	flow = ppe_drv_flow_v6_get_by_dev(&v6_5tuple, dev);
	if (!flow) {
		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.v6_flush_conn_not_found);
		ppe_drv_info("%p: flow entry not found for cpu_code: %d", keys, cc);
		return;
	}

	pcf = flow->pcf.v6;
	cn = pcf->conn;
	pcr = (pcf == &cn->pcf) ? &cn->pcr : &cn->pcf;

	if (ppe_drv_v6_conn_flow_flags_check(pcf, PPE_DRV_V6_CONN_FLAG_FLOW_RFS_PPE_ASSIST)) {
		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.v6_flush_skip_conn_rfs);
		ppe_drv_info("%p: connection flush called for ppe_rfs cpu_code: %d", keys, cc);
		return;
	}

	if (ppe_drv_v6_conn_flow_flags_check(pcr, PPE_DRV_V6_CONN_FLAG_FLOW_RFS_PPE_ASSIST)) {
		spin_unlock_bh(&p->lock);
		ppe_drv_stats_inc(&p->stats.gen_stats.v6_flush_skip_conn_rfs);
		ppe_drv_info("%p: connection flush called for ppe_rfs cpu_code: %d", keys, cc);
		return;
	}

	if (ppe_drv_v6_flush(pcf->conn) != PPE_DRV_RET_SUCCESS) {
		spin_unlock_bh(&p->lock);
		ppe_drv_info("%p: connection flush failed for cpu_code: %d", keys, cc);
		return;
	}

	/*
	 * Capture remaining stats.
	 */
	cns = ppe_drv_v6_conn_stats_alloc();
	if (cns) {
		ppe_drv_v6_conn_sync_one(ppe_drv_v6_conn_flow_conn_get(pcf), cns, PPE_DRV_STATS_SYNC_REASON_FLUSH);
	}

	spin_unlock_bh(&p->lock);

	/*
	 * Sync stats with CM
	 */
	if (cns) {
		ppe_drv_v6_conn_stats_sync_invoke_cb(cns);
		ppe_drv_v6_conn_stats_free(cns);
	}

	ppe_drv_v6_conn_free(ppe_drv_v6_conn_flow_conn_get(pcf));
}

#ifdef PPE_DRV_DDRQ_ENABLE
/*
 * ppe_drv_cc_ucast_qbase_profile_set()
 *	API to configure qbase profile for input CPU code
 */
bool ppe_drv_cc_ucast_qbase_profile_set(uint16_t cc, uint32_t qbase)
{
	sw_error_t err;
	fal_ucast_queue_dest_t q_dst = {0};

	q_dst.src_profile = PPE_DRV_PORT_SRC_PROFILE;
	q_dst.cpu_code_en = A_TRUE;
	q_dst.cpu_code = cc;

	err = fal_ucast_queue_base_profile_set(PPE_DRV_SWITCH_ID, &q_dst, qbase, PPE_DRV_REDIR_PROFILE_ID);
	if (err != SW_OK) {
		ppe_drv_warn("Unable to do qbase (%d) config profile set of %d cpu code\n", qbase, cc);
		return false;
	}

	ppe_drv_info("port ucast queue base id: %d for cc: %d\n", qbase, cc);
	return true;
}
EXPORT_SYMBOL(ppe_drv_cc_ucast_qbase_profile_set);
#endif

/*
 * ppe_drv_cc_process_skbuff()
 *	Process skbuff with a non-zero cpu code.
 */
bool ppe_drv_cc_process_skbuff(struct ppe_drv_cc_metadata *cc_info, struct sk_buff *skb)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_cc *pcc;
	struct flow_keys keys = {0};
	ppe_drv_cc_callback_t cb;
	void *app_data;
	bool ret = false;
	uint16_t cc = cc_info->cpu_code;
	struct iphdr *iph;
	struct net_device *src_dev = NULL;

	ppe_drv_assert((cc > 0) && (cc < PPE_DRV_CC_MAX), "%p: invalid cpu code %u", p, cc);

	/*
	 * Check if this CPU code needs flush.
	 */
	pcc = &p->cc[cc];
	if (flow_deacclr_dis || !pcc->flush) {
		goto done;
	}

	/*
	 * Extract flow key from skbuff
	 * We are using skb flow dissect and ignoring its performance impact,
	 * since PPE generate explicit CPU code only for first packet which is
	 * responsible for auto flow deceleration in PPE, all subsequent packets
	 * corresponding to decelerated flow would be sent to host without a
	 * specific CPU code which we are monitoring.
	 *
	 * Note:
	 * 1. we need to parse the 1st fragment of an IP fragment chain to flush
	 *    associated rule.
	 * 2. PPE uses 3 tuple rule for all ip protocol other than TCP, UDP & UDP_LITE,
	 *    so we STOP_AT_ENCAP.
	 * 3. For packets with fake MAC header PPE can generate a flow based exception,
	 *    only when it's an IPv4 or IPv6 packets.
	 */
	if (cc_info->fake_mac) {
		/*
		 * Discard L2 header
		 */
		skb_pull_inline(skb, ETH_HLEN);
		iph = (struct iphdr *)skb->data;

		if (iph->version == 4) {
			skb->protocol = htons(ETH_P_IP);
		} else if (iph->version == 6) {
			skb->protocol = htons(ETH_P_IPV6);
		} else {
			ppe_drv_info("%p: Non-IP packet with fake mac set :%p for cc:%u ",
					p, skb, cc);
			goto push;
		}
	} else {
		skb->protocol = eth_type_trans(skb, skb->dev);
	}

	skb_reset_network_header(skb);
	if (!skb_flow_dissect_flow_keys(skb, &keys,
			FLOW_DISSECTOR_F_PARSE_1ST_FRAG | FLOW_DISSECTOR_F_STOP_AT_ENCAP)) {
		ppe_drv_info("%p: dissection failed for skb:%p", p, skb);
		goto push;
	}

	/*
	 * Attempt flush
	 */
	if (cc_info->is_src_vp) {
		src_dev = ppe_drv_port_num_to_dev(cc_info->src_vp_num);
	} else {
		src_dev = skb->dev;
	}

	switch (keys.control.addr_type) {
		case FLOW_DISSECTOR_KEY_IPV4_ADDRS:
#ifdef PPE_TUNNEL_ENABLE
			/*
			 * Tunnel (outer) connections are tracked in a separate connection list
			 * and are not present in the flow table. Attempt the tunnel deactivation
			 * path first, it looks up the tunnel connection for this tuple and returns
			 * failure if there is none, in which case fall back to the regular flow
			 * flush below.
			 */
			if (ppe_drv_tun_exception_is_flush_required((ppe_drv_cc_t)cc) &&
					ppe_drv_cc_tun_process_v4(&keys) == PPE_DRV_RET_SUCCESS) {
				break;
			}
#endif
			ppe_drv_cc_process_v4((ppe_drv_cc_t)cc, &keys, src_dev);
			break;

		case FLOW_DISSECTOR_KEY_IPV6_ADDRS:
#ifdef PPE_TUNNEL_ENABLE
			/*
			 * Tunnel (outer) connections are tracked in a separate connection list
			 * and are not present in the flow table. Attempt the tunnel deactivation
			 * path first, it looks up the tunnel connection for this tuple and returns
			 * failure if there is none, in which case fall back to the regular flow
			 * flush below.
			 */
			if (ppe_drv_tun_exception_is_flush_required((ppe_drv_cc_t)cc) &&
					ppe_drv_cc_tun_process_v6(&keys) == PPE_DRV_RET_SUCCESS) {
				break;
			}
#endif
			ppe_drv_cc_process_v6((ppe_drv_cc_t)cc, &keys, src_dev);
			break;

		default:
			ppe_drv_info("%p: dissection failed for skb:%p", p, skb);
	}

push:
	skb_push(skb, ETH_HLEN);
done:
	spin_lock_bh(&p->lock);
	cb = pcc->cb;
	app_data = pcc->app_data;
	spin_unlock_bh(&p->lock);

	ppe_drv_trace("%p: processing skb:%p cc:%u cb:%p app:%p",
			p, skb, cc, cb, app_data);


	if (cb) {
		ret = cb(app_data, skb, (void *)cc_info);
	}

	return ret;
}
EXPORT_SYMBOL(ppe_drv_cc_process_skbuff);

/*
 * ppe_drv_cc_unregister_cb()
 *	Unregister callback for a give service code
 */
void ppe_drv_cc_unregister_cb(ppe_drv_cc_t cc)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_cc *pcc;

	spin_lock_bh(&p->lock);
	pcc = &p->cc[cc];
	ppe_drv_assert(pcc->cb, "%p: no cb registered for cc: %u", p, cc);
	pcc->cb = NULL;
	pcc->app_data = NULL;
	spin_unlock_bh(&p->lock);

	ppe_drv_info("%p: unregistered cb/app_data for cc:%u", p, cc);
}
EXPORT_SYMBOL(ppe_drv_cc_unregister_cb);

/*
 * ppe_drv_cc_register_cb()
 *	Registers a callback for a given cpu code.
 */
void ppe_drv_cc_register_cb(ppe_drv_cc_t cc, ppe_drv_cc_callback_t cb, void *app_data)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_cc *pcc;

	ppe_drv_assert(cb, "%p: cannot register null cb for cc %u", p, cc);

	spin_lock_bh(&p->lock);
	pcc = &p->cc[cc];
	ppe_drv_assert(!pcc->cb, "%p: multiple registration for cc:%u - "
				"prev cb:%p current cb:%p", p, cc, pcc->cb, cb);
	pcc->cb = cb;
	pcc->app_data = app_data;
	spin_unlock_bh(&p->lock);

	ppe_drv_info("%p: registered cb:%p app_data:%p for cc:%u", p, cb, app_data, cc);
}
EXPORT_SYMBOL(ppe_drv_cc_register_cb);

/*
 * ppe_drv_cc_entries_free()
 *	Free cc instance if it was allocated.
 */
void ppe_drv_cc_entries_free(struct ppe_drv_cc *cc)
{
	nss_ppe_drv_minidump_free(cc, "ppe_drv_cc");
	kfree(cc);
}

/*
 * ppe_drv_cc_entries_alloc()
 *	Allocates and initializes cpu code table and update which cpu code we plan to maintain.
 */
struct ppe_drv_cc *ppe_drv_cc_entries_alloc(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_cc *cc;
	struct ppe_drv_cc *pcc;
	const uint8_t max_exception = ppe_drv_exception_max();
	uint8_t cpu_code, i;

	cc = kzalloc(sizeof(struct ppe_drv_cc) * PPE_DRV_CC_MAX, GFP_KERNEL);
	if (!cc) {
		ppe_drv_warn("%p: Failed to allocate cpu code table entries", p);
		return NULL;
	}

	nss_ppe_drv_minidump_log(cc, sizeof(struct ppe_drv_cc) * PPE_DRV_CC_MAX, "ppe_drv_cc");

	/*
	 * Enable cpu code for which connection flush would be attempted.
	 *
	 * Note: we start with all the exceptions for which flow-deceleration is enabled.
	 * If this creates problem we can manually enable specific CPU code for which flush
	 * is needed.
	 */
	for (i = 1; i < max_exception; i++) {
		cpu_code = ppe_drv_exception_list[i].code;
		pcc = &cc[cpu_code];
		pcc->flush = (ppe_drv_exception_list[i].deaccel_en == PPE_DRV_EXCEPTION_DEACCEL_EN)
				? true : false;
	}

	pcc = &cc[PPE_DRV_CC_L2_EXP_MTU_FAIL];
	pcc->flush = true;

	pcc = &cc[PPE_DRV_CC_L3_EXP_MTU_FAIL];
	pcc->flush = true;

	pcc = &cc[PPE_DRV_CC_L3_EXP_FLOW_MTU_FAIL];
	pcc->flush = true;

	/*
	 * Sometime PPE decelerate the flow but, it doesn't produce the original CPU
	 * code with the first exceptioned packet that caused the auto-deceleration.
	 * If we miss that, there is no way to flush an auto-decelerated flow. In such
	 * cases, PPE still generate a specific CPU code which indicate there is a matching
	 * flow for the given packet, but in decelerated status. We rely on this and try to
	 * flush the associated flow with sub-sequent packets.
	 */
	pcc = &cc[PPE_DRV_CC_L3_FLOW_DE_ACCELEARTE];
	pcc->flush = true;

#ifdef PPE_TUNNEL_ENABLE
	/*
	 * Enable connection flush for tunnel exceptions that require a flush.
	 */
	for (i = 0; i < ppe_drv_tun_exception_max(); i++) {
		if (ppe_drv_tun_exception_list[i].flush_required) {
			cpu_code = ppe_drv_tun_exception_list[i].code;
			pcc = &cc[cpu_code];
			pcc->flush = true;
		}
	}
#endif

	return cc;
}
