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
 * flow_auto_capwapmgr.h
 *	Flow auto capwapmgr
 */
#ifndef __FLOW_AUTO_CAPWAPMGR_H
#define __FLOW_AUTO_CAPWAPMGR_H

#include <netfn_auto.h>
#include <netfn_capwapmgr.h>
#include <netfn_flowmgr.h>
#include <netfn_capwapmgr_legacy.h>
#include <netfn_auto_flowmgr.h>



/*
 * netfn_auto_capwapmgr_get_cmd_index()
 * 	Returns the cmd index for capwapmgr cmd.
 */
int netfn_auto_capwapmgr_get_cmd_index(const char *cmd)
{
	const char *capwapmgr_cmds[] = {
		"RULE_ADD",
		"RULE_DEL",
		"RULE_GET",
		"CAPWAPMGR_LEGACY_IPV4_TUNNEL_CREATE",
		"CAPWAPMGR_LEGACY_ENABLE_TUNNEL",
		"CAPWAPMGR_LEGACY_DISABLE_TUNNEL",
		"CAPWAPMGR_LEGACY_UPDATE_PATH_MTU",
		"CAPWAPMGR_LEGACY_UPDATE_DEST_MAC",
		"CAPWAPMGR_LEGACY_UPDATE_SRC_INTERFACE",
		"CAPWAPMGR_LEGACY_CHANGE_VERSION",
		"CAPWAPMGR_LEGACY_TUNNEL_DESTROY" ,
		"CAPWAPMGR_LEGACY_NETDEV_DESTROY",
		"CAPWAPMGR_LEGACY_IPV6_TUNNEL_CREATE",
		"CAPWAPMGR_LEGACY_CONFIG_DTLS",
		"CAPWAPMGR_LEGACY_DTLS_REKEY_RX_CIPHER_UPDATE",
		"CAPWAPMGR_LEGACY_DTLS_REKEY_TX_CIPHER_UPDATE",
		"CAPWAPMGR_LEGACY_DTLS_REKEY_RX_CIPHER_SWITCH",
		"CAPWAPMGR_LEGACY_DTLS_REKEY_TX_CIPHER_SWITCH",
		"CAPWAPMGR_LEGACY_CONFIG_FLOW_RULE"};

	int i = 0;
	for(i = 0; i < sizeof(capwapmgr_cmds)/sizeof(capwapmgr_cmds[0]); i++) {
		if (strcmp(cmd, capwapmgr_cmds[i]) == 0) {
			return i;
		}
	}

	return -1;
}


/*
 * netfn_auto_parse_vlan_rule()
 * 	parse vlan rule.
 */
bool netfn_auto_parse_vlan_rule(struct netfn_flowmgr_vlan_rule *vlan_rule, struct nlattr *vlan_info)
{
	struct nlattr *attr = NULL;
	int rem;
	uint32_t inner, outer;
	char *_inner, *_outer;
	netfn_auto_info("\nVLAN_RULE\n");
	nla_for_each_nested(attr, vlan_info, rem) {
		switch(nla_type(attr)) {
		case INNER_INGRESS:
			_inner = nla_data(attr);
			if(kstrtou32(_inner, 16, &inner) < 0) {
				netfn_auto_warn("\nUnable to convert vlan_rule inner tag\n");
				return false;
			}

			netfn_auto_info("\nFrom Netlink, vlan_rule inner_ingress tag: %x\n", inner);
			vlan_rule->inner.ingress_vlan_tag = inner;
			break;

		case INNER_EGRESS:
			_inner = nla_data(attr);
			if(kstrtou32(_inner, 16, &inner) < 0) {
				netfn_auto_warn("\nUnable to convert vlan_rule inner tag\n");
				return false;
			}

			netfn_auto_info("\nFrom Netlink, vlan_rule inner_egress tag: %x\n", inner);
			vlan_rule->inner.egress_vlan_tag = inner;
			break;

		case OUTER_INGRESS:
			_outer = nla_data(attr);
			if(kstrtou32(_outer, 16, &outer) < 0) {
				netfn_auto_warn("\nUnable to convert vlan_rule outer tag\n");
				return false;
			}

			netfn_auto_info("\nFrom Netlink, vlan_rule outer_ingress tag: %x\n", outer);
			vlan_rule->outer.ingress_vlan_tag = outer;
			break;

		case OUTER_EGRESS:
			_outer = nla_data(attr);
			if(kstrtou32(_outer, 16, &outer) < 0) {
				netfn_auto_warn("\nUnable to convert vlan_rule outer tag\n");
				return false;
			}

			netfn_auto_info("\nFrom Netlink, vlan_rule outer_egress tag: %d\n", outer);
			vlan_rule->outer.egress_vlan_tag = outer;
			break;

		default:
			netfn_auto_warn("\nUnknown type in vlan_rule\n");
		}
	}

	return true;
}

bool netfn_auto_capwap_parse_encap_cfg(struct netfn_capwap_enc_cfg *cfg, struct nlattr *encap_cfg)
{
	struct nlattr *attr = NULL, *snap_hdr = NULL;
	char *bssid;
	int rem, rem_snap_hdr, i = 0;

	nla_for_each_nested(attr, encap_cfg, rem) {
		struct nlattr *flags = NULL;
		int rem_flag, flag = 0;
		char *data;

		switch(nla_type(attr)) {
		case FLAGS:
			nla_for_each_nested(flags, attr, rem_flag) {
				switch(nla_type(flags)) {
				case VLAN:
					flag |= NSS_CAPWAP_RULE_CREATE_VLAN_CONFIGURED;
					netfn_auto_info("FLAG: VLAN\n");
					break;
				case PPPOE:
					flag |= NSS_CAPWAP_RULE_CREATE_PPPOE_CONFIGURED;
					netfn_auto_info("FLAG: PPPOE\n");
					break;
				case UDP_LITE:
					flag |= NSS_CAPWAP_ENCAP_UDPLITE_HDR_CSUM;
					netfn_auto_info("FLAG: UDP_LITE\n");
					break;
				default:
					netfn_auto_info("FLAG: WRONG FLAG: %d\n", nla_type(flags));
				}
			}
			netfn_auto_info("FLAG is %d\n", flag);
			cfg->flags = flag;
			break;

		case CHECKSUM_COVERAGE:
			cfg->csum_cov = (uint16_t)nla_get_u32(attr);
			break;

		case MTU:
			cfg->mtu = (uint16_t)nla_get_u32(attr);
			break;

		case TTL:
			cfg->ttl = (uint8_t)nla_get_u32(attr);
			break;

		case TOS:
			cfg->tos = (uint8_t)nla_get_u32(attr);
			break;

		case BSSID:
			bssid = nla_data(attr);
			if (!netfn_auto_verify_mac(bssid, cfg->bssid)) {
				netfn_auto_info("Invalid bssid address\n");
				return false;
			}
			break;

		case SNAP_HDR:
			data = nla_data(attr);
			nla_for_each_nested(snap_hdr, attr, rem_snap_hdr) {
				data = nla_data(snap_hdr);
				if((i >= 6) || (kstrtou8(data, 16, &cfg->snap_hdr[i]) < 0)) {
					netfn_auto_info("Unable to convert snap_hdr\n");
					break;
				}

				netfn_auto_info("Able to convert snap_hdr %d\n", cfg->snap_hdr[i]);
				netfn_auto_info("Able to convert snap_hdr %s\n", data);
				i += 1;
			}

			break;

		default:
			netfn_auto_info("Wrong attribute in capwap_encap_cfg %d\n", nla_type(attr));
			return false;
		}
	}

	return true;
}

bool netfn_auto_capwap_parse_decap_cfg(struct netfn_capwap_dec_cfg *cfg, struct nlattr *decap_cfg)
{
	struct nlattr *attr = NULL;
	int rem;

	nla_for_each_nested(attr, decap_cfg, rem) {
		if(nla_type(attr) == MAX_FRAGS) {
			cfg->max_frags = nla_get_u32(attr);
		} else if (nla_type(attr) == MAX_PAYLOAD_SIZE) {
			cfg->max_payload_sz = (uint16_t)nla_get_u32(attr);
		}
	}

	return true;
}

int netfn_auto_capwap_legacy_parse_ipv4_create(struct nlattr *ip_rule_info, struct nss_ipv4_create *ip_rule)
{
	int rem, flow_info_rem, return_info_rem, qos_info_rem, dscp_info_rem, flag, rem_flag;
	char *ip_addr, *vlan_tag, *dev_name;
	char *mac_addr;
	struct nlattr *return_info = NULL, *flow_info = NULL, *qos_info = NULL, *dscp_info = NULL, *attr = NULL, *flags = NULL;
	nla_for_each_nested(attr, ip_rule_info, rem) {
		netfn_auto_info("IP_RULE attr->nla_type: %d\n", nla_type(attr));
		switch(nla_type(attr)) {
		case SRC_IF_NUM:
			ip_rule->src_interface_num = nla_get_u32(attr);
			netfn_auto_info(" src_interface_num: %d\n", ip_rule->src_interface_num);
			break;
		case DEST_IF_NUM:
			ip_rule->dest_interface_num = nla_get_u32(attr);
			netfn_auto_info(" dest_interface_num: %d\n", ip_rule->dest_interface_num);
			break;
		case PROTOCOL:
			ip_rule->protocol = nla_get_u32(attr);
			netfn_auto_info(" protocol: %d\n", ip_rule->protocol);
			break;
		case FLAGS :
			flag = 0;
			nla_for_each_nested(flags, attr, rem_flag) {
				switch(nla_type(flags)) {
				case VLAN:
					flag |= NSS_CAPWAP_RULE_CREATE_VLAN_CONFIGURED;
					netfn_auto_info("FLAG: VLAN\n");
					break;
				case PPPOE:
					flag |= NSS_CAPWAP_RULE_CREATE_PPPOE_CONFIGURED;
					netfn_auto_info("FLAG: PPPOE\n");
					break;
				default:
					netfn_auto_info("Wrong flag in nss_ipv4_create\n");
				}
			}
			ip_rule->flags = flag;
			netfn_auto_info(" flags: %d\n", ip_rule->flags);
			break;
		case FROM_MTU :
			ip_rule->from_mtu = nla_get_u32(attr);
			netfn_auto_info(" from_mtu: %d\n", ip_rule->from_mtu);
			break;
		case TO_MTU :
			ip_rule->to_mtu = nla_get_u32(attr);
			netfn_auto_info(" to_mtu: %d\n", ip_rule->to_mtu);
			break;
		case SRC_IP :
			ip_addr = nla_data(attr);
			netfn_auto_info("NETLINK IP_RULE_SRC_IP<str> %s\n", ip_addr);
			if(!netfn_auto_verify_ip(ip_addr, &ip_rule->src_ip, 4)) {
				netfn_auto_info("NETLINK IP_RULE_SRC_IP<str> %s failed to copy\n", ip_addr);
				return -EINVAL;
			}
			break;

		case SRC_PORT :
			ip_rule->src_port = nla_get_u32(attr);
			netfn_auto_info(" src_port: %d\n", ip_rule->src_port);
			break;

		case SRC_IP_XLATE :
			ip_addr = nla_data(attr);
			netfn_auto_info("NETLINK IP_RULE_SRC_IP_XLATE<str> %s\n", ip_addr);
			if(!netfn_auto_verify_ip(ip_addr, &ip_rule->src_ip_xlate, 4)) {
				netfn_auto_info("NETLINK IP_RULE_SRC_IP_XLATE<str> %s failed to copy\n", ip_addr);
				return -EINVAL;
			}

			break;
		case SRC_PORT_XLATE:
			ip_rule->src_port_xlate = nla_get_u32(attr);
			netfn_auto_info(" src_port_xlate: %d\n", ip_rule->src_port_xlate);
			break;

		case DEST_IP :
			ip_addr = nla_data(attr);
			netfn_auto_info("NETLINK IP_RULE_DEST_IP<str> %s\n", ip_addr);
			if(!netfn_auto_verify_ip(ip_addr, &ip_rule->dest_ip, 4)) {
				netfn_auto_info("NETLINK IP_RULE_DEST_IP<str> %s failed to copy\n", ip_addr);
				return -EINVAL;
			}
			break;

		case DEST_PORT :
			ip_rule->dest_port = nla_get_u32(attr);
			netfn_auto_info(" Dest_port: %d\n", ip_rule->dest_port);
			break;

		case DEST_IP_XLATE :
			ip_addr = nla_data(attr);
			netfn_auto_info("NETLINK IP_RULE_DEST_IP_XLATE<str> %s\n", ip_addr);
			if(!netfn_auto_verify_ip(ip_addr, &ip_rule->dest_ip_xlate, 4)) {
				netfn_auto_info("NETLINK IP_RULE_DEST_IP_XLATE<str> %s failed to copy\n", ip_addr);
				return -EINVAL;
			}

			break;
		case DEST_PORT_XLATE :
			ip_rule->dest_port_xlate = nla_get_u32(attr);
			netfn_auto_info(" dest_port_xlate: %d\n", ip_rule->dest_port_xlate);
			break;

		case SRC_MAC :
			mac_addr = nla_data(attr);
			netfn_auto_info("NETLINK MAC_RULE_SRC_MAC<str> %s\n", mac_addr);
			if(!netfn_auto_verify_mac(mac_addr, ip_rule->src_mac)) {
				netfn_auto_info("NETLINK IP_RULE_SRC_MAC<str> %s failed to copy\n", mac_addr);
				return -EINVAL;
			}
			break;
		case DEST_MAC :
			mac_addr = nla_data(attr);
			netfn_auto_info("NETLINK MAC_RULE_DEST_IP<str> %s\n", mac_addr);
			if(!netfn_auto_verify_mac(mac_addr, ip_rule->dest_mac)) {
				netfn_auto_info("NETLINK IP_RULE_SRC_MAC<str> %s failed to copy\n", mac_addr);
				return -EINVAL;
			}
			break;

		case SRC_MAC_XLATE :
			mac_addr = nla_data(attr);
			netfn_auto_info("NETLINK MAC_RULE_SRC_MAC_XLATE<str> %s\n", mac_addr);
			if(!netfn_auto_verify_mac(mac_addr, ip_rule->src_mac_xlate)) {
				netfn_auto_info("NETLINK IP_RULE_SRC_MAC_XLATE<str> %s failed to copy\n", mac_addr);
				return -EINVAL;
			}
			break;

		case DEST_MAC_XLATE :
			mac_addr = nla_data(attr);
			netfn_auto_info("NETLINK MAC_RULE_DEST_MAC_XLATE<str> %s\n", mac_addr);
			if(!netfn_auto_verify_mac(mac_addr, ip_rule->dest_mac_xlate)) {
				netfn_auto_info("NETLINK IP_RULE_DEST_MAC_XLATE<str> %s failed to copy\n", mac_addr);
				return -EINVAL;
			}
			break;

		case FLOW_INFO :
			nla_for_each_nested(flow_info, attr, flow_info_rem) {
				netfn_auto_info("IP_RULE FLOW_INFO attr->nla_type: %d\n", nla_type(flow_info));
				switch(nla_type(flow_info)) {
				case FLOW_PPPOE_IF_EXIST :
					ip_rule->flow_pppoe_if_exist = nla_get_u32(flow_info);
					netfn_auto_info("IP_RULE_flow_pppoe_if_exist : %d\n", ip_rule->flow_pppoe_if_exist);
					break;
				case FLOW_PPPOE_IF_NUM :
					ip_rule->flow_pppoe_if_num = nla_get_u32(flow_info);
					netfn_auto_info("IP_RULE_flow_pppoe_if_num : %d\n", ip_rule->flow_pppoe_if_num);
					break;

				default :
					netfn_auto_info("Wrong attr %d in IP_RULE\n", nla_type(flow_info));
					break;
				}
			}
			break;

		case RETURN_INFO :
			nla_for_each_nested(return_info, attr, return_info_rem) {
				netfn_auto_info("IP_RULE RETURN_INFO attr->nla_type: %d\n", nla_type(return_info));
				switch(nla_type(return_info)) {
				case RETURN_PPPOE_IF_EXIST :
					ip_rule->return_pppoe_if_exist = nla_get_u32(return_info);
					netfn_auto_info("IP_RULE_return_pppoe_if_exist : %d\n", ip_rule->return_pppoe_if_exist);
					break;
				case RETURN_PPPOE_IF_NUM :
					ip_rule->return_pppoe_if_num = nla_get_u32(return_info);
					netfn_auto_info("IP_RULE_return_pppoe_if_num : %d\n", ip_rule->return_pppoe_if_num);
					break;
				default :
					netfn_auto_info("Wrong attr %d in IP_RULE\n", nla_type(return_info));
					break;
				}
			}
			break;

		case TOP_NDEV :
			dev_name = nla_data(attr);
			netfn_auto_info("ip_rule->top_ndev<str>: %s\n", dev_name);
			ip_rule->top_ndev = dev_get_by_name(&init_net, dev_name);
			break;

		case QOS_INFO :
			nla_for_each_nested(qos_info, attr, qos_info_rem) {
				netfn_auto_info("IP_RULE QOS_INFO attr->nla_type: %d\n", nla_type(qos_info));
				switch(nla_type(qos_info)) {
				case QOS_TAG:
					ip_rule->qos_tag = nla_get_u32(qos_info);
					netfn_auto_info("IP_RULE_qos_tag : %d\n", ip_rule->qos_tag);
					break;
				case FLOW_QOS_TAG :
					ip_rule->flow_qos_tag = nla_get_u32(qos_info);
					netfn_auto_info("IP_RULE_flow_qos_tag : %d\n", ip_rule->flow_qos_tag);
					break;
				case RETURN_QOS_TAG :
					ip_rule->return_qos_tag = nla_get_u32(qos_info);
					netfn_auto_info("IP_RULE_return_qos_tag : %d\n", ip_rule->return_qos_tag);
					break;
				default :
					netfn_auto_info("Wrong attr %d in IP_RULE\n", nla_type(return_info));
					break;
				}
			}
			break;

		case DSCP_INFO :
			nla_for_each_nested(dscp_info, attr, dscp_info_rem) {
				netfn_auto_info("IP_RULE DSCP_INFO attr->nla_type: %d\n", nla_type(dscp_info));
				switch(nla_type(dscp_info)) {
				case DSCP_ITAG :
					ip_rule->dscp_itag = nla_get_u32(dscp_info);
					netfn_auto_info("IP_RULE_dscp_itag : %d\n", ip_rule->dscp_itag);
					break;
				case DSCP_IMASK:
					ip_rule->dscp_imask = nla_get_u32(dscp_info);
					netfn_auto_info("IP_RULE_dscp_imask : %d\n", ip_rule->dscp_imask);
					break;
				case DSCP_OMASK :
					ip_rule->dscp_omask = nla_get_u32(dscp_info);
					netfn_auto_info("IP_RULE_dscp_omask : %d\n", ip_rule->dscp_omask);
					break;
				case DSCP_OVAL :
					ip_rule->dscp_oval = nla_get_u32(dscp_info);
					netfn_auto_info("IP_RULE_dscp_oval : %d\n", ip_rule->dscp_oval);
					break;
				case FLOW_DSCP :
					ip_rule->flow_dscp = nla_get_u32(dscp_info);
					netfn_auto_info("IP_RULE_flow_dscp : %d\n", ip_rule->flow_dscp);
					break;
				case RETURN_DSCP :
					ip_rule->return_dscp = nla_get_u32(dscp_info);
					netfn_auto_info("IP_RULE_return_dscp : %d\n", ip_rule->return_dscp);
					break;
				default :
					netfn_auto_info("Wrong attr %d in IP_RULE_DSCP_INFO\n", nla_type(dscp_info));
					break;
				}
			}
			break;

		case IN_VLAN_TAG0 :
			vlan_tag = nla_data(attr);
			if(kstrtou32(vlan_tag, 16, &ip_rule->in_vlan_tag[0]) < 0) {
				netfn_auto_info("Unable to convert in_vlan_tag[0]\n");
				break;
			}

			netfn_auto_info("in_vlan_tag[0] %d\n", ip_rule->in_vlan_tag[0]);
			break;

		case IN_VLAN_TAG1 :
			vlan_tag = nla_data(attr);
			if(kstrtou32(vlan_tag, 16, &ip_rule->in_vlan_tag[1]) < 0) {
				netfn_auto_info("Unable to convert in_vlan_tag[1]\n");
				break;
			}

			netfn_auto_info("in_vlan_tag[1] %d\n", ip_rule->in_vlan_tag[1]);
			break;

		case OUT_VLAN_TAG0 :
			vlan_tag = nla_data(attr);
			if(kstrtou32(vlan_tag, 16, &ip_rule->out_vlan_tag[0]) < 0) {
				netfn_auto_info("Unable to convert out_vlan_tag[0]\n");
				break;
			}

			netfn_auto_info("out_vlan_tag[0] %d\n", ip_rule->out_vlan_tag[0]);
			break;

		case OUT_VLAN_TAG1 :
			vlan_tag = nla_data(attr);
			if(kstrtou32(vlan_tag, 16, &ip_rule->out_vlan_tag[1]) < 0) {
				netfn_auto_info("Unable to convert out_vlan_tag[1]\n");
				break;
			}

			netfn_auto_info("out_vlan_tag[1] %d\n", ip_rule->out_vlan_tag[1]);
			break;

		default :
			netfn_auto_info("Wrong attr %d in IP_RULE\n", nla_type(attr));
			break;
		}
	}

	return 1;
}

int netfn_auto_capwap_legacy_parse_capwap_rule(struct nlattr *capwap_rule_info, struct nss_capwap_rule_msg *capwap_rule, uint8_t ip_proto)
{
	struct nlattr *encap = NULL, *decap = NULL, *attr = NULL, *flags = NULL;
	char *bssid, *sip, *dip;
	int rem, decap_rem, encap_rem, rem_flag, flag;
	nla_for_each_nested(attr, capwap_rule_info, rem) {
		netfn_auto_info("CAPWAP_RULE attr->nla_type: %d\n", nla_type(attr));
		switch(nla_type(attr)) {
		/*
		 * TODO: OUTER_SGT_VALUE missing
		 */
		case ENCAP:
			nla_for_each_nested(encap, attr, encap_rem) {
				netfn_auto_info("CAPWAP_RULE ENCAP attr->nla_type: %d\n", nla_type(encap));
				switch (nla_type(encap)) {
				case SRC_IP:
					sip = nla_data(encap);
					netfn_auto_info("capwap_legacy_encap_sip<str> %s\n", sip);
					if (ip_proto == 4) {
						if(!netfn_auto_verify_ip(sip, &capwap_rule->encap.src_ip.ip.ipv4, 4)) {
							netfn_auto_info("ss_dtlsmgr_config->encap.sip<str> %s failed to copy\n", sip);
							return -EINVAL;
						}
					} else if (ip_proto == 6) {
						if(!netfn_auto_verify_ip(sip, &capwap_rule->encap.src_ip.ip.ipv6[0], 6)) {
							netfn_auto_info("ss_dtlsmgr_config->encap.sip<str> %s failed to copy\n", sip);
							return -EINVAL;
						}
					}

					break;

				case SRC_PORT:
					capwap_rule->encap.src_port = nla_get_u32(encap);
					netfn_auto_info("CAPWAP_RULE_ENCAP_SRC_PORT: %d\n", capwap_rule->encap.src_port);
					break;

				case DEST_IP:
					dip = nla_data(encap);
					netfn_auto_info("capwap_legacy_encap_dip<str> %s\n", dip);
					if (ip_proto == 4) {
						if(!netfn_auto_verify_ip(dip, &capwap_rule->encap.dest_ip.ip.ipv4, 4)) {
							netfn_auto_info("ss_dtlsmgr_config->encap.dip<str> %s failed to copy\n", dip);
							return -EINVAL;
						}
					} else if (ip_proto == 6) {
						if(!netfn_auto_verify_ip(dip, &capwap_rule->encap.dest_ip.ip.ipv6[0], 6)) {
							netfn_auto_info("ss_dtlsmgr_config->encap.dip<str> %s failed to copy\n", dip);
							return -EINVAL;
						}
					}
					break;

				case DEST_PORT:
					capwap_rule->encap.dest_port = nla_get_u32(encap);
					netfn_auto_info("CAPWAP_RULE_ENCAP_DEST_PORT: %d\n", capwap_rule->encap.dest_port);
					break;

				case PATH_MTU:
					capwap_rule->encap.path_mtu = nla_get_u32(encap);
					netfn_auto_info("CAPWAP_RULE_ENCAP_PATH_MTU: %d\n", capwap_rule->encap.path_mtu);
					break;

				case TTL:
					capwap_rule->encap.ttl = (uint8_t)nla_get_u32(encap);
					netfn_auto_info("CAPWAP_RULE_ENCAP_TTL: %d\n", capwap_rule->encap.ttl);
					break;

				case TOS:
					capwap_rule->encap.tos = (uint8_t)nla_get_u32(encap);
					netfn_auto_info("CAPWAP_RULE_ENCAP_TOS: %d\n", capwap_rule->encap.tos);
					break;

				default :
					netfn_auto_info("nWrong attr %d in CAPWAP_ENCAP_RULE\n", nla_type(encap));
				}
			}
			break;

		case DECAP:
			nla_for_each_nested(decap, attr, decap_rem) {
				netfn_auto_info("CAPWAP_RULE DECAP attr->nla_type: %d\n", nla_type(decap));
				switch (nla_type(decap)) {
				case MAX_FRAGS:
					capwap_rule->decap.max_fragments = nla_get_u32(decap);
					netfn_auto_info("CAPWAP_RULE_DECAP_MAX_FRAGS: %d\n", capwap_rule->decap.max_fragments);
					break;

				case MAX_BUFFER_SIZE:
					capwap_rule->decap.max_buffer_size = nla_get_u32(decap);
					netfn_auto_info("CAPWAP_RULE_DECAP_MAX_BUFFER_SIZE: %d\n", capwap_rule->decap.max_buffer_size);
					break;

				default :
					netfn_auto_info("nWrong attr %d in CAPWAP_DECAP_RULE\n", nla_type(decap));
				}
			}
			break;

		case MTU_ADJUST:
			capwap_rule->mtu_adjust = nla_get_u32(attr);
			netfn_auto_info("capwap_rule->mtu_adjust: %d", capwap_rule->mtu_adjust);
			break;

		case OUTER_SGT_VALUE:
			capwap_rule->outer_sgt_value = nla_get_u32(attr);
			netfn_auto_info("capwap_rule->outer_sgt_value: %d", capwap_rule->outer_sgt_value);
			break;

		case TYPE_FLAGS:
			flag = 0;
			nla_for_each_nested(flags, attr, rem_flag) {
				switch(nla_type(flags)) {
				case VLAN:
					flag |= NSS_CAPWAP_RULE_CREATE_VLAN_CONFIGURED;
					netfn_auto_info("FLAG: VLAN\n");
					break;
				case PPPOE:
					flag |= NSS_CAPWAP_RULE_CREATE_PPPOE_CONFIGURED;
					netfn_auto_info("FLAG: PPPOE\n");
					break;
				default:
					netfn_auto_info("Wrong flag in nss_ipv4_create\n");
				}
			}

			capwap_rule->type_flags = flag;
			netfn_auto_info("capwap_rule->type_flags: %d", capwap_rule->type_flags);
			break;

		case L3_PROTO:
			capwap_rule->l3_proto = (uint8_t)nla_get_u32(attr);
			netfn_auto_info("capwap_rule->l3_proto: %d", capwap_rule->l3_proto);
			break;

		case WHICH_UDP:
			capwap_rule->which_udp = (uint8_t)nla_get_u32(attr);
			netfn_auto_info("capwap_rule->which_udp: %d", capwap_rule->which_udp);
			break;

		case ENABLED_FEATURES:
			capwap_rule->enabled_features = nla_get_u32(attr);
			netfn_auto_info("capwap_rule->enabled_features: %d", capwap_rule->enabled_features);
			break;

		case BSSID:
			bssid = nla_data(attr);
			netfn_auto_info("CAPWAP_RULE_BSSID<str> %s\n", bssid);
			if(!netfn_auto_verify_mac(bssid, capwap_rule->bssid)) {
				netfn_auto_info("CAPWAP_RULE_BSSID<str> %s failed to copy\n", bssid);
				return -EINVAL;
			}
			break;

		default:
			netfn_auto_info("Wrong attr %d in CAPWAP_RULE\n", nla_type(attr));
			break;
		}
	}

	return 1;

}

void print_array(const uint8_t *array, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++) {
		pr_err("%02x ", array[i]);
	}
	netfn_auto_info("");
}

void hex_string_to_byte_array(const char *hex_string, uint8_t *byte_array, uint8_t len)
{
	int i;
	for (i = 0; i < len; i++) {
		sscanf(hex_string + 2*i + 2, "%2hhx", &byte_array[i]);
	}
}

int netfn_auto_capwap_legacy_parse_crypto_config(struct nlattr * crypto_config, struct nss_dtlsmgr_crypto *crypto)
{
	struct nlattr * crypto_info;
	int crypto_rem, len;
	uint8_t *alloc_data;
	char *data;

	nla_for_each_nested(crypto_info, crypto_config, crypto_rem) {
		switch(nla_type(crypto_info)) {
		case DTLS_CRYPTO_ALGO:
			crypto->algo = nla_get_u32(crypto_info);
			netfn_auto_info("crypto->algo :%d\n", crypto->algo);
			break;

		case DTLS_CRYPTO_CIPHER_KEY_DATA:
			data = nla_data(crypto_info);
			netfn_auto_info("DTLS crypto cipher key data: %s\n", data);
			alloc_data = (uint8_t *)kzalloc(len, GFP_KERNEL);
			if (alloc_data) {
				hex_string_to_byte_array(data, alloc_data, len);
				print_array(alloc_data, len);
				crypto->cipher_key.data = alloc_data;
				print_array(crypto->cipher_key.data, len);
			}

			break;

		case DTLS_CRYPTO_CIPHER_KEY_LEN:
			len = nla_get_u32(crypto_info);
			crypto->cipher_key.len = len;
			netfn_auto_info("nss_dtlsmgr_config->encap.crypto.len %d\n", crypto->cipher_key.len);
			break;

		case DTLS_CRYPTO_AUTHKEY_DATA:
			data = nla_data(crypto_info);
			netfn_auto_info("DTLS crypto auth key data: %s\n", data);
			alloc_data = (uint8_t *)kzalloc(len, GFP_KERNEL);
			if (alloc_data) {
				hex_string_to_byte_array(data, alloc_data, len);
				print_array(alloc_data, len);
				crypto->auth_key.data = alloc_data;
				print_array(crypto->auth_key.data, len);
			}

			break;

		case DTLS_CRYPTO_AUTHKEY_LEN:
			len = nla_get_u32(crypto_info);
			crypto->auth_key.len = len;
			netfn_auto_info("nss_dtlsmgr_config->encap.crypto.auth_key.len :%d\n", crypto->auth_key.len);
			break;

		case DTLS_CRYPTO_NONCE_DATA:
			data = nla_data(crypto_info);
			netfn_auto_info("DTLS crypto nonce data: %s\n", data);
			alloc_data = (uint8_t *)kzalloc(len, GFP_KERNEL);
			if (alloc_data) {
				hex_string_to_byte_array(data, alloc_data, len);
				print_array(alloc_data, len);
				crypto->nonce.data = alloc_data;
				print_array(crypto->nonce.data, len);
			}

			break;

		case DTLS_CRYPTO_NONCE_LEN:
			len = nla_get_u32(crypto_info);
			crypto->nonce.len = len;
			netfn_auto_info("nss_dtlsmgr_config->encap.crypto.nonce.len :%d\n", crypto->nonce.len);
			break;

		default:
			netfn_auto_info("Wrong value for dtls_crypto: %d\n", nla_type(crypto_info));
		}
	}

	return 0;
}

int netfn_auto_capwap_legacy_parse_dtlsmgr_config(struct nlattr *dtlsmgr_config_info, struct nss_dtlsmgr_config *nss_dtlsmgr_config)
{
	struct nlattr *encap = NULL, *decap = NULL, *attr = NULL, *flags = NULL;
	int rem, encap_rem, decap_rem, rem_flag, flag;
	struct nss_dtlsmgr_crypto *crypto;
	char *sip, *dip;

	nla_for_each_nested(attr, dtlsmgr_config_info, rem) {
		netfn_auto_info("DTLS_CONFIG_INFO attr->nla_type: %d\n", nla_type(attr));
		switch(nla_type(attr)) {
		case FLAGS:
			flag = 0;
			nla_for_each_nested(flags, attr, rem_flag) {
				switch(nla_type(flags)) {
				case ENC:
					flag |= NETFN_DTLS_FLAG_ENC;
					netfn_auto_info("FLAG: ENC\n");
					break;

				case IPV6:
					flag |= NETFN_DTLS_FLAG_IPV6;
					netfn_auto_info("FLAG: IPV6\n");
					break;

				case UDP_LITE:
					flag |= NETFN_DTLS_FLAG_UDPLITE;
					netfn_auto_info("FLAG: UDPLITE\n");
					break;

				case CAPWAP:
					flag |= NETFN_DTLS_FLAG_CAPWAP;
					netfn_auto_info("FLAG: CAPWAP\n");
					break;

				case TOS:
					flag |= NETFN_DTLS_FLAG_CP_TOS;
					netfn_auto_info("FLAG: CP_TOS\n");
					break;

				case DF:
					flag |= NETFN_DTLS_FLAG_CP_DF;
					netfn_auto_info("FLAG: CP_DF\n");
					break;

				default:
					netfn_auto_info("Wrong flag in nss_dtlsmgr_config\n");
				}
			}
			nss_dtlsmgr_config->flags = flag;
			netfn_auto_info("DTLS_CONFIG_FLAG: %d\n", nss_dtlsmgr_config->flags);
			break;

		case ENCAP:
			nla_for_each_nested(encap, attr, encap_rem) {
				netfn_auto_info("DTLS_CONFIG_INFO_ENCAP attr->nla_type: %d\n", nla_type(encap));
				switch (nla_type(encap)) {
				case DTLS_CRYPTO:
					crypto = &nss_dtlsmgr_config->encap.crypto;
					netfn_auto_capwap_legacy_parse_crypto_config(encap, crypto);
					break;

				case DTLS_VERSION:
					nss_dtlsmgr_config->encap.ver = nla_get_u32(encap);
					netfn_auto_info("nss_dtlsmgr_config->encap.ver: %d\n", nss_dtlsmgr_config->encap.ver);
					break;

				case DTLS_SIP:
					sip = nla_data(encap);
					netfn_auto_info("dtls_encap_config_sip<str> %s\n", sip);
					if(!netfn_auto_verify_ip(sip, &nss_dtlsmgr_config->encap.sip[0], 4)) {
						netfn_auto_info("ss_dtlsmgr_config->encap.sip<str> %s failed to copy\n", sip);
						return -EINVAL;
					}
					break;

				case DTLS_DIP:
					dip = nla_data(encap);
					netfn_auto_info("dtls_encap_config_dip<str> %s\n", dip);
					if(!netfn_auto_verify_ip(dip, &nss_dtlsmgr_config->encap.dip[0], 4)) {
						netfn_auto_info("nss_dtlsmgr_config->encap.dip<str> %s failed to copy\n", dip);
						return -EINVAL;
					}
					break;

				case DTLS_SPORT:
					nss_dtlsmgr_config->encap.sport = (uint16_t)nla_get_u32(encap);
					netfn_auto_info("nss_dtlsmgr_config->encap.sport: %d\n", nss_dtlsmgr_config->encap.sport);
					break;

				case DTLS_DPORT:
					nss_dtlsmgr_config->encap.dport = (uint16_t)nla_get_u32(encap);
					netfn_auto_info("nss_dtlsmgr_config->encap.dport: %d\n", nss_dtlsmgr_config->encap.dport);
					break;

				case DTLS_EPOCH:
					nss_dtlsmgr_config->encap.epoch = (uint16_t)nla_get_u32(encap);
					netfn_auto_info("nss_dtlsmgr_config->encap.epoch: %d\n", nss_dtlsmgr_config->encap.epoch);
					break;

				case DTLS_IP_TTL:
					nss_dtlsmgr_config->encap.ip_ttl = (uint8_t)nla_get_u32(encap);
					netfn_auto_info("nss_dtlsmgr_config->encap.ip_ttl: %d\n", nss_dtlsmgr_config->encap.ip_ttl);
					break;

				case DTLS_DSCP:
					nss_dtlsmgr_config->encap.dscp = (uint8_t)nla_get_u32(encap);
					netfn_auto_info("nss_dtlsmgr_config->encap.dscp: %d\n", nss_dtlsmgr_config->encap.dscp);
					break;

				case DTLS_DSCP_COPY:
					nss_dtlsmgr_config->encap.dscp_copy = (bool)nla_get_u32(encap);
					netfn_auto_info("nss_dtlsmgr_config->encap.dscp_copy: %d\n", nss_dtlsmgr_config->encap.dscp_copy);
					break;

				case DTLS_DF:
					nss_dtlsmgr_config->encap.df = (bool)nla_get_u32(encap);
					netfn_auto_info("nss_dtlsmgr_config->encap.df: %d\n", nss_dtlsmgr_config->encap.df);
					break;
				}
			}
			break;

		case DECAP:
			nla_for_each_nested(decap, attr, decap_rem) {
				netfn_auto_info("DTLS_CONFIG_INFO_DECAP attr->nla_type: %d\n", nla_type(decap));
				switch(nla_type(decap)) {
				case DTLS_CRYPTO:
					crypto = &nss_dtlsmgr_config->decap.crypto;
					netfn_auto_capwap_legacy_parse_crypto_config(decap, crypto);
					break;

				case DTLS_NEXTHOP_IFNUM:
					nss_dtlsmgr_config->decap.nexthop_ifnum = nla_get_u32(decap);
					netfn_auto_info("nss_dtlsmgr_config->decap.nexthop_ifnum: %d\n", nss_dtlsmgr_config->decap.nexthop_ifnum);
					break;

				case DTLS_WINDOW_SIZE:
					nss_dtlsmgr_config->decap.window_size = (uint16_t)nla_get_u32(decap);
					netfn_auto_info("nss_dtlsmgr_config->decap.window_size: %d\n", nss_dtlsmgr_config->decap.window_size);
					break;

				default:
					netfn_auto_info("Wrong value for DTLS_DECAP: %d\n", nla_type(decap));
				}
			}
			break;
		default:
			netfn_auto_info("Wrong val for DTLS_CONFIG_INFO attr->nla_type: %d\n", nla_type(attr));
		}
	}
	return 1;
}

int netfn_auto_capwap_legacy_parse_ipv6_create(struct nlattr *ip_rule_info, struct nss_ipv6_create *ip_rule)
{
	int rem, flow_info_rem, return_info_rem, qos_info_rem, dscp_info_rem, rem_flag, flag;
	char *ip_addr, *vlan_tag, *dev_name;
	char *mac_addr;
	struct nlattr *return_info = NULL, *flow_info = NULL, *qos_info = NULL, *dscp_info = NULL, *attr = NULL, *flags = NULL;
	nla_for_each_nested(attr, ip_rule_info, rem) {
		netfn_auto_info("IP_RULE attr->nla_type: %d\n", nla_type(attr));
		switch(nla_type(attr)) {
		case SRC_IF_NUM:
			ip_rule->src_interface_num = nla_get_u32(attr);
			netfn_auto_info(" src_interface_num: %d\n", ip_rule->src_interface_num);
			break;
		case DEST_IF_NUM:
			ip_rule->dest_interface_num = nla_get_u32(attr);
			netfn_auto_info(" dest_interface_num: %d\n", ip_rule->dest_interface_num);
			break;
		case PROTOCOL:
			ip_rule->protocol = nla_get_u32(attr);
			netfn_auto_info(" protocol: %d\n", ip_rule->protocol);
			break;
		case FLAGS :
			flag = 0;
			nla_for_each_nested(flags, attr, rem_flag) {
				switch(nla_type(flags)) {
				case VLAN:
					flag |= NSS_CAPWAP_RULE_CREATE_VLAN_CONFIGURED;
					netfn_auto_info("FLAG: VLAN\n");
					break;
				case PPPOE:
					flag |= NSS_CAPWAP_RULE_CREATE_PPPOE_CONFIGURED;
					netfn_auto_info("FLAG: PPPOE\n");
					break;
				default:
					netfn_auto_info("Wrong flag in nss_ipv6_create\n");
				}
			}

			ip_rule->flags = flag;
			netfn_auto_info(" flags: %d\n", ip_rule->flags);
			break;
		case FROM_MTU :
			ip_rule->from_mtu = nla_get_u32(attr);
			netfn_auto_info(" from_mtu: %d\n", ip_rule->from_mtu);
			break;
		case TO_MTU :
			ip_rule->to_mtu = nla_get_u32(attr);
			netfn_auto_info(" to_mtu: %d\n", ip_rule->to_mtu);
			break;
		case SRC_IP :
			ip_addr = nla_data(attr);
			netfn_auto_info("NETLINK IP_RULE_SRC_IP<str> %s\n", ip_addr);
			if(!netfn_auto_verify_ip(ip_addr, &ip_rule->src_ip[0], 6)) {
				netfn_auto_info("NETLINK IP_RULE_SRC_IP<str> %s failed to copy\n", ip_addr);
				return -EINVAL;
			}
			break;

		case SRC_PORT :
			ip_rule->src_port = nla_get_u32(attr);
			netfn_auto_info(" src_port: %d\n", ip_rule->src_port);
			break;

		case DEST_IP :
			ip_addr = nla_data(attr);
			netfn_auto_info("NETLINK IP_RULE_DEST_IP<str> %s\n", ip_addr);
			if(!netfn_auto_verify_ip(ip_addr, &ip_rule->dest_ip[0], 6)) {
				netfn_auto_info("NETLINK IP_RULE_DEST_IP<str> %s failed to copy\n", ip_addr);
				return -EINVAL;
			}
			break;

		case DEST_PORT :
			ip_rule->dest_port = nla_get_u32(attr);
			netfn_auto_info(" Dest_port: %d\n", ip_rule->dest_port);
			break;

		case SRC_MAC :
			mac_addr = nla_data(attr);
			netfn_auto_info("NETLINK MAC_RULE_SRC_MAC<str> %s\n", mac_addr);
			if(!netfn_auto_verify_mac(mac_addr, ip_rule->src_mac)) {
				netfn_auto_info("NETLINK IP_RULE_SRC_MAC<str> %s failed to copy\n", mac_addr);
				return -EINVAL;
			}
			break;
		case DEST_MAC :
			mac_addr = nla_data(attr);
			netfn_auto_info("NETLINK MAC_RULE_DEST_IP<str> %s\n", mac_addr);
			if(!netfn_auto_verify_mac(mac_addr, ip_rule->dest_mac)) {
				netfn_auto_info("NETLINK IP_RULE_SRC_MAC<str> %s failed to copy\n", mac_addr);
				return -EINVAL;
			}
			break;

		case FLOW_INFO :
			nla_for_each_nested(flow_info, attr, flow_info_rem) {
				netfn_auto_info("IP_RULE FLOW_INFO attr->nla_type: %d\n", nla_type(flow_info));
				switch(nla_type(flow_info)) {
				case FLOW_WINDOW_SCALE:
					ip_rule->flow_window_scale = (uint8_t)nla_get_u32(flow_info);
					netfn_auto_info("IP_RULE_flow_window_size : %d\n", ip_rule->flow_window_scale);
					break;
				case FLOW_MAX_WINDOW :
					ip_rule->flow_max_window = nla_get_u32(flow_info);
					netfn_auto_info("IP_RULE_flow_max_window : %d\n", ip_rule->flow_max_window);
					break;
				case FLOW_END :
					ip_rule->flow_end = nla_get_u32(flow_info);
					netfn_auto_info("IP_RULE_flow_end : %d\n", ip_rule->flow_end);
					break;
				case FLOW_MAX_END :
					ip_rule->flow_max_end = nla_get_u32(flow_info);
					netfn_auto_info("IP_RULE_flow_max_end : %d\n", ip_rule->flow_max_end);
					break;
				case FLOW_PPPOE_IF_EXIST :
					ip_rule->flow_pppoe_if_exist = nla_get_u32(flow_info);
					netfn_auto_info("IP_RULE_flow_pppoe_if_exist : %d\n", ip_rule->flow_pppoe_if_exist);
					break;
				case FLOW_PPPOE_IF_NUM :
					ip_rule->flow_pppoe_if_num = nla_get_u32(flow_info);
					netfn_auto_info("IP_RULE_flow_pppoe_if_num : %d\n", ip_rule->flow_pppoe_if_num);
					break;
				default :
					netfn_auto_info("Wrong attr %d in IP_RULE\n", nla_type(flow_info));
					break;
				}
			}
			break;

		case INGRESS_VLAN_TAG :
			vlan_tag = nla_data(attr);
			if(kstrtou16(vlan_tag, 16, &ip_rule->ingress_vlan_tag) < 0) {
				netfn_auto_info("Unable to convert ingress_vlan_tag\n");
				break;
			}

			netfn_auto_info("ingress_vlan_tag %d\n", ip_rule->ingress_vlan_tag);
			break;

		case RETURN_INFO :
			nla_for_each_nested(return_info, attr, return_info_rem) {
				netfn_auto_info("IP_RULE RETURN_INFO attr->nla_type: %d\n", nla_type(return_info));
				switch(nla_type(return_info)) {
				case RETURN_WINDOW_SCALE:
					ip_rule->return_window_scale = (uint8_t)nla_get_u32(return_info);
					netfn_auto_info("IP_RULE_return_window_size : %d\n", ip_rule->return_window_scale);
					break;
				case RETURN_MAX_WINDOW :
					ip_rule->return_max_window = nla_get_u32(return_info);
					netfn_auto_info("IP_RULE_return_max_window : %d\n", ip_rule->return_max_window);
					break;
				case RETURN_END :
					ip_rule->return_end = nla_get_u32(return_info);
					netfn_auto_info("IP_RULE_return_end : %d\n", ip_rule->return_end);
					break;
				case RETURN_MAX_END :
					ip_rule->return_max_end = nla_get_u32(return_info);
					netfn_auto_info("IP_RULE_return_max_end : %d\n", ip_rule->return_max_end);
					break;
				case RETURN_PPPOE_IF_EXIST :
					ip_rule->return_pppoe_if_exist = nla_get_u32(return_info);
					netfn_auto_info("IP_RULE_return_pppoe_if_exist : %d\n", ip_rule->return_pppoe_if_exist);
					break;
				case RETURN_PPPOE_IF_NUM :
					ip_rule->return_pppoe_if_num = nla_get_u32(return_info);
					netfn_auto_info("IP_RULE_return_pppoe_if_num : %d\n", ip_rule->return_pppoe_if_num);
					break;
				default :
					netfn_auto_info("Wrong attr %d in IP_RULE\n", nla_type(return_info));
					break;
				}
			}
			break;

		case EGRESS_VLAN_TAG :
			vlan_tag = nla_data(attr);
			if(kstrtou16(vlan_tag, 16, &ip_rule->egress_vlan_tag) < 0) {
				netfn_auto_info("Unable to convert egress_vlan_tag\n");
				break;
			}

			pr_err("\egress_vlan_tag %d\n", ip_rule->egress_vlan_tag);
			break;

		case TOP_NDEV :
			dev_name = nla_data(attr);
			netfn_auto_info("ip_rule->top_ndev<str>: %s\n", dev_name);
			ip_rule->top_ndev = dev_get_by_name(&init_net, dev_name);
			break;

		case QOS_INFO :
			nla_for_each_nested(qos_info, attr, qos_info_rem) {
				netfn_auto_info("IP_RULE QOS_INFO attr->nla_type: %d\n", nla_type(qos_info));
				switch(nla_type(qos_info)) {
				case QOS_TAG:
					ip_rule->qos_tag = nla_get_u32(qos_info);
					netfn_auto_info("IP_RULE_qos_tag : %d\n", ip_rule->qos_tag);
					break;
				case FLOW_QOS_TAG :
					ip_rule->flow_qos_tag = nla_get_u32(qos_info);
					netfn_auto_info("IP_RULE_flow_qos_tag : %d\n", ip_rule->flow_qos_tag);
					break;
				case RETURN_QOS_TAG :
					ip_rule->return_qos_tag = nla_get_u32(qos_info);
					netfn_auto_info("IP_RULE_return_qos_tag : %d\n", ip_rule->return_qos_tag);
					break;
				default :
					netfn_auto_info("Wrong attr %d in IP_RULE\n", nla_type(qos_info));
					break;
				}
			}
			break;

		case DSCP_INFO :
			nla_for_each_nested(dscp_info, attr, dscp_info_rem) {
				netfn_auto_info("IP_RULE DSCP_INFO attr->nla_type: %d\n", nla_type(dscp_info));
				switch(nla_type(dscp_info)) {
				case DSCP_ITAG :
					ip_rule->dscp_itag = nla_get_u32(dscp_info);
					netfn_auto_info("IP_RULE_dscp_itag : %d\n", ip_rule->dscp_itag);
					break;
				case DSCP_IMASK:
					ip_rule->dscp_imask = nla_get_u32(dscp_info);
					netfn_auto_info("IP_RULE_dscp_imask : %d\n", ip_rule->dscp_imask);
					break;
				case DSCP_OMASK :
					ip_rule->dscp_omask = nla_get_u32(dscp_info);
					netfn_auto_info("IP_RULE_dscp_omask : %d\n", ip_rule->dscp_omask);
					break;
				case DSCP_OVAL :
					ip_rule->dscp_oval = nla_get_u32(dscp_info);
					netfn_auto_info("IP_RULE_dscp_oval : %d\n", ip_rule->dscp_oval);
					break;
				case FLOW_DSCP :
					ip_rule->flow_dscp = nla_get_u32(dscp_info);
					netfn_auto_info("IP_RULE_flow_dscp : %d\n", ip_rule->flow_dscp);
					break;
				case RETURN_DSCP :
					ip_rule->return_dscp = nla_get_u32(dscp_info);
					netfn_auto_info("IP_RULE_return_dscp : %d\n", ip_rule->return_dscp);
					break;
				default :
					netfn_auto_info("Wrong attr %d in IP_RULE_DSCP_INFO\n", nla_type(dscp_info));
					break;
				}
			}
			break;

		case IN_VLAN_TAG0 :
			vlan_tag = nla_data(attr);
			if(kstrtou32(vlan_tag, 16, &ip_rule->in_vlan_tag[0]) < 0) {
				netfn_auto_info("Unable to convert in_vlan_tag[0]\n");
				break;
			}

			netfn_auto_info("in_vlan_tag[0] %d\n", ip_rule->in_vlan_tag[0]);
			break;

		case IN_VLAN_TAG1 :
			vlan_tag = nla_data(attr);
			if(kstrtou32(vlan_tag, 16, &ip_rule->in_vlan_tag[1]) < 0) {
				netfn_auto_info("Unable to convert in_vlan_tag[1]\n");
				break;
			}

			netfn_auto_info("in_vlan_tag[1] %d\n", ip_rule->in_vlan_tag[1]);
			break;

		case OUT_VLAN_TAG0 :
			vlan_tag = nla_data(attr);
			if(kstrtou32(vlan_tag, 16, &ip_rule->out_vlan_tag[0]) < 0) {
				netfn_auto_info("Unable to convert out_vlan_tag[0]\n");
				break;
			}

			netfn_auto_info("out_vlan_tag[0] %d\n", ip_rule->out_vlan_tag[0]);
			break;

		case OUT_VLAN_TAG1 :
			vlan_tag = nla_data(attr);
			if(kstrtou32(vlan_tag, 16, &ip_rule->out_vlan_tag[1]) < 0) {
				netfn_auto_info("Unable to convert out_vlan_tag[1]\n");
				break;
			}

			netfn_auto_info("out_vlan_tag[1] %d\n", ip_rule->out_vlan_tag[1]);
			break;

		default :
			netfn_auto_info("Wrong attr %d in IP_RULE\n", nla_type(attr));
			break;
		}
	}

	return 1;
}

int netfn_auto_capwap_legacy_parse_dtls_config_update(struct nlattr *dtls_config, struct nss_dtlsmgr_config_update *udata)
{
	struct nlattr *attr = NULL;
	int rem;

	nla_for_each_nested(attr, dtls_config, rem) {
		if(nla_type(attr) == DTLS_CRYPTO) {
			netfn_auto_capwap_legacy_parse_crypto_config(attr, &udata->crypto);
		} else if (nla_type(attr) == DTLS_EPOCH) {
			udata->epoch = (uint16_t)nla_get_u32(attr);
		} else if (nla_type(attr) == DTLS_WINDOW_SIZE) {
			udata->window_size = (uint16_t)nla_get_u32(attr);
		}
	}

	return 1;
}

int netfn_auto_capwap_parse_dtls_cfg(struct netfn_dtls_cfg *cfg, struct nlattr *dtls_cfg)
{
	struct nlattr *attr = NULL, *flags, *crypto;
	int rem, rem_flag, rem_crypto;
	uint32_t flag;
	uint8_t *alloc_data;
	char *data;

	nla_for_each_nested(attr, dtls_cfg, rem) {
		switch(nla_type(attr)) {
		case DTLS_CRYPTO:
			nla_for_each_nested(crypto, attr, rem_crypto) {
				switch(nla_type(crypto)) {
				case DTLS_CRYPTO_CIPHER_KEY_LEN:
					cfg->base.cipher.key_len = (uint16_t)nla_get_u32(crypto);
					break;

				case DTLS_CRYPTO_CIPHER_KEY_DATA:
					data = nla_data(crypto);
					netfn_auto_info("DTLS CRYPTO DATA<str>: %s\n", data);
					alloc_data = (uint8_t*)kzalloc(cfg->base.cipher.key_len, GFP_KERNEL);
					if (alloc_data) {
						hex_string_to_byte_array(data, alloc_data, cfg->base.cipher.key_len);
						print_array(alloc_data, cfg->base.cipher.key_len);
						cfg->base.cipher.key_data = alloc_data;
						print_array(cfg->base.cipher.key_data, cfg->base.cipher.key_len);
					}

					break;

				case DTLS_CRYPTO_AUTHKEY_LEN:
					cfg->base.auth.key_len = (uint16_t)nla_get_u32(crypto);
					break;

				case DTLS_CRYPTO_AUTHKEY_DATA:
					data = nla_data(crypto);
					netfn_auto_info("DTLS CRYPTO DATA<str>: %s\n", data);
					alloc_data = (uint8_t*)kzalloc(cfg->base.auth.key_len, GFP_KERNEL);
					if (alloc_data) {
						hex_string_to_byte_array(data, alloc_data, cfg->base.auth.key_len);
						print_array(alloc_data, cfg->base.auth.key_len);
						cfg->base.auth.key_data = alloc_data;
						print_array(cfg->base.auth.key_data, cfg->base.auth.key_len);
					}
					break;

				case DTLS_CRYPTO_NONCE_LEN:
					cfg->base.nonce = (uint16_t)nla_get_u32(crypto);
					break;

				default:
					netfn_auto_info("Wrong crypto data\n");
				}
			}
			break;

		case DTLS_WINDOW_SIZE:
			cfg->replay_win = nla_get_u32(attr);
			break;

		case FLAGS:
			flag = 0;
			nla_for_each_nested(flags, attr, rem_flag) {
				switch(nla_type(flags)) {
				case ENC:
					flag |= NETFN_DTLS_FLAG_ENC;
					netfn_auto_info("FLAG: NETFN_DTLS_FLAG_ENC\n");
					break;

				case IPV6:
					flag |= NETFN_DTLS_FLAG_IPV6;
					netfn_auto_info("FLAG: NETFN_DTLS_FLAG_IPV6\n");
					break;

				case UDP_LITE:
					flag |= NETFN_DTLS_FLAG_UDPLITE;
					netfn_auto_info("FLAG: NETFN_DTLS_FLAG_UDPLITE\n");
					break;

				case CAPWAP:
					flag |= NETFN_DTLS_FLAG_CAPWAP;
					netfn_auto_info("FLAG: NETFN_DTLS_FLAG_CAPWAP\n");
					break;

				case TOS:
					flag |= NETFN_DTLS_FLAG_CP_TOS;
					netfn_auto_info("FLAG: NETFN_DTLS_FLAG_CP_TOS\n");
					break;

				case DF:
					flag |= NETFN_DTLS_FLAG_CP_DF;
					netfn_auto_info("FLAG: NETFN_DTLS_FLAG_CP_DF\n");
					break;

				default:
					netfn_auto_info("Wrong flag in nss_ipv4_create\n");
				}
			}

			cfg->flags = flag;
		break;

		case DTLS_VERSION:
			cfg->version = (uint16_t)nla_get_u32(attr);
		break;

		case DTLS_EPOCH:
			cfg->epoch = (uint16_t)nla_get_u32(attr);
		break;

		case DTLS_DF:
			cfg->df = (uint8_t)nla_get_u32(attr);
		break;

		case DTLS_DSCP:
			cfg->tos = (uint8_t)nla_get_u32(attr);
		break;

		case DTLS_HOP_LIMIT:
			cfg->hop_limit = (uint8_t)nla_get_u32(attr);
		break;

		default:
			netfn_auto_info("Wrong arg in dtls cfg\n");
		}
	}

	return 0;
}
#endif
