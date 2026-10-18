/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/module.h>
#include <linux/inet.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <net/ipv6.h>
#include <linux/etherdevice.h>
#include <net/sch_generic.h>
#ifdef ECM_INTERFACE_PPPOE_ENABLE
#include <linux/if_pppox.h>
#endif

/*
 * Debug output levels
 * 0 = OFF
 * 1 = ASSERTS / ERRORS
 * 2 = 1 + WARN
 * 3 = 2 + INFO
 * 4 = 3 + TRACE
 */
#define DEBUG_LEVEL ECM_SFE_COMMON_DEBUG_LEVEL

#include <sfe_api.h>

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
#include "ecm_front_end_common.h"
#include "ecm_sfe_ipv4.h"
#include "ecm_sfe_ipv6.h"
#include "ecm_sfe_common.h"
#include "exports/ecm_sfe_common_public.h"
#include "ecm_sfe_ported_ipv4.h"
#include "ecm_sfe_ported_ipv6.h"

#if defined(ECM_MHT_ENABLE) || defined(ECM_FRONT_END_PPE_ENABLE)
#include <ppe_drv.h>
#endif
#ifdef ECM_FRONT_END_PPE_ENABLE
#include <ppe_tun.h>
#endif

#include "ecm_sfe_stats_v4.h"
#include "ecm_sfe_stats_v6.h"

/*
 * Callback object to support SFE frontend interaction with external code
 */
struct ecm_sfe_common_callbacks ecm_sfe_cb;

/*
 * Sysctl table
 */
static struct ctl_table_header *ecm_sfe_ctl_tbl_hdr;

/*
 * Flag to indicate fast_xmit is enabled for rule push from SFE frontend.
 */
static int ecm_sfe_fast_xmit_enable = 1;

/*
 * Flag to indicate FSE rule push from ECM SFE frontend.
 */
unsigned int ecm_sfe_fse_enable = 1;

#ifdef ECM_MHT_ENABLE
/*
 * Flag to indicate MHT is enabled.
 */
unsigned int ecm_sfe_mht_enable = 1;
#endif

/*
 * Flag to indicate fast_xmit is enabled for tunnel interfaces.
 * This works only when global "ecm_sfe_fast_xmit_enable" flag is enabled.
 */
static int ecm_sfe_tun_fast_xmit_enable = 1;

/*
 * ecm_sfe_common_fast_xmit_check()
 *	Check the fast transmit feasibility.
 *
 * It checks for source and destination interface type.
 */
static bool ecm_sfe_common_fast_xmit_check(struct ecm_db_iface_instance *to_ii, struct ecm_db_iface_instance *from_ii)
{
	ecm_db_iface_type_t type;

	/*
	 * Return failure if user has disabled SFE fast_xmit
	 */
	if (!ecm_sfe_fast_xmit_enable) {
		return false;
	}

	BUG_ON(!rcu_read_lock_bh_held());

	type = ecm_db_iface_type_get(to_ii);
	switch (type) {
#ifdef ECM_INTERFACE_IPSEC_ENABLE
	case ECM_DB_IFACE_TYPE_IPSEC_TUNNEL:
		DEBUG_INFO("%px: Fast xmit is not enabled for ipsec device\n", to_ii);
		return false;
#endif
	default:
		break;
	}

	/*
	 * Do not set fast_xmit for flow coming from tunnel interface where host may
	 * modify more than 256B of skb content. With fast_xmit flag set, complete
	 * cache flush does not happen in WLAN.
	 */
	if (!ecm_sfe_tun_fast_xmit_enable) {
		type = ecm_db_iface_type_get(from_ii);
		switch (type) {
		case ECM_DB_IFACE_TYPE_SIT:
		case ECM_DB_IFACE_TYPE_TUNIPIP6:
		case ECM_DB_IFACE_TYPE_PPPOL2TPV2:
		case ECM_DB_IFACE_TYPE_PPTP:
		case ECM_DB_IFACE_TYPE_MAP_T:
		case ECM_DB_IFACE_TYPE_GRE_TUN:
		case ECM_DB_IFACE_TYPE_GRE_TAP:
		case ECM_DB_IFACE_TYPE_VXLAN:
		case ECM_DB_IFACE_TYPE_OVPN:
		case ECM_DB_IFACE_TYPE_L2TPV3:
			DEBUG_INFO("%px: Fast xmit is not enabled for: %s", from_ii, ecm_db_interface_type_to_string(type));
			return false;

		default:
			break;
		}
	}

	return true;
}

/*
 * ecm_sfe_feature_check()
 *	Check some specific features for SFE acceleration
 */
bool ecm_sfe_feature_check(struct sk_buff *skb, struct ecm_tracker_ip_header *ip_hdr, bool is_routed)
{
	if (!is_routed && !sfe_is_l2_feature_enabled()) {
		return false;
	}

	return ecm_front_end_feature_check(skb, ip_hdr);
}

/*
 * ecm_sfe_common_is_l2_iface_supported()
 *	Check if full offload can be supported in SFE engine for the given L2 interface and interface type
 */
bool ecm_sfe_common_is_l2_iface_supported(ecm_db_iface_type_t ii_type, int cur_heirarchy_index, int first_heirarchy_index)
{
	/*
	 * If extended feature is not supported, we dont need to check interface heirarchy.
	 */
	if (!sfe_is_l2_feature_enabled()) {
		DEBUG_TRACE("There is no support for extended features\n");
		return false;
	}

	switch (ii_type) {
	case ECM_DB_IFACE_TYPE_BRIDGE:
	case ECM_DB_IFACE_TYPE_OVS_BRIDGE:

		/*
		 * Below checks ensure that bridge slave interface is not a subinterce and top interface is bridge interface.
		 * This means that we support only ethX-br-lan in the herirarchy.
		 * We can remove all these checks if all l2 features are supported.
		 */

		if (cur_heirarchy_index != (ECM_DB_IFACE_HEIRARCHY_MAX - 1)) {
			DEBUG_TRACE("Top interface is not bridge, current index=%d\n", cur_heirarchy_index);
			goto fail;
		}
		return true;

	case ECM_DB_IFACE_TYPE_MACVLAN:
		return true;

	default:
		break;
	}

fail:
	return false;
}

/*
 * ecm_sfe_common_fast_xmit_set()
 *	Configure the qdisc and fast transmit settings in the rule.
 *
 * Note: We configure SFE to use full L2 offload in case a single qdisc or no qdisc is enabled in the xmit interface
 * hierarchy for the direction. This configuration sequence works as below.
 *
 * Step 1.) Based on the qdisc checks, we first decide the destination interface for the direction as below:
 *   a.) More than one qdisc in heirarchy - use top interface and disable L2 forwarding by disabling bottom
 *   	interface setting.
 *   b.) Single qdisc or no qdisc in heirarchy - enable L2 offload (set bottom interface flag). If qdisc is found,
 *   we also configure the interface number on which qdisc is enabled, in the qdisc rule. SFE will use bottom
 *   interface and qdisc interface settings to enable full L2 offload along with qdisc processing.
 *
 * Step 2.) Fast transmit setting is enabled on the destination interface only when no qdisc
 * 	is found in the hierarchy.
 */
void ecm_sfe_common_fast_xmit_set(uint32_t *rule_flags, uint32_t *valid_flags, struct sfe_qdisc_rule *qdisc_rule, struct ecm_db_iface_instance *from_ifaces[ECM_DB_IFACE_HEIRARCHY_MAX], struct ecm_db_iface_instance *to_ifaces[ECM_DB_IFACE_HEIRARCHY_MAX], int32_t from_interfaces_first, int32_t to_interfaces_first)
{
	struct ecm_db_iface_instance *from_ii;
	struct ecm_db_iface_instance *to_ii;
	bool qdisc_found = false;
	bool is_ppeq = false;
	s32 interface_num;
	int list_index;

	rcu_read_lock_bh();

	/*
	 * Get FROM and TO interface instance (top/bottom).
	 */
	from_ii = from_ifaces[ECM_DB_IFACE_HEIRARCHY_MAX - 1];
        if (*rule_flags & SFE_RULE_CREATE_FLAG_USE_FLOW_BOTTOM_INTERFACE) {
		from_ii = from_ifaces[from_interfaces_first];
	}

	to_ii = to_ifaces[ECM_DB_IFACE_HEIRARCHY_MAX - 1];
        if (*rule_flags & SFE_RULE_CREATE_FLAG_USE_RETURN_BOTTOM_INTERFACE) {
		to_ii = to_ifaces[to_interfaces_first];
	}

	/*
	 * Check if a single qdisc is enabled in the interface heirarchy. If yes, configure qdisc rule
	 */
	qdisc_rule->flow_qdisc_interface = -1;
	for (list_index = from_interfaces_first; list_index < ECM_DB_IFACE_HEIRARCHY_MAX; list_index++) {
		interface_num = ecm_db_iface_interface_identifier_get(from_ifaces[list_index]);
		if (ecm_front_end_common_intf_qdisc_check(interface_num, &is_ppeq)) {
			if (qdisc_found) {
				qdisc_rule->valid_flags &= ~SFE_QDISC_RULE_FLOW_VALID;
				qdisc_rule->flow_qdisc_interface = -1;
				qdisc_rule->valid_flags &= ~SFE_QDISC_RULE_FLOW_PPE_QDISC_FAST_XMIT;

				/*
				 * We have found more than one qdisc enabled in the interface heirarchy.
				 * So strip the bottom interface flag for this case.
				 */
				*rule_flags &= ~SFE_RULE_CREATE_FLAG_USE_FLOW_BOTTOM_INTERFACE;
				*rule_flags |= SFE_RULE_CREATE_FLAG_FLOW_L2_DISABLE;
				break;
			}

			qdisc_found = true;
			qdisc_rule->valid_flags |= SFE_QDISC_RULE_FLOW_VALID;
			qdisc_rule->flow_qdisc_interface = interface_num;
			if (is_ppeq) {
				/*
				 * Set SFE_QDISC_RULE_FLOW_PPE_QDISC_FAST_XMIT to identify PPE Qdisc is
				 * configured for the flow direction and packets can be fast transmitted
				 */
				qdisc_rule->valid_flags |= SFE_QDISC_RULE_FLOW_PPE_QDISC_FAST_XMIT;
			}
		}
	}

	/*
	 * Check if we can enable fast transmit for destination (FROM) interface.
	 * This depends on qdisc and source (TO) interface also.
	 */
	if ((!qdisc_found || (qdisc_rule->valid_flags & SFE_QDISC_RULE_FLOW_PPE_QDISC_FAST_XMIT))
		&& ecm_sfe_common_fast_xmit_check(from_ii, to_ii)) {
		*rule_flags |= SFE_RULE_CREATE_FLAG_RETURN_TRANSMIT_FAST;
	}
	qdisc_found = false;
	is_ppeq = false;

	/*
	 * Check if a single qdisc is enabled in the interface heirarchy. If yes, configure qdisc rule
	 */
	qdisc_rule->return_qdisc_interface = -1;
	for (list_index = to_interfaces_first; list_index < ECM_DB_IFACE_HEIRARCHY_MAX; list_index++) {
		interface_num = ecm_db_iface_interface_identifier_get(to_ifaces[list_index]);
		if (ecm_front_end_common_intf_qdisc_check(interface_num, &is_ppeq)) {
			if (qdisc_found) {
				qdisc_rule->valid_flags &= ~SFE_QDISC_RULE_RETURN_VALID;
				qdisc_rule->return_qdisc_interface = -1;
				qdisc_rule->valid_flags &= ~SFE_QDISC_RULE_RETURN_PPE_QDISC_FAST_XMIT;

				/*
				 * We have found more than one qdisc enabled in the interface heirarchy.
				 * So strip the bottom interface flag for this case.
				 */
				*rule_flags &= ~SFE_RULE_CREATE_FLAG_USE_RETURN_BOTTOM_INTERFACE;
				*rule_flags |= SFE_RULE_CREATE_FLAG_RETURN_L2_DISABLE;
				break;
			}

			qdisc_found = true;
			qdisc_rule->return_qdisc_interface = interface_num;
			qdisc_rule->valid_flags |= SFE_QDISC_RULE_RETURN_VALID;
			if (is_ppeq) {
				/*
				 * Set SFE_QDISC_RULE_RETURN_PPE_QDISC_FAST_XMIT to identify PPE Qdisc is
				 * configured for the return direction and packets can be fast transmitted
				 */
				qdisc_rule->valid_flags |= SFE_QDISC_RULE_RETURN_PPE_QDISC_FAST_XMIT;
			}
		}
	}

	/*
	 * Check if we can enable fast transmit for destination (TO) interface.
	 * This depends on qdisc and source (FROM) interface also.
	 */
	if ((!qdisc_found || (qdisc_rule->valid_flags & SFE_QDISC_RULE_RETURN_PPE_QDISC_FAST_XMIT))
		&& ecm_sfe_common_fast_xmit_check(to_ii, from_ii)) {
		*rule_flags |= SFE_RULE_CREATE_FLAG_FLOW_TRANSMIT_FAST;
	}

	/*
	 * Set the Qdisc rule valid flag if qdisc is present in any direction.
	 */
	if ((qdisc_rule->valid_flags & SFE_QDISC_RULE_FLOW_VALID) || (qdisc_rule->valid_flags & SFE_QDISC_RULE_RETURN_VALID)) {
		*valid_flags |= SFE_RULE_CREATE_QDISC_RULE_VALID;
	}

	rcu_read_unlock_bh();
}

/*
 * ecm_sfe_fast_xmit_enable_handler()
 *	Fast transmit sysctl node handler.
 */
static int ecm_sfe_fast_xmit_enable_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	/*
	 * Write the variable with user input
	 */
	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		return ret;
	}

	if ((ecm_sfe_fast_xmit_enable != 0) && (ecm_sfe_fast_xmit_enable != 1)) {
		DEBUG_WARN("Invalid input. Valid values 0/1\n");
		return -EINVAL;
	}

	return ret;
}

/*
 * ecm_sfe_fse_enable_handler()
 *	Sysctl to enable/disable FSE programming through ECM SFE frontend.
 */
static int ecm_sfe_fse_enable_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;
	int current_val;

	/*
	 * Write the variable with user input
	 */
	current_val = ecm_sfe_fse_enable;
	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		/*
		 * Return failure.
		 */
		return ret;
	}

	if ((ecm_sfe_fse_enable != 0) && (ecm_sfe_fse_enable != 1)) {
		ecm_sfe_fse_enable = current_val;
		DEBUG_WARN("Invalid input. Valid values 0/1\n");
		return -EINVAL;
	}

	return ret;
}

#ifdef ECM_MHT_ENABLE
/*
 * ecm_sfe_mht_enable_handler()
 *	Sysctl to enable/disable MHT feature through ECM SFE frontend.
 */
static int ecm_sfe_mht_enable_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;
	int current_val;

	/*
	 * Write the variable with user input
	 */
	current_val = ecm_sfe_mht_enable;
	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		/*
		 * Return failure.
		 */
		return ret;
	}

	if ((ecm_sfe_mht_enable != 0) && (ecm_sfe_mht_enable != 1)) {
		ecm_sfe_mht_enable = current_val;
		DEBUG_WARN("Invalid input. Valid values 0/1\n");
		return -EINVAL;
	}

	return ret;
}
#endif

/*
 * ecm_sfe_tun_fast_xmit_enable_handler()
 *	Tunnel fast transmit enable sysctl node handler.
 */
static int ecm_sfe_tun_fast_xmit_enable_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	/*
	 * Write the variable with user input
	 */
	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		return ret;
	}

	if ((ecm_sfe_tun_fast_xmit_enable != 0) && (ecm_sfe_tun_fast_xmit_enable != 1)) {
		DEBUG_WARN("Invalid input. Valid values 0/1\n");
		return -EINVAL;
	}

	return ret;
}

/*
 * ecm_sfe_ipv4_is_conn_limit_reached()
 *	Connection limit is reached or not ?
 */
bool ecm_sfe_ipv4_is_conn_limit_reached(void)
{

#if !defined(ECM_FRONT_END_CONN_LIMIT_ENABLE)
	return false;
#endif

	if (likely(!((ecm_front_end_is_feature_supported(ECM_FE_FEATURE_CONN_LIMIT)) && ecm_front_end_conn_limit))) {
		return false;
	}

	if ((ecm_sfe_ipv4_pending_accel_count + ecm_sfe_ipv4_accelerated_count) >= sfe_ipv4_max_conn_count()) {
		DEBUG_INFO("ECM DB connection limit reached with accelerated count:%d, pending accel count:%d, for SFE frontend \
				new flows cannot be accelerated.\n",
				ecm_sfe_ipv4_accelerated_count, ecm_sfe_ipv4_pending_accel_count);
		return true;
	}

	return false;
}

#ifdef ECM_IPV6_ENABLE
/*
 * ecm_sfe_ipv6_is_conn_limit_reached()
 *	Connection limit is reached or not ?
 */
bool ecm_sfe_ipv6_is_conn_limit_reached(void)
{

#if !defined(ECM_FRONT_END_CONN_LIMIT_ENABLE)
	return false;
#endif

	if (likely(!((ecm_front_end_is_feature_supported(ECM_FE_FEATURE_CONN_LIMIT)) && ecm_front_end_conn_limit))) {
		return false;
	}

	if ((ecm_sfe_ipv6_pending_accel_count + ecm_sfe_ipv6_accelerated_count) >= sfe_ipv6_max_conn_count()) {
		DEBUG_INFO("ECM DB connection limit reached with accelerated count:%d, pending accel count:%d, for SFE frontend \
				new flows cannot be accelerated.\n",
				ecm_sfe_ipv6_accelerated_count, ecm_sfe_ipv6_pending_accel_count);
		return true;
	}

	return false;
}

#endif

static struct ctl_table ecm_sfe_sysctl_tbl[] = {
	{
		.procname	= "sfe_fast_xmit_enable",
		.data		= &ecm_sfe_fast_xmit_enable,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &ecm_sfe_fast_xmit_enable_handler,
	},
	{
		.procname       = "sfe_fse_enable",
		.data           = &ecm_sfe_fse_enable,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = &ecm_sfe_fse_enable_handler,
	},
#ifdef ECM_MHT_ENABLE
	{
		.procname       = "sfe_mht_enable",
		.data           = &ecm_sfe_mht_enable,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = &ecm_sfe_mht_enable_handler,
	},
#endif
	{
		.procname	= "sfe_tun_fast_xmit_enable",
		.data		= &ecm_sfe_tun_fast_xmit_enable,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &ecm_sfe_tun_fast_xmit_enable_handler,
	},
};

/*
 * ecm_sfe_sysctl_tbl_init()
 * 	Register sysctl for SFE
 */
int ecm_sfe_sysctl_tbl_init(void)
{
	ecm_sfe_ctl_tbl_hdr = register_sysctl(ECM_FRONT_END_SYSCTL_PATH, ecm_sfe_sysctl_tbl);
	if (!ecm_sfe_ctl_tbl_hdr) {
		DEBUG_WARN("Unable to register ecm_sfe_sysctl_tbl");
		return -EINVAL;
	}

	return 0;
}

/*
 * ecm_sfe_sysctl_tbl_exit()
 * 	Unregister sysctl for SFE
 */
void ecm_sfe_sysctl_tbl_exit(void)
{
	if (ecm_sfe_ctl_tbl_hdr) {
		unregister_sysctl_table(ecm_sfe_ctl_tbl_hdr);
	}
}

/*
 * ecm_sfe_common_init_fe_info()
 *	Initialize common fe info
 */
void ecm_sfe_common_init_fe_info(struct ecm_front_end_common_fe_info *info)
{
	info->from_stats_bitmap = 0;
	info->to_stats_bitmap = 0;
	info->front_end_flags = 0;
}

#ifdef ECM_BRIDGE_VLAN_FILTERING_ENABLE
/*
 * ecm_sfe_common_vlan_filter_set()
 * 	Initialize rule create structure with Bridge VLAN Filter information in connection instance.
 */
bool ecm_sfe_common_vlan_filter_set(struct ecm_db_connection_instance *ci, struct sfe_vlan_filter_rule *vlan_filter_rule, bool is_flow_dir)
{
	uint16_t index;

	/*
	 * Bridge VLAN Filter offload can only be achieved when l2_feature is enabled in SFE.
	 */
	if (!sfe_is_l2_feature_enabled()) {
		DEBUG_TRACE("%px: Bridge VLAN filter rule is not programmed as SFE L2 Feature Flag is disabled\n", ci);
		goto no_rule;
	}

	/*
	 * Ingress
	 */
	index = ECM_VLAN_FILTER_RULE_MAX;
	if (is_flow_dir) {
		if (ci->vlan_filter[ECM_VLAN_FILTER_RULE_FLOW_INGRESS1].is_valid) {
			index = ECM_VLAN_FILTER_RULE_FLOW_INGRESS1;
		} else if (ci->vlan_filter[ECM_VLAN_FILTER_RULE_FLOW_EGRESS1].is_valid) {
			index = ECM_VLAN_FILTER_RULE_FLOW_EGRESS1;
		} else {
			DEBUG_TRACE("%px: Bridge VLAN filter rule: no valid index is valid at ingress for FLOW Dir\n", ci);
			goto no_rule;
		}
	} else {
		if (ci->vlan_filter[ECM_VLAN_FILTER_RULE_RET_INGRESS1].is_valid) {
			index = ECM_VLAN_FILTER_RULE_RET_INGRESS1;
		} else if (ci->vlan_filter[ECM_VLAN_FILTER_RULE_RET_EGRESS1].is_valid) {
			index = ECM_VLAN_FILTER_RULE_RET_EGRESS1;
		} else {
			DEBUG_TRACE("%px: Bridge VLAN filter rule: no valid index is valid at ingress for RET Dir\n", ci);
			goto no_rule;
		}
	}

	if (index != ECM_VLAN_FILTER_RULE_MAX) {
		DEBUG_TRACE("Ingress Rule selected: for index %d %s\n", index, ecm_db_connection_vlan_filter_type_strings[index]);
		vlan_filter_rule->ingress_vlan_tag = (
				((ci->vlan_filter[index].vlan_tpid) << 16) |
				ci->vlan_filter[index].vlan_tag);
		vlan_filter_rule->ingress_flags = ci->vlan_filter[index].flags;
	}

	/*
	 * Egress
	 */
	index = ECM_VLAN_FILTER_RULE_MAX;
	if (is_flow_dir) {
		if (ci->vlan_filter[ECM_VLAN_FILTER_RULE_FLOW_EGRESS2].is_valid) {
			index = ECM_VLAN_FILTER_RULE_FLOW_EGRESS2;
		} else if (ci->vlan_filter[ECM_VLAN_FILTER_RULE_FLOW_EGRESS1].is_valid) {
			index = ECM_VLAN_FILTER_RULE_FLOW_EGRESS1;
		} else if (ci->vlan_filter[ECM_VLAN_FILTER_RULE_FLOW_INGRESS2].is_valid) {
			index = ECM_VLAN_FILTER_RULE_FLOW_INGRESS2;
		} else {
			DEBUG_TRACE("%px: Bridge VLAN filter rule: no valid index is valid at egress for FLOW Dir\n", ci);
			goto no_rule;
		}
	} else {
		if (ci->vlan_filter[ECM_VLAN_FILTER_RULE_RET_EGRESS2].is_valid) {
			index = ECM_VLAN_FILTER_RULE_RET_EGRESS2;
		} else if (ci->vlan_filter[ECM_VLAN_FILTER_RULE_RET_EGRESS1].is_valid) {
			index = ECM_VLAN_FILTER_RULE_RET_EGRESS1;
		} else if (ci->vlan_filter[ECM_VLAN_FILTER_RULE_RET_INGRESS2].is_valid) {
			index = ECM_VLAN_FILTER_RULE_RET_INGRESS2;
		} else {
			DEBUG_TRACE("%px: Bridge VLAN filter rule: no valid index is valid at egress for RET Dir\n", ci);
			goto no_rule;
		}
	}

	if (index != ECM_VLAN_FILTER_RULE_MAX) {
		DEBUG_TRACE("Egress Rule selected: for index %d %s", index, ecm_db_connection_vlan_filter_type_strings[index]);
		vlan_filter_rule->egress_vlan_tag = (
				((ci->vlan_filter[index].vlan_tpid) << 16) |
				ci->vlan_filter[index].vlan_tag);
		vlan_filter_rule->egress_flags = ci->vlan_filter[index].flags;
	}

	DEBUG_TRACE("Filling sfe rule (is_flow:%d): ingress_vlan_tag: 0x%x : ingress_vlan_flags: %d , egress_vlan_tag: 0x%x : egress_vlan_flags: %d",
			is_flow_dir, vlan_filter_rule->ingress_vlan_tag, vlan_filter_rule->ingress_flags,
			vlan_filter_rule->egress_vlan_tag, vlan_filter_rule->egress_flags);

	return true;

no_rule:
	vlan_filter_rule->ingress_vlan_tag = SFE_VLAN_ID_NOT_CONFIGURED;
	vlan_filter_rule->egress_vlan_tag = SFE_VLAN_ID_NOT_CONFIGURED;
	vlan_filter_rule->ingress_flags = 0;
	return false;
}
#endif

/*
 * ecm_sfe_common_update_rule()
 *	Updates the frontend specifc data.
 */
void ecm_sfe_common_update_rule(struct ecm_front_end_connection_instance *feci, enum ecm_rule_update_type type,
		void *arg)
{
	bool status;

	switch (type) {
		case ECM_RULE_UPDATE_TYPE_UNI_DI_QOS:
		{
			if (feci->ip_version == 4) {
				struct sfe_ipv4_msg *msg_v4;
				struct ecm_cmn_unidir_update_info *update_info = (struct ecm_cmn_unidir_update_info *) arg;

				msg_v4 = (struct sfe_ipv4_msg *)kzalloc(sizeof(struct sfe_ipv4_msg), GFP_ATOMIC | __GFP_NOWARN);
				if (!msg_v4) {
					DEBUG_WARN("%px: no memory for sfe ipv4 message structure instance: %px\n", feci, feci->ci);
					ecm_sfe_stats_v4_inc(feci, ECM_SFE_STATS_V4_EXCEPTION_PORTED, ECM_SFE_STATS_V4_EXCEPTION_PORTED_UNIDIR_UPDATE_NO_MEM);
					return;
				}

				sfe_ipv4_msg_init(msg_v4, SFE_SPECIAL_INTERFACE_IPV4, SFE_TX_UPDATE_RULE_MSG,
					sizeof(struct sfe_rule_update_msg), NULL, NULL);

				status = ecm_sfe_ported_ipv4_unidir_rule_update(feci->ci, update_info->pr, update_info->sender, msg_v4);
				if (!status) {
						DEBUG_WARN("%p: Uni-directional v4 flow update failed in SFE.\n", feci);
						ecm_sfe_stats_v4_inc(feci, ECM_SFE_STATS_V4_EXCEPTION_PORTED, ECM_SFE_STATS_V4_EXCEPTION_PORTED_UNIDIR_UPDATE_FAIL);
				}

				kfree(msg_v4);
			} else {
				struct sfe_ipv6_msg *msg_v6;
				struct ecm_cmn_unidir_update_info *update_info = (struct ecm_cmn_unidir_update_info *) arg;

				msg_v6 = (struct sfe_ipv6_msg *)kzalloc(sizeof(struct sfe_ipv6_msg), GFP_ATOMIC | __GFP_NOWARN);
				if (!msg_v6) {
					DEBUG_WARN("%px: no memory for sfe ipv6 message structure instance: %px\n", feci, feci->ci);
					ecm_sfe_stats_v6_inc(feci, ECM_SFE_STATS_V6_EXCEPTION_PORTED, ECM_SFE_STATS_V6_EXCEPTION_PORTED_UNIDIR_UPDATE_NO_MEM);
					return;
				}

				sfe_ipv6_msg_init(msg_v6, SFE_SPECIAL_INTERFACE_IPV6, SFE_TX_UPDATE_RULE_MSG,
							sizeof(struct sfe_rule_update_msg), NULL, NULL);

				status = ecm_sfe_ported_ipv6_unidir_rule_update(feci->ci, update_info->pr, update_info->sender, msg_v6);
				if (!status) {
					DEBUG_WARN("%p: Uni-directional v6 flow update failed in SFE.\n", feci);
					ecm_sfe_stats_v6_inc(feci, ECM_SFE_STATS_V6_EXCEPTION_PORTED, ECM_SFE_STATS_V6_EXCEPTION_PORTED_UNIDIR_UPDATE_FAIL);
				}

				kfree(msg_v6);
			}
			break;
		}
		case ECM_RULE_UPDATE_TYPE_CONNMARK:
		{
			struct nf_conn *ct = (struct nf_conn *)arg;
			ip_addr_t src_addr;
			ip_addr_t dest_addr;
			int aci_index;
			int assignment_count;
			struct ecm_classifier_instance *assignments[ECM_CLASSIFIER_TYPES];
			struct sfe_rule_update_msg *update_msg = NULL;

			if (feci->ip_version == 4) {
				struct sfe_ipv4_msg *msg_v4;

				msg_v4 = (struct sfe_ipv4_msg *)kzalloc(sizeof(struct sfe_ipv4_msg), GFP_ATOMIC | __GFP_NOWARN);
				if (!msg_v4) {
					DEBUG_WARN("%px: no memory for sfe ipv4 message structure instance: %px\n", feci, feci->ci);
					ecm_sfe_stats_v4_inc(feci, ECM_SFE_STATS_V4_EXCEPTION_PORTED, ECM_SFE_STATS_V4_EXCEPTION_PORTED_UNIDIR_UPDATE_NO_MEM);
					return;
				}

				sfe_ipv4_msg_init(msg_v4, SFE_SPECIAL_INTERFACE_IPV4, SFE_TX_UPDATE_RULE_MSG,
										sizeof(struct sfe_rule_update_msg), NULL, NULL);
				update_msg = &msg_v4->msg.rule_update;

				if (ecm_front_end_connection_accel_state_get(feci) != ECM_FRONT_END_ACCELERATION_MODE_ACCEL) {
					DEBUG_WARN("%px: connection is not in accelerated mode\n", feci);
					kfree(msg_v4);
					return;
				}

				/*
				 * Get connection information
				 */
				update_msg->flow_rule_id = ecm_db_connection_serial_get(feci->ci);
				update_msg->type = SFE_CONNECTION_MARK_TYPE_CONNMARK;
				update_msg->protocol = (int32_t)ecm_db_connection_protocol_get(feci->ci);
				update_msg->src_port = htons(ecm_db_connection_port_get(feci->ci, ECM_DB_OBJ_DIR_FROM));
				update_msg->dest_port = htons(ecm_db_connection_port_get(feci->ci, ECM_DB_OBJ_DIR_TO_NAT));
				ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_FROM, src_addr);
				ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_TO_NAT, dest_addr);
				update_msg->info.connmark.flow_mark = ct->mark;
				update_msg->info.connmark.return_mark = ct->mark;

				DEBUG_INFO("%px: Update the mark value for the SFE connection\n", feci);

				ECM_IP_ADDR_TO_NIN4_ADDR(update_msg->src_ip[0], src_addr);
				ECM_IP_ADDR_TO_NIN4_ADDR(update_msg->dest_ip[0], dest_addr);

				if (sfe_ipv4_tx(NULL, msg_v4) != SFE_TX_SUCCESS) {
					DEBUG_WARN("%px: Failed to update mark value in SFE", feci);
					kfree(msg_v4);
					return;
				}

				DEBUG_INFO("%px: src_ip: %pI4 dest_ip: %pI4 src_port: %d dest_port: %d protocol: %d\n",
						feci, &update_msg->src_ip[0], &update_msg->dest_ip[0],
						ntohs(update_msg->src_port), ntohs(update_msg->dest_port), update_msg->protocol);

				/*
				 * Get the assigned classifiers and call their update callbacks. If they are interested in this type of
				 * update, they will handle the event.
				 */
				assignment_count = ecm_db_connection_classifier_assignments_get_and_ref(feci->ci, assignments);
				for (aci_index = 0; aci_index < assignment_count; ++aci_index) {
					struct ecm_classifier_instance *aci;
					aci = assignments[aci_index];
					if (aci->update) {
						aci->update(aci, type, ct);
					}
				}
				ecm_db_connection_assignments_release(assignment_count, assignments);
				kfree(msg_v4);
			} else {
				struct sfe_ipv6_msg *msg_v6;

				msg_v6 = (struct sfe_ipv6_msg *)kzalloc(sizeof(struct sfe_ipv6_msg), GFP_ATOMIC | __GFP_NOWARN);
				if (!msg_v6) {
					DEBUG_WARN("%px: no memory for sfe ipv6 message structure instance: %px\n", feci, feci->ci);
					ecm_sfe_stats_v6_inc(feci, ECM_SFE_STATS_V6_EXCEPTION_PORTED, ECM_SFE_STATS_V6_EXCEPTION_PORTED_UNIDIR_UPDATE_NO_MEM);
					return;
				}

				sfe_ipv6_msg_init(msg_v6, SFE_SPECIAL_INTERFACE_IPV6, SFE_TX_UPDATE_RULE_MSG,
							sizeof(struct sfe_rule_update_msg), NULL, NULL);
				update_msg = &msg_v6->msg.rule_update;

				if (ecm_front_end_connection_accel_state_get(feci) != ECM_FRONT_END_ACCELERATION_MODE_ACCEL) {
					DEBUG_WARN("%px: connection is not in accelerated mode\n", feci);
					kfree(msg_v6);
					return;
				}

				/*
				* Get connection information
				*/
				update_msg->type = SFE_CONNECTION_MARK_TYPE_CONNMARK;
				update_msg->flow_rule_id = ecm_db_connection_serial_get(feci->ci);
				update_msg->protocol = (int32_t)ecm_db_connection_protocol_get(feci->ci);
				update_msg->src_port = htons(ecm_db_connection_port_get(feci->ci, ECM_DB_OBJ_DIR_FROM));
				update_msg->dest_port = htons(ecm_db_connection_port_get(feci->ci, ECM_DB_OBJ_DIR_TO_NAT));
				ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_FROM, src_addr);
				ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_TO_NAT, dest_addr);
				update_msg->info.connmark.flow_mark = ct->mark;
				update_msg->info.connmark.return_mark = ct->mark;

				DEBUG_INFO("%px: Update the mark value for the SFE connection\n", feci);

				ECM_IP_ADDR_TO_SFE_IPV6_ADDR(update_msg->src_ip, src_addr);
				ECM_IP_ADDR_TO_SFE_IPV6_ADDR(update_msg->dest_ip, dest_addr);

				if (sfe_ipv6_tx(NULL, msg_v6) != SFE_TX_SUCCESS) {
					DEBUG_WARN("%px: Failed to update mark value in SFE", feci);
					kfree(msg_v6);
					return;
				}

				DEBUG_TRACE("%px: src_ip: " ECM_IP_ADDR_OCTAL_FMT "dest_ip: " ECM_IP_ADDR_OCTAL_FMT
						" src_port: %d dest_port: %d protocol: %d\n",
						feci, ECM_IP_ADDR_TO_OCTAL(src_addr), ECM_IP_ADDR_TO_OCTAL(dest_addr),
						ntohs(update_msg->src_port), ntohs(update_msg->dest_port), update_msg->protocol);

				/*
				 * Get the assigned classifiers and call their update callbacks. If they are interested in this type of
				 * update, they will handle the event.
				 */
				assignment_count = ecm_db_connection_classifier_assignments_get_and_ref(feci->ci, assignments);
				for (aci_index = 0; aci_index < assignment_count; ++aci_index) {
					struct ecm_classifier_instance *aci;
					aci = assignments[aci_index];
					if (aci->update) {
						aci->update(aci, type, ct);
					}
				}
				ecm_db_connection_assignments_release(assignment_count, assignments);
				kfree(msg_v6);
			}
			break;
		}
		case ECM_RULE_UPDATE_TYPE_BI_DI_SAWF_QOS:
		{
			struct ecm_front_end_flowsawf_msg *msg = (struct ecm_front_end_flowsawf_msg *)arg;
			int aci_index;
			int assignment_count;
			struct ecm_classifier_instance *assignments[ECM_CLASSIFIER_TYPES];
			struct sfe_rule_update_msg *update_msg = NULL;
			sfe_tx_status_t sfe_tx_status;

			if (feci->ip_version == 4) {
				struct sfe_ipv4_msg *msg_v4;

				msg_v4 = (struct sfe_ipv4_msg *)kzalloc(sizeof(struct sfe_ipv4_msg), GFP_ATOMIC | __GFP_NOWARN);
				if (!msg_v4) {
					DEBUG_WARN("%px: no memory for sfe ipv4 message structure instance: %px\n", feci, feci->ci);
					ecm_sfe_stats_v4_inc(feci, ECM_SFE_STATS_V4_EXCEPTION_PORTED, ECM_SFE_STATS_V4_EXCEPTION_PORTED_UNIDIR_UPDATE_NO_MEM);
					return;
				}

				sfe_ipv4_msg_init(msg_v4, SFE_SPECIAL_INTERFACE_IPV4, SFE_TX_UPDATE_RULE_MSG,
										sizeof(struct sfe_rule_update_msg), NULL, NULL);
				update_msg = &msg_v4->msg.rule_update;

				DEBUG_INFO("control reached to deprio in update rule \n");

				update_msg->type = SFE_CONNECTION_MARK_TYPE_BIDIR_SAWF_MARK;
				update_msg->info.sawf.flow_mark = msg->flow_mark;
				update_msg->info.sawf.flow_svc_id = msg->flow_service_class_id;

				if (SFE_GET_SAWF_TAG(update_msg->info.sawf.flow_mark) == SFE_SAWF_VALID_TAG ||
						(msg->flags & ECM_FRONT_END_PRIO_UPDATE_FLOW)) {
					update_msg->flags |= SFE_UPDATE_RULE_SAWF_FLOW_VALID;
				}

				update_msg->info.sawf.return_mark = msg->return_mark;
				update_msg->info.sawf.return_svc_id = msg->return_service_class_id;

				if (SFE_GET_SAWF_TAG(update_msg->info.sawf.return_mark) == SFE_SAWF_VALID_TAG
						|| (msg->flags & ECM_FRONT_END_PRIO_UPDATE_RETURN)) {
					update_msg->flags |= SFE_UPDATE_RULE_SAWF_RETURN_VALID;
				}

				update_msg->flow_rule_id = ecm_db_connection_serial_get(feci->ci);
				update_msg->protocol = msg->protocol;
				update_msg->src_port = msg->flow_src_port;
				update_msg->dest_port = msg->flow_dest_port;
				update_msg->src_ip[0] = msg->flow_src_ip[0];
				update_msg->dest_ip[0] = msg->flow_dest_ip[0];

				sfe_tx_status = sfe_ipv4_tx(NULL, msg_v4);

				atomic64_set(&feci->unidir_accel_fail_reason, ecm_front_end_set_ae_failure_reason(sfe_tx_status));
				if (sfe_tx_status != SFE_TX_SUCCESS) {
					DEBUG_WARN("%px: Failed to update mark value in SFE", feci);
					kfree(msg_v4);
					return;
				}

				msg->status = true;

				DEBUG_TRACE("%px: sawf flow/return mark=0x%08x/0x%08x %pI4:%u -> %pI4:%u protocol=%u\n",
						feci, update_msg->info.sawf.flow_mark, update_msg->info.sawf.return_mark,
						update_msg->src_ip, ntohs(update_msg->src_port),
						update_msg->dest_ip, ntohs(update_msg->dest_port),
						update_msg->protocol);

				/*
				 * Get the assigned classifiers and call their update callbacks. If they are interested in this type of
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
				kfree(msg_v4);
			} else {
				struct sfe_ipv6_msg *msg_v6;

				msg_v6 = (struct sfe_ipv6_msg *)kzalloc(sizeof(struct sfe_ipv6_msg), GFP_ATOMIC | __GFP_NOWARN);
				if (!msg_v6) {
					DEBUG_WARN("%px: no memory for sfe ipv6 message structure instance: %px\n", feci, feci->ci);
					ecm_sfe_stats_v6_inc(feci, ECM_SFE_STATS_V6_EXCEPTION_PORTED, ECM_SFE_STATS_V6_EXCEPTION_PORTED_UNIDIR_UPDATE_NO_MEM);
					return;
				}

				sfe_ipv6_msg_init(msg_v6, SFE_SPECIAL_INTERFACE_IPV6, SFE_TX_UPDATE_RULE_MSG,
							sizeof(struct sfe_rule_update_msg), NULL, NULL);
				update_msg = &msg_v6->msg.rule_update;

				DEBUG_INFO("control reached to deprio in update rule \n");

				update_msg->type = SFE_CONNECTION_MARK_TYPE_BIDIR_SAWF_MARK;
				update_msg->info.sawf.flow_mark = msg->flow_mark;
				update_msg->info.sawf.flow_svc_id = msg->flow_service_class_id;

				if (SFE_GET_SAWF_TAG(update_msg->info.sawf.flow_mark) == SFE_SAWF_VALID_TAG
						|| (msg->flags & ECM_FRONT_END_PRIO_UPDATE_FLOW)) {
					update_msg->flags |= SFE_UPDATE_RULE_SAWF_FLOW_VALID;
				}

				update_msg->info.sawf.return_mark = msg->return_mark;
				update_msg->info.sawf.return_svc_id = msg->return_service_class_id;

				if (SFE_GET_SAWF_TAG(update_msg->info.sawf.return_mark) == SFE_SAWF_VALID_TAG
						|| (msg->flags & ECM_FRONT_END_PRIO_UPDATE_RETURN)) {
					update_msg->flags |= SFE_UPDATE_RULE_SAWF_RETURN_VALID;
				}

				update_msg->flow_rule_id = ecm_db_connection_serial_get(feci->ci);
				update_msg->protocol = msg->protocol;
				update_msg->src_port = msg->flow_src_port;
				update_msg->dest_port = msg->flow_dest_port;

				ECM_IP_ADDR_COPY(update_msg->src_ip, msg->flow_src_ip);
				ECM_IP_ADDR_COPY(update_msg->dest_ip, msg->flow_dest_ip);

				sfe_tx_status = sfe_ipv6_tx(NULL, msg_v6);

				atomic64_set(&feci->unidir_accel_fail_reason, ecm_front_end_set_ae_failure_reason(sfe_tx_status));
				if (sfe_tx_status != SFE_TX_SUCCESS) {
					DEBUG_WARN("%px: Failed to update mark value in SFE", feci);
					kfree(msg_v6);
					return;
				}

				msg->status = true;

				DEBUG_TRACE("%px: sawf flow/return mark=0x%08x/0x%08x %pI6c@%u -> %pI6c@%u protocol=%u\n",
						feci, update_msg->info.sawf.flow_mark, update_msg->info.sawf.return_mark,
						update_msg->src_ip, ntohs(update_msg->src_port),
						update_msg->dest_ip, ntohs(update_msg->dest_port),
						update_msg->protocol);

				/*
				 * Get the assigned classifiers and call their update callbacks. If they are interested in this type of
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
				kfree(msg_v6);
			}
			break;
		}
		default:
		{
			DEBUG_WARN("%px: unsupported update rule type: %d\n", feci, type);
			break;
		}
	}
}

/*
 * ecm_sfe_common_tuple_set()
 *	Sets the SFE common tuple object with the ECM connection rule paramaters.
 *
 * This tuple object will be used by external module to make decision on L2 acceleration.
 */
void ecm_sfe_common_tuple_set(struct ecm_front_end_connection_instance *feci,
			      int32_t from_iface_id, int32_t to_iface_id,
			      struct ecm_sfe_common_tuple *tuple)
{
	ip_addr_t saddr;
	ip_addr_t daddr;

	tuple->protocol = ecm_db_connection_protocol_get(feci->ci);
	tuple->ip_ver = feci->ip_version;

	tuple->src_port = ecm_db_connection_port_get(feci->ci, ECM_DB_OBJ_DIR_FROM);
        tuple->dest_port = ecm_db_connection_port_get(feci->ci, ECM_DB_OBJ_DIR_TO);

	tuple->src_ifindex = from_iface_id;
	tuple->dest_ifindex = to_iface_id;

	ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_FROM, saddr);
	ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_TO, daddr);

	if (feci->ip_version == 4) {
		ECM_IP_ADDR_TO_NIN4_ADDR(tuple->src_addr[0], saddr);
		ECM_IP_ADDR_TO_NIN4_ADDR(tuple->dest_addr[0], daddr);
	} else {
		ECM_IP_ADDR_TO_SFE_IPV6_ADDR(tuple->src_addr, saddr);
		ECM_IP_ADDR_TO_SFE_IPV6_ADDR(tuple->dest_addr, daddr);
	}
}

/*
 * ecm_sfe_common_defunct_5tuple_connection()
 *	Defunct an IPv4/v6 5-tuple connection.
 */
bool ecm_sfe_common_defunct_5tuple_connection(char *buf)
{
	return ecm_db_connection_defunct_5tuple_buffer(buf);
}
EXPORT_SYMBOL(ecm_sfe_common_defunct_5tuple_connection);

/*
 * ecm_sfe_common_defunct_by_protocol()
 *	Defunct the connections by the protocol type (e.g:TCP, UDP)
 */
void ecm_sfe_common_defunct_by_protocol(int protocol)
{
	ecm_db_connection_defunct_by_protocol(protocol);
}
EXPORT_SYMBOL(ecm_sfe_common_defunct_by_protocol);

/*
 * ecm_sfe_common_defunct_by_port()
 *	Defunct the connections associated with this port in the direction
 * relative to the ECM's connection direction as well.
 *
 * TODO:
 *	For now, all the connections from/to this port number are defuncted.
 *	Directional defunct can be implemented later, but there is a trade of here:
 *	For each connection in the database, the connection's from/to interfaces will
 *	be checked with the wan_name and direction will be determined and then the connection
 *	will be defuncted if there is a match with this port number. This process may be heavier
 *	than defuncting all the connections from/to this port number. So, the direction and  wan_name
 *	are optional for this API for now.
 */
void ecm_sfe_common_defunct_by_port(int port, int direction, char *wan_name)
{
	ecm_db_connection_defunct_by_port(htons(port), ECM_DB_OBJ_DIR_FROM);
	ecm_db_connection_defunct_by_port(htons(port), ECM_DB_OBJ_DIR_TO);
}
EXPORT_SYMBOL(ecm_sfe_common_defunct_by_port);

/*
 * ecm_sfe_common_callbacks_register()
 *	Registers SFE common callbacks.
 */
int ecm_sfe_common_callbacks_register(struct ecm_sfe_common_callbacks *sfe_cb)
{
	if (!sfe_cb || !sfe_cb->l2_accel_check) {
		DEBUG_ERROR("SFE L2 acceleration check callback is NULL\n");
		return -EINVAL;
	}

	rcu_assign_pointer(ecm_sfe_cb.l2_accel_check, sfe_cb->l2_accel_check);
	synchronize_rcu();

	return 0;
}
EXPORT_SYMBOL(ecm_sfe_common_callbacks_register);

/*
 * ecm_sfe_common_callbacks_unregister()
 *	Unregisters SFE common callbacks.
 */
void ecm_sfe_common_callbacks_unregister(void)
{
	rcu_assign_pointer(ecm_sfe_cb.l2_accel_check, NULL);
	synchronize_rcu();
}
EXPORT_SYMBOL(ecm_sfe_common_callbacks_unregister);

#ifdef ECM_MHT_ENABLE
/*
 * ecm_sfe_common_get_mht_port_id()
 *	Returns true if getting mht port is succesful.
 */
bool ecm_sfe_common_get_mht_port_id(struct ecm_front_end_connection_instance *feci,
				    struct ecm_db_iface_instance *from_sfe_iface,
				    struct ecm_db_iface_instance *to_sfe_iface,
				    u32 *valid_flags, struct sfe_mark_rule *mark_rule)
{
	struct net_device *dev = NULL;
	int32_t port_info = -1;
	uint8_t mht_mac[ETH_ALEN];

	dev  = dev_get_by_index(&init_net, ecm_db_iface_interface_identifier_get(from_sfe_iface));
	if (!dev) {
		DEBUG_WARN("%px: Failed to get net device for from sfe iface\n", feci);
		return true;
	}

	/*
	 * MHT port is found on the from interface.
	 * DSA interfaces are also marked as mht_dev in multiple TX rings enabled case, to automically switch to
	 * SFE acceleration to avoid HOLB issue. While DSA interfaces mark the skb->mark at the DSA TX datapath,
	 * here it doesn't need to query the switch FDB to get the skb->mark.
	 */
	if (ppe_drv_is_mht_dev(dev) && (ecm_db_iface_type_get(from_sfe_iface) != ECM_DB_IFACE_TYPE_DSA)) {
		ecm_db_connection_node_address_get(feci->ci, ECM_DB_OBJ_DIR_FROM, mht_mac);
		port_info = ppe_drv_mht_port_from_fdb(mht_mac, 0);

		if (port_info != -1) {
			spin_lock_bh(&feci->lock);
			feci->mht_port_query_count = 0;
			spin_unlock_bh(&feci->lock);
			dev_put(dev);
			mark_rule->return_mark = ((SFE_MHT_VALID_TAG << SFE_MHT_TAG_SHIFT) | port_info);
			*valid_flags |= SFE_RULE_CREATE_MARK_VALID;
			return true;
		}

		spin_lock_bh(&feci->lock);
		/*
		 * Check if we can re-try to find the port with subsequent packets.
		 */
		if (feci->mht_port_query_count < SFE_MHT_MAX_ACCELERATION_RETRY) {
			feci->mht_port_query_count++;
			spin_unlock_bh(&feci->lock);
			dev_put(dev);
			return false;
		}

		/*
		 * We reached to max re-try count, return true without marking the rule.
		 */
		feci->mht_port_query_count = 0;
		spin_unlock_bh(&feci->lock);
		dev_put(dev);
		return true;
	}

	dev_put(dev);
	dev  = dev_get_by_index(&init_net, ecm_db_iface_interface_identifier_get(to_sfe_iface));
	if (!dev) {
		DEBUG_WARN("%px: Failed to get net device for to sfe iface\n", feci);
		return true;
	}

	/*
	 * MHT port is found on the To interface.
	 * DSA interfaces are also marked as mht_dev in multiple TX rings enabled case, to automically switch to
	 * SFE acceleration to avoid HOLB issue. While DSA interfaces mark the skb->mark at the DSA TX datapath,
	 * here it doesn't need to query the switch FDB to get the skb->mark.
	 */
	if (ppe_drv_is_mht_dev(dev) && (ecm_db_iface_type_get(to_sfe_iface) != ECM_DB_IFACE_TYPE_DSA)) {
		ecm_db_connection_node_address_get(feci->ci, ECM_DB_OBJ_DIR_TO, mht_mac);
		port_info = ppe_drv_mht_port_from_fdb(mht_mac, 0);

		if (port_info != -1) {
			spin_lock_bh(&feci->lock);
			feci->mht_port_query_count = 0;
			spin_unlock_bh(&feci->lock);
			dev_put(dev);
			mark_rule->flow_mark = ((SFE_MHT_VALID_TAG << SFE_MHT_TAG_SHIFT) | port_info);
			*valid_flags |= SFE_RULE_CREATE_MARK_VALID;
			return true;
		}

		spin_lock_bh(&feci->lock);
		/*
		 * Check if we can re-try to find the port with subsequent packets.
		 */
		if (feci->mht_port_query_count < SFE_MHT_MAX_ACCELERATION_RETRY) {
			feci->mht_port_query_count++;
			spin_unlock_bh(&feci->lock);
			dev_put(dev);
			return false;
		}

		/*
		 * We reached to max re-try count, return true without marking the rule.
		 */
		feci->mht_port_query_count = 0;
		spin_unlock_bh(&feci->lock);
		dev_put(dev);
		return true;
	}

	dev_put(dev);
	return true;
}
#endif

#ifdef ECM_FRONT_END_PPE_ENABLE
/*
 * ecm_sfe_common_get_vp_from_iface_id()
 *	Looks up for a tunnel VP in PPE.
 */
int ecm_sfe_common_get_vp_from_iface_id(int32_t iface_id)
{
	int vp = -1;
	struct net_device *dev = NULL;

	dev = dev_get_by_index(&init_net, iface_id);
	if (!dev) {
		goto done;
	}

#ifdef ECM_INTERFACE_GRE_TAP_ENABLE
	if (dev->priv_flags_ext & (IFF_EXT_GRE_V4_TAP | IFF_EXT_GRE_V6_TAP)) {
		vp = ppe_tun_hybrid_ol_ctx_get(dev);
	}
#endif
	dev_put(dev);

done:
	return vp;
}

#if defined(CONFIG_IPQ_PON) && defined(ECM_FRONT_END_PPE_ENABLE)
/*
 * ecm_sfe_common_get_veip_iface_id()
 *	Return the VEIP interface id for direct VEIP or PPPoE-over-VEIP.
 */
int ecm_sfe_common_get_veip_iface_id(int32_t iface_id)
{
	struct net_device *dev;
#ifdef ECM_INTERFACE_PPPOE_ENABLE
	struct pppoe_opt addressing;
#endif
	int32_t veip_iface_id = -1;
	int channel_protocol = 0;

	dev = dev_get_by_index(&init_net, iface_id);
	if (!dev) {
		goto done;
	}

	if (ppe_drv_is_veip_dev(dev)) {
		veip_iface_id = dev->ifindex;
		goto done_put;
	}

#ifdef ECM_INTERFACE_PPPOE_ENABLE
	if (dev->type == ARPHRD_PPP) {
		struct ppp_channel *ppp_chan[1];
		int channel_count;

		channel_count = ppp_hold_channels(dev, ppp_chan, 1);
		if (channel_count == 1) {
			channel_protocol = ppp_channel_get_protocol(ppp_chan[0]);

			if (channel_protocol == PX_PROTO_OE) {
				if (!pppoe_channel_addressing_get(ppp_chan[0], &addressing)) {
					if (ppe_drv_is_veip_dev(addressing.dev)) {
						veip_iface_id = addressing.dev->ifindex;
					}
					dev_put(addressing.dev);
				}
			}

			ppp_release_channels(ppp_chan, 1);
		}
	}
#endif

done_put:
	dev_put(dev);
done:
	return veip_iface_id;
}
#endif
#endif
