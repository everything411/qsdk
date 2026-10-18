// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_ipa.h"
#include "../../../qcn_extns/ipa/dp_ipa.h"
#include "../../../fse.h"
#include "dp_ipa_fse.h"

extern bool ath12k_fse_enable;

/**
 * ath12k_dp_rx_ipa_fse_fill_flow_info() - Fill flow information from IPA FSE rule
 * @ab: ath12k base structure
 * @flow_info: Output flow information structure to be filled
 * @rule: Input IPA FSE rule containing flow tuple information
 * @operation: FSE operation type (add/delete)
 *
 * Converts IPA FSE rule tuple information into the internal rx_flow_info format.
 * Handles both IPv4 and IPv6 address families. Logs flow information for
 * debugging purposes.
 *
 * Context: Can be called in process context
 */
static void
ath12k_dp_rx_ipa_fse_fill_flow_info(struct ath12k_base *ab,
				    struct rx_flow_info *flow_info,
				    struct ipa_fse_rule *rule,
				    enum fse_operation operation)
{
	struct hal_flow_tuple_info *out_tuple = &flow_info->flow_tuple_info;
	struct ipa_fse_tuple *in_tuple = &rule->tuple;

	flow_info->ring_id = rule->rdi;
	out_tuple->l4_protocol = in_tuple->protocol;
	out_tuple->src_port = in_tuple->src_port;
	out_tuple->dest_port = in_tuple->dst_port;

	if (rule->flags & IPA_FSE_IPV4) {
		ath12k_ipa_info(ab,
				"%s IPv4 S_IP:%x,sPort:%u,D_IP:%x,dPort:%u,Proto:%d,flags:%d",
				fse_state_to_string(operation),
				in_tuple->ip.v4.src_ip,
				in_tuple->src_port,
				in_tuple->ip.v4.dst_ip,
				in_tuple->dst_port,
				in_tuple->protocol,
				rule->flags);

		flow_info->is_addr_ipv4 = 1;
		out_tuple->src_ip_31_0 = ntohl(in_tuple->ip.v4.src_ip);
		out_tuple->dest_ip_31_0 = ntohl(in_tuple->ip.v4.dst_ip);
	} else if (rule->flags & IPA_FSE_IPV6) {
		ath12k_ipa_info(ab,
				"%s IPv6 S_IP:%x:%x:%x:%x,sPort:%u,D_IP:%x:%x:%x:%x,dPort:%u,Proto:%d,flags:%d",
				fse_state_to_string(operation),
				in_tuple->ip.v6.src_ip[0], in_tuple->ip.v6.src_ip[1],
				in_tuple->ip.v6.src_ip[2], in_tuple->ip.v6.src_ip[3],
				in_tuple->src_port,
				in_tuple->ip.v6.dst_ip[0], in_tuple->ip.v6.dst_ip[1],
				in_tuple->ip.v6.dst_ip[2], in_tuple->ip.v6.dst_ip[3],
				in_tuple->dst_port,
				in_tuple->protocol,
				rule->flags);

		out_tuple->src_ip_31_0 = ntohl(in_tuple->ip.v6.src_ip[3]);
		out_tuple->src_ip_63_32 = ntohl(in_tuple->ip.v6.src_ip[2]);
		out_tuple->src_ip_95_64 = ntohl(in_tuple->ip.v6.src_ip[1]);
		out_tuple->src_ip_127_96 = ntohl(in_tuple->ip.v6.src_ip[0]);

		out_tuple->dest_ip_31_0 = ntohl(in_tuple->ip.v6.dst_ip[3]);
		out_tuple->dest_ip_63_32 = ntohl(in_tuple->ip.v6.dst_ip[2]);
		out_tuple->dest_ip_95_64 = ntohl(in_tuple->ip.v6.dst_ip[1]);
		out_tuple->dest_ip_127_96 = ntohl(in_tuple->ip.v6.dst_ip[0]);
	}
}

/**
 * ath12k_dp_rx_ipa_fse_get_ab() - Get ath12k base structure for IPA FSE
 *
 * Retrieves the ath12k_base of CUMAC by obtaining the hardware group
 * and extracting the base pointer from the datapath hardware group.
 * This is used for IPA flow search entry operations.
 *
 * Context: Process context
 * Return: Pointer to ath12k_base on success, NULL on failure
 */
static struct ath12k_base *
ath12k_dp_rx_ipa_fse_get_ab(void)
{
	struct ath12k_hw_group *ag;

	ag = ath12k_core_get_ag();
	if (!ag) {
		ath12k_ipa_err(NULL, "ag is NULL");
		return NULL;
	}

	if (!ag->dp_hw_grp) {
		ath12k_ipa_err(NULL, "dp_hw_grp is NULL");
		return NULL;
	}

	return ath12k_dp_get_ab_from_dp_hw_group(ag->dp_hw_grp);
}

/**
 * ath12k_dp_rx_ipa_fse_add_flow_entry() - Add IPA flow search entry
 * @rule: IPA FSE rule containing flow tuple and interface index
 *
 * Adds a flow search entry for IPA offload. Resolves the ath12k_base
 * structure from the interface index and programs the flow entry into
 * hardware for packet steering.
 *
 * Context: Process context
 * Return: 0 on success, -ve on failure
 */
int
ath12k_dp_rx_ipa_fse_add_flow_entry(struct ipa_fse_rule *rule)
{

	struct rx_flow_info flow_info = { 0 };
	struct ath12k_base *ab = ath12k_dp_rx_ipa_fse_get_ab();

	if(ath12k_cfg_get(ab, ATH12K_INI_DP_IPA_SFE_ENABLE)) {
		if (!ath12k_fse_enable) {
			ath12k_ipa_err(NULL, "fse is disabled");
			return -EOPNOTSUPP;
		}

		if (!rule) {
			ath12k_ipa_err(NULL, "rule is NULL");
			return -EINVAL;
		}

		if (!ab) {
			ath12k_ipa_err(NULL, "ab is NULL");
			return -ENODEV;
		}

		ath12k_dp_rx_ipa_fse_fill_flow_info(ab, &flow_info, rule,
							FSE_RULE_ADD);

		return ath12k_dp_rx_flow_add_entry(ab, &flow_info);
	} else {
		return 0;
	}
}
EXPORT_SYMBOL(ath12k_dp_rx_ipa_fse_add_flow_entry);

/**
 * ath12k_dp_rx_ipa_fse_del_flow_entry() - Delete IPA flow search entry
 * @rule: IPA FSE rule containing flow tuple and interface index
 *
 * Removes a flow search entry from hardware. Resolves the ath12k_base
 * structure from the interface index and deletes the corresponding flow
 * entry.
 *
 * Context: Process context
 * Return: 0 on success, -ve on failure
 */
int
ath12k_dp_rx_ipa_fse_del_flow_entry(struct ipa_fse_rule *rule)
{
	struct rx_flow_info flow_info = { 0 };
	struct ath12k_base *ab = ath12k_dp_rx_ipa_fse_get_ab();

	if(ath12k_cfg_get(ab, ATH12K_INI_DP_IPA_SFE_ENABLE)) {
		if (!ath12k_fse_enable) {
			ath12k_ipa_err(NULL, "fse is disabled");
			return -EOPNOTSUPP;
		}

		if (!rule) {
			ath12k_ipa_err(NULL, "rule is NULL");
			return -EINVAL;
		}

		if (!ab) {
			ath12k_ipa_err(NULL, "ab is NULL");
			return -ENODEV;
		}

		ath12k_dp_rx_ipa_fse_fill_flow_info(ab, &flow_info, rule,
						FSE_RULE_DELETE);

		return ath12k_dp_rx_flow_delete_entry(ab, &flow_info);
	} else {
		return 0;
	}
}
EXPORT_SYMBOL(ath12k_dp_rx_ipa_fse_del_flow_entry);
