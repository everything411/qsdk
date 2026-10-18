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

/*
 * netfn_auto_flowmgr.c
 *	Netfn auto flowmgr
 */

#include <netfn_auto.h>
#include "netfn_auto_flowmgr.h"

/*
 * genl_family for flowmgr
 */
static struct genl_family netfn_auto_flowmgr_genl_family;

/*
 * prototypes
 */
int netfn_auto_flowmgr_rule_add(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_flowmgr_rule_delete(struct sk_buff *skb, struct genl_info *info);
static inline int netfn_auto_flowmgr_genl_cmd(struct sk_buff *skb, struct genl_info *info);

/*
 * nla_policy for flowmgr
 */
static struct nla_policy netfn_auto_flowmgr_genl_policy[NETFN_AUTO_FLOWMGR_GNL_MAX + 1] = {
	[FLOW_FLAGS]	= { .type = NLA_U32, },
	[RULE_INFO]	= { .type = NLA_NESTED, },
};

/*
 * operation table called by the generic netlink layer based on the command
 */
static const struct genl_ops netfn_auto_flowmgr_genl_ops[] = {
    {
		.cmd = NETFN_AUTO_FLOWMGR_GENL_CMD,
		.doit = netfn_auto_flowmgr_genl_cmd,
		.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.flags = GENL_ADMIN_PERM,
	},
};

/*
 * flowmgr family definition
 */
static struct genl_family netfn_auto_flowmgr_genl_family = {
	.name           = "flowmgr",
	.version        = 1,
	.hdrsize        = 0,
	.maxattr        = NETFN_AUTO_FLOWMGR_GNL_MAX,
	.policy     	= netfn_auto_flowmgr_genl_policy,
	.netnsok        = true,
	.module         = THIS_MODULE,
	.ops            = netfn_auto_flowmgr_genl_ops,
	.n_ops          = ARRAY_SIZE(netfn_auto_flowmgr_genl_ops),
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
	.resv_start_op  = NETFN_AUTO_FLOWMGR_CMD_MAX,
#endif
};

/*
 * netfn_auto_flowmgr_genl_cmd()
 * 	general command handler.
 */
static inline int netfn_auto_flowmgr_genl_cmd(struct sk_buff *skb, struct genl_info *info)
{
	struct sk_buff *msg = NULL;
	void *hdr = NULL;
	char *cmd;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg) {
		netfn_auto_warn("Failed to allocate netlink message to accomodate rule\n");
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq,
			&netfn_auto_flowmgr_genl_family, 0, NETFN_AUTO_FLOWMGR_GENL_CMD);

	if (!hdr) {
		netfn_auto_warn("Failed to put hdr in netlink buffer\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	if(info->attrs[CMD]) {
		cmd = nla_data(info->attrs[CMD]);
		netfn_auto_info("Flowmgr cmd %s\n", cmd);
		if(strcmp("RULE_ADD", cmd) == 0) {
			netfn_auto_flowmgr_rule_add(skb, info);
		} else if(strcmp("RULE_DEL", cmd) == 0) {
			netfn_auto_flowmgr_rule_delete(skb, info);
		} else {
			netfn_auto_warn("Wrong cmd for flowmgr\n");
			goto fail;
		}

	} else {
		netfn_auto_warn("Command not found for flowmgr\n");
		goto fail;
	}

	genlmsg_end(msg, hdr);
	return genlmsg_unicast(genl_info_net(info), msg, info->snd_portid);

fail:
	return -EINVAL;
}

/*
 * netfn_auto_flowmgr_dump_tuple_info()
 * 	Dump tuple info
 */
static inline void netfn_auto_flowmgr_dump_tuple_info(struct netfn_tuple *original, struct netfn_tuple *reply)
{
	netfn_auto_info("tuple_ip_version: %d\n"
			"tuple_type: %d\n", original->ip_version, original->tuple_type);
	switch (original->tuple_type) {
	case NETFN_AUTO_THREE_TUPLE:
		netfn_auto_info("tuple protocol<Original> : %d\n", original->tuples.tuple_3.protocol);
		netfn_auto_info("IPv4 addr<Original src> :%pI4\n", &original->tuples.tuple_3.src_ip.ip4.s_addr);
		netfn_auto_info("IPv4 addr<Original dest> :%pI4\n", &original->tuples.tuple_3.dest_ip.ip4.s_addr);
		netfn_auto_info("tuple protocol<REPLY> : %d\n", reply->tuples.tuple_3.protocol);
		netfn_auto_info("IPv4 addr<Reply src> :%pI4\n", &reply->tuples.tuple_3.src_ip.ip4.s_addr);
		netfn_auto_info("IPv4 addr<Reply dest> :%pI4\n", &reply->tuples.tuple_3.dest_ip.ip4.s_addr);
	break;

	case NETFN_AUTO_FOUR_TUPLE:
		/*
		 * TODO: get the input from user about port type - source or destination.
		 */
		netfn_auto_info("tuple protocol<Original> : %d\n", original->tuples.tuple_4.protocol);
		netfn_auto_info("IPv4 addr<Original src> :%pI4\n", &original->tuples.tuple_4.src_ip.ip4.s_addr);
		netfn_auto_info("IPv4 addr<Original dest> :%pI4\n", &original->tuples.tuple_4.dest_ip.ip4.s_addr);
		netfn_auto_info("tuple port<Original> : %d\n", original->tuples.tuple_4.l4_ident);
		netfn_auto_info("tuple protocol<REPLY> : %d\n", reply->tuples.tuple_4.protocol);
		netfn_auto_info("IPv4 addr<Reply src> :%pI4\n", &reply->tuples.tuple_4.src_ip.ip4.s_addr);
		netfn_auto_info("IPv4 addr<Reply dest> :%pI4\n", &reply->tuples.tuple_4.dest_ip.ip4.s_addr);
		netfn_auto_info("tuple port<REPLY> : %d\n", reply->tuples.tuple_4.l4_ident);
	break;

	case NETFN_AUTO_FIVE_TUPLE:
		netfn_auto_info("tuple protocol<Original> : %d\n", original->tuples.tuple_5.protocol);
		netfn_auto_info("IPv4 addr<Original src> :%pI4\n", &original->tuples.tuple_5.src_ip.ip4.s_addr);
		netfn_auto_info("IPv4 addr<Original dest> :%pI4\n", &original->tuples.tuple_5.dest_ip.ip4.s_addr);
		netfn_auto_info("src port<Original> : %d\n", original->tuples.tuple_5.l4_src_ident);
		netfn_auto_info("dest port<original> : %d\n", original->tuples.tuple_5.l4_dest_ident);
		netfn_auto_info("tuple protocol<REPLY> : %d\n", reply->tuples.tuple_5.protocol);
		netfn_auto_info("IPv4 addr<Reply src> :%pI4\n", &reply->tuples.tuple_5.src_ip.ip4.s_addr);
		netfn_auto_info("IPv4 addr<Reply dest> :%pI4\n", &reply->tuples.tuple_5.dest_ip.ip4.s_addr);
		netfn_auto_info("src port<Reply> : %d\n", reply->tuples.tuple_5.l4_src_ident);
		netfn_auto_info("dest port<Reply> : %d\n", reply->tuples.tuple_5.l4_dest_ident);
	break;
	}

}
/*
 * netfn_auto_flowmgr_dump_rule_delete()
 */
static inline void netfn_auto_flowmgr_dump_rule_delete(struct netfn_flowmgr_destroy_rule *original, struct netfn_flowmgr_destroy_rule *reply, int accel_mode)
{
	netfn_auto_info("netfn_auto flowmgr params dump for rule_delete cmd\n");
	netfn_auto_info("accel_mode: %d\n", accel_mode);
	netfn_auto_flowmgr_dump_tuple_info(&original->tuple, &reply->tuple);
}

static inline void netfn_auto_flowmgr_create_rule_param_dump(struct netfn_flowmgr_create_rule *flowmgr_rule)
{
	netfn_auto_info("flow_flags: %u\n"
					"flow_src_mac:%pM\n"
					"flow_dest_mac:%pM\n"
					"flow_mtu: %u\n"
					"rule_valid_flags: 0x%llx\n"
					"ip_version: %u\n"
					"src_ip_xlate: %pI4\n"
					"dest_ip_xlate: %pI4\n"
					"src_port_xlate: %u\n"
					"dest_port_xlate: %u\n"
					"src_mac: %pM\n"
					"dest_mac: %pM\n"
					"inner_ingress_vlan_tag: 0x%x\n"
					"inner_egress_vlan_tag: 0x%x\n"
					"outer_ingress_vlan_tag: 0x%x\n"
					"outer_egress_vlan_tag: 0x%x\n"
					"inner_vlan_tpid: %d\n"
					"outer_vlan_tpid: %d\n"
					"dscp_val: %u\n"
					"qos_tag: %u\n"
					"priority: %u\n"
					"pppoe_rule_session_id: %u\n"
					"pppoe_server_mac: %pM\n"
					"br_vlan_filter_rule_ingress_vlan_tag: 0x%x\n"
					"br_vlan_filter_rule_egress_vlan_tag: 0x%x\n"
					"br_vlan_filter_rule_flags: 0x%x\n"
					"br_vlan_filter_rule_vlan_tpid: %u\n"
					"udp_lite_rule_csum_cov: %u\n"
					"sawf_rule_mark: %u\n"
					"sawf_rule_svc_id: %u\n"
					"qdisc_rule_valid_flags: 0x%x\n",
			flowmgr_rule->flow_flags,
			flowmgr_rule->flow_info.flow_src_mac,
			flowmgr_rule->flow_info.flow_dest_mac,
			flowmgr_rule->flow_info.flow_mtu,
			flowmgr_rule->rule_info.rule_valid_flags,
			flowmgr_rule->rule_info.ip_xlate_rule.ip_version,
			flowmgr_rule->rule_info.ip_xlate_rule.src_ip_xlate,
			flowmgr_rule->rule_info.ip_xlate_rule.dest_ip_xlate,
			flowmgr_rule->rule_info.ip_xlate_rule.src_port_xlate,
			flowmgr_rule->rule_info.ip_xlate_rule.dest_port_xlate,
			flowmgr_rule->rule_info.mac_xlate_rule.src_mac,
			flowmgr_rule->rule_info.mac_xlate_rule.dest_mac,
			flowmgr_rule->rule_info.vlan_rule.inner.ingress_vlan_tag,
			flowmgr_rule->rule_info.vlan_rule.inner.egress_vlan_tag,
			flowmgr_rule->rule_info.vlan_rule.outer.ingress_vlan_tag,
			flowmgr_rule->rule_info.vlan_rule.outer.egress_vlan_tag,
			flowmgr_rule->rule_info.vlan_rule.inner_vlan_tpid,
			flowmgr_rule->rule_info.vlan_rule.outer_vlan_tpid,
			flowmgr_rule->rule_info.dscp_rule.dscp_val,
			flowmgr_rule->rule_info.qos_rule.qos_tag,
			flowmgr_rule->rule_info.qos_rule.priority,
			flowmgr_rule->rule_info.pppoe_rule.session_id,
			flowmgr_rule->rule_info.pppoe_rule.server_mac,
			flowmgr_rule->rule_info.vlan_filter_rule.vlan_info.ingress_vlan_tag,
			flowmgr_rule->rule_info.vlan_filter_rule.vlan_info.egress_vlan_tag,
			flowmgr_rule->rule_info.vlan_filter_rule.flags,
			flowmgr_rule->rule_info.vlan_filter_rule.vlan_tpid,
			flowmgr_rule->rule_info.udp_lite_rule.csum_cov,
			flowmgr_rule->rule_info.sawf_rule.mark,
			flowmgr_rule->rule_info.sawf_rule.svc_id,
			flowmgr_rule->rule_info.qdisc_rule.valid_flags);
}

/*
 * netfn_auto_flowmgr_dump_rule_add()
 * 	Dump params for rule_add cmd
 */
static inline void netfn_auto_flowmgr_dump_rule_add(struct netfn_flowmgr_create_rule *original, struct netfn_flowmgr_create_rule *reply, int accel_mode)
{
	netfn_auto_info("netfn_auto flowmgr params dump for rule_add cmd\n");
	netfn_auto_info("accel_mode: %d\n", accel_mode);
	netfn_auto_flowmgr_dump_tuple_info(&original->tuple, &reply->tuple);
	netfn_auto_info("\n**********************************************************\n");
	netfn_auto_info("*** Flow direction ***\n");
	netfn_auto_flowmgr_create_rule_param_dump(original);
	netfn_auto_info("\n**********************************************************\n");
	netfn_auto_info("*** Return direction ***\n");
	netfn_auto_flowmgr_create_rule_param_dump(reply);
}

/*
 * netfn_auto_flowmgr_rule_delete()
 * 	flowmgr rule delete handler
 */
int netfn_auto_flowmgr_rule_delete(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *tuple = NULL;
	struct netfn_flowmgr_destroy_rule original = {0}, reply = {0};
	char *mode;
	uint8_t accel_mode = NETFN_AUTO_FLOWMGR_ACCEL_PPE;
	netfn_flowmgr_ret_t ret;

	if(info->attrs[ACCEL_MODE]) {
		mode = nla_data(info->attrs[ACCEL_MODE]);
		netfn_auto_info("accel_mode: %s\n", mode);
		if(strcmp("SFE", mode) == 0) {
			accel_mode = NETFN_AUTO_FLOWMGR_ACCEL_SFE;
		} else if(strcmp("PPE", mode) == 0) {
			accel_mode = NETFN_AUTO_FLOWMGR_ACCEL_PPE;
		} else {
			netfn_auto_warn("Wrong accel_mode, default PPE\n");
		}
	} else {
		netfn_auto_warn("Accel mode not present, default PPE\n");
	}

	if(info->attrs[TUPLE_INFO]) {
		tuple = info->attrs[TUPLE_INFO];
		if (!netfn_auto_flowmgr_parse_tuple(&original.tuple, &reply.tuple, tuple)) {
			netfn_auto_warn("Tuple parsing fail for flowmgr rule delete\n");
			goto fail;
		}

	} else {
		netfn_auto_warn("Tuple info not present\n");
		goto fail;
	}

	/*
	 * Print rule delete parameters
	 */
	netfn_auto_flowmgr_dump_rule_delete(&original, &reply, accel_mode);

	/*
	 * call netfn_flowmgr_rule_decel() api with the parameters
	 */
	ret = netfn_flowmgr_rule_decel(&original, &reply, accel_mode);
	if (ret) {
		netfn_auto_info_always("Flowmgr rule delete failed with return: 0x%x\n", ret);
		goto fail;
	}

	netfn_auto_info_always("Flowmgr rule delete success\n");
	return ret;

fail:
	return -EINVAL;
}

/*
 * netfn_auto_flowmgr_rule_add()
 * 	flowmgr rule add handler
 */
int netfn_auto_flowmgr_rule_add(struct sk_buff *skb, struct genl_info *info)
{
	uint32_t flag = 0, flag_ret = 0;
	int rem;
	struct nlattr *rule = NULL, *attr = NULL, *flow_info = NULL;
	struct netfn_flowmgr_create_rule original = {0}, reply = {0};
	char *mode;
	uint8_t accel_mode = NETFN_AUTO_FLOWMGR_ACCEL_PPE;
	netfn_flowmgr_ret_t ret;

	if(info->attrs[ACCEL_MODE]) {
		mode = nla_data(info->attrs[ACCEL_MODE]);
		netfn_auto_info("From Netlink, accel_mode: %s\n", mode);
		if(strcmp("SFE", mode) == 0) {
			accel_mode = NETFN_AUTO_FLOWMGR_ACCEL_SFE;
		} else if(strcmp("PPE", mode) == 0) {
			accel_mode = NETFN_AUTO_FLOWMGR_ACCEL_PPE;
		} else {
			netfn_auto_warn("Wrong accel_mode, default PPE\n");
		}
	} else {
		netfn_auto_warn("Accel_mode not present, default PPE\n");
	}

	if(info->attrs[FLOW_FLAGS]) {
		struct nlattr *flow_flags = NULL, *attr = info->attrs[FLOW_FLAGS];
		int rem;

		nla_for_each_nested(flow_flags, attr, rem) {
			switch(nla_type(flow_flags)) {
			case BRIDGE:
				flag |= NETFN_FLOWMGR_FLOW_FLAG_BRIDGE_FLOW;
				netfn_auto_info("FLOW FLAG BRIDGE FLOW, updated flag val : 0x%x\n", flag);
			break;

			case DS:
				flag |= NETFN_FLOWMGR_FLOW_FLAG_DS_FLOW;
				netfn_auto_info("FLOW FLAG DS FLOW, updated flag val : 0x%x\n", flag);
			break;

			case VP:
				flag |= NETFN_FLOWMGR_FLOW_FLAG_VP_FLOW;
				netfn_auto_info("FLOW FLAG VP FLOW, updated flag val : 0x%x\n", flag);
			break;

			case SRC_MAC:
				flag |= NETFN_FLOWMGR_FLOW_FLAG_SRC_MAC;
				netfn_auto_info("FLOW FLAG SRC MAC, updated flag val : 0x%x\n", flag);
			break;

			default:
				netfn_auto_warn("Wrong flow flag\n");
			}
		}

		netfn_auto_info("flow flags: 0x%x\n", flag);
		original.flow_flags = flag;
		reply.flow_flags = flag;
	} else {
		netfn_auto_warn("flow flag not present\n");
	}

	if(info->attrs[TUPLE_INFO]) {
		rule = info->attrs[TUPLE_INFO];
		if (!netfn_auto_flowmgr_parse_tuple(&original.tuple, &reply.tuple, rule)) {
			netfn_auto_warn("Error in tuple parsing for flowmgr rule add\n");
			goto fail;
		}

	} else {
		netfn_auto_warn("Tuple info not present\n");
		goto fail;
	}

	if (info->attrs[FLOW_INFO]) {
		char *dev_name, *smac, *dmac;
		uint32_t flow_mtu;
		int rem;
		flow_info = info->attrs[FLOW_INFO];

		nla_for_each_nested(attr ,flow_info, rem) {
			netfn_auto_info("Flow_INFO attr->nla_type: %d\n", nla_type(attr));
			switch (nla_type(attr)) {
			case FLOW_IN_DEV:
				dev_name = nla_data(attr);
				netfn_auto_info("FLOW INFO FLOW_IN_DEV: %s\n", dev_name);
				original.flow_info.in_dev = dev_get_by_name(&init_net, dev_name);
				reply.flow_info.out_dev = original.flow_info.in_dev;
				if (original.flow_info.in_dev) {
					dev_put(original.flow_info.in_dev);
				}

				break;

			case FLOW_OUT_DEV:
				dev_name = nla_data(attr);
				netfn_auto_info("FLOW INFO FLOW_OUT_DEV: %s\n", dev_name);
				original.flow_info.out_dev = dev_get_by_name(&init_net, dev_name);
				reply.flow_info.in_dev = original.flow_info.out_dev;
				if (original.flow_info.out_dev) {
					dev_put(original.flow_info.out_dev);
				}

				break;

			case FLOW_SRC_MAC:
				smac = nla_data(attr);
				netfn_auto_info("flow src mac: %s\n", smac);
				if (!netfn_auto_flowmgr_verify_mac(smac, original.flow_info.flow_src_mac, reply.flow_info.flow_dest_mac)) {
					netfn_auto_warn("Invalid flow src mac address\n");
					goto fail;
				}

				break;

			case FLOW_DEST_MAC:
				dmac = nla_data(attr);
				netfn_auto_info("flow dest mac: %s\n", dmac);
				if (!netfn_auto_flowmgr_verify_mac(dmac, original.flow_info.flow_dest_mac, reply.flow_info.flow_src_mac)) {
					netfn_auto_warn("Invalid flow dest mac address\n");
					goto fail;
				}

				break;

			case FLOW_TOP_INDEV:
				dev_name = nla_data(attr);
				netfn_auto_info("FLOW INFO FLOW_TOP_INDEV: %s\n", dev_name);
				original.flow_info.top_indev = dev_get_by_name(&init_net, dev_name);
				reply.flow_info.top_outdev = original.flow_info.top_indev;
				if (original.flow_info.top_indev) {
					dev_put(original.flow_info.top_indev);
				}

				break;

			case FLOW_TOP_OUTDEV:
				dev_name = nla_data(attr);
				netfn_auto_info("FLOW INFO FLOW_TOP_OUTDEV: %s\n", dev_name);
				original.flow_info.top_outdev = dev_get_by_name(&init_net, dev_name);
				reply.flow_info.top_indev = original.flow_info.top_outdev;
				if (original.flow_info.top_outdev) {
					dev_put(original.flow_info.top_outdev);
				}

				break;

			case FLOW_MTU:
				flow_mtu = nla_get_u32(attr);
				netfn_auto_info("FLOW INFO FLOW_MTU: %d\n", flow_mtu);
				original.flow_info.flow_mtu = flow_mtu;
				reply.flow_info.flow_mtu = flow_mtu;
				break;

			default:
				netfn_auto_warn("WRONG attr: %d in FLOW INFO\n", nla_type(attr));
				break;
			}
		}

	} else {
		netfn_auto_warn("flow info not present\n");
		goto fail;
	}

	if (info->attrs[RULE_INFO]) {
		rule = info->attrs[RULE_INFO];
		netfn_auto_info("RULE INFO NOT EMPTY\n");

		nla_for_each_nested(attr ,rule, rem) {
			netfn_auto_info("attr->nla_type: %d\n", nla_type(attr));
			if(nla_type(attr) == VALID_FLAGS) {
				struct nlattr * valid_flags = NULL;
				int rem;

				nla_for_each_nested(valid_flags, attr, rem) {
					flag = 0;
					switch(nla_type(valid_flags)) {
						case VLAN:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_VLAN;
							netfn_auto_info("VALID FLAG vlan, updated flag val : 0x%x\n", flag);
							break;

						case PPPOE_ORG:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_PPPOE;
							netfn_auto_info("VALID FLAG PPPOE ORG, updated flag val : 0x%x\n", flag);
							break;

						case PPPOE_REPLY:
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_PPPOE;
							netfn_auto_info("VALID FLAG PPPOE_REPLY, updated flag val : 0x%x\n", flag_ret);
							break;

						case DSCP_MARKING_ORG:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_DSCP_MARKING;
							netfn_auto_info("VALID FLAG DSCP MARKING ORG, updated flag val : 0x%x\n", flag);
							break;

						case DSCP_MARKING_REPLY:
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_DSCP_MARKING;
							netfn_auto_info("VALID FLAG DSCP MARKING REPLY, updated flag val : 0x%x\n", flag_ret);
							break;

						case PRIORITY:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_PRIORITY;
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_PRIORITY;
							netfn_auto_info("VALID FLAG PRIORITY, updated flag val : 0x%x\n", flag);
							break;

						case TRUSTSEC:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_TRUSTSEC;
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_PRIORITY;
							netfn_auto_info("VALID FLAG PRIORITY, updated flag val : 0x%x\n", flag);
							break;

						case QOS_ORG:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_QOS;
							netfn_auto_info("VALID FLAG QOS ORG, updated flag val : 0x%x\n", flag);
							break;

						case QOS_REPLY:
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_QOS;
							netfn_auto_info("VALID FLAG QOS REPLY, updated flag val : 0x%x\n", flag_ret);
							break;

						case SAWF:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_SAWF;
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_SAWF;
							netfn_auto_info("VALID FLAG SAWF, updated flag val : 0x%x\n", flag);
							break;

						case QDISC_ORG:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_QDISC;
							netfn_auto_info("VALID FLAG QDISC, updated flag val : 0x%x\n", flag);
							break;

						case QDISC_REPLY:
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_QDISC;
							netfn_auto_info("VALID FLAG QDISC reply, updated flag val : 0x%x\n", flag_ret);
							break;

						case VLAN_FILTER:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_VLAN_FILTER;
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_VLAN_FILTER;
							netfn_auto_info("VALID FLAG VLAN_FILTER, updated flag val : 0x%x\n", flag);
							break;

						case UDP_LITE:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_UDP_LITE;
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_UDP_LITE;
							netfn_auto_info("VALID FLAG UDP LITE, updated flag val : 0x%x\n", flag);
							break;

						case SRC_NAT:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_SRC_NAT;
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_SRC_NAT;
							netfn_auto_info("VALID FLAG SRC_NAT, updated flag val : 0x%x\n", flag);
							break;

						case DST_NAT:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_DST_NAT;
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_DST_NAT;
							netfn_auto_info("VALID FLAG DST_NAT, updated flag val : 0x%x\n", flag);
							break;

						case MAC:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_MAC;
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_MAC;
							netfn_auto_info("VALID FLAG MAC, updated flag val : 0x%x\n", flag);
							break;

						case TCP:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_TCP;
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_TCP;
							netfn_auto_info("VALID FLAG TCP, updated flag val : 0x%x\n", flag);
							break;

						case NOEDIT_ORG:
							flag |= NETFN_FLOWMGR_VALID_RULE_FLAG_NOEDIT_RULE;
							netfn_auto_info("VALID FLAG NOEDIT, updated flag val : 0x%x\n", flag);
							break;

						case NOEDIT_REPLY:
							flag_ret |= NETFN_FLOWMGR_VALID_RULE_FLAG_NOEDIT_RULE;
							netfn_auto_info("VALID FLAG NOEDIT reply, updated flag val : 0x%x\n", flag_ret);
							break;

						default:
							netfn_auto_warn("Wrong valid flag %d\n", nla_type(valid_flags));
					}
				}

				original.rule_info.rule_valid_flags = flag;
				reply.rule_info.rule_valid_flags = flag_ret;
				netfn_auto_info("original rule flags: 0x%x\n", flag);
				netfn_auto_info("reply rule flags: 0x%x\n", flag_ret);
			} else if (nla_type(attr) == IP_XLATE_RULE) {
				if (!netfn_auto_flowmgr_parse_ip_xlate(&original, &reply, attr)) {
					netfn_auto_warn("Error in IP_XLATE_RULE parsing for flowmgr rule add\n");
					goto fail;
				}

				netfn_auto_info("IP_XLATE_RULE parsing success for flowmgr rule add\n");
			} else if (nla_type(attr) == MAC_XLATE_RULE) {
					struct nlattr *mac_xlate_rule = NULL;
					char *smac, *dmac;
					int rem;
					netfn_auto_info("MAC_XLATE_RULE\n");
					nla_for_each_nested(mac_xlate_rule, attr, rem) {
						switch(nla_type(mac_xlate_rule)) {
						case SRC_MAC:
							smac = nla_data(mac_xlate_rule);
							netfn_auto_info("src mac: %s\n", smac);
							if (!netfn_auto_flowmgr_verify_mac(smac, original.rule_info.mac_xlate_rule.src_mac, reply.rule_info.mac_xlate_rule.dest_mac)) {
								netfn_auto_warn("Invalid src mac address\n");
								goto fail;
							}
							break;

						case DEST_MAC:
							dmac = nla_data(mac_xlate_rule);
							netfn_auto_info("dest mac: %s\n", dmac);
							if (!netfn_auto_flowmgr_verify_mac(dmac, original.rule_info.mac_xlate_rule.dest_mac, reply.rule_info.mac_xlate_rule.src_mac)) {
								netfn_auto_warn("Invalid dest mac address\n");
								goto fail;
							}
							break;

						default:
							netfn_auto_warn("Invalid type in mac xlate rule\n");
						}
					}

					netfn_auto_info("MAC_XLATE_RULE parsing success for flowmgr rule add\n");
			} else if(nla_type(attr) == UDP_LITE_RULE) {
				struct nlattr *udp_lite_rule = NULL;
				int rem;
				uint32_t c_cov;

				netfn_auto_info("UDP_LITE_RULE\n");
				nla_for_each_nested(udp_lite_rule, attr, rem) {

					switch(nla_type(udp_lite_rule)) {
					case CHECKSUM_COVERAGE:
						c_cov = nla_get_u32(udp_lite_rule);
						netfn_auto_info("From netlink, udp_lite_rule checksum_coverage: %d\n", c_cov);
						original.rule_info.udp_lite_rule.csum_cov = c_cov;
						reply.rule_info.udp_lite_rule.csum_cov = c_cov;
						break;

					default:
						netfn_auto_warn("Unknown type in udp_lite_rule");
					}
				}

			} else if (nla_type(attr) == PPPOE_RULE) {

				/*
				 * TODO: Need to seperate out PPPOE_RULE for flow and return direction
				 */
				struct nlattr *pppoe_rule = NULL;
				int session_id, rem;
				char *server_mac;
				netfn_auto_info("PPPOE_RULE\n");

				nla_for_each_nested(pppoe_rule, attr, rem) {
					switch(nla_type(pppoe_rule)) {
					case SESSION_ID:
						session_id = nla_get_u32(pppoe_rule);
						netfn_auto_info("From Netlink, pppoe_rule session_id: %d\n", session_id);
						original.rule_info.pppoe_rule.session_id = session_id;
						reply.rule_info.pppoe_rule.session_id = session_id;
						break;

					case SERVER_MAC:
						server_mac = nla_data(pppoe_rule);
						netfn_auto_info("From Netlink, pppoe_rule server_mac: %s\n", server_mac);
						if (!netfn_auto_flowmgr_verify_mac(server_mac, original.rule_info.pppoe_rule.server_mac, reply.rule_info.pppoe_rule.server_mac)) {
							netfn_auto_warn("Invalid server mac address\n");
							goto fail;
						}

						break;

					default:
						netfn_auto_warn("Unknown type in pppoe_rule\n");
					}
				}

				netfn_auto_info("PPPOE RULE parsing successful for flowmgr rule add\n");
			} else if (nla_type(attr) == QOS_RULE) {
				struct nlattr *qos_rule = NULL;
				int qos_tag, priority, rem;
				char *net_dev;
				netfn_auto_info("QOS_RULE\n");

				nla_for_each_nested(qos_rule, attr, rem) {
					switch(nla_type(qos_rule)) {
					case WIFI_QOS_TAG:
						qos_tag = nla_get_u32(qos_rule);
						netfn_auto_info("qos_rule qos_tag: %d\n", qos_tag);
						original.rule_info.qos_rule.qos_tag = qos_tag;
						reply.rule_info.qos_rule.qos_tag = qos_tag;
						break;

					case PRIORITY:
						priority = nla_get_u32(qos_rule);
						netfn_auto_info("qos_rule priority: %d\n", priority);
						original.rule_info.qos_rule.priority = priority;
						reply.rule_info.qos_rule.priority = priority;
						break;

					case NET_DEV:
						net_dev = nla_data(qos_rule);
						netfn_auto_info("qos_rule net_dev: %s\n", net_dev);
						original.rule_info.qos_rule.dev = dev_get_by_name(&init_net, net_dev);
						if (original.rule_info.qos_rule.dev) {
							dev_put(original.rule_info.qos_rule.dev);
						}

						break;

					default:
						netfn_auto_warn("Unknown type in qos_rule\n");
					}
				}

				netfn_auto_info("QOS RULE parsing successful for flowmgr rule add\n");
			} else if (nla_type(attr) == DSCP_RULE) {
				struct nlattr *dscp_rule = NULL;
				int dscp_val, rem;
				netfn_auto_info("DSCP_RULE\n");

				nla_for_each_nested(dscp_rule, attr, rem) {
					switch(nla_type(dscp_rule)) {
					case DSCP_VAL:
						dscp_val = nla_get_u32(dscp_rule);
						netfn_auto_info("dscp_rule dscp_val: %d\n", dscp_val);
						original.rule_info.dscp_rule.dscp_val = dscp_val;
						reply.rule_info.dscp_rule.dscp_val = dscp_val;
						break;

					default:
						netfn_auto_warn("Unknown type in dscp_rule\n");
					}
				}

				netfn_auto_info("DSCP RULE parsing successful for flowmgr rule add\n");
			} else if (nla_type(attr) == VLAN_RULE) {
				if (!netfn_auto_flowmgr_parse_vlan(&original, &reply, attr)) {
					netfn_auto_warn("Error in VLAN_RULE for flowmgr rule add\n");
					goto fail;
				}

				netfn_auto_info("VLAN_RULE parsing successful for flowmgr rule add\n");
			} else if (nla_type(attr) == VLAN_FILTER_RULE) {
				struct nlattr *vlan_filter_rule = NULL;
				int rem;
				uint32_t tag, flags;
				char *_tag;
				netfn_auto_info("VLAN_FILTER_RULE\n");

				nla_for_each_nested(vlan_filter_rule, attr, rem) {
					switch(nla_type(vlan_filter_rule)) {
					case VLAN_TAG:
						_tag = nla_data(vlan_filter_rule);
						if(kstrtou32(_tag, 16, &tag) < 0) {
							netfn_auto_warn("Unable to convert VLAN_FILTER_RULE tag\n");
							goto fail;
						}

						netfn_auto_info("VLAN_FILTER_RULE tag: 0x%x\n", tag);
						original.rule_info.vlan_filter_rule.vlan_tpid = tag;
						reply.rule_info.vlan_filter_rule.vlan_tpid = tag;
						break;

					case VLAN_FLAGS:
						flags = nla_get_u32(vlan_filter_rule);
						netfn_auto_info("VLAN_FILTER_RULE flags: %d\n", flags);
						original.rule_info.vlan_filter_rule.flags = flags;
						reply.rule_info.vlan_filter_rule.flags = flags;
						break;

					case INGRESS_VLAN_TAG:
						tag = nla_get_u32(vlan_filter_rule);
						original.rule_info.vlan_filter_rule.vlan_info.ingress_vlan_tag = tag;
						break;

					case EGRESS_VLAN_TAG:
						tag = nla_get_u32(vlan_filter_rule);
						original.rule_info.vlan_filter_rule.vlan_info.egress_vlan_tag = tag;
						break;

					default:
						netfn_auto_warn("Unknown type in VLAN_FILTER_RULE\n");
					}
				}

				netfn_auto_info("VLAN_FILTER_RULE parsing successful for flowmgr rule add\n");
			}

			else {
				netfn_auto_warn("Unknown attribute for RULE_INFO\n");
			}
		}
	} else {
		netfn_auto_warn("RULE_INFO parse fail for flowmgr rule add\n");
		goto fail;
	}

	/*
	 * Print rule delete parameters
	 */
	netfn_auto_flowmgr_dump_rule_add(&original, &reply, accel_mode);

	/*
	 * netfn_flowmgr_rule_accel() api
	 */
	ret = netfn_flowmgr_rule_accel(&original, &reply, accel_mode);
	if (NETFN_FLOWMGR_GET_NETFN_ERROR_CODE(ret) || NETFN_FLOWMGR_GET_AE_ERROR_CODE(ret)) {
		netfn_auto_info_always("Flowmgr rule add failed with return: 0x%x\n", ret);
		goto fail;
	}

	netfn_auto_info_always("Flowmgr rule add success\n");
	return ret;

fail:
	return -EINVAL;
}

/*
 * netfn_auto_flowmgr_init()
 *	init module
 */
static int __init netfn_auto_flowmgr_init(void)
{
	int err;
	err = genl_register_family(&netfn_auto_flowmgr_genl_family);
	if(err) {
		netfn_auto_info_always("Failed to register flowmgr generic netlink family with error: %d\n", err);
		return -1;
	}

	netfn_auto_info_always("Flowmgr family registration successful\n");

	return 0;
}

/*
 * netfn_auto_flowmgr_exit()
 *	deinit module
 */
static void __exit netfn_auto_flowmgr_exit(void)
{

	int err;
	err = genl_unregister_family(&netfn_auto_flowmgr_genl_family);
	if(err) {
		netfn_auto_info_always("Failed to unregister flowmgr generic netlink family with error: %d\n", err);
		return;
	}

	netfn_auto_info_always("netfn_auto flowmgr exit complete\n");
	return;

}

module_init(netfn_auto_flowmgr_init);
module_exit(netfn_auto_flowmgr_exit);

MODULE_DESCRIPTION("NETFN AUTO FLOWMGR");
MODULE_LICENSE("Dual BSD/GPL");
