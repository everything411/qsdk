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
 * flow_auto_capwapmgr.c
 *	Flow auto capwapmgr
 */
#include "netfn_auto_capwapmgr.h"

/*
 * genl_family for flowmgr
 */
static struct genl_family netfn_auto_capwapmgr_genl_family;

/*
 * prototypes
 */
static inline int netfn_auto_capwapmgr_genl_cmd(struct sk_buff *skb, struct genl_info *info);

int netfn_auto_capwapmgr_rule_add(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwapmgr_rule_delete(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwapmgr_rule_get(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwap_legacy_ipv4_tunnle_create(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwap_legacy_enable_tunnel(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwap_legacy_disable_tunnel(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwap_legacy_update_path_mtu(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwap_legacy_update_dest_mac(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwap_legacy_update_src_interface(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwap_legacy_change_version(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwap_legacy_tunnel_destroy(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwap_legacy_netdev_destroy(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwap_legacy_ipv6_tunnle_create(struct sk_buff *skb, struct genl_info *info);
int netfn_auto_capwap_legacy_config_dtls(struct sk_buff *skb, struct genl_info *info);

/*
 * nla_policy for capwapmgr
 */
static struct nla_policy netfn_auto_capwapmgr_genl_policy[NETFN_AUTO_CAPWAPMGR_GNL_MAX + 1] = {
	[L2_INFO]		=	{ .type = NLA_NESTED, },
	[IP_RULE]		=	{ .type = NLA_NESTED, },
	[FLOW_INFO]		=	{ .type = NLA_NESTED, },
	[CAPWAP_TUN_RULE]   =	{ .type = NLA_NESTED, },
};

/*
 * operation table based called by the generic netlink layer
 * based on the command
 */
static const struct genl_ops capwapmgr_genl_ops[] = {
	{
		.cmd	=	NETFN_AUTO_CAPWAPMGR_GENL_CMD,
		.doit	=	netfn_auto_capwapmgr_genl_cmd,
		.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.flags = GENL_ADMIN_PERM,
	},
};

/*
 * capwapmgr family defination
 */
static struct genl_family netfn_auto_capwapmgr_genl_family = {
	.name		= "capwapmgr",
	.version	= 1,
	.hdrsize	= 0,
	.maxattr	= NETFN_AUTO_CAPWAPMGR_GNL_MAX,
	.policy		= netfn_auto_capwapmgr_genl_policy,
	.netnsok	= true,
	.module		= THIS_MODULE,
	.ops		= capwapmgr_genl_ops,
	.n_ops		= ARRAY_SIZE(capwapmgr_genl_ops),
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
	.resv_start_op  = NETFN_AUTO_CAPWAPMGR_CMD_MAX,
#endif
};


/*
 * netfn_auto_capwapmgr_rule_add()
 * 	capwapmgr rule add cmd.
 */
int netfn_auto_capwapmgr_rule_add(struct sk_buff *skb, struct genl_info *info)
{
	struct netfn_capwapmgr_tun_cfg cfg = {0};

	if (info->attrs[FLOW_INFO]) {
		struct nlattr *flow_info = info->attrs[FLOW_INFO];
		netfn_auto_capwapmgr_parse_flow_info(&cfg.flow, flow_info);
	} else {
		netfn_auto_warn("FLOW INFO EMPTY!\n");
	}

	if(info->attrs[L2_INFO]) {
		struct nlattr *attr = NULL, *l2_info;
		int rem, flag = 0;
		l2_info = info->attrs[L2_INFO];
		nla_for_each_nested(attr ,l2_info, rem) {

			if(nla_type(attr) == VALID_FLAGS) {
				struct nlattr *valid_flags = NULL;
				int rem;

				nla_for_each_nested(valid_flags, attr, rem) {
					switch(nla_type(valid_flags)) {
					case VLAN:
						flag |= NETFN_CAPWAPMGR_EXT_VALID_VLAN;
						netfn_auto_info("VALID_FLAG: VLAN\n");
						break;
					case PPPOE:
						flag |= NETFN_CAPWAPMGR_EXT_VALID_PPPOE;
						netfn_auto_info("VALID_FLAG: PPPOE\n");
						break;
					case DMAC_XLATE:
						flag |= NETFN_CAPWAPMGR_EXT_VALID_DMAC_XLATE;
						netfn_auto_info("VALID_FLAG: DMAC_XLATE\n");
						break;
					case DTLS_ENC:
						flag |= NETFN_CAPWAPMGR_EXT_VALID_DTLS_ENC;
						netfn_auto_info("VALID_FLAG: DTLS_ENC\n");
						break;
					case DTLS_DEC:
						flag |= NETFN_CAPWAPMGR_EXT_VALID_DTLS_DEC;
						netfn_auto_info("VALID_FLAG: DTLS_DEC\n");
						break;
					default:
						netfn_auto_warn("VALID_FLAG: WRONG FLAG: %d\n", nla_type(valid_flags));
					}
				}
				netfn_auto_info("VALID_FLAG is %d\n", flag);
				cfg.ext_cfg.ext_valid_flags = flag;
			} else if(nla_type(attr) == VLAN_RULE) {
				netfn_auto_parse_vlan_rule(&cfg.ext_cfg.vlan, attr);

			} else if(nla_type(attr) == PPPOE_RULE) {
				struct nlattr *pppoe_rule = NULL;
				int session_id, rem;
				char *server_mac;
				netfn_auto_info("PPPOE_RULE\n");
				nla_for_each_nested(pppoe_rule, attr, rem) {
					switch(nla_type(pppoe_rule)) {
						case SESSION_ID:
							session_id = nla_get_u32(pppoe_rule);
							netfn_auto_info("From Netlink, pppoe_rule session_id: %d\n", session_id);
							cfg.ext_cfg.pppoe.session_id = session_id;
							break;
						case SERVER_MAC:
							server_mac = nla_data(pppoe_rule);
							netfn_auto_info("From Netlink, pppoe_rule server_mac: %s\n", server_mac);
							if (!netfn_auto_verify_mac(server_mac, cfg.ext_cfg.pppoe.server_mac)) {
								netfn_auto_info("Invalid server mac address\n");
								return -EINVAL;
							}

							break;
						default:
							netfn_auto_warn("Unknown type in pppoe_rule\n");
					}
				}
			}
		}
	} else {
		netfn_auto_warn("L2 INFO EMPTY!\n");
	}

	if(info->attrs[IP_RULE]) {
		struct nlattr *ip_rule = NULL, *ip_attr = NULL;
		int rem;
		netfn_auto_info("Inside IP RULE\n");
		ip_rule = info->attrs[IP_RULE];
		if(!ip_rule) {
			netfn_auto_warn("IP_RULE empty\n");
			return -EINVAL;
		}
		nla_for_each_nested(ip_attr ,ip_rule, rem) {
			if(nla_type(ip_attr) == TUPLE_INFO) {
				if(netfn_auto_parse_tuple(&cfg.tuple, ip_attr) == false)
				{
					return -EINVAL;
				}
			}
		}
	} else {
		netfn_auto_warn("IP_RULE EMPTY!\n");
	}

	if(info->attrs[CAPWAP_TUN_RULE]) {
		struct nlattr *capwap_tun_rule = NULL, *attr = NULL;
		int rem, capwap_ver;
		netfn_auto_info("Inside CAPWAP_TUN_RULE\n");
		capwap_tun_rule = info->attrs[CAPWAP_TUN_RULE];
		if(!capwap_tun_rule) {
			netfn_auto_warn("Capwap_tun_rule empty\n");
			return -EINVAL;
		}

		nla_for_each_nested(attr ,capwap_tun_rule, rem) {
			if(nla_type(attr) == ENCAP) {
				netfn_auto_capwap_parse_encap_cfg(&cfg.capwap.enc, attr);
			} else if (nla_type(attr) == DECAP) {
				netfn_auto_capwap_parse_decap_cfg(&cfg.capwap.dec, attr);
			} else if (nla_type(attr) == CAPWAP_VER) {
				capwap_ver = nla_get_u32(attr);
				netfn_auto_info("NL_CAPWAP_VER : %d\n", capwap_ver);
				cfg.capwap.capwap_ver = capwap_ver;
			} else if (nla_type(attr) == FEATURES) {
				int features = nla_get_u32(attr);
				netfn_auto_info("NL_CAPWAP_FEATURES : %d\n", features);
				cfg.capwap.features = features;
			}
		}

	} else {
		netfn_auto_warn("CAPWAP_TUN_RULE EMPTY!\n");
	}

	netfn_capwapmgr_tun_alloc(&cfg);
	return 0;
}

/*
 * netfn_auto_capwapmgr_rule_delete()
 * 	capwapmgr rule delete cmd.
 */
int netfn_auto_capwapmgr_rule_delete(struct sk_buff *skb, struct genl_info *info)
{
	char *dev_name;
	struct net_device *dev;
	netfn_capwapmgr_ret_t ret;

	if (info->attrs[CAPWAP_TUN_DEV]) {
		dev_name = nla_data(info->attrs[CAPWAP_TUN_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_TUN_DESTROY_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_TUN_DESTROY_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	ret = netfn_capwapmgr_tun_free(dev);
	netfn_auto_info_always("netfn_capwapmgr_tun_free ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwapmgr_rule_get()
 * 	capwapmgr rule get cmd
 * 	TODO: Incomplete L2 info
 */
int netfn_auto_capwapmgr_rule_get(struct sk_buff *skb, struct genl_info *info)
{
	char *dev_name, *mac;
	struct net_device *dev;
	struct netfn_capwapmgr_tun_update cfg = {0};
	netfn_capwapmgr_ret_t ret;

	if (info->attrs[CAPWAP_TUN_DEV]) {
		dev_name = nla_data(info->attrs[CAPWAP_TUN_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_TUN_DESTROY_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_TUN_DESTROY_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[CAPWAPMGR_TUN_UPDATE_TYPE]) {
		cfg.type = nla_get_u32(info->attrs[CAPWAPMGR_TUN_UPDATE_TYPE]);
	}

	if (info->attrs[DTLS_DATA]) {
		struct nlattr *attr = NULL, *dtls_data = info->attrs[DTLS_DATA];
		int rem;

		nla_for_each_nested(attr ,dtls_data, rem) {
			if(nla_type(attr) == ENCAP) {
				netfn_auto_capwap_parse_dtls_cfg(&cfg.update_cfg.dtls.enc, attr);

			} else if (nla_type(attr) == DECAP) {
				netfn_auto_capwap_parse_dtls_cfg(&cfg.update_cfg.dtls.dec, attr);
			}
		}

	} else if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("SOURCE INTERFACE: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get SOURCE INTERFACE\n");
			return -EINVAL;
		}

		cfg.update_cfg.dev = dev_get_by_name(&init_net, dev_name);

	} else if (info->attrs[MTU]) {
		cfg.update_cfg.mtu = nla_get_u32(info->attrs[MTU]);

	} else if (info->attrs[DEST_MAC]) {
		mac = nla_data(info->attrs[DEST_MAC]);
		netfn_auto_info("CAPWAPMGR_TUN_UPDATE_DEST_MAC: %s\n", mac);
		if(!netfn_auto_verify_mac(mac, cfg.update_cfg.dest_mac)) {
				netfn_auto_warn("MAC<str> %s failed to copy\n", mac);
				return -EINVAL;
		}

	} else if (info->attrs[CAPWAP_VER]) {
		cfg.update_cfg.ver = nla_get_u32(info->attrs[MTU]);
	}

	ret = netfn_capwapmgr_tun_update(dev, &cfg);
	netfn_auto_info_always("netfn_capwapmgr_tun_update ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_ipv4_tunnle_create()
 * 	Parse capwap_legacy_ipv4_tunnel_create
 */
int netfn_auto_capwap_legacy_ipv4_tunnle_create(struct sk_buff *skb, struct genl_info *info)
{
	char *dev_name;
	struct net_device *dev;
	uint8_t tunnel_id, ip_protocol = 4;
	struct nss_ipv4_create ip_rule = {0};
	struct nss_capwap_rule_msg capwap_rule = {0};
	struct nss_dtlsmgr_config dtls_data = {0};
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_TUNNEL_CREATE_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_TUNNEL_CREATE_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_TUNNEL_CREATE_TUNNEL_ID: %d\n", tunnel_id);
	}

	if(info->attrs[IP_RULE]) {
		struct nlattr *ip_rule_info = info->attrs[IP_RULE];
		netfn_auto_capwap_legacy_parse_ipv4_create(ip_rule_info, &ip_rule);
	} else {
		netfn_auto_warn("IP_RULE empty!\n");
	}

	if(info->attrs[CAPWAP_RULE]) {
		struct nlattr *capwap_rule_info = info->attrs[CAPWAP_RULE];
		netfn_auto_capwap_legacy_parse_capwap_rule(capwap_rule_info, &capwap_rule, ip_protocol);
	} else {
		netfn_auto_warn("CAPWAP_RULE_EMPTY!\n");
	}

	if(info->attrs[DTLS_DATA]) {
		struct nlattr *dtlsmgr_config_info = info->attrs[DTLS_DATA];
		netfn_auto_capwap_legacy_parse_dtlsmgr_config(dtlsmgr_config_info, &dtls_data);
	} else {
		netfn_auto_warn("DTLS_DATA_EMPTY!\n");
	}

	ret = nss_capwapmgr_ipv4_tunnel_create(dev, tunnel_id, &ip_rule, &capwap_rule, &dtls_data);
	netfn_auto_info_always("nss_capwapmgr_ipv4_tunnel_create ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_enable_tunnel()
 * 	capwap_legacy_tunnel_enble.
 */
int netfn_auto_capwap_legacy_enable_tunnel(struct sk_buff *skb, struct genl_info *info)
{
	char *dev_name;
	struct net_device *dev;
	uint8_t tunnel_id;
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_LEGACY_ENABLE_TUN_DEV:: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_LEGACY_ENABLE_TUN_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_LEGACY_ENABLE_TUN_TUNNEL_ID: %d\n", tunnel_id);
	}

	ret = nss_capwapmgr_enable_tunnel(dev, tunnel_id);
	netfn_auto_info_always("nss_capwapmgr_enable_tunnel ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_disable_tunnel()
 * 	capwap legacy disable tunnel.
 */
int netfn_auto_capwap_legacy_disable_tunnel(struct sk_buff *skb, struct genl_info *info)
{
	char *dev_name;
	struct net_device *dev;
	uint8_t tunnel_id;
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_LEGACY_DISABLE_TUN_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_LEGACY_DISABLE_TUN_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_LEGACY_DISABLE_TUN_TUNNEL_ID: %d\n", tunnel_id);
	}

	ret = nss_capwapmgr_disable_tunnel(dev, tunnel_id);
	netfn_auto_info_always("nss_capwapmgr_disable_tunnel ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_update_path_mtu()
 * 	capwap legacy path mtu update
 */
int netfn_auto_capwap_legacy_update_path_mtu(struct sk_buff *skb, struct genl_info *info)
{
	char *dev_name;
	struct net_device *dev;
	uint8_t tunnel_id;
	uint32_t path_mtu;
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_LEGACY_UPDATE_PATH_MTU_TUN_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_LEGACY_UPDATE_PATH_MTU_TUN_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_LEGACY_UPDATE_PATH_MTU_TUNNEL_ID: %d\n", tunnel_id);
	}

	if (info->attrs[PATH_MTU]) {
		path_mtu = nla_get_u32(info->attrs[PATH_MTU]);
		netfn_auto_info("CAPWAP_LEGACY_UPDATE_PATH_MTU: %d\n", path_mtu);
	}

	ret = nss_capwapmgr_update_path_mtu(dev, tunnel_id, path_mtu);
	netfn_auto_info_always("nss_capwapmgr_update_path_mtu ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_update_dest_mac()
 * 	capwap legacy dest mac update
 */
int netfn_auto_capwap_legacy_update_dest_mac(struct sk_buff *skb, struct genl_info *info)
{
	char *dev_name;
	struct net_device *dev;
	uint8_t tunnel_id;
	char *mac;
	uint8_t mac_addr[ETH_ALEN];
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_LEGACY_UPDATE_DEST_MAC_TUN_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_LEGACY_UPDATE_DEST_MAC_TUN_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_LEGACY_UPDATE_DEST_MAC_TUNNEL_ID: %d\n", tunnel_id);
	}

	if (info->attrs[DEST_MAC]) {
		mac = nla_data(info->attrs[DEST_MAC]);
		netfn_auto_info("CAPWAP_LEGACY_UPDATE_DEST_MAC: %s\n", mac);
		if(!netfn_auto_verify_mac(mac, mac_addr)) {
				netfn_auto_warn("IP_RULE_SRC_MAC<str> %s failed to copy\n", mac);
				return -EINVAL;
			}
	}

	ret = nss_capwapmgr_update_dest_mac_addr(dev, tunnel_id, mac_addr);
	netfn_auto_info_always("nss_capwapmgr_update_dest_mac_addr ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_update_src_interface()
 * 	capwap legacy src interface update
 */
int netfn_auto_capwap_legacy_update_src_interface(struct sk_buff *skb, struct genl_info *info)
{
	char *dev_name;
	struct net_device *dev;
	uint8_t tunnel_id;
	int32_t src_if_num;
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_LEGACY_UPDATE_SRC_IF_NUM_TUN_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_LEGACY_UPDATE_SRC_IF_NUM_TUN_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_LEGACY_UPDATE_SRC_IF_NUM_TUNNEL_ID: %d\n", tunnel_id);
	}

	if (info->attrs[SRC_IF_NUM]) {
		src_if_num = (int32_t)nla_get_u32(info->attrs[SRC_IF_NUM]);
		netfn_auto_info("CAPWAP_LEGACY_UPDATE_SRC_IF_NUM: %d\n", src_if_num);
	}

	ret = nss_capwapmgr_update_src_interface(dev, tunnel_id, src_if_num);
	netfn_auto_info_always("nss_capwapmgr_update_src_interface ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_change_version()
 * 	capwap legacy version change.
 */
int netfn_auto_capwap_legacy_change_version(struct sk_buff *skb, struct genl_info *info)
{
	char *dev_name;
	struct net_device *dev;
	uint8_t tunnel_id;
	uint8_t ver;
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_LEGACY_UPDATE_CAPWAP_VER_TUN_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_LEGACY_UPDATE_CAPWAP_VER_TUN_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_LEGACY_UPDATE_CAPWAP_VER_TUNNEL_ID: %d\n", tunnel_id);
	}

	if (info->attrs[CAPWAP_VER]) {
		ver = (uint8_t)nla_get_u32(info->attrs[CAPWAP_VER]);
		netfn_auto_info("CAPWAP_LEGACY_UPDATE_CAPWAP_VER: %d\n", ver);
	}

	ret = nss_capwapmgr_change_version(dev, tunnel_id, ver);
	netfn_auto_info_always("nss_capwapmgr_change_version ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_tunnel_destroy()
 * 	capwap legacy destroy tunnel
 */
int netfn_auto_capwap_legacy_tunnel_destroy(struct sk_buff *skb, struct genl_info *info)
{
	char *dev_name;
	struct net_device *dev;
	uint8_t tunnel_id;
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_LEGACY_TUNNEL_DESTROY_TUN_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_LEGACY_TUNNEL_DESTROY_TUN_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_LEGACY_TUNNEL_DESTROY_TUNNEL_ID: %d\n", tunnel_id);
	}

	ret = nss_capwapmgr_tunnel_destroy(dev, tunnel_id);
	netfn_auto_info_always("nss_capwapmgr_tunnel_destroy ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_netdev_destroy()
 * 	capwap legacy destroy netdev
 */
int netfn_auto_capwap_legacy_netdev_destroy(struct sk_buff *skb, struct genl_info *info)
{
	char *dev_name;
	struct net_device *dev;
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_LEGACY_NETDEV_DESTROY_TUN_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_LEGACY_NETDEV_DESTROY_TUN_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	ret = nss_capwapmgr_netdev_destroy(dev);
	netfn_auto_info_always("nss_capwapmgr_netdev_destroy ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_config_dtls()
 * 	capwap legacy dtls config
 */
int netfn_auto_capwap_legacy_config_dtls(struct sk_buff *skb, struct genl_info *info)
{
	char *dev_name;
	struct net_device *dev;
	uint8_t tunnel_id, enable_dtls;
	struct nss_dtlsmgr_config dtls_data = {0};
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_LEGACY_CONFIG_DTLS_TUN_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_LEGACY_CONFIG_DTLS_TUN_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_LEGACY_CONFIG_DTLS_TUNNEL_ID: %d\n", tunnel_id);
	}

	if (info->attrs[ENABLE_DTLS]) {
		enable_dtls = (uint8_t)nla_get_u32(info->attrs[ENABLE_DTLS]);
		netfn_auto_info("CAPWAP_LEGACY_CONFIG_DTLS_ENABLE: %d\n", enable_dtls);
	}

	if(info->attrs[DTLS_DATA]) {
		struct nlattr *dtlsmgr_config_info = info->attrs[DTLS_DATA];
		netfn_auto_capwap_legacy_parse_dtlsmgr_config(dtlsmgr_config_info, &dtls_data);
	} else {
		netfn_auto_warn("DTLS_DATA_EMPTY!\n");
	}

	ret = nss_capwapmgr_configure_dtls(dev, tunnel_id, enable_dtls, &dtls_data);
	netfn_auto_info_always("nss_capwapmgr_configure_dtls ret: %x\n", ret);
	return ret;
}

int netfn_auto_capwap_legacy_ipv6_tunnle_create(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ipv6_create ip_rule = {0};
	struct nss_capwap_rule_msg capwap_rule = {0};
	struct nss_dtlsmgr_config dtls_data = {0};
	char *dev_name;
	struct net_device *dev;
	uint8_t tunnel_id, ip_protocol = 6;
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_TUNNEL_CREATE_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_TUNNEL_CREATE_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_TUNNEL_CREATE_TUNNEL_ID: %d\n", tunnel_id);
	}

	if(info->attrs[IP_RULE]) {
		struct nlattr *ip_rule_info = info->attrs[IP_RULE];
		netfn_auto_capwap_legacy_parse_ipv6_create(ip_rule_info, &ip_rule);
	} else {
		netfn_auto_warn("IP_RULE empty!\n");
	}

	if(info->attrs[CAPWAP_RULE]) {
		struct nlattr *capwap_rule_info = info->attrs[CAPWAP_RULE];
		netfn_auto_capwap_legacy_parse_capwap_rule(capwap_rule_info, &capwap_rule, ip_protocol);
	} else {
		netfn_auto_warn("CAPWAP_RULE_EMPTY!\n");
	}

	if(info->attrs[DTLS_DATA]) {
		struct nlattr *dtlsmgr_config_info = info->attrs[DTLS_DATA];
		netfn_auto_capwap_legacy_parse_dtlsmgr_config(dtlsmgr_config_info, &dtls_data);
	} else {
		netfn_auto_warn("DTLS_DATA_EMPTY!\n");
	}

	ret = nss_capwapmgr_ipv6_tunnel_create(dev, tunnel_id, &ip_rule, &capwap_rule, &dtls_data);
	netfn_auto_info_always("nss_capwapmgr_ipv6_tunnel_create ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_dtls_rekey_rx_cipher_update()
 * 	netfn auto capwapmgr legacy dtls rekey rx cipher update.
 */
static inline int netfn_auto_capwap_legacy_dtls_rekey_rx_cipher_update(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_dtlsmgr_config_update udata = {0};
	struct net_device *dev;
	uint8_t tunnel_id;
	char *dev_name;
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_DTLS_REKEY_RX_CIPHER_UPDATE_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_DTLS_REKEY_RX_CIPHER_UPDATE_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_DTLS_REKEY_RX_CIPHER_UPDATE_TUNNEL_ID: %d\n", tunnel_id);
	}

	if (info->attrs[DTLS_CONFIG]) {
		struct nlattr *dtls_config = info->attrs[DTLS_CONFIG];
		netfn_auto_capwap_legacy_parse_dtls_config_update(dtls_config, &udata);
	}

	ret = nss_capwapmgr_dtls_rekey_rx_cipher_update(dev, tunnel_id, &udata);
	netfn_auto_info_always("nss_capwapmgr_dtls_rekey_rx_cipher_update ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_dtls_rekey_tx_cipher_update()
 * 	netfn auto capwapmgr legacy dtls rekey tx cipher update.
 */
static inline int netfn_auto_capwap_legacy_dtls_rekey_tx_cipher_update(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_dtlsmgr_config_update udata = {0};
	struct net_device *dev;
	uint8_t tunnel_id;
	char *dev_name;
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_DTLS_REKEY_TX_CIPHER_UPDATE_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_DTLS_REKEY_TX_CIPHER_UPDATE_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_DTLS_REKEY_TX_CIPHER_UPDATE_TUNNEL_ID: %d\n", tunnel_id);
	}

	if (info->attrs[DTLS_CONFIG]) {
		struct nlattr *dtls_config = info->attrs[DTLS_CONFIG];
		netfn_auto_capwap_legacy_parse_dtls_config_update(dtls_config, &udata);
	}

	ret = nss_capwapmgr_dtls_rekey_tx_cipher_update(dev, tunnel_id, &udata);
	netfn_auto_info_always("nss_capwapmgr_dtls_rekey_tx_cipher_update ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_dtls_rekey_rx_cipher_switch()
 * 	netfn auto capwapmgr legacy dtls rekey rx cipher switch.
 */
static inline int netfn_auto_capwap_legacy_dtls_rekey_rx_cipher_switch(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *dev;
	uint8_t tunnel_id;
	char *dev_name;
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_DTLS_REKEY_RX_CIPHER_SWITCH_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_DTLS_REKEY_RX_CIPHER_SWITCH_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_DTLS_REKEY_RX_CIPHER_SWITCH_TUNNEL_ID: %d\n", tunnel_id);
	}

	ret = nss_capwapmgr_dtls_rekey_rx_cipher_switch(dev, tunnel_id);
	netfn_auto_info_always("nss_capwapmgr_dtls_rekey_rx_cipher_switch ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_dtls_rekey_tx_cipher_switch()
 * 	netfn auto capwapmgr legacy dtls rekey tx cipher switch.
 */
static inline int netfn_auto_capwap_legacy_dtls_rekey_tx_cipher_switch(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *dev;
	uint8_t tunnel_id;
	char *dev_name;
	nss_capwapmgr_status_t ret;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			netfn_auto_info("CAPWAP_DTLS_REKEY_TX_CIPHER_SWITCH_DEV: %s\n", dev_name);
		} else {
			netfn_auto_warn("Unable to get CAPWAP_DTLS_REKEY_TX_CIPHER_SWITCH_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		netfn_auto_info("CAPWAP_DTLS_REKEY_TX_CIPHER_SWITCH_TUNNEL_ID: %d\n", tunnel_id);
	}

	ret = nss_capwapmgr_dtls_rekey_tx_cipher_switch(dev, tunnel_id);
	netfn_auto_info_always("nss_capwapmgr_dtls_rekey_tx_cipher_switch ret: %x\n", ret);
	return ret;
}

/*
 * netfn_auto_capwap_legacy_add_flow_rule()
 * 	netfn auto capwap legacy flow rule add
 */
static inline int netfn_auto_capwap_legacy_add_flow_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_capwapmgr_flow_info capwapmgr_flow_info = {0};
	nss_capwapmgr_status_t ret = NSS_CAPWAPMGR_SUCCESS;
	struct net_device *dev;
	uint8_t tunnel_id;
	char *dev_name, *data;
	bool add;

	if (info->attrs[NET_DEV]) {
		dev_name = nla_data(info->attrs[NET_DEV]);
		if(dev_name) {
			pr_err("\nCAPWAP_LEGACY_ADD_FLOW_RULE_DEV: %s\n", dev_name);
		} else {
			pr_err("\nCAPWAP_LEGACY_ADD_FLOW_RULE_DEV\n");
			return -EINVAL;
		}
		dev = dev_get_by_name(&init_net, dev_name);
	}

	if (info->attrs[TUNNEL_ID]) {
		tunnel_id = (uint8_t)nla_get_u32(info->attrs[TUNNEL_ID]);
		pr_err("\nCAPWAP_LEGACY_ADD_FLOW_RULE_TUNNEL_ID: %d\n", tunnel_id);
	}

	if (info->attrs[FLOW_INFO]) {
		struct nlattr *attr, *flow_info = info->attrs[FLOW_INFO];
		int rem;

		nla_for_each_nested(attr, flow_info, rem) {
			switch(attr->nla_type) {
			case IP_VERSION:
				capwapmgr_flow_info.ip_version = (uint16_t)nla_get_u32(attr);
				pr_err("\nCAPWAPMGR FLOW INFO IP_VER: %d\n", capwapmgr_flow_info.ip_version);
			break;

			case PROTOCOL:
				capwapmgr_flow_info.protocol = (uint16_t)nla_get_u32(attr);
				pr_err("\nCAPWAPMGR FLOW INFO PROTOCOL: %d\n", capwapmgr_flow_info.protocol);
			break;

			case SRC_IP:
				data = nla_data(attr);
				if (!netfn_auto_verify_ip(data, capwapmgr_flow_info.src_ip, capwapmgr_flow_info.ip_version)) {
					pr_err("\nWRONG IP FOR SRC IP IN CAPWAPMGR FLOW INFO\n");
					return -EINVAL;
				}
			break;

			case DEST_IP:
				data = nla_data(attr);
				if (!netfn_auto_verify_ip(data, capwapmgr_flow_info.dst_ip, capwapmgr_flow_info.ip_version)) {
					pr_err("\nWRONG IP FOR DST IP IN CAPWAPMGR FLOW INFO\n");
					return -EINVAL;
				}
			break;

			case SRC_PORT:
				capwapmgr_flow_info.src_port = (uint16_t)nla_get_u32(attr);
				pr_err("\nCAPWAPMGR FLOW INFO SRC_PORT: %d\n", capwapmgr_flow_info.src_port);
			break;

			case DEST_PORT:
				capwapmgr_flow_info.dst_port = (uint16_t)nla_get_u32(attr);
				pr_err("\nCAPWAPMGR FLOW INFO DST_PORT: %d\n", capwapmgr_flow_info.dst_port);
			break;

			case TYPE:
				capwapmgr_flow_info.flow_attr.type = (uint8_t)nla_get_u32(attr);
				pr_err("\nCAPWAPMGR FLOW INFO FLOW ATTR TYPE: %d\n", capwapmgr_flow_info.flow_attr.type);
			break;

			case FLOW_ID:
				capwapmgr_flow_info.flow_attr.flow_id = nla_get_u32(attr);
				pr_err("\nCAPWAPMGR FLOW INFO FLOW ATTR FLOW ID: %d\n", capwapmgr_flow_info.flow_attr.flow_id);
			break;

			case SCS_SDWF_ID:
				capwapmgr_flow_info.flow_attr.scs_sdwf_id = nla_get_u32(attr);
				pr_err("\nCAPWAPMGR FLOW INFO FLOW ATTR SCS SDWF ID: %d\n", capwapmgr_flow_info.flow_attr.scs_sdwf_id);
			break;

			case ADD:
				add = !!nla_get_u32(attr);
				pr_err("\nCAPWAPMGR flow add called : %d\n", add);
			break;

			default:
				pr_err("\nWrong attr in CAPWAP_LEGACY_ADD_FLOW_RULE\n");
			}
		}
	}

	if (add) {
		ret = nss_capwapmgr_add_flow_rule(dev, tunnel_id, &capwapmgr_flow_info);
		netfn_auto_info_always("nss capwapmgr flow rule add: %d\n", ret);
	} else {
		ret = nss_capwapmgr_del_flow_rule(dev, tunnel_id, &capwapmgr_flow_info);
		netfn_auto_info_always("nss capwapmgr flow rule del: %d\n", ret);
	}
	return ret;
}

/*
 * netfn_auto_capwapmgr_genl_cmd()
 * 	Generic command for capwapmgr family.
 */
static inline int netfn_auto_capwapmgr_genl_cmd(struct sk_buff *skb, struct genl_info *info)
{
	void *hdr = NULL;
	struct sk_buff *msg = NULL;
	char *cmd;
	int cmd_index;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg) {
		pr_err("Failed to allocate netlink message to accomodate rule\n");
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq,
			&netfn_auto_capwapmgr_genl_family, 0, NETFN_AUTO_CAPWAPMGR_GENL_CMD);

	if (!hdr) {
		pr_err("Failed to put hdr in netlink buffer\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	if(info->attrs[CMD]) {
		cmd = nla_data(info->attrs[CMD]);
		netfn_auto_info("CAPWAPMGR cmd %s\n", cmd);
		cmd_index = netfn_auto_capwapmgr_get_cmd_index(cmd);

		switch(cmd_index) {
		case 0:
			netfn_auto_capwapmgr_rule_add(skb, info);
			break;
		case 1:
			netfn_auto_capwapmgr_rule_delete(skb, info);
			break;
		case 2:
			netfn_auto_capwapmgr_rule_get(skb, info);
			break;
		case 3:
			netfn_auto_capwap_legacy_ipv4_tunnle_create(skb, info);
			break;
		case 4:
			netfn_auto_capwap_legacy_enable_tunnel(skb, info);
			break;
		case 5:
			netfn_auto_capwap_legacy_disable_tunnel(skb, info);
			break;
		case 6:
			netfn_auto_capwap_legacy_update_path_mtu(skb, info);
			break;
		case 7:
			netfn_auto_capwap_legacy_update_dest_mac(skb, info);
			break;
		case 8:
			netfn_auto_capwap_legacy_update_src_interface(skb, info);
			break;
		case 9:
			netfn_auto_capwap_legacy_change_version(skb, info);
			break;
		case 10:
			netfn_auto_capwap_legacy_tunnel_destroy(skb, info);
			break;
		case 11:
			netfn_auto_capwap_legacy_netdev_destroy(skb, info);
			break;
		case 12:
			netfn_auto_capwap_legacy_ipv6_tunnle_create(skb, info);
			break;
		case 13:
			netfn_auto_capwap_legacy_config_dtls(skb, info);
			break;
		case 14:
			netfn_auto_capwap_legacy_dtls_rekey_rx_cipher_update(skb, info);
			break;
		case 15:
			netfn_auto_capwap_legacy_dtls_rekey_tx_cipher_update(skb, info);
			break;
		case 16:
			netfn_auto_capwap_legacy_dtls_rekey_rx_cipher_switch(skb, info);
			break;
		case 17:
			netfn_auto_capwap_legacy_dtls_rekey_tx_cipher_switch(skb, info);
			break;
		case 18:
			netfn_auto_capwap_legacy_add_flow_rule(skb, info);
			break;
		default:
			netfn_auto_warn("Wrong cmd for capwapmgr\n");
			goto error;
		}
	} else {
		netfn_auto_info_always("Command not found for capwapmgr\n");
		goto error;
	}

	genlmsg_end(msg, hdr);
	return genlmsg_unicast(genl_info_net(info), msg, info->snd_portid);

error:
	return -EINVAL;

}

/*
 * netfn_auto_capwapmgr_init()
 * 	init module
 */
int netfn_auto_capwapmgr_init(void)
{
	int error;

	netfn_auto_info_always("Init netfn-auto capwapmgr handler\n");

	/*
	 * register the ops family
	 */
	error = genl_register_family(&netfn_auto_capwapmgr_genl_family);
	if (error != 0) {
		netfn_auto_info_always("unable to register netfn-auto capwapmgr family with error :%d\n", error);
		return -EINVAL;
	}

	netfn_auto_info_always("netfn_auto_capwapmgr_init complete\n");
	return error;
}

/*
 * netfn_auto_capwapmgr_exit()
 * 	deinit module
 */
void netfn_auto_capwapmgr_exit(void)
{
	int error;

	netfn_auto_info_always("Exit netfn-auto capwapmgr handler\n");

	/*
	 * unregister the ops family
	 */
	error = genl_unregister_family(&netfn_auto_capwapmgr_genl_family);
	if (error != 0) {
		pr_err("unable to unregister netfn-auto capwapmgr family with error :%d\n", error);
	}

	netfn_auto_info_always("netfn_auto_capwapmgr_exit complete\n");
}

module_init(netfn_auto_capwapmgr_init);
module_exit(netfn_auto_capwapmgr_exit);

MODULE_DESCRIPTION("NETFN AUTO CAPWAPMGR");
MODULE_LICENSE("Dual BSD/GPL");
