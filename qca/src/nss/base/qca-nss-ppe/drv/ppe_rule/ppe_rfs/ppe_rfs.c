/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/version.h>
#include <linux/sysctl.h>
#include <linux/sysfs.h>
#include <linux/skbuff.h>
#include <net/addrconf.h>
#include <linux/inetdevice.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <ppe_drv.h>
#include <ppe_drv_v4.h>
#include <ppe_drv_v6.h>
#include <ppe_drv_port.h>
#include <ppe_rfs.h>
#include "ppe_rfs.h"

struct ppe_rfs gbl_ppe_rfs;

/*
 * Module parameter to enable/disable PPE RFS for eth-to-eth flows.
 */
static bool eth_rfs_enable = true;
MODULE_PARM_DESC(eth_rfs_enable, "PPE RFS enable/disable for ethernet flows");

/*
 * Module parameter to enable/disable PPE RFS for eth-wlan flows (DL flows).
 */
static bool wlan_rfs_enable = true;
MODULE_PARM_DESC(wlan_rfs_enable, "PPE RFS enable/disable for DL flows");

/*
 * Sysctl variable to enable/disable PPE RFS for multicast flows.
 */
static int mcast_rfs_enable = 1;

/*
 * ppe_rfs_rule_eligible()
 *	Checks if RFS is eligible
 */
bool ppe_rfs_rule_eligible_get(int src_interface_num, int dst_interface_num, bool mcast_flow, uint32_t valid_flags)
{
	struct net_device *src_dev = NULL, *dst_dev = NULL;
	bool dst_rfs_enabled = false;
	bool status = false;
	bool tun_flow = valid_flags & PPE_RFS_ELIG_VALID_FLAG_TUNNEL_FLOW;
	struct ppe_drv_iface *src_iface;

	if (valid_flags & PPE_RFS_ELIG_VALID_FLAG_TUNNEL_OUTER_FLOW) {
		status = false;
		ppe_rfs_warn("Outer tunnel flows are not supported for RFS.\n");
		goto ret_status;
	}

	src_dev = dev_get_by_index(&init_net, src_interface_num);
	if (!src_dev) {
		ppe_rfs_warn("dev not found during ppe dummy config for flow iface: %d\n", src_interface_num);
		goto ret_status;
	};

	/*
	 * Multicast RFS rules only steer ingress-side (flow) processing; there is no
	 * meaningful return interface, so eligibility depends solely on the source
	 * interface being a physical PPE interface.
	 */
	src_iface = ppe_drv_iface_get_by_dev(src_dev);
	ppe_rfs_info("src_dev: %s (ifindex: %d) src_iface: %p is_physical: %d mcast_flow: %d\n",
			src_dev->name, src_interface_num, src_iface,
			src_iface ? ppe_drv_iface_is_physical(src_iface) : 0, mcast_flow);
	if (!(src_iface && ppe_drv_iface_is_physical(src_iface))) {
		status = false;
		goto ret_status;
	}

	if (mcast_flow) {
		status = mcast_rfs_enable;
		goto ret_status;
	}

	dst_dev = dev_get_by_index(&init_net, dst_interface_num);
	if (!dst_dev) {
		ppe_rfs_warn("dev not found during ppe dummy config for flow iface: %d\n", dst_interface_num);
		goto ret_status;
	};

	dst_rfs_enabled = ppe_drv_port_check_rfs_support(dst_dev);

	/*
	 * RFS is eligible when the flow is a tunnel flow, or when the
	 * destination interface itself supports RFS.
	 */
	if (tun_flow || dst_rfs_enabled) {
		status = true;
		goto ret_status;
	}

ret_status:
	if (src_dev) {
		dev_put(src_dev);
	}
	if (dst_dev) {
		dev_put(dst_dev);
	}
	return status;
}
EXPORT_SYMBOL(ppe_rfs_rule_eligible_get);

/*
 * ppe_rfs_ipv6_rule_destroy()
 * 	Destroy IPv6 PPE RFS rule
 */
enum ppe_rfs_ret ppe_rfs_ipv6_rule_destroy(struct ppe_rfs_ipv6_rule_destroy_msg *destroy_ipv6)
{
	struct ppe_rfs *p = &gbl_ppe_rfs;
	struct ppe_drv_v6_rule_destroy pd6rd = {0};

	ppe_rfs_stats_inc(&p->stats.v6_destroy_ppe_rule_rfs);

	pd6rd.tuple.flow_ip[0] = destroy_ipv6->tuple.flow_ip[3];
	pd6rd.tuple.flow_ip[1] = destroy_ipv6->tuple.flow_ip[2];
	pd6rd.tuple.flow_ip[2] = destroy_ipv6->tuple.flow_ip[1];
	pd6rd.tuple.flow_ip[3] = destroy_ipv6->tuple.flow_ip[0];
	pd6rd.tuple.flow_ident = destroy_ipv6->tuple.flow_ident;
	pd6rd.tuple.return_ip[0] = destroy_ipv6->tuple.return_ip[3];
	pd6rd.tuple.return_ip[1] = destroy_ipv6->tuple.return_ip[2];
	pd6rd.tuple.return_ip[2] = destroy_ipv6->tuple.return_ip[1];
	pd6rd.tuple.return_ip[3] = destroy_ipv6->tuple.return_ip[0];
	pd6rd.tuple.return_ident = destroy_ipv6->tuple.return_ident;
	pd6rd.tuple.protocol = destroy_ipv6->tuple.protocol;
	pd6rd.tuple.flow_rule_id = destroy_ipv6->tuple.flow_rule_id;

	if (ppe_drv_v6_assist_rule_destroy(&pd6rd) != PPE_DRV_RET_SUCCESS) {
		ppe_rfs_warn("%p: error in pushing Passive PPE RFS rules\n", destroy_ipv6);
		ppe_rfs_stats_inc(&p->stats.v6_destroy_ppe_rule_fail);
		return PPE_RFS_RET_FAILURE;
	}

	return PPE_RFS_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_rfs_ipv6_rule_destroy);

/*
 * ppe_rfs_ipv6_rule_create()
 * 	Create IPv6 PPE RFS rule
 */
enum ppe_rfs_ret ppe_rfs_ipv6_rule_create(struct ppe_rfs_ipv6_rule_create_msg *create_ipv6)
{
	struct ppe_drv_v6_rule_create pd6rc = {0};
	struct ppe_rfs *p = &gbl_ppe_rfs;
	struct net_device *rx_dev = NULL;
	struct net_device *tx_dev = NULL;
	struct net_device *top_rule_rx_dev, *top_rule_tx_dev;
	uint32_t feature = PPE_DRV_ASSIST_FEATURE_RFS_DEFAULT;
	struct ppe_drv_iface *rx_iface, *tx_iface;
	ppe_drv_iface_t top_rx_if, rx_if;
	ppe_drv_iface_t top_tx_if, tx_if = 0;
	bool mcast_flow;
	ppe_drv_ret_t ret;

	ppe_rfs_stats_inc(&p->stats.v6_create_ppe_rule_rfs);

	rx_dev = dev_get_by_index(&init_net, create_ipv6->conn_rule.flow_interface_num);
	if (!rx_dev) {
		ppe_rfs_warn("dev not found during ppe dummy config for flow iface: %d\n",
										create_ipv6->conn_rule.flow_interface_num);
		ppe_rfs_stats_inc(&p->stats.v6_create_flow_interface_fail);
		return PPE_RFS_RET_FAILURE;
	};

	mcast_flow = (create_ipv6->rule_flags & PPE_RFS_V6_RULE_FLAG_MCAST_FLOW);

	tx_dev = dev_get_by_index(&init_net, create_ipv6->conn_rule.return_interface_num);
	if (!tx_dev && !mcast_flow) {
		ppe_rfs_warn("dev not found during ppe dummy config for return iface: %d\n",
										create_ipv6->conn_rule.return_interface_num);
		ppe_rfs_stats_inc(&p->stats.v6_create_return_interface_fail);
		dev_put(rx_dev);
		return PPE_RFS_RET_FAILURE;
	};

	rx_iface = ppe_drv_iface_get_by_dev(rx_dev);
	if (!(rx_iface && ppe_drv_iface_is_physical(rx_iface))) {
		if (tx_dev) {
			dev_put(tx_dev);
		}
		dev_put(rx_dev);
		ppe_rfs_warn("%p: Rule cannot be pushed in this direction\n", create_ipv6);
		return PPE_RFS_RET_FAILURE;
	}

	rx_if = ppe_drv_iface_idx_get_by_dev(rx_dev);
	dev_put(rx_dev);

	if (tx_dev) {
		tx_iface = ppe_drv_iface_get_by_dev(tx_dev);
		if (tx_iface && ppe_drv_iface_is_physical(tx_iface)) {
			feature = PPE_DRV_ASSIST_FEATURE_RFS_ETH;
		} else if (tx_dev->ieee80211_ptr) {
			feature = PPE_DRV_ASSIST_FEATURE_RFS_WLAN;
		} else if (create_ipv6->rule_flags & PPE_RFS_V6_RULE_FLAG_TUN_FLOW) {
			feature = PPE_DRV_ASSIST_FEATURE_RFS_TUNNEL;
		} else if (create_ipv6->rule_flags & PPE_RFS_V6_RULE_UDP_ST) {
			feature = PPE_DRV_ASSIST_FEATURE_RFS_UDP_ST;
			pd6rc.qos_rule.flow_qos_tag = create_ipv6->qos_rule.flow_qos_tag;
		} else if (ppe_drv_get_unknown_dst_rfs_enable()) {
			feature = PPE_DRV_ASSIST_FEATURE_RFS_UNKNOWN_DST;
		}

		tx_if = ppe_drv_iface_idx_get_by_dev(tx_dev);
		dev_put(tx_dev);
	} else if (mcast_flow) {
		feature = PPE_DRV_ASSIST_FEATURE_RFS_MCAST;
	}

	top_rule_rx_dev = dev_get_by_index(&init_net, create_ipv6->conn_rule.flow_top_interface_num);
	if (!top_rule_rx_dev) {
		ppe_rfs_warn("Top rule dev not found during ppe dummy config for flow iface: %d\n",
												create_ipv6->conn_rule.flow_top_interface_num);
		ppe_rfs_stats_inc(&p->stats.v6_create_flow_top_interface_fail);
		return PPE_RFS_RET_FAILURE;
	};

	top_rx_if = ppe_drv_iface_idx_get_by_dev(top_rule_rx_dev);
	dev_put(top_rule_rx_dev);

	top_rule_tx_dev = dev_get_by_index(&init_net, create_ipv6->conn_rule.return_top_interface_num);
	if (!top_rule_tx_dev && !mcast_flow) {
		ppe_rfs_warn("Top rule return dev not found during ppe dummy config for flow iface: %d\n",
													create_ipv6->conn_rule.return_top_interface_num);
		ppe_rfs_stats_inc(&p->stats.v6_create_return_top_interface_fail);
		return PPE_RFS_RET_FAILURE;
	};

	if (mcast_flow) {
		top_tx_if = top_rx_if;
	} else if (top_rule_tx_dev) {
		top_tx_if = ppe_drv_iface_idx_get_by_dev(top_rule_tx_dev);
	}

	if (top_rule_tx_dev) {
		dev_put(top_rule_tx_dev);
	}

	if (create_ipv6->rule_flags & PPE_RFS_V6_RULE_FLAG_BRIDGE_FLOW) {
		pd6rc.rule_flags |= PPE_DRV_V6_RULE_FLAG_BRIDGE_FLOW;
	}

	if (create_ipv6->rule_flags & PPE_RFS_V6_RULE_FLAG_PASSIVE_FLOW) {
		pd6rc.rule_flags |= PPE_DRV_V6_RULE_FLAG_PASSIVE_FLOW;
	}

	pd6rc.rule_flags |= PPE_DRV_V6_RULE_FLAG_FLOW_VALID;

	pd6rc.tuple.flow_ip[0] = create_ipv6->tuple.flow_ip[3];
	pd6rc.tuple.flow_ip[1] = create_ipv6->tuple.flow_ip[2];
	pd6rc.tuple.flow_ip[2] = create_ipv6->tuple.flow_ip[1];
	pd6rc.tuple.flow_ip[3] = create_ipv6->tuple.flow_ip[0];
	pd6rc.tuple.flow_ident = create_ipv6->tuple.flow_ident;
	pd6rc.tuple.return_ip[0] = create_ipv6->tuple.return_ip[3];
	pd6rc.tuple.return_ip[1] = create_ipv6->tuple.return_ip[2];
	pd6rc.tuple.return_ip[2] = create_ipv6->tuple.return_ip[1];
	pd6rc.tuple.return_ip[3] = create_ipv6->tuple.return_ip[0];
	pd6rc.tuple.return_ident = create_ipv6->tuple.return_ident;
	pd6rc.tuple.protocol = create_ipv6->tuple.protocol;
	pd6rc.tuple.flow_rule_id = create_ipv6->tuple.flow_rule_id;
	pd6rc.conn_rule.flow_mtu = create_ipv6->conn_rule.return_mtu;

	/*
	 * Fill from and to interface for this direction.
	 */
	pd6rc.conn_rule.rx_if = rx_if;
	pd6rc.top_rule.rx_if = top_rx_if;
	pd6rc.conn_rule.tx_if = tx_if;
	pd6rc.top_rule.tx_if = top_tx_if;

	if (create_ipv6->rule_flags & PPE_RFS_V6_RULE_FLAG_QOS_VALID) {
		pd6rc.qos_rule.flow_qos_tag = create_ipv6->qos_rule.flow_qos_tag;
		pd6rc.qos_rule.return_qos_tag = create_ipv6->qos_rule.return_qos_tag;
		pd6rc.valid_flags |= PPE_DRV_V6_VALID_FLAG_QOS;
	}

	ret = ppe_drv_v6_assist_rule_create(&pd6rc, feature);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_rfs_warn("%p: Error in pushing Passive PPE RFS rules\n", create_ipv6);
		ppe_rfs_stats_inc(&p->stats.v6_create_ppe_rule_fail);
		return PPE_RFS_RET_FAILURE;
	}

	return PPE_RFS_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_rfs_ipv6_rule_create);

/*
 * ppe_rfs_ipv4_rule_destroy()
 * 	Destroy IPv4 PPE RFS rule
 */
enum ppe_rfs_ret ppe_rfs_ipv4_rule_destroy(struct ppe_rfs_ipv4_rule_destroy_msg *destroy_ipv4)
{
	struct ppe_rfs *p = &gbl_ppe_rfs;
	struct ppe_drv_v4_rule_destroy pd4rd = {0};

	ppe_rfs_stats_inc(&p->stats.v4_destroy_ppe_rule_rfs);

	pd4rd.tuple.flow_ip = destroy_ipv4->tuple.flow_ip;
	pd4rd.tuple.flow_ident = destroy_ipv4->tuple.flow_ident;
	pd4rd.tuple.return_ip = destroy_ipv4->tuple.return_ip;
	pd4rd.tuple.return_ident = destroy_ipv4->tuple.return_ident;
	pd4rd.tuple.protocol = destroy_ipv4->tuple.protocol;
	pd4rd.tuple.flow_rule_id = destroy_ipv4->tuple.flow_rule_id;

	if (ppe_drv_v4_assist_rule_destroy(&pd4rd) != PPE_DRV_RET_SUCCESS) {
		ppe_rfs_stats_inc(&p->stats.v4_destroy_ppe_rule_fail);
		return PPE_RFS_RET_FAILURE;
	}

	return PPE_RFS_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_rfs_ipv4_rule_destroy);

/*
 * ppe_rfs_ipv4_rule_create()
 * 	Create IPv4 PPE RFS rule
 */
enum ppe_rfs_ret ppe_rfs_ipv4_rule_create(struct ppe_rfs_ipv4_rule_create_msg *create_ipv4)
{
	struct ppe_drv_v4_rule_create pd4rc = {0};
	struct ppe_rfs *p = &gbl_ppe_rfs;
	struct net_device *rx_dev = NULL;
	struct net_device *tx_dev = NULL;
	struct net_device *top_rule_rx_dev, *top_rule_tx_dev;
	uint32_t feature = PPE_DRV_ASSIST_FEATURE_RFS_DEFAULT;
	struct ppe_drv_iface *rx_iface, *tx_iface;
	ppe_drv_iface_t top_rx_if, rx_if;
	ppe_drv_iface_t top_tx_if, tx_if = 0;
	bool mcast_flow;
	ppe_drv_ret_t ret;

	ppe_rfs_stats_inc(&p->stats.v4_create_ppe_rule_rfs);

	rx_dev = dev_get_by_index(&init_net, create_ipv4->conn_rule.flow_interface_num);
	if (!rx_dev) {
		ppe_rfs_warn("dev not found during ppe dummy config for flow iface: %d\n",
										create_ipv4->conn_rule.flow_interface_num);
		ppe_rfs_stats_inc(&p->stats.v4_create_flow_interface_fail);
		return PPE_RFS_RET_FAILURE;
	};

	mcast_flow = (create_ipv4->rule_flags & PPE_RFS_V4_RULE_FLAG_MCAST_FLOW);

	tx_dev = dev_get_by_index(&init_net, create_ipv4->conn_rule.return_interface_num);
	if (!tx_dev && !mcast_flow) {
		ppe_rfs_warn("dev not found during ppe dummy config for return iface: %d\n",
										create_ipv4->conn_rule.return_interface_num);
		ppe_rfs_stats_inc(&p->stats.v4_create_return_interface_fail);
		dev_put(rx_dev);
		return PPE_RFS_RET_FAILURE;
	};

	rx_iface = ppe_drv_iface_get_by_dev(rx_dev);
	if (!(rx_iface && ppe_drv_iface_is_physical(rx_iface))) {
		if (tx_dev) {
			dev_put(tx_dev);
		}
		dev_put(rx_dev);
		ppe_rfs_warn("%p: Rule cannot be pushed in this direction\n", create_ipv4);
		return PPE_RFS_RET_FAILURE;
	}

	rx_if = ppe_drv_iface_idx_get_by_dev(rx_dev);
	dev_put(rx_dev);

	if (tx_dev) {
		tx_iface = ppe_drv_iface_get_by_dev(tx_dev);
		if (tx_iface && ppe_drv_iface_is_physical(tx_iface)) {
			feature = PPE_DRV_ASSIST_FEATURE_RFS_ETH;
		} else if (tx_dev->ieee80211_ptr) {
			feature = PPE_DRV_ASSIST_FEATURE_RFS_WLAN;
		} else if (create_ipv4->rule_flags & PPE_RFS_V4_RULE_FLAG_TUN_FLOW) {
			feature = PPE_DRV_ASSIST_FEATURE_RFS_TUNNEL;
		} else if (create_ipv4->rule_flags & PPE_RFS_V4_RULE_UDP_ST) {
			feature = PPE_DRV_ASSIST_FEATURE_RFS_UDP_ST;
			pd4rc.qos_rule.flow_qos_tag = create_ipv4->qos_rule.flow_qos_tag;
		} else if (ppe_drv_get_unknown_dst_rfs_enable()){
			feature = PPE_DRV_ASSIST_FEATURE_RFS_UNKNOWN_DST;
		}

		tx_if = ppe_drv_iface_idx_get_by_dev(tx_dev);
		dev_put(tx_dev);
	} else if (mcast_flow) {
		feature = PPE_DRV_ASSIST_FEATURE_RFS_MCAST;
	}

	top_rule_rx_dev = dev_get_by_index(&init_net, create_ipv4->conn_rule.flow_top_interface_num);
	if (!top_rule_rx_dev) {
		ppe_rfs_warn("Top rule dev not found during ppe dummy config for flow iface: %d\n",
												create_ipv4->conn_rule.flow_top_interface_num);
		ppe_rfs_stats_inc(&p->stats.v4_create_flow_top_interface_fail);
		return PPE_RFS_RET_FAILURE;
	};
	top_rx_if = ppe_drv_iface_idx_get_by_dev(top_rule_rx_dev);
	dev_put(top_rule_rx_dev);

	top_rule_tx_dev = dev_get_by_index(&init_net, create_ipv4->conn_rule.return_top_interface_num);
	if (!top_rule_tx_dev && !mcast_flow) {
		ppe_rfs_warn("Top rule return dev not found during ppe dummy config for flow iface: %d\n",
													create_ipv4->conn_rule.return_top_interface_num);
		ppe_rfs_stats_inc(&p->stats.v4_create_return_top_interface_fail);
		return PPE_RFS_RET_FAILURE;
	};

	if (mcast_flow) {
		top_tx_if = top_rx_if;
	} else if (top_rule_tx_dev) {
		top_tx_if = ppe_drv_iface_idx_get_by_dev(top_rule_tx_dev);
	}

	if (top_rule_tx_dev) {
		dev_put(top_rule_tx_dev);
	}

	if (create_ipv4->rule_flags & PPE_RFS_V4_RULE_FLAG_BRIDGE_FLOW) {
		pd4rc.rule_flags |= PPE_DRV_V4_RULE_FLAG_BRIDGE_FLOW;
	}
	if (create_ipv4->rule_flags & PPE_RFS_V4_RULE_FLAG_PASSIVE_FLOW) {
		pd4rc.rule_flags |= PPE_DRV_V4_RULE_FLAG_PASSIVE_FLOW;
	}
	pd4rc.rule_flags |= PPE_DRV_V4_RULE_FLAG_FLOW_VALID;

	pd4rc.tuple.flow_ip = create_ipv4->tuple.flow_ip;
	pd4rc.tuple.flow_ident = create_ipv4->tuple.flow_ident;
	pd4rc.tuple.return_ip = create_ipv4->tuple.return_ip;
	pd4rc.tuple.return_ident = create_ipv4->tuple.return_ident;
	pd4rc.tuple.protocol = create_ipv4->tuple.protocol;
	pd4rc.tuple.flow_rule_id = create_ipv4->tuple.flow_rule_id;
	pd4rc.conn_rule.flow_mtu = create_ipv4->conn_rule.return_mtu;

	/*
	 * PPE RFS does not support NAT on egress interface; hence filling xlate apis
	 * to be same as that of original tupe
	 */
	pd4rc.conn_rule.flow_ip_xlate =  pd4rc.tuple.flow_ip;
	pd4rc.conn_rule.flow_ident_xlate =  pd4rc.tuple.flow_ident;
	pd4rc.conn_rule.return_ip_xlate =  pd4rc.tuple.return_ip;
	pd4rc.conn_rule.return_ident_xlate =  pd4rc.tuple.return_ident;

	/*
	 * Fill from and to interface for this direction.
	 */
	pd4rc.conn_rule.rx_if = rx_if;
	pd4rc.top_rule.rx_if = top_rx_if;
	pd4rc.conn_rule.tx_if = tx_if;
	pd4rc.top_rule.tx_if = top_tx_if;

	if (create_ipv4->rule_flags & PPE_RFS_V4_RULE_FLAG_QOS_VALID) {
		pd4rc.qos_rule.flow_qos_tag = create_ipv4->qos_rule.flow_qos_tag;
		pd4rc.qos_rule.return_qos_tag = create_ipv4->qos_rule.return_qos_tag;
		pd4rc.valid_flags |= PPE_DRV_V4_VALID_FLAG_QOS;
	}

	ret = ppe_drv_v4_assist_rule_create(&pd4rc, feature);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_rfs_warn("%p: Error in pushing Passive PPE RFS rules\n", create_ipv4);
		ppe_rfs_stats_inc(&p->stats.v4_create_ppe_rule_fail);
		return PPE_RFS_RET_FAILURE;
	}

	return PPE_RFS_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_rfs_ipv4_rule_create);

/*
 * ppe_rfs_eth_rfs_enable_handler()
 *	Handler function called when eth_rfs_enable is set.
 */
static int ppe_rfs_eth_rfs_enable_handler(const char *val, const struct kernel_param *kp)
{
	int res = param_set_bool(val, kp);

	if (eth_rfs_enable) {
		ppe_drv_port_phy_rfs_set();
	} else {
		ppe_drv_port_phy_rfs_clear();
	}

	ppe_rfs_trace("RFS is %s for ethernet flows.\n", (eth_rfs_enable ? "enabled":"disabled"));

	return res;
}

static const struct kernel_param_ops eth_rfs_enable_ops = {
	.set = ppe_rfs_eth_rfs_enable_handler,
	.get = param_get_bool,
};

module_param_cb(eth_rfs_enable, &eth_rfs_enable_ops, &eth_rfs_enable, 0644);

/*
 * ppe_rfs_wlan_rfs_enable_set_handler()
 *	Handler function called when wlan_rfs_enable is set.
 */
static int ppe_rfs_wlan_rfs_enable_set_handler(const char *val, const struct kernel_param *kp)
{
	int res = param_set_bool(val, kp);

	ppe_drv_wlan_rfs_enable_set(wlan_rfs_enable);
	ppe_rfs_trace("RFS is %s for wlan flows.\n", (wlan_rfs_enable ? "enabled":"disabled"));

	return res;
}

static const struct kernel_param_ops wlan_rfs_enable_ops = {
	.set = ppe_rfs_wlan_rfs_enable_set_handler,
	.get = param_get_bool,
};

module_param_cb(wlan_rfs_enable, &wlan_rfs_enable_ops, &wlan_rfs_enable, 0644);

/*
 * ppe_rfs_mcast_rfs_enable_handler()
 *	Sysctl handler called when mcast_rfs_enable is set.
 */
static int ppe_rfs_mcast_rfs_enable_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	ppe_rfs_trace("RFS is %s for multicast flows.\n", (mcast_rfs_enable ? "enabled":"disabled"));

	return ret;
}

/*
 * ppe_rfs_sysctl_tbl
 *	PPE RFS sysctl table
 */
static struct ctl_table ppe_rfs_sysctl_tbl[] = {
	{
		.procname	= "mcast_rfs_enable",
		.data		= &mcast_rfs_enable,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= ppe_rfs_mcast_rfs_enable_handler,
	},
	{}
};

/*
 * ppe_rfs_deinit()
 *	RFS deinit API
 */
void ppe_rfs_deinit(void)
{
	struct ppe_rfs *g_rfs = &gbl_ppe_rfs;

	if (g_rfs->rfs_hdr) {
		unregister_sysctl_table(g_rfs->rfs_hdr);
		g_rfs->rfs_hdr = NULL;
	}

	ppe_rfs_stats_debugfs_exit();
}

/*
 * ppe_rfs_init()
 *	RFS init API
 */
void ppe_rfs_init(struct dentry *d_rule)
{
	struct ppe_rfs *g_rfs = &gbl_ppe_rfs;
	spin_lock_init(&g_rfs->lock);
	ppe_rfs_stats_debugfs_init(d_rule);

	if (eth_rfs_enable) {
		ppe_drv_port_phy_rfs_set();
	}

	ppe_drv_wlan_rfs_enable_set(wlan_rfs_enable);

	g_rfs->rfs_hdr = register_sysctl("ppe/ppe_rfs", ppe_rfs_sysctl_tbl);
	if (!g_rfs->rfs_hdr) {
		ppe_rfs_warn("Unable to register ppe_rfs sysctl table\n");
	}
}
