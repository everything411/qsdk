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
 * netfn_auto_flowmgr.h
 *	Netfn auto flowmgr
 */

#ifndef __NETFN_AUTO_FLOWMGR_H
#define __NETFN_AUTO_FLOWMGR_H

/*
 * netfn_auto_flowmgr_accel_mode
 * 	accel mode for flowmgr
 */
enum netfn_auto_flowmgr_accel_mode {
	NETFN_AUTO_FLOWMGR_ACCEL_SFE = 1,	/* accel mode for sfe */
	NETFN_AUTO_FLOWMGR_ACCEL_PPE = 2,	/* accel mode for ppe */
	NETFN_AUTO_FLOWMGR_ACCEL_MAX,
};

/*
 * netfn_auto_flowmgr_verify_ip()
 * 	converts ip address from string
 */
bool netfn_auto_flowmgr_verify_ip(const char *ip_str, uint32_t *original_ip_addr, uint32_t *reply_ip_addr, uint8_t protocol)
{
	return netfn_auto_verify_ip(ip_str, original_ip_addr, protocol) && netfn_auto_verify_ip(ip_str, reply_ip_addr, protocol);
}

/*
 * netfn_auto_flowmgr_verify_mac()
 * 	converts mac address from string
 */
bool netfn_auto_flowmgr_verify_mac(char *str_mac, uint8_t original_mac[], uint8_t reply_mac[])
{
	return netfn_auto_verify_mac(str_mac, original_mac) && netfn_auto_verify_mac(str_mac, reply_mac);
}

/*
 * netfn_auto_flowmgr_parse_tuple()
 * 	parse tuple info
 */
bool netfn_auto_flowmgr_parse_tuple(struct netfn_tuple *original, struct netfn_tuple *reply, struct nlattr *tuple)
{
	int tuple_type, tuple_ip_version;

	if (!netfn_auto_parse_tuple(original, tuple)) {
		return false;
	}

	if (!netfn_auto_parse_tuple(reply, tuple)) {
		return false;
	}

	tuple_type = original->tuple_type;
	tuple_ip_version = original->ip_version;

	switch(tuple_type) {
	case NETFN_AUTO_THREE_TUPLE:
		if (tuple_ip_version == NETFN_AUTO_IPV4) {
			reply->tuples.tuple_3.dest_ip.ip4.s_addr = original->tuples.tuple_3.src_ip.ip4.s_addr;
			reply->tuples.tuple_3.src_ip.ip4.s_addr = original->tuples.tuple_3.dest_ip.ip4.s_addr;
			netfn_auto_info("IPv4 addr<REPLY SRC> :%pI4\n", &reply->tuples.tuple_3.src_ip.ip4.s_addr);
			netfn_auto_info("IPv4 addr<REPLY DEST> :%pI4\n", &reply->tuples.tuple_3.dest_ip.ip4.s_addr);

		} else if (tuple_ip_version == NETFN_AUTO_IPV6) {
			memcpy(reply->tuples.tuple_3.dest_ip.ip6.s6_addr32, original->tuples.tuple_3.src_ip.ip6.s6_addr32, sizeof(original->tuples.tuple_3.src_ip.ip6.s6_addr32));
			memcpy(reply->tuples.tuple_3.src_ip.ip6.s6_addr32, original->tuples.tuple_3.dest_ip.ip6.s6_addr32, sizeof(original->tuples.tuple_3.dest_ip.ip6.s6_addr32));
			netfn_auto_info("IPv6 addr<REPLY SRC> :%pI6\n", &reply->tuples.tuple_3.src_ip.ip6.s6_addr32);
			netfn_auto_info("IPv6 addr<REPLY DEST> :%pI6\n", &reply->tuples.tuple_3.dest_ip.ip6.s6_addr32);
		}
		break;

	case NETFN_AUTO_FOUR_TUPLE:
		if (tuple_ip_version == NETFN_AUTO_IPV4) {
			reply->tuples.tuple_4.dest_ip.ip4.s_addr = original->tuples.tuple_4.src_ip.ip4.s_addr;
			reply->tuples.tuple_4.src_ip.ip4.s_addr = original->tuples.tuple_4.dest_ip.ip4.s_addr;
			netfn_auto_info("IPv4 addr<REPLY SRC> :%pI4\n", &reply->tuples.tuple_4.src_ip.ip4.s_addr);
			netfn_auto_info("IPv4 addr<REPLY DEST> :%pI4\n", &reply->tuples.tuple_4.dest_ip.ip4.s_addr);
			netfn_auto_info("REPLY L4_IDENT :%pI4\n", &reply->tuples.tuple_4.l4_ident);

		} else if (tuple_ip_version == NETFN_AUTO_IPV6) {
			memcpy(reply->tuples.tuple_4.dest_ip.ip6.s6_addr32, original->tuples.tuple_4.src_ip.ip6.s6_addr32, sizeof(original->tuples.tuple_4.src_ip.ip6.s6_addr32));
			memcpy(reply->tuples.tuple_4.src_ip.ip6.s6_addr32, original->tuples.tuple_4.dest_ip.ip6.s6_addr32, sizeof(original->tuples.tuple_4.dest_ip.ip6.s6_addr32));
			netfn_auto_info("IPv6 addr<REPLY SRC> :%pI6\n", &reply->tuples.tuple_4.src_ip.ip6.s6_addr32);
			netfn_auto_info("IPv6 addr<REPLY DEST> :%pI6\n", &reply->tuples.tuple_4.dest_ip.ip6.s6_addr32);
		}
		break;

	case NETFN_AUTO_FIVE_TUPLE:
		if (tuple_ip_version == NETFN_AUTO_IPV4) {
			reply->tuples.tuple_5.dest_ip.ip4.s_addr = original->tuples.tuple_5.src_ip.ip4.s_addr;
			reply->tuples.tuple_5.src_ip.ip4.s_addr = original->tuples.tuple_5.dest_ip.ip4.s_addr;
			netfn_auto_info("IPv4 addr<REPLY SRC> :%pI4\n", &reply->tuples.tuple_5.src_ip.ip4.s_addr);
			netfn_auto_info("IPv4 addr<REPLY DEST> :%pI4\n", &reply->tuples.tuple_5.dest_ip.ip4.s_addr);

		} else if (tuple_ip_version == NETFN_AUTO_IPV6) {
			memcpy(reply->tuples.tuple_5.dest_ip.ip6.s6_addr32, original->tuples.tuple_5.src_ip.ip6.s6_addr32, sizeof(original->tuples.tuple_3.src_ip.ip6.s6_addr32));
			memcpy(reply->tuples.tuple_5.src_ip.ip6.s6_addr32, original->tuples.tuple_5.dest_ip.ip6.s6_addr32, sizeof(original->tuples.tuple_3.dest_ip.ip6.s6_addr32));
			netfn_auto_info("IPv6 addr<REPLY SRC> :%pI6\n", &reply->tuples.tuple_5.src_ip.ip6.s6_addr32);
			netfn_auto_info("IPv6 addr<REPLY DEST> :%pI6\n", &reply->tuples.tuple_5.dest_ip.ip6.s6_addr32);
		}

		reply->tuples.tuple_5.l4_dest_ident = original->tuples.tuple_5.l4_src_ident;
		reply->tuples.tuple_5.l4_src_ident = original->tuples.tuple_5.l4_dest_ident;
		netfn_auto_info("REPLY SRC PORT :%d\n", reply->tuples.tuple_5.l4_src_ident);
		netfn_auto_info("REPLY DEST PORT :%d\n", reply->tuples.tuple_5.l4_dest_ident);

	}

	return true;
}

/*
 * netfn_auto_flowmgr_parse_ip_xlate()
 * 	parse ip_xlate info
 */
bool netfn_auto_flowmgr_parse_ip_xlate(struct netfn_flowmgr_create_rule *original, struct netfn_flowmgr_create_rule *reply, struct nlattr *attr)
{
	struct nlattr *ip_xlate_rule = NULL;
	char *sip, *dip;
	int rem, port;
	uint32_t ip_version;

	nla_for_each_nested(ip_xlate_rule, attr, rem) {
		netfn_auto_info("ip_xlate_rule->nla_type: %d\n", nla_type(ip_xlate_rule));

		switch(nla_type(ip_xlate_rule)) {
		case IP_VERSION :
			ip_version = nla_get_u32(ip_xlate_rule);
			netfn_auto_info("From Netlink, ip version: %d\n", ip_version);
			original->rule_info.ip_xlate_rule.ip_version = ip_version;
			reply->rule_info.ip_xlate_rule.ip_version = ip_version;
			break;

		case SRC_IP :
			sip = nla_data(ip_xlate_rule);
			netfn_auto_info("From Netlink, src ip: %s\n", sip);
			if(!netfn_auto_flowmgr_verify_ip(sip, original->rule_info.ip_xlate_rule.src_ip_xlate, reply->rule_info.ip_xlate_rule.dest_ip_xlate, ip_version)) {
				return false;
			}
			break;

		case DEST_IP :
			dip = nla_data(ip_xlate_rule);
			netfn_auto_info("From Netlink, dest ip: %s\n", dip);
			if(!netfn_auto_flowmgr_verify_ip(dip, original->rule_info.ip_xlate_rule.dest_ip_xlate, reply->rule_info.ip_xlate_rule.src_ip_xlate, ip_version)) {
				return false;
			}
			break;

		case SRC_PORT :
			port = nla_get_u32(ip_xlate_rule);
			netfn_auto_info("From Netlink, src port: %d\n", port);
			original->rule_info.ip_xlate_rule.src_port_xlate = port;
			reply->rule_info.ip_xlate_rule.dest_port_xlate = port;
			break;

		case DEST_PORT :
			port = nla_get_u32(ip_xlate_rule);
			netfn_auto_info("From Netlink, dest port: %d\n", port);
			original->rule_info.ip_xlate_rule.dest_port_xlate = port;
			reply->rule_info.ip_xlate_rule.src_port_xlate = port;
			break;

		default:
			netfn_auto_warn("Invalid type in ip xlate rule\n");
			return false;
		}
	}

	return true;
}

/*
 * netfn_auto_flowmgr_parse_vlan()
 * 	parse vlan info
 */
bool netfn_auto_flowmgr_parse_vlan(struct netfn_flowmgr_create_rule *original, struct netfn_flowmgr_create_rule *reply, struct nlattr *attr)
{
	struct nlattr *vlan_rule;
	uint32_t inner, outer;
	char *_inner, *_outer;
	uint16_t inner_vlan_tpid, outer_vlan_tpid;
	int rem;

	netfn_auto_info("VLAN_RULE\n");
	nla_for_each_nested(vlan_rule, attr, rem) {
		switch(nla_type(vlan_rule)) {
		case INNER_INGRESS:
			_inner = nla_data(vlan_rule);
			if(kstrtou32(_inner, 16, &inner) < 0) {
				netfn_auto_warn("Failed to convert vlan_rule inner tag\n");
				return false;
			}

			netfn_auto_info("From Netlink, vlan_rule inner_ingress tag: %x\n", inner);
			original->rule_info.vlan_rule.inner.ingress_vlan_tag = inner;
			reply->rule_info.vlan_rule.inner.egress_vlan_tag = inner;
			break;

		case INNER_EGRESS:
			_inner = nla_data(vlan_rule);
			if(kstrtou32(_inner, 16, &inner) < 0) {
				netfn_auto_warn("Unable to convert vlan_rule inner tag\n");
				return false;
			}

			netfn_auto_info("From Netlink, vlan_rule inner_egress tag: %x\n", inner);
			original->rule_info.vlan_rule.inner.egress_vlan_tag = inner;
			reply->rule_info.vlan_rule.inner.ingress_vlan_tag = inner;
			break;

		case OUTER_INGRESS:
			_outer = nla_data(vlan_rule);
			if(kstrtou32(_outer, 16, &outer) < 0) {
				netfn_auto_warn("Unable to convert vlan_rule outer tag\n");
				return false;
			}

			netfn_auto_info("From Netlink, vlan_rule outer_ingress tag: %x\n", outer);
			original->rule_info.vlan_rule.outer.ingress_vlan_tag = outer;
			reply->rule_info.vlan_rule.outer.egress_vlan_tag = outer;
			break;

		case OUTER_EGRESS:
			_outer = nla_data(vlan_rule);
			if(kstrtou32(_outer, 16, &outer) < 0) {
				netfn_auto_warn("Unable to convert vlan_rule outer tag\n");
				return false;
			}

			netfn_auto_info("From Netlink, vlan_rule outer_egress tag: %x\n", outer);
			original->rule_info.vlan_rule.outer.egress_vlan_tag = outer;
			reply->rule_info.vlan_rule.outer.ingress_vlan_tag = outer;
			break;

		case VLAN_TPID_INNER:
			inner_vlan_tpid = (uint16_t)nla_get_u32(vlan_rule);
			original->rule_info.vlan_rule.inner_vlan_tpid = inner_vlan_tpid;
			break;

		case VLAN_TPID_OUTER:
			outer_vlan_tpid = (uint16_t)nla_get_u32(vlan_rule);
			original->rule_info.vlan_rule.outer_vlan_tpid = outer_vlan_tpid;
			break;

		default:
			netfn_auto_warn("Unknown type in vlan_rule\n");
			return false;
		}
	}

	return true;
}
#endif
