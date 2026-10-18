/*
 **************************************************************************
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/**
 * @file netfn_flowmgr_stats.h
 *      Netfn flow manager stats header file.
 */
#ifndef __NETFN_FLOWMGR_STATS_H
#define __NETFN_FLOWMGR_STATS_H

#include <netfn_flowmgr.h>

/*
 * netfn_flowmgr_stats
 *	Message structure for netfn flowmgr stats
 */
struct netfn_flowmgr_debug_stats {
	/* v4 generic stats */
	atomic64_t v4_create_rule_req_fail_no_mem;		/**< No of v4 create rule failed with no memory */
	atomic64_t v4_create_rule_flow_flag_mismatch;		/**< No of v4 create rule failed due to flow flag mismatched */
	atomic64_t v4_create_rule_bridge_flow;			/**< No of v4 create rule for bridged flow */
	atomic64_t v4_create_rule_routed_flow;			/**< No of v4 create rule for routed flow */
	atomic64_t v4_create_validate_tuple_mismatch;		/**< No of v4 create rule failed due to tuple mismatch */
	atomic64_t v4_create_validate_tuple_type_3;		/**< No of v4 create rule tuple type 3 */
	atomic64_t v4_create_validate_tuple_type_4;		/**< No of v4 create rule tuple type 4 */
	atomic64_t v4_create_validate_tuple_type_5;		/**< No of v4 create rule tuple type 5 */
	atomic64_t v4_create_validate_tuple_simul_snat_dnat;	/**< No of v4 create rule failed due to simul snat and dnat */
	atomic64_t v4_create_validate_tuple_invalid_snat_ip;	/**< No of v4 create rule failed due to invalid snat ip */
	atomic64_t v4_create_validate_tuple_invalid_dnat_ip;	/**< No of v4 create rule failed due to invalid dnat ip */
	atomic64_t v4_create_validate_tuple_invalid_snat_port;	/**< No of v4 create rule failed due to invalid snat port */
	atomic64_t v4_create_validate_tuple_invalid_dnat_port;	/**< No of v4 create rule failed due to invalid dnat port */
	atomic64_t v4_create_validate_tuple_ip_addr_mismatch;	/**< No of v4 create rule failed due to ip addr mismatch */
	atomic64_t v4_create_validate_tuple_ident_mismatch;	/**< No of v4 create rule failed due to ident mismatch */
	atomic64_t v4_create_validate_tuple_proto_mismatch;	/**< No of v4 create rule failed due to proto mismatch */
	atomic64_t v4_create_validate_tuple_type_invalid;	/**< No of v4 create rule failed due to invalid tuple type */
	atomic64_t v4_create_validate_tuple_invalid_nat_bridge;	/**< No of v4 create rule failed due to unsupported nat + bridge */
	atomic64_t v4_create_validate_tuple_invalid_net_dev;	/**< No of v4 create rule failed due to invalid net device */
	atomic64_t v4_destroy_validate_tuple_mismatch;		/**< No of v4 destroy rule failed due to tuple mismatch */
	atomic64_t v4_destroy_validate_tuple_type_3;		/**< No of v4 destroy rule tuple type 3 */
	atomic64_t v4_destroy_validate_tuple_type_4;		/**< No of v4 destroy rule tuple type 4 */
	atomic64_t v4_destroy_validate_tuple_type_5;		/**< No of v4 destroy rule tuple type 5 */
	atomic64_t v4_destroy_validate_tuple_type_invalid;	/**< No of v4 destroy rule failed due to invalid tuple type */
	atomic64_t v4_destroy_validate_tuple_ip_addr_mismatch;	/**< No of v4 destroy rule failed due to ip addr mismatch */
	atomic64_t v4_destroy_validate_tuple_ident_mismatch;	/**< No of v4 destroy rule failed due to ident mismatch */
	atomic64_t v4_destroy_validate_tuple_proto_mismatch;	/**< No of v4 destroy rule failed due to proto mismatch */
	atomic64_t v4_destroy_rule_req_fail_no_mem;		/**< No of v4 destroy rule failed with no memory */
	atomic64_t v4_stats_sync_callback_not_registered;	/**< No of v4 sync requests with unregistered cb */
	atomic64_t v4_stats_sync_invalid_msg_type;		/**< No of v4 sync requests with inavalid msg */
	atomic64_t v4_get_stats_fail;				/**< No of v4 get stats failed */
	atomic64_t v4_get_stats_tuple_type_unsupported;		/**< No of v4 get stats failed due to unspported tuple type */

	/* v4 PPE stats */
	atomic64_t v4_create_ppe_rule_req;			/**< No of v4 ppe create rule requests */
	atomic64_t v4_create_rule_invalid_ppe_tx_if;		/**< No of v4 create rule failed due to invalid ppe tx if */
	atomic64_t v4_create_rule_invalid_ppe_rx_if;		/**< No of v4 create rule failed due to invalid ppe rx if */
	atomic64_t v4_create_rule_invalid_ppe_top_tx_if;	/**< No of v4 create rule failed due to invalid ppe top tx if */
	atomic64_t v4_create_rule_invalid_ppe_top_rx_if;	/**< No of v4 create rule failed due to invalid ppe top rx if */
	atomic64_t v4_create_rule_ppe_success;			/**< No of v4 successful ppe create rule requests */
	atomic64_t v4_create_rule_req_fail_ppe_fail;		/**< No of v4 failed ppe create rule requests */
	atomic64_t v4_destroy_ppe_rule_req;			/**< No of v4 ppe destroy rule requests */
	atomic64_t v4_destroy_rule_ppe_success;			/**< No of v4 successful ppe destroy rule requests */
	atomic64_t v4_destroy_rule_req_fail_ppe_fail;		/**< No of v4 failed ppe destroy rule requests */
	atomic64_t v4_get_stats_failed;				/**< No of v4 get stats failed requests */

	/* v4 SFE stats */
	atomic64_t v4_create_sfe_rule_req;			/**< No of v4 sfe create rule requests */
	atomic64_t v4_create_rule_sfe_success;			/**< No of v4 successful sfe create rule requests */
	atomic64_t v4_create_rule_req_fail_sfe_fail;		/**< No of v4 failed sfe create rule requests */
	atomic64_t v4_destroy_sfe_rule_req;			/**< No of v4 sfe destroy rule requests */
	atomic64_t v4_destroy_rule_sfe_success;			/**< No of v4 successful sfe destroy rule requests */
	atomic64_t v4_destroy_rule_req_fail_sfe_fail;		/**< No of v4 failed sfe destroy rule requests */

	/* v6 generic stats */
	atomic64_t v6_create_rule_req_fail_no_mem;		/**< No of v6 create rule failed with no memory */
	atomic64_t v6_create_rule_flow_flag_mismatch;		/**< No of v6 create rule failed due to flow flag mismatched */
	atomic64_t v6_create_rule_bridge_flow;			/**< No of v6 create rule for bridged flow */
	atomic64_t v6_create_rule_routed_flow;			/**< No of v6 create rule for routed flow */
	atomic64_t v6_validate_tuple_mismatch;			/**< No of v6 create rule failed due to tuple mismatch */
	atomic64_t v6_validate_tuple_type_3;			/**< No of v6 create rule tuple type 3 */
	atomic64_t v6_validate_tuple_type_4;			/**< No of v6 create rule tuple type 4 */
	atomic64_t v6_validate_tuple_type_5;			/**< No of v6 create rule tuple type 5 */
	atomic64_t v6_validate_tuple_ip_addr_mismatch;		/**< No of v6 create rule failed due to ip addr mismatch */
	atomic64_t v6_validate_tuple_ident_mismatch;		/**< No of v6 create rule failed due to ident mismatch */
	atomic64_t v6_validate_tuple_proto_mismatch;		/**< No of v6 create rule failed due to proto mismatch */
	atomic64_t v6_validate_tuple_type_invalid;		/**< No of v6 create rule failed due to invalid tuple type */
	atomic64_t v6_create_validate_tuple_mismatch;		/**< No of v6 create rule failed due to tuple mismatch */
	atomic64_t v6_create_validate_tuple_type_3;		/**< No of v6 create rule tuple type 3 */
	atomic64_t v6_create_validate_tuple_type_4;		/**< No of v6 create rule tuple type 4 */
	atomic64_t v6_create_validate_tuple_type_5;		/**< No of v6 create rule tuple type 5 */
	atomic64_t v6_create_validate_tuple_simul_snat_dnat;	/**< No of v6 create rule failed due to simul snat and dnat */
	atomic64_t v6_create_validate_tuple_invalid_snat_ip;	/**< No of v6 create rule failed due to invalid snat ip */
	atomic64_t v6_create_validate_tuple_invalid_dnat_ip;	/**< No of v6 create rule failed due to invalid dnat ip */
	atomic64_t v6_create_validate_tuple_invalid_snat_port;	/**< No of v6 create rule failed due to invalid snat port */
	atomic64_t v6_create_validate_tuple_invalid_dnat_port;	/**< No of v6 create rule failed due to invalid dnat port */
	atomic64_t v6_create_validate_tuple_ip_addr_mismatch;	/**< No of v6 create rule failed due to ip addr mismatch */
	atomic64_t v6_create_validate_tuple_ident_mismatch;	/**< No of v6 create rule failed due to ident mismatch */
	atomic64_t v6_create_validate_tuple_proto_mismatch;	/**< No of v6 create rule failed due to proto mismatch */
	atomic64_t v6_create_validate_tuple_type_unsupported;	/**< No of v6 create rule failed due to unsupported tuple type */
	atomic64_t v6_create_validate_tuple_type_invalid;	/**< No of v6 create rule failed due to invalid tuple type */
	atomic64_t v6_create_validate_tuple_invalid_nat_bridge;	/**< No of v6 create rule failed due to unsupported nat + bridge */
	atomic64_t v6_create_validate_tuple_invalid_net_dev;	/**< No of v6 create rule failed due to invalid net device */
	atomic64_t v6_destroy_validate_tuple_mismatch;		/**< No of v6 destroy rule failed due to tuple mismatch */
	atomic64_t v6_destroy_validate_tuple_type_3;		/**< No of v6 destroy rule tuple type 3 */
	atomic64_t v6_destroy_validate_tuple_type_4;		/**< No of v6 destroy rule tuple type 4 */
	atomic64_t v6_destroy_validate_tuple_type_5;		/**< No of v6 destroy rule tuple type 5 */
	atomic64_t v6_destroy_validate_tuple_type_invalid;	/**< No of v6 destroy rule failed due to invalid tuple type */
	atomic64_t v6_destroy_validate_tuple_ip_addr_mismatch;	/**< No of v6 destroy rule failed due to ip addr mismatch */
	atomic64_t v6_destroy_validate_tuple_ident_mismatch;	/**< No of v6 destroy rule failed due to ident mismatch */
	atomic64_t v6_destroy_validate_tuple_proto_mismatch;	/**< No of v6 destroy rule failed due to proto mismatch */
	atomic64_t v6_destroy_rule_req_fail_no_mem;		/**< No of v6 destroy rule failed with no memory */
	atomic64_t v6_stats_sync_callback_not_registered;	/**< No of v6 sync requests with unregistered cb */
	atomic64_t v6_stats_sync_invalid_msg_type;		/**< No of v6 sync requests with inavalid msg */
	atomic64_t v6_get_stats_fail;				/**< No of v6 get stats failed */
	atomic64_t v6_get_stats_tuple_type_unsupported;		/**< No of v6 get stats failed due to unspported tuple type */

	/* v6 PPE stats */
	atomic64_t v6_create_ppe_rule_req;			/**< No of v6 ppe create rule requests */
	atomic64_t v6_create_rule_invalid_ppe_tx_if;		/**< No of v6 create rule failed due to invalid ppe tx if */
	atomic64_t v6_create_rule_invalid_ppe_rx_if;		/**< No of v6 create rule failed due to invalid ppe rx if */
	atomic64_t v6_create_rule_invalid_ppe_top_tx_if;	/**< No of v6 create rule failed due to invalid ppe top tx if */
	atomic64_t v6_create_rule_invalid_ppe_top_rx_if;	/**< No of v6 create rule failed due to invalid ppe top rx if */
	atomic64_t v6_create_rule_ppe_success;			/**< No of v6 successful ppe create rule requests */
	atomic64_t v6_create_rule_req_fail_ppe_fail;		/**< No of v6 failed ppe create rule requests */
	atomic64_t v6_destroy_ppe_rule_req;			/**< No of v6 ppe destroy rule requests */
	atomic64_t v6_destroy_rule_ppe_success;			/**< No of v6 successful ppe destroy rule requests */
	atomic64_t v6_destroy_rule_req_fail_ppe_fail;		/**< No of v6 failed ppe destroy rule requests */
	atomic64_t v6_get_stats_failed;				/**< No of v6 get stats failed requests */

	/* v6 SFE stats */
	atomic64_t v6_create_sfe_rule_req;			/**< No of v4 sfe create rule requests */
	atomic64_t v6_create_rule_sfe_success;			/**< No of v6 successful sfe create rule requests */
	atomic64_t v6_create_rule_req_fail_sfe_fail;		/**< No of v6 failed sfe create rule requests */
	atomic64_t v6_destroy_sfe_rule_req;			/**< No of v6 sfe destroy rule requests */
	atomic64_t v6_destroy_rule_sfe_success;			/**< No of v6 successful sfe destroy rule requests */
	atomic64_t v6_destroy_rule_req_fail_sfe_fail;		/**< No of v6 failed sfe destroy rule requests */

	/* common stats */
	atomic64_t validate_vlan_inner_mismatch;		/**< No of v4/v6 create rule failed due to inner vlan mismatch */
	atomic64_t validate_vlan_outer_mismatch;		/**< No of v4/v6 create rule failed due to outer vlan mismatch */
	atomic64_t validate_unsupported_tuple_type;		/**< No of v4/v6 failure due to unsupported tuple type */
	atomic64_t dscp_priority_add_req;			/**< No of add dscp priority requests */
	atomic64_t dscp_priority_del_req;			/**< No of del dscp priority requests */
	atomic64_t dscp_priority_add_req_fail_inval_core_id;	/**< No of add dscp priority request fail due to invalid core id */
	atomic64_t dscp_priority_add_req_fail_inval_pri_queue;	/**< No of add dscp priority request fail due to invalid priority queue */
	atomic64_t dscp_priority_add_req_fail_no_wan_dev;	/**< No of add dscp priority request fail due to no WAN dev */
	atomic64_t dscp_priority_add_req_rule_create_fail;	/**< No of add dscp priority request fail due to rule create fail */
	atomic64_t dscp_priority_del_req_rule_destroy_fail;	/**< No of del dscp priority request fail due to rule destroy fail */
};

/*
 * netfn_flowmgr_stats_dec()
 *	Decrement stats counter.
 */
static inline void netfn_flowmgr_stats_dec(atomic64_t *stat)
{
	atomic64_dec(stat);
}

/*
 * netfn_flowmgr_stats_inc()
 *	Increment stats counter.
 */
static inline void netfn_flowmgr_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

int netfn_flowmgr_stats_debugfs_init(void);
void netfn_flowmgr_stats_debugfs_exit(void);

#endif /*__NETFN_FLOWMGR_STATS_H */
