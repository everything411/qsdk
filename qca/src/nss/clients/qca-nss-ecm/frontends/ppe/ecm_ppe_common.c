/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <net/ipv6.h>
#include <linux/tcp.h>
#include <linux/module.h>
#include <linux/inet.h>
#include <linux/etherdevice.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <ecm_interface_ipsec.h>

#define DEBUG_LEVEL ECM_PPE_COMMON_DEBUG_LEVEL

#include "ecm_types.h"
#include "ecm_db_types.h"
#include "ecm_state.h"
#include "ecm_tracker.h"
#include "ecm_classifier.h"
#include "ecm_front_end_types.h"
#include "ecm_tracker_datagram.h"
#include "ecm_tracker_udp.h"
#include "ecm_tracker_tcp.h"
#include "ecm_db.h"
#include "ecm_interface.h"
#include "ecm_ppe_common.h"
#include "ecm_front_end_common.h"
#include "ecm_ppe_ipv6.h"
#include "ecm_ppe_ipv4.h"
#include "ecm_ppe_ported_ipv4.h"
#include "ecm_ppe_ported_ipv6.h"
#ifdef ECM_INTERFACE_VXLAN_ENABLE
#include <net/vxlan.h>
#endif

#ifdef ECM_OPENWRT_SUPPORT
extern int nf_ct_tcp_no_window_check;
#endif

#ifdef ECM_INTERFACE_TUNIPIP6_ENABLE
#include <net/ip6_tunnel.h>
#endif

#include "ecm_ppe_stats_v4.h"
#include "ecm_ppe_stats_v6.h"

#ifdef ECM_IPV6_ENABLE
/*
 * ecm_ppe_ipv6_is_conn_limit_reached()
 *	Connection limit is reached or not ?
 */
bool ecm_ppe_ipv6_is_conn_limit_reached(void)
{
#if !defined(ECM_FRONT_END_CONN_LIMIT_ENABLE)
	return false;
#endif

	if (likely(!((ecm_front_end_is_feature_supported(ECM_FE_FEATURE_CONN_LIMIT)) && ecm_front_end_conn_limit))) {
		return false;
	}

	spin_lock_bh(&ecm_ppe_ipv6_lock);
	if (ecm_ppe_ipv6_accelerated_count == PPE_DRV_V6_MAX_CONN_COUNT) {
		spin_unlock_bh(&ecm_ppe_ipv6_lock);
		DEBUG_INFO("ECM DB connection limit %d reached, for PPE frontend \
			   new flows cannot be accelerated.\n",
			   ecm_ppe_ipv6_accelerated_count);
		return true;
	}
	spin_unlock_bh(&ecm_ppe_ipv6_lock);
	return false;
}
#endif

/*
 * ecm_ppe_ipv4_is_conn_limit_reached()
 *	Connection limit is reached or not ?
 */
bool ecm_ppe_ipv4_is_conn_limit_reached(void)
{
#if !defined(ECM_FRONT_END_CONN_LIMIT_ENABLE)
	return false;
#endif

	if (likely(!((ecm_front_end_is_feature_supported(ECM_FE_FEATURE_CONN_LIMIT)) && ecm_front_end_conn_limit))) {
		return false;
	}

	spin_lock_bh(&ecm_ppe_ipv4_lock);
	if (ecm_ppe_ipv4_accelerated_count == PPE_DRV_V4_MAX_CONN_COUNT) {
		spin_unlock_bh(&ecm_ppe_ipv4_lock);
		DEBUG_INFO("ECM DB connection limit %d reached, for PPE frontend \
			   new flows cannot be accelerated.\n",
			   ecm_ppe_ipv4_accelerated_count);
		return true;
	}
	spin_unlock_bh(&ecm_ppe_ipv4_lock);
	return false;
}

#ifdef ECM_INTERFACE_VXLAN_ENABLE
/*
 * ecm_ppe_ported_get_vxlan_gpe_ppe_dev_index()
 *	Check whether virtual port is created for the given ecm iface instance of type VXLAN-GPE and get the index.
 */
int ecm_ppe_ported_get_vxlan_gpe_ppe_dev_index(struct ecm_front_end_connection_instance *feci, struct ecm_db_iface_instance *ii,
		struct sk_buff *skb, enum nss_ppe_vxlanmgr_vp_creation *vp_status)
{
	struct ecm_db_interface_info_vxlan vxlan_info = {0};
	union vxlan_addr remote_ip = {0};
	struct net_device *dev;
	int if_index = -1;
	__be32 vni;

	dev = dev_get_by_index(&init_net, ecm_db_iface_interface_identifier_get(ii));
	if (!dev) {
		DEBUG_INFO("%px: VXLAN-GPE: could not get the dev\n", feci);
		return if_index;
	}

	ecm_db_iface_vxlan_info_get(ii, &vxlan_info);
	vni = vxlan_info.vni;
	if (vxlan_info.if_type == ECM_DB_IFACE_VXLAN_OUTER) {
		ip_addr_t addr = {0};

		DEBUG_TRACE("%px: VXLAN-GPE: It is an outer rule\n", feci);
		ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_TO_NAT, addr);
		if (ip_hdr(skb)->version == IPVERSION) {
			remote_ip.sa.sa_family = AF_INET;
			ECM_IP_ADDR_TO_NIN4_ADDR(remote_ip.sin.sin_addr.s_addr, addr);
			DEBUG_TRACE("%px: VXLAN-GPE: remote ip:%pI4h\n", feci, &remote_ip.sin.sin_addr.s_addr);
		} else {
			remote_ip.sa.sa_family = AF_INET6;
			ECM_IP_ADDR_TO_NIN6_ADDR(remote_ip.sin6.sin6_addr, addr);
			DEBUG_TRACE("%px: VXLAN-GPE: remote ip:%pI6h\n", feci, &remote_ip.sin6.sin6_addr.s6_addr32);
		}
	} else {
		DEBUG_TRACE("%px: VXLAN-GPE: It is an inner rule\n", feci);

		/*
		 * For VxLAN-GPE inner flow we cannot determine the remote ip address family
		 * from skb. For an inner flow, outer could be IPv4/IPv6.
		 */
		if (!ecm_interface_vxlan_gpe_get_vni_remote_ip_from_inner(dev, skb, &remote_ip)) {
			DEBUG_WARN("%px: VXLAN-GPE: failed to get the remote IP\n", feci);
			goto vxlan_gpe_fail;
		}
	}

	*vp_status = nss_ppe_vxlanmgr_get_ifindex_and_vp_status(dev, &remote_ip, vni, &if_index);
	DEBUG_TRACE("%px: VXLAN-GPE: vni: %X, netdev:%s if_index:%d vp_status:%u\n", feci, vni, dev->name, if_index, *vp_status);

vxlan_gpe_fail:
	dev_put(dev);
	return if_index;
}

/*
 * ecm_ppe_ported_get_vxlan_ppe_dev_index()
 *	Check and get whether the given ecm iface instance of type VXLAN virtual port is created in PPE.
 */
int ecm_ppe_ported_get_vxlan_ppe_dev_index(struct ecm_front_end_connection_instance *feci, struct ecm_db_iface_instance *ii,
		ecm_db_obj_dir_t dir, enum nss_ppe_vxlanmgr_vp_creation *vp_status)
{
	int if_index = -1;
	struct ecm_db_interface_info_vxlan vxlan_info = {0};
	union vxlan_addr remote_ip = {0};
	struct net_device *dev;
	struct vxlan_dev *priv;
	union vxlan_addr *src_ip;
	__be32 vni;

	dev = dev_get_by_index(&init_net, ecm_db_iface_interface_identifier_get(ii));
	if (!dev) {
		DEBUG_TRACE("%px: VXLAN: could not get the dev", feci);
		return -1;
	}

	priv = netdev_priv(dev);
	src_ip = &priv->cfg.saddr;
	vni = vxlan_vni_field(priv->cfg.vni);
	remote_ip.sa.sa_family = (src_ip->sa.sa_family == AF_INET) ? AF_INET : AF_INET6;

	ecm_db_iface_vxlan_info_get(ii, &vxlan_info);
	if (vxlan_info.if_type == ECM_DB_IFACE_VXLAN_OUTER) {
		ip_addr_t addr = {0};

		DEBUG_TRACE("%px: VXLAN: It is an outer rule", feci);
		ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_TO_NAT, addr);
		if (remote_ip.sa.sa_family == AF_INET) {
			ECM_IP_ADDR_TO_NIN4_ADDR(remote_ip.sin.sin_addr.s_addr, addr);
			DEBUG_TRACE("%px: VXLAN: rip:%pI4h\n", feci, &remote_ip.sin.sin_addr.s_addr);
		} else {
			ECM_IP_ADDR_TO_NIN6_ADDR(remote_ip.sin6.sin6_addr, addr);
			DEBUG_TRACE("%px: VXLAN: rip:%pI6h\n", feci, &remote_ip.sin6.sin6_addr.s6_addr32);
		}
	} else {
		uint8_t mac_addr[ETH_ALEN] = {0};

		DEBUG_TRACE("%px: VXLAN: It is an inner rule", feci);
		ecm_db_connection_node_address_get(feci->ci, dir, mac_addr);
		if (vxlan_find_remote_ip(priv, mac_addr, priv->cfg.vni, &remote_ip) < 0) {
			DEBUG_WARN("%px: VXLAN: failed to get the remote IP from kernel", feci);
			goto vxlan_fail;
		}
	}

	*vp_status = nss_ppe_vxlanmgr_get_ifindex_and_vp_status(dev, &remote_ip, vni, &if_index);
	DEBUG_TRACE("%px: VXLAN: vni: %X netdev:%s if_index:%d vp_status:%u\n", feci, vni, dev->name, if_index, *vp_status);

vxlan_fail:
	dev_put(dev);
	return if_index;
}
#endif

/*
 * ecm_ppe_feature_check()
 *	Check some specific features for PPE acceleration
 */
bool ecm_ppe_feature_check(struct sk_buff *skb, struct ecm_tracker_ip_header *ip_hdr)
{
	enum ecm_xfrm_flow_type flow_type;
	bool inner_accel = false;
	bool outer_accel = false;
	struct net_device *ipsec_dev;

#ifdef ECM_OPENWRT_SUPPORT
	if (ip_hdr->protocol == IPPROTO_TCP) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0))
		uint32_t tcp_no_window_check = nf_ct_tcp_no_window_check;
#else
		struct nf_conn *ct;
		enum ip_conntrack_info ctinfo;
		struct nf_tcp_net *tn;
		uint32_t tcp_no_window_check;

		ct = nf_ct_get(skb, &ctinfo);

		/*
		 * If there is no ct, we shouldn't care about the conntrack's TCP window check.
		 * Returning false here will break OVS bridge flow acceleration in PPE, since OVS bridge
		 * flows do not have ct.
		 */
		if (unlikely(!ct)) {
			DEBUG_TRACE("%px: No Conntrack found for packet\n", skb);
			return true;
		}

		tn = nf_tcp_pernet(nf_ct_net(ct));
		tcp_no_window_check = tn->tcp_no_window_check;
#endif
		if(unlikely(tcp_no_window_check == 0)) {
			DEBUG_TRACE("%px: TCP window check feature not supported for PPE; skip it\n", skb);
			return false;
		}
	}
#endif

	flow_type = ecm_front_end_xfrm_flow_accel_check(skb, &inner_accel, &outer_accel);
	if (flow_type != ECM_XFRM_FLOW_NOT_XFRM) {
		/*
		 * If flow is inner, check if inner accel is supported by PPE.
		 * Inner IPsec flows with HW offload are handled by SFE, not PPE.
		 */
		if ((flow_type == ECM_XFRM_FLOW_INNER) && !inner_accel) {
			DEBUG_TRACE("%px: IPsec inner flow with HW offload - not accelerated by PPE\n", skb);
			return false;
		}

		/*
		 * Check if the transformation for this flow
		 * is done by AE. If yes, then try to accelerate.
		 */
		ipsec_dev = ecm_front_end_get_xfrm_dev_n_hold(skb);
		if (!ipsec_dev) {
			DEBUG_TRACE("%px: xfrm flow not managed by AE; skip it\n", skb);
			return false;
		}

		dev_put(ipsec_dev);
	}

	return true;
}

/* ecm_ppe_common_init_fe_info()
 *	Initialize common fe info
 */
void ecm_ppe_common_init_fe_info(struct ecm_front_end_common_fe_info *info)
{
	info->from_stats_bitmap = 0;
	info->to_stats_bitmap = 0;
	info->front_end_flags = 0;
}

/*
 * ecm_ppe_common_set_host_assist_qdisc_rule()
 *	Configure the qdisc settings for PPE rule.
 *
 * Note: Configure PPE datapath to use full L2 offload in case
 * a single qdisc is enabled in the xmit interface hierarchy
 * for the direction. Determination of only single qdisc
 * config to be taken care before calling this API.
 */
bool ecm_ppe_common_set_host_assist_qdisc_rule(struct ecm_db_iface_instance *ifaces[ECM_DB_IFACE_HEIRARCHY_MAX],
		int32_t interfaces_first, int32_t qdisc_iface_idx, uint32_t qos_tag,
		bool flow_valid, bool is_ppe_vp, struct ppe_drv_vp_dl_qdisc_rule *qdisc_rule)
{
	struct net_device *qdisc_netdev;
	uint32_t qdisc_flags;
	uint32_t qdisc_iface_num = ecm_db_iface_interface_identifier_get(ifaces[qdisc_iface_idx]);

	/*
	 * Find the qdisc net device from the given interface number.
	 */
	qdisc_netdev = dev_get_by_index(&init_net, qdisc_iface_num);
	if (!qdisc_netdev) {
		DEBUG_WARN("Failed to get net device with %d index\n", qdisc_iface_num);
		return false;
	}

	/*
	 * Set the flag if its a ppe_vp based wlan downlink flow or eth 2 eth flow.
	 */
	if (is_ppe_vp) {
		qdisc_flags = (qdisc_iface_idx == interfaces_first) ?
			PPE_DRV_HOST_QDISC_DEV_QUEUE_XMIT : PPE_DRV_HOST_QDISC_DEV_FAST_XMIT_QDISC;
	} else {
		qdisc_flags = (qdisc_iface_idx == interfaces_first) ?
			PPE_DRV_HOST_QDISC_ON_BOTTOM_IFACE : PPE_DRV_HOST_QDISC_ON_NON_BOTTOM_IFACE;
	}

	if (flow_valid) {
		qdisc_rule->flow_flags = qdisc_flags;
		qdisc_rule->flow_netdev = qdisc_netdev;
		qdisc_rule->flow_class_id = qos_tag;

		DEBUG_TRACE("qdisc_rule in flow direction flags %d, netdev %s, class_id %d\n",
			qdisc_rule->flow_flags, qdisc_rule->flow_netdev->name, qdisc_rule->flow_class_id);
	} else {
		qdisc_rule->return_flags = qdisc_flags;
		qdisc_rule->return_netdev = qdisc_netdev;
		qdisc_rule->return_class_id = qos_tag;

		DEBUG_TRACE("qdisc_rule in return direction flags %d, netdev %s, class_id %d\n",
			qdisc_rule->return_flags, qdisc_rule->return_netdev->name, qdisc_rule->return_class_id);
	}

	dev_put(qdisc_netdev);
	return true;
}

#ifdef ECM_INTERFACE_TUNIPIP6_ENABLE
/*
 * ecm_ppe_tunipip6_is_flow_offload_enabled()
 *	Is tunipip6 flow offload enabled in PPE?
 */
bool ecm_ppe_tunipip6_is_flow_offload_enabled(struct ecm_front_end_connection_instance *feci,
			struct ecm_db_iface_instance *ii, bool is_encap)
{
	struct net_device *tundev = NULL;
	uint32_t local_ip[4] = {0};
	uint32_t remote_ip[4] = {0};
	ip_addr_t saddr = {0};
	ip_addr_t daddr = {0};
	enum ppe_drv_iface_type iface;
	uint8_t ip_type;

	tundev = dev_get_by_index(&init_net, ecm_db_iface_interface_identifier_get(ii));

	ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_FROM, saddr);
	ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_TO, daddr);

	/*
	 * If encap flow, local/remote address would be SRC/DST address.
	 * Otherwise, local/remote address would be DST/SRC address
	 */
	if (feci->ip_version == 4) {
		ip_type = AF_INET;

		if (is_encap) {
			ECM_IP_ADDR_TO_HIN4_ADDR(local_ip[0], saddr);
			ECM_IP_ADDR_TO_HIN4_ADDR(remote_ip[0], daddr);
		} else {
			ECM_IP_ADDR_TO_HIN4_ADDR(local_ip[0], daddr);
			ECM_IP_ADDR_TO_HIN4_ADDR(remote_ip[0], saddr);
		}
	} else {
		ip_type = AF_INET6;

		if (is_encap) {
			ECM_IP_ADDR_TO_PPE_IPV6_ADDR(local_ip, saddr);
			ECM_IP_ADDR_TO_PPE_IPV6_ADDR(remote_ip, daddr);
		} else {
			ECM_IP_ADDR_TO_PPE_IPV6_ADDR(local_ip, daddr);
			ECM_IP_ADDR_TO_PPE_IPV6_ADDR(remote_ip, saddr);
		}
	}

	iface = ppe_tun_tunipip6_iface_get(tundev, local_ip, remote_ip, ip_type);
	dev_put(tundev);

	/*
	 * If interface is invalid, packet may have matched FMR as
	 * FMR rule is not offloaded to PPE
	 */
	if (iface == PPE_DRV_IFACE_TYPE_INVALID) {
		DEBUG_TRACE("%px: TUNIPIP6 FMR is used for the flow\n", ii);
		return false;
	}

	DEBUG_TRACE("%px: TUNIPIP6 BMR is used for the flow\n", ii);
	return true;
}
#endif

/*
 * ecm_ppe_common_update_rule()
 *	Updates the frontend specifc data.
 */
void ecm_ppe_common_update_rule(struct ecm_front_end_connection_instance *feci, enum ecm_rule_update_type type, void *arg)
{
	uint8_t rule_type = 0;
	bool status;

	switch (type) {
	case ECM_RULE_UPDATE_TYPE_UNI_DI_QOS:
	{
		struct ecm_cmn_unidir_update_info *update_info = (struct ecm_cmn_unidir_update_info *) arg;

		rule_type |= PPE_DRV_UPDATE_RULE_TYPE_QOS;
		rule_type |= PPE_DRV_UPDATE_RULE_TYPE_DSCP;

		if (feci->ip_version == 4) {
			status = ecm_ppe_ported_ipv4_unidir_rule_update(feci->ci, update_info->pr, update_info->sender, rule_type);
			if (!status) {
				DEBUG_WARN("%p: Uni-directional v4 flow update failed in PPE.\n", feci);
				ecm_ppe_stats_v4_inc(feci, ECM_PPE_STATS_V4_EXCEPTION_PORTED, ECM_PPE_STATS_V4_EXCEPTION_PORTED_UNIDIR_UPDATE_FAIL);
			}
		} else {
			status = ecm_ppe_ported_ipv6_unidir_rule_update(feci->ci, update_info->pr, update_info->sender, rule_type);
			if (!status) {
				DEBUG_WARN("%p: Uni-directional v6 flow update failed in PPE.\n", feci);
				ecm_ppe_stats_v6_inc(feci, ECM_PPE_STATS_V6_EXCEPTION_PORTED, ECM_PPE_STATS_V6_EXCEPTION_PORTED_UNIDIR_UPDATE_FAIL);
			}
		}
		break;
	}
	case ECM_RULE_UPDATE_TYPE_BI_DI_SAWF_QOS:
	{
		struct ecm_front_end_flowsawf_msg *msg = (struct ecm_front_end_flowsawf_msg *)arg;
		struct ecm_classifier_instance *assignments[ECM_CLASSIFIER_TYPES];
		int aci_index;
		int assignment_count;

		rule_type |= PPE_DRV_UPDATE_RULE_TYPE_SAWF;
		if (msg->ip_version == 4) {
			status = ecm_ppe_ported_ipv4_bidir_sawf_rule_update(feci->ci, msg, rule_type);
			if (!status) {
				DEBUG_WARN("%px: Failed to update v4 sawf mark value in PPE\n", feci);
				ecm_ppe_stats_v4_inc(feci, ECM_PPE_STATS_V4_EXCEPTION_PORTED, ECM_PPE_STATS_V4_EXCEPTION_PORTED_BIDIR_UPDATE_FAIL);
				return;
			}
		} else {
			status = ecm_ppe_ported_ipv6_bidir_sawf_rule_update(feci->ci, msg, rule_type);
			if (!status) {
				DEBUG_WARN("%px: Failed to update v6 sawf mark value in PPE\n", feci);
				ecm_ppe_stats_v6_inc(feci, ECM_PPE_STATS_V6_EXCEPTION_PORTED, ECM_PPE_STATS_V6_EXCEPTION_PORTED_BIDIR_UPDATE_FAIL);
				return;
			}
		}

		msg->status = true;

		/*
		 * Get the assigned classifiers and call their update callbacks. If they are interested in this type of case
		 * update, they will handle the event.
		 */
		assignment_count = ecm_db_connection_classifier_assignments_get_and_ref(feci->ci, assignments);
		for (aci_index = 0; aci_index < assignment_count; ++aci_index) {
			struct ecm_classifier_instance *aci;
			aci = assignments[aci_index];
			if (aci->update) {
				aci->update(aci, type, msg);
			}
		}

		ecm_db_connection_assignments_release(assignment_count, assignments);

		break;
	}
	default:
		DEBUG_WARN("%px: unsupported update rule type: %d\n", feci, type);
		break;
	}
}
