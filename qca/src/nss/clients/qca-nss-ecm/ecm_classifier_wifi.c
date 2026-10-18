/*
 ***************************************************************************
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 ***************************************************************************
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/netfilter_bridge.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/ip.h>
#include <linux/inet.h>

/*
 * Debug output levels
 * 0 = OFF
 * 1 = ASSERTS / ERRORS
 * 2 = 1 + WARN
 * 3 = 2 + INFO
 * 4 = 3 + TRACE
 */
#define DEBUG_LEVEL ECM_CLASSIFIER_WIFI_DEBUG_LEVEL

#include "ecm_types.h"
#include "ecm_db_types.h"
#include "ecm_state.h"
#include "ecm_tracker.h"
#include "ecm_classifier.h"
#include "ecm_front_end_types.h"
#include "ecm_db.h"
#include "ecm_interface.h"
#include "ecm_front_end_ipv4.h"
#include "ecm_front_end_ipv6.h"
#include "ecm_front_end_common.h"
#include "ecm_classifier_wifi.h"
#include "ecm_classifier_wifi_public.h"

/*
 * Magic numbers
 */
#define ECM_CLASSIFIER_WIFI_INSTANCE_MAGIC 0xFE35

/*
 * Default path for sysctl
 */
#define ECM_CLASSIFIER_WIFI_PATH "net/ecm/ecm_classifier_wifi"

/*
 * struct ecm_classifier_wifi_instance
 */
struct ecm_classifier_wifi_instance {
	struct ecm_classifier_instance base;			/* Base type */

	struct ecm_classifier_wifi_instance *next;		/* Next classifier state instance (for accouting and reporting purposes) */
	struct ecm_classifier_wifi_instance *prev;		/* Prev classifier state instance (for accouting and reporting purposes) */

	uint32_t ci_serial;					/* RO: Serial of the connection */
	uint32_t pcp[ECM_CONN_DIR_MAX];				/* PCP values for the connections */
	bool packet_seen[ECM_CONN_DIR_MAX];                     /* Per-direction packet seen flag */
	struct ecm_classifier_process_response process_response;/* Last process response computed */

	int refs;						/* Integer to trap we never go negative */

	uint8_t wifi_qm_type;					/* Wi-Fi QoS management type like SCS/MSCS */
	uint8_t wifi_qm_id;						/* Wi-Fi QoS management id like SCS id */
#if (DEBUG_LEVEL > 0)
	uint16_t magic;
#endif
};

/*
 * struct ecm_classifier_wifi_fill_metadata_params
 *	Parameters for filling WiFi metadata
 */
struct ecm_classifier_wifi_fill_metadata_params {
	uint32_t flow_wifi_metadata;
	uint32_t flow_wifi_ds_node_id;
	uint32_t return_wifi_metadata;
	uint32_t return_wifi_ds_node_id;
	ecm_tracker_sender_type_t sender;
	uint16_t flow_ast_info;
	uint16_t return_ast_info;
	uint16_t return_peer_id;
	uint16_t flow_peer_id;
	uint8_t flow_stream_id;
	uint8_t return_stream_id;
};

/*
 * Operational control
 */
static uint32_t ecm_classifier_wifi_enabled = 1;			/* Operational behaviour */

/*
 * Management thread control
 */
static bool ecm_classifier_wifi_terminate_pending = false;	/* True when the user wants us to terminate */

/*
 * Locking of the classifier structures
 */
static DEFINE_SPINLOCK(ecm_classifier_wifi_lock);		/* Protect SMP access. */

/*
 * List of our classifier instances
 */
static struct ecm_classifier_wifi_instance *ecm_classifier_wifi_instances = NULL;
								/* list of all active instances */
static int ecm_classifier_wifi_count = 0;			/* Tracks number of instances allocated */

/*
 * Callback structure to support fetching wifi metadata
 */
static struct ecm_classifier_wifi_callbacks ecm_wifi;

/*
 * Debugfs dentry object.
 */
static struct dentry *ecm_classifier_wifi_dentry;

static struct ctl_table_header *ecm_classifier_wifi_ctl_table_header; /* Sysctl table header */

/*
 * ecm_classifier_wifi_type_get()
 *	Get type of classifier this is
 */
static ecm_classifier_type_t ecm_classifier_wifi_type_get(struct ecm_classifier_instance *ci)
{
	struct ecm_classifier_wifi_instance *cwifii;
	cwifii = (struct ecm_classifier_wifi_instance *)ci;

	DEBUG_CHECK_MAGIC(cwifii, ECM_CLASSIFIER_WIFI_INSTANCE_MAGIC, "%px: magic failed\n", cwifii);
	return ECM_CLASSIFIER_TYPE_WIFI;
}

/*
 * ecm_classifier_wifi_reclassify_allowed()
 *	Indicate if reclassify is allowed
 */
static bool ecm_classifier_wifi_reclassify_allowed(struct ecm_classifier_instance *ci)
{
	struct ecm_classifier_wifi_instance *cwifii;
	cwifii = (struct ecm_classifier_wifi_instance *)ci;
	DEBUG_CHECK_MAGIC(cwifii, ECM_CLASSIFIER_WIFI_INSTANCE_MAGIC, "%px: magic failed\n", cwifii);

	return true;
}

/*
 * ecm_classifier_wifi_reclassify()
 *	Reclassify
 */
static void ecm_classifier_wifi_reclassify(struct ecm_classifier_instance *ci)
{
	struct ecm_classifier_wifi_instance *cwifii;
	cwifii = (struct ecm_classifier_wifi_instance *)ci;
	DEBUG_CHECK_MAGIC(cwifii, ECM_CLASSIFIER_WIFI_INSTANCE_MAGIC, "%px: magic failed\n", cwifii);

	/*
	 * Revert back to MAYBE relevant - we will evaluate when we get the next process() call.
	 */
	spin_lock_bh(&ecm_classifier_wifi_lock);
	cwifii->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_MAYBE;
	spin_unlock_bh(&ecm_classifier_wifi_lock);
}

/*
 * ecm_classifier_wifi_fill_metadata()
 *	Save the wifi metadata in the classifier instance.
 */
static void ecm_classifier_wifi_fill_metadata(struct ecm_classifier_wifi_instance *cwifii,
					       struct ecm_classifier_wifi_fill_metadata_params *params)
{
	spin_lock_bh(&ecm_classifier_wifi_lock);
	if (params->sender == ECM_TRACKER_SENDER_TYPE_SRC)
	{
		cwifii->process_response.process_actions |= (ECM_CLASSIFIER_PROCESS_ACTION_MARK | ECM_CLASSIFIER_PROCESS_ACTION_WIFI_TAG);
		cwifii->process_response.flow_mark = params->flow_wifi_metadata;
		cwifii->process_response.flow_wifi_ds_node_id = params->flow_wifi_ds_node_id;
		cwifii->process_response.return_mark = params->return_wifi_metadata;
		cwifii->process_response.return_wifi_ds_node_id = params->return_wifi_ds_node_id;

		/* Store flow WiFi metadata fields */
		cwifii->process_response.flow_ast_info = params->flow_ast_info;
		cwifii->process_response.flow_stream_id = params->flow_stream_id;
		cwifii->process_response.flow_peer_id = params->flow_peer_id;
		cwifii->process_response.return_ast_info = params->return_ast_info;
		cwifii->process_response.return_stream_id = params->return_stream_id;
		cwifii->process_response.return_peer_id = params->return_peer_id;
	} else {
		cwifii->process_response.process_actions |= (ECM_CLASSIFIER_PROCESS_ACTION_MARK | ECM_CLASSIFIER_PROCESS_ACTION_WIFI_TAG);
		cwifii->process_response.flow_mark = params->return_wifi_metadata;
		cwifii->process_response.flow_wifi_ds_node_id = params->return_wifi_ds_node_id;
		cwifii->process_response.return_mark = params->flow_wifi_metadata;
		cwifii->process_response.return_wifi_ds_node_id = params->flow_wifi_ds_node_id;

		/* Store return WiFi metadata fields (swapped for DEST sender) */
		cwifii->process_response.flow_ast_info = params->return_ast_info;
		cwifii->process_response.flow_stream_id = params->return_stream_id;
		cwifii->process_response.flow_peer_id = params->return_peer_id;
		cwifii->process_response.return_ast_info = params->flow_ast_info;
		cwifii->process_response.return_stream_id = params->flow_stream_id;
		cwifii->process_response.return_peer_id = params->flow_peer_id;
	}

	spin_unlock_bh(&ecm_classifier_wifi_lock);
}

/*
 * ecm_classifier_wifi_fill_pcp()
 *	Save the PCP value in the classifier instance.
 */
static void ecm_classifier_wifi_fill_pcp(struct ecm_classifier_wifi_instance *cwifii,
		 ecm_tracker_sender_type_t sender, struct sk_buff *skb)
{
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		cwifii->pcp[ECM_CONN_DIR_FLOW] = skb->priority;
		cwifii->packet_seen[ECM_CONN_DIR_FLOW] = true;
	} else {
		cwifii->pcp[ECM_CONN_DIR_RETURN] = skb->priority;
		cwifii->packet_seen[ECM_CONN_DIR_RETURN] = true;
	}
}

/*
 * ecm_classifier_wifi_process()
 *	Process new data for connection
 */
static void ecm_classifier_wifi_process(struct ecm_classifier_instance *aci, ecm_tracker_sender_type_t sender,
						struct ecm_tracker_ip_header *ip_hdr, struct sk_buff *skb,
						struct ecm_classifier_process_response *process_response)
{
	struct ecm_classifier_wifi_instance *cwifii;
	ecm_classifier_relevence_t relevance;
	struct ecm_db_connection_instance *ci = NULL;
	struct ecm_front_end_connection_instance *feci;
	ecm_front_end_acceleration_mode_t accel_mode;
	uint32_t became_relevant = 0;
	uint32_t wifi_flow_metadata = 0;
	uint32_t wifi_return_metadata = 0;
	uint32_t return_ds_metadata = ECM_CLASSIFIER_WIFI_INVALID_DS_NODE_ID;
	uint32_t flow_ds_metadata = ECM_CLASSIFIER_WIFI_INVALID_DS_NODE_ID;
	int protocol;
	uint8_t dmac[ETH_ALEN];
	uint8_t smac[ETH_ALEN];
	struct net_device *src_dev = NULL;
	struct net_device *dest_dev = NULL;
	struct ecm_classifier_wifi_metadata wifi_metadata_info = {0};
	uint8_t flow_hlos_tid_override = ECM_CLASSIFIER_WIFI_INVALID_HLOS_TID_OVERRIDE;
	uint8_t return_hlos_tid_override = ECM_CLASSIFIER_WIFI_INVALID_HLOS_TID_OVERRIDE;
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;
	uint64_t slow_pkts = 0;
	uint16_t flow_ast_info = ECM_CLASSIFIER_WIFI_INVALID_AST_INFO;
	uint16_t return_ast_info = ECM_CLASSIFIER_WIFI_INVALID_AST_INFO;
	uint8_t flow_stream_id = ECM_CLASSIFIER_WIFI_INVALID_STREAM_ID;
	uint8_t return_stream_id = ECM_CLASSIFIER_WIFI_INVALID_STREAM_ID;
	uint16_t flow_peer_id = ECM_CLASSIFIER_WIFI_INVALID_PEER_ID;
	uint16_t return_peer_id = ECM_CLASSIFIER_WIFI_INVALID_PEER_ID;
	struct ecm_classifier_wifi_fill_metadata_params metadata_params;

	cwifii = (struct ecm_classifier_wifi_instance *)aci;
	DEBUG_CHECK_MAGIC(cwifii, ECM_CLASSIFIER_WIFI_INSTANCE_MAGIC, "%px: magic failed\n", cwifii);

	spin_lock_bh(&ecm_classifier_wifi_lock);
	relevance = cwifii->process_response.relevance;

	/*
	 * Are we relevant?
	 * If the classifier is set as ir-relevant to the connection,
	 * the process response of the classifier instance was set from
	 * the earlier packets.
	 */
	if (relevance == ECM_CLASSIFIER_RELEVANCE_NO) {
		/*
		 * Lock still held
		 */
		goto process_wifi_classifier_out;
	}

	/*
	 * Yes or maybe relevant.
	 *
	 * Need to decide our relevance to this connection.
	 * We are only relevant to a connection if:
	 * We are enabled.
	 * Any other condition and we are not and will stop analysing this connection.
	 */
	if (!ecm_classifier_wifi_enabled) {
		/*
		 * Lock still held
		 */
		DEBUG_WARN("%px: classifier not enabled serial:%u \n", cwifii, cwifii->ci_serial);
		cwifii->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_NO;
		goto process_wifi_classifier_out;
	}

	spin_unlock_bh(&ecm_classifier_wifi_lock);

	if (!ecm_wifi.get_wifi_metadata) {
		spin_lock_bh(&ecm_classifier_wifi_lock);
		DEBUG_WARN("%px: No callback registered to get metadata serial:%u \n", cwifii, cwifii->ci_serial);
		cwifii->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_NO;
		goto process_wifi_classifier_out;
	}

	/*
	 * Can we accelerate?
	 */
	ci = ecm_db_connection_serial_find_and_ref(cwifii->ci_serial);
	if (!ci) {
		DEBUG_TRACE("%px: No ci found for serial:%u\n", cwifii, cwifii->ci_serial);
		spin_lock_bh(&ecm_classifier_wifi_lock);
		cwifii->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_NO;
		goto process_wifi_classifier_out;
	}

	feci = ecm_db_connection_front_end_get_and_ref(ci);
	accel_mode = ecm_front_end_connection_accel_state_get(feci);
	slow_pkts = ecm_front_end_get_slow_packet_count(feci);
	ecm_front_end_connection_deref(feci);
	ecm_db_connection_deref(ci);

	if (ECM_FRONT_END_ACCELERATION_NOT_POSSIBLE(accel_mode)) {
		spin_lock_bh(&ecm_classifier_wifi_lock);
		DEBUG_TRACE("%px: Accel not possible serial:%u\n", cwifii, cwifii->ci_serial);
		goto process_wifi_classifier_out;
	}

	/*
	 * Based on direction store the smac and dmac
	 */
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		DEBUG_TRACE("%px: sender is SRC\n", aci);
		ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_FROM, smac);
		ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_TO, dmac);
	} else {
		DEBUG_TRACE("%px: sender is DEST\n", aci);
		ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_TO, smac);
		ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_FROM, dmac);
	}

	protocol = ecm_db_connection_protocol_get(ci);
	if ((protocol != IPPROTO_UDP) && (protocol != IPPROTO_TCP) && (protocol != IPPROTO_GRE)) {
#if defined(ECM_FRONT_END_ESP_SPI_PASSTHROUGH)
		if ((protocol != IPPROTO_ESP) || !ecm_front_end_esp_spi_passthrough_enable) {
			spin_lock_bh(&ecm_classifier_wifi_lock);
			cwifii->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_NO;
			DEBUG_TRACE("%px: Invalid protocol 0x:%x serial:%u\n", cwifii, protocol, cwifii->ci_serial);
			goto process_wifi_classifier_out;
		}
#else
		spin_lock_bh(&ecm_classifier_wifi_lock);
		cwifii->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_NO;
		DEBUG_TRACE("%px: Invalid protocol 0x:%x serial:%u\n", cwifii, protocol, cwifii->ci_serial);
		goto process_wifi_classifier_out;
#endif
	}

	/*
	 * Fetch the src and dest net devices to check their interfaces.
	 */
	ecm_db_netdevs_get_and_hold(ci, sender, &src_dev, &dest_dev);

	if (dest_dev) {
		wifi_metadata_info.valid_params_flag |= ECM_CLASSIFIER_WIFI_MLO_PARAM_VALID;
		wifi_metadata_info.wifi_mdata.dest_dev = dest_dev;
		wifi_metadata_info.wifi_mdata.dest_mac = dmac;

		wifi_flow_metadata = ecm_wifi.get_wifi_metadata(&wifi_metadata_info);
		flow_ds_metadata = wifi_metadata_info.wifi_mdata.out_ppe_ds_node_id;
		flow_hlos_tid_override = wifi_metadata_info.wifi_mdata.hlos_tid_override;

		if (wifi_metadata_info.wifi_mdata.out_ast_valid) {
			flow_ast_info = wifi_metadata_info.wifi_mdata.out_ast_info;
			flow_peer_id = wifi_metadata_info.wifi_mdata.out_peer_id;
		}
	}

	memset(&wifi_metadata_info, 0, sizeof(struct ecm_classifier_wifi_metadata));

	if (src_dev) {
		wifi_metadata_info.valid_params_flag |= ECM_CLASSIFIER_WIFI_MLO_PARAM_VALID;
		wifi_metadata_info.wifi_mdata.dest_dev = src_dev;
		wifi_metadata_info.wifi_mdata.dest_mac = smac;

		wifi_return_metadata = ecm_wifi.get_wifi_metadata(&wifi_metadata_info);
		return_ds_metadata = wifi_metadata_info.wifi_mdata.out_ppe_ds_node_id;
		return_hlos_tid_override = wifi_metadata_info.wifi_mdata.hlos_tid_override;

		if (wifi_metadata_info.wifi_mdata.out_ast_valid) {
			return_ast_info = wifi_metadata_info.wifi_mdata.out_ast_info;
			return_peer_id = wifi_metadata_info.wifi_mdata.out_peer_id;
		}
	}

	/*
	 * Make this classifier not relevant if neither DS node id nor wifi metadata is
	 * valid in any of the direction.
	 */
	if (!wifi_flow_metadata && !wifi_return_metadata && (flow_ds_metadata == ECM_CLASSIFIER_WIFI_INVALID_DS_NODE_ID)
			&& (return_ds_metadata == ECM_CLASSIFIER_WIFI_INVALID_DS_NODE_ID)) {
		spin_lock_bh(&ecm_classifier_wifi_lock);
		cwifii->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_NO;
		DEBUG_TRACE("%px: WiFi classifier wifi_flow_mdata:0x%x Return_wifi_mdata:0x%x flow_ds_mdata:0x%x return_ds_mdata:0x%x serial:%u\n",
				cwifii, wifi_flow_metadata, wifi_return_metadata, flow_ds_metadata, return_ds_metadata, cwifii->ci_serial);
		goto process_wifi_classifier_out;
	}

	metadata_params.flow_wifi_metadata = wifi_flow_metadata;
	metadata_params.flow_wifi_ds_node_id = flow_ds_metadata;
	metadata_params.return_wifi_metadata = wifi_return_metadata;
	metadata_params.return_wifi_ds_node_id = return_ds_metadata;
	metadata_params.sender = sender;
	metadata_params.flow_ast_info = flow_ast_info;
	metadata_params.flow_stream_id = flow_stream_id;
	metadata_params.flow_peer_id = flow_peer_id;
	metadata_params.return_ast_info = return_ast_info;
	metadata_params.return_stream_id = return_stream_id;
	metadata_params.return_peer_id = return_peer_id;

	ecm_classifier_wifi_fill_metadata(cwifii, &metadata_params);

	/*
	 * We are relevant to the connection.
	 * Set the process response to its default value, that is, to
	 * allow the acceleration.
	 */
	became_relevant = ecm_db_time_get();

	spin_lock_bh(&ecm_classifier_wifi_lock);
	cwifii->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_YES;
	cwifii->process_response.became_relevant = became_relevant;

	cwifii->process_response.process_actions |= ECM_CLASSIFIER_PROCESS_ACTION_ACCEL_MODE;
	cwifii->process_response.accel_mode = ECM_CLASSIFIER_ACCELERATION_MODE_ACCEL;

	if (flow_hlos_tid_override || return_hlos_tid_override) {

		/*
		 * Get the flow tag from skb priority only if hlos_tid_override is set.
		 * These are used during certification cases where different dscp/tid values
		 * are passed to the client.
		 */
		ct = nf_ct_get(skb, &ctinfo);
		if (!ct) {
			cwifii->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_NO;
			DEBUG_TRACE("%px: WiFi classifier Conntrack is not established serial:%u\n", cwifii, cwifii->ci_serial);
			goto process_wifi_classifier_out;
		}

		if (ecm_classifier_accel_delay_pkts) {

			/*
			 * Store the PCP value in the classifier instance and
			 * deny acceleration until specified number of slow packets are seen.
			 */
			ecm_classifier_wifi_fill_pcp(cwifii, sender, skb);

			if ((ecm_classifier_accel_delay_pkts == 1) || (slow_pkts < ecm_classifier_accel_delay_pkts)) {
				DEBUG_TRACE("%px: accel_delay_pkts: %d slow_pkts: %llu accel is not allowed yet\n",
						cwifii, ecm_classifier_accel_delay_pkts, slow_pkts);
				cwifii->process_response.accel_mode = ECM_CLASSIFIER_ACCELERATION_MODE_NO;
				goto process_wifi_classifier_out;
			}
		}

		/*
		 * Store the skb priority in flow/return PCP value
		 * of classifier instance if accel delay is not set.
		 */
		cwifii->pcp[ECM_CONN_DIR_FLOW] = skb->priority;
		cwifii->pcp[ECM_CONN_DIR_RETURN] = skb->priority;

		DEBUG_TRACE("%px: Protocol: %d, Flow Priority: %d, Return priority: %d, sender: %d\n",
				cwifii, protocol, cwifii->pcp[ECM_CONN_DIR_FLOW],
				cwifii->pcp[ECM_CONN_DIR_RETURN], sender);

		if (((sender == ECM_TRACKER_SENDER_TYPE_SRC) && (IP_CT_DIR_ORIGINAL == CTINFO2DIR(ctinfo))) ||
				((sender == ECM_TRACKER_SENDER_TYPE_DEST) && (IP_CT_DIR_REPLY == CTINFO2DIR(ctinfo)))) {
			cwifii->process_response.flow_qos_tag = cwifii->pcp[ECM_CONN_DIR_FLOW];
			cwifii->process_response.return_qos_tag = cwifii->pcp[ECM_CONN_DIR_RETURN];
		} else {
			cwifii->process_response.flow_qos_tag = cwifii->pcp[ECM_CONN_DIR_RETURN];
			cwifii->process_response.return_qos_tag = cwifii->pcp[ECM_CONN_DIR_FLOW];
		}

		cwifii->process_response.process_actions |= ECM_CLASSIFIER_PROCESS_ACTION_HLOS_TID_VALID;
		cwifii->process_response.process_actions |= ECM_CLASSIFIER_PROCESS_ACTION_QOS_TAG;
	}

	DEBUG_TRACE("%px: flow mark: %x, return mark: %x, flow DS node id %d, return DS node id %d, sender %d flow_hlos_tid_override: %d, return_hlos_tid_override: %d skb->priority:%d serial:%u\n",
			cwifii, cwifii->process_response.flow_mark, cwifii->process_response.return_mark,
			cwifii->process_response.flow_wifi_ds_node_id, cwifii->process_response.return_wifi_ds_node_id,
			sender, flow_hlos_tid_override, return_hlos_tid_override, skb->priority, cwifii->ci_serial);

process_wifi_classifier_out:

	/*
	 * Return our process response
	 * Release the source and destination dev count
	 */
	dev_put(src_dev);
	dev_put(dest_dev);

	*process_response = cwifii->process_response;
	spin_unlock_bh(&ecm_classifier_wifi_lock);
}

#ifdef ECM_STATE_OUTPUT_ENABLE
/*
 * ecm_classifier_wifi_state_get()
 *	Return state
 */
static int ecm_classifier_wifi_state_get(struct ecm_classifier_instance *ci, struct ecm_state_file_instance *sfi)
{
	int result;
	struct ecm_classifier_wifi_instance *cwifii;
	struct ecm_classifier_process_response process_response;
	uint32_t flow_wifi_ds_node_id;
	uint32_t return_wifi_ds_node_id;
	uint8_t wifi_qm_id, wifi_qm_type;

	cwifii = (struct ecm_classifier_wifi_instance *)ci;
	DEBUG_CHECK_MAGIC(cwifii, ECM_CLASSIFIER_WIFI_INSTANCE_MAGIC, "%px: magic failed", cwifii);

	if ((result = ecm_state_prefix_add(sfi, "wifi"))) {
		return result;
	}

	spin_lock_bh(&ecm_classifier_wifi_lock);
	process_response = cwifii->process_response;
	flow_wifi_ds_node_id = cwifii->process_response.flow_wifi_ds_node_id;
	return_wifi_ds_node_id = cwifii->process_response.return_wifi_ds_node_id;
	wifi_qm_id = cwifii->wifi_qm_id;
	wifi_qm_type = cwifii->wifi_qm_type;
	spin_unlock_bh(&ecm_classifier_wifi_lock);

	if ((result = ecm_classifier_process_response_state_get(sfi, &process_response))) {
		return result;
	}

	if ((result = ecm_state_write(sfi, "flow_wifi_ds_node_id", "0x%x", flow_wifi_ds_node_id))) {
		return result;
	}
	if ((result = ecm_state_write(sfi, "return_wifi_ds_node_id", "0x%x", return_wifi_ds_node_id))) {
		return result;
	}

	if ((result = ecm_state_write(sfi, "wifi_qm_id", "%u", wifi_qm_id))) {
		return result;
	}

	if ((result = ecm_state_write(sfi, "wifi_qm_type", "%u", wifi_qm_type))) {
		return result;
	}

	return ecm_state_prefix_remove(sfi);
}
#endif

/*
 * ecm_classifier_wifi_sync_to_v4()
 *	Front end is pushing accel engine state to us
 */
static void ecm_classifier_wifi_sync_to_v4(struct ecm_classifier_instance *aci, struct ecm_classifier_rule_sync *sync)
{
}

/*
 * ecm_classifier_wifi_sync_from_v4()
 *	Front end is retrieving accel engine state from us
 */
static void ecm_classifier_wifi_sync_from_v4(struct ecm_classifier_instance *aci, struct ecm_classifier_rule_create *ecrc)
{
	struct ecm_classifier_wifi_flow_info *flow_info;

	if (!ecrc->flow_info_valid || !ecrc->flow_info) {
		return;
	}

	flow_info = ecrc->flow_info;

	if (!ecm_wifi.wifi_flowq_setup) {
		DEBUG_WARN("No callback registered for wifi queue setup\n");
		return;
	}

	ecm_wifi.wifi_flowq_setup(flow_info);
}

/*
 * ecm_classifier_wifi_sync_to_v6()
 *	Front end is pushing accel engine state to us
 */
static void ecm_classifier_wifi_sync_to_v6(struct ecm_classifier_instance *aci, struct ecm_classifier_rule_sync *sync)
{
}

/*
 * ecm_classifier_wifi_sync_from_v6()
 *	Front end is retrieving accel engine state from us
 */
static void ecm_classifier_wifi_sync_from_v6(struct ecm_classifier_instance *aci, struct ecm_classifier_rule_create *ecrc)
{
	struct ecm_classifier_wifi_flow_info *flow_info;

	if (!ecrc->flow_info_valid || !ecrc->flow_info) {
		return;
	}

	flow_info = ecrc->flow_info;

	if (!ecm_wifi.wifi_flowq_setup) {
		DEBUG_WARN("No callback registered for wifi queue setup\n");
		return;
	}

	ecm_wifi.wifi_flowq_setup(flow_info);
}

/*
 * ecm_classifier_wifi_last_process_response_get()
 *	Get result code returned by the last process call
 */
static void ecm_classifier_wifi_last_process_response_get(struct ecm_classifier_instance *ci,
							struct ecm_classifier_process_response *process_response)
{
	struct ecm_classifier_wifi_instance *cwifii;

	cwifii = (struct ecm_classifier_wifi_instance *)ci;
	DEBUG_CHECK_MAGIC(cwifii, ECM_CLASSIFIER_WIFI_INSTANCE_MAGIC, "%px: magic failed\n", cwifii);

	spin_lock_bh(&ecm_classifier_wifi_lock);
	*process_response = cwifii->process_response;
	spin_unlock_bh(&ecm_classifier_wifi_lock);
}

/*
 * ecm_classifier_wifi_ref()
 *	Ref
 */
static void ecm_classifier_wifi_ref(struct ecm_classifier_instance *ci)
{
	struct ecm_classifier_wifi_instance *cwifii;
	cwifii = (struct ecm_classifier_wifi_instance *)ci;

	DEBUG_CHECK_MAGIC(cwifii, ECM_CLASSIFIER_WIFI_INSTANCE_MAGIC, "%px: magic failed\n", cwifii);
	spin_lock_bh(&ecm_classifier_wifi_lock);
	cwifii->refs++;
	DEBUG_ASSERT(cwifii->refs > 0, "%px: ref wrap\n", cwifii);
	DEBUG_TRACE("%px: cwifii ref %d\n", cwifii, cwifii->refs);
	spin_unlock_bh(&ecm_classifier_wifi_lock);
}

/*
 * ecm_classifier_wifi_deref()
 *	Deref
 */
static int ecm_classifier_wifi_deref(struct ecm_classifier_instance *ci)
{
	struct ecm_classifier_wifi_instance *cwifii;
	cwifii = (struct ecm_classifier_wifi_instance *)ci;
	DEBUG_CHECK_MAGIC(cwifii, ECM_CLASSIFIER_WIFI_INSTANCE_MAGIC, "%px: magic failed\n", cwifii);
	spin_lock_bh(&ecm_classifier_wifi_lock);
	cwifii->refs--;
	DEBUG_ASSERT(cwifii->refs >= 0, "%px: refs wrapped\n", cwifii);
	DEBUG_TRACE("%px: Wi-Fi classifier deref %d\n", cwifii, cwifii->refs);
	if (cwifii->refs) {
		int refs = cwifii->refs;
		spin_unlock_bh(&ecm_classifier_wifi_lock);
		return refs;
	}

	/*
	 * Object to be destroyed
	 */
	ecm_classifier_wifi_count--;
	DEBUG_ASSERT(ecm_classifier_wifi_count >= 0, "%px: ecm_classifier_wifi_count wrap\n", cwifii);

	/*
	 * UnLink the instance from our list
	 */
	if (cwifii->next) {
		cwifii->next->prev = cwifii->prev;
	}

	if (cwifii->prev) {
		cwifii->prev->next = cwifii->next;
	} else {
		DEBUG_ASSERT(ecm_classifier_wifi_instances == cwifii, "%px: list bad %px\n", cwifii, ecm_classifier_wifi_instances);
		ecm_classifier_wifi_instances = cwifii->next;
	}
	cwifii->prev = NULL;
	cwifii->next = NULL;
	spin_unlock_bh(&ecm_classifier_wifi_lock);

	/*
	 * Final
	 */
	DEBUG_INFO("%px: Final Wi-Fi classifier instance\n", cwifii);
	kfree(cwifii);

	return 0;
}

/*
 * ecm_classifier_wifi_update()
 *	Update Wi-Fi classifier related information
 */
void ecm_classifier_wifi_update(struct ecm_classifier_instance *aci, enum ecm_rule_update_type type, void *arg)
{
	struct ecm_front_end_flowsawf_msg *msg = (struct ecm_front_end_flowsawf_msg *)arg;
	struct ecm_classifier_wifi_instance *cwifii;
	uint8_t wifi_qm_id, wifi_qm_type;

	if (type != ECM_RULE_UPDATE_TYPE_FLOWMARK_WIFI_QM) {
		DEBUG_WARN("%px: unsupported update type: %d\n", aci, type);
		return;
	}

	cwifii = (struct ecm_classifier_wifi_instance *)aci;
	wifi_qm_id = (msg->flow_mark >> 8) & 0xFF;

	/*
	 * QM type 0 : SCS, QM type 1 : MSCS
	 */
	wifi_qm_type = ((msg->flow_mark & ECM_INTERFACE_WIFI_QOS_TAG_MASK) == ECM_INTERFACE_WIFI_QOS_SCS_TAG) ? 0 : 1;

	spin_lock_bh(&ecm_classifier_wifi_lock);
	cwifii->wifi_qm_id = wifi_qm_id;
	cwifii->wifi_qm_type = wifi_qm_type;
	spin_unlock_bh(&ecm_classifier_wifi_lock);
	DEBUG_TRACE("aci=%px wifi classifier: set qm_id %d and qm_type : %d", aci, wifi_qm_id, wifi_qm_type);
}

/*
 * ecm_classifier_wifi_should_keep_connection()
 *	Check whether the connection needs to be maintained
 */
static void ecm_classifier_wifi_should_keep_connection(struct ecm_classifier_instance *aci,
						       struct ecm_db_connection_defunct_info *info)
{
	struct ecm_classifier_wifi_instance *cwifii;

	if (info->type != ECM_DB_CONNECTION_DEFUNCT_TYPE_SCS_MSCS_TEARDOWN) {
		/*
		 * Classifier does not care about the connection deletion.
		 */
		return;
	}

	/*
	 * In case of SCS, MSCS tear down event, defunct the connections related to the SCS/MSCS
	 */
	cwifii = (struct ecm_classifier_wifi_instance *)aci;

	spin_lock_bh(&ecm_classifier_wifi_lock);

	if (cwifii->wifi_qm_id == info->wifi_qm_id && cwifii->wifi_qm_type == info->wifi_qm_type) {
		info->should_keep_connection = false;
	} else {
		info->should_keep_connection = true;
	}

	spin_unlock_bh(&ecm_classifier_wifi_lock);
}

/*
 * ecm_classifier_wifi_enable_handler
 * 	Proc handler to enable or disable WIFI classifier
 */
static int ecm_classifier_wifi_enable_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer, size_t *lenp, loff_t *ppos)
{
	/*
	 * Usage:
	 *
	 * Enable WIFI classifier
	 * echo 1 > /proc/sys/net/ecm/ecm_classifier_wifi/enabled
	 *
	 * Disable WIFI classifier
	 * echo 0 >/proc/sys/net/ecm/ecm_classifier_wifi/enabled
	 *
	 * To read status:
	 * cat /proc/sys/net/ecm/ecm_classifier_wifi/enabled
	 */

	int ret;
	int current_val;

	/*
	 * Write the value with user input
	 */
	current_val = ecm_classifier_wifi_enabled;
	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		/*
		 * Return if failure or read operations
		 */
		return ret;
	}

	if ((ecm_classifier_wifi_enabled != 0) && (ecm_classifier_wifi_enabled != 1)) {
		ecm_classifier_wifi_enabled = current_val;
		DEBUG_ERROR("Invalid input, Valid input 0/1\n");
		return -EINVAL;
	}

	return ret;
}

static struct ctl_table ecm_classifier_wifi_ctl_table[] = {
	{
		.procname	= "enabled",
		.data		= &ecm_classifier_wifi_enabled,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &ecm_classifier_wifi_enable_handler,
	},
};

/*
 * ecm_classifier_wifi_instance_alloc()
 *	Allocate an instance of the wifi classifier
 */
struct ecm_classifier_wifi_instance *ecm_classifier_wifi_instance_alloc(struct ecm_db_connection_instance *ci)
{
	struct ecm_classifier_wifi_instance *cwifii;

	/*
	 * Allocate the instance
	 */
	cwifii = (struct ecm_classifier_wifi_instance *)kzalloc(sizeof(struct ecm_classifier_wifi_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!cwifii) {
		DEBUG_WARN("Failed to allocate wifi classifier instance\n");
		return NULL;
	}

	DEBUG_SET_MAGIC(cwifii, ECM_CLASSIFIER_WIFI_INSTANCE_MAGIC);
	cwifii->refs = 1;
	cwifii->base.process = ecm_classifier_wifi_process;
	cwifii->base.sync_from_v4 = ecm_classifier_wifi_sync_from_v4;
	cwifii->base.sync_to_v4 = ecm_classifier_wifi_sync_to_v4;
	cwifii->base.sync_from_v6 = ecm_classifier_wifi_sync_from_v6;
	cwifii->base.sync_to_v6 = ecm_classifier_wifi_sync_to_v6;
	cwifii->base.type_get = ecm_classifier_wifi_type_get;
	cwifii->base.last_process_response_get = ecm_classifier_wifi_last_process_response_get;
	cwifii->base.reclassify_allowed = ecm_classifier_wifi_reclassify_allowed;
	cwifii->base.reclassify = ecm_classifier_wifi_reclassify;
#ifdef ECM_STATE_OUTPUT_ENABLE
	cwifii->base.state_get = ecm_classifier_wifi_state_get;
#endif
	cwifii->base.ref = ecm_classifier_wifi_ref;
	cwifii->base.deref = ecm_classifier_wifi_deref;
	cwifii->base.update = ecm_classifier_wifi_update;
	cwifii->base.should_keep_connection = ecm_classifier_wifi_should_keep_connection;
	cwifii->ci_serial = ecm_db_connection_serial_get(ci);
	cwifii->process_response.process_actions = 0;
	cwifii->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_MAYBE;

	spin_lock_bh(&ecm_classifier_wifi_lock);

	/*
	 * Final check if we are pending termination
	 */
	if (ecm_classifier_wifi_terminate_pending) {
		spin_unlock_bh(&ecm_classifier_wifi_lock);
		DEBUG_INFO("%px: Terminating\n", ci);
		kfree(cwifii);
		return NULL;
	}

	/*
	 * Link the new instance into our list at the head
	 */
	cwifii->next = ecm_classifier_wifi_instances;
	if (ecm_classifier_wifi_instances) {
		ecm_classifier_wifi_instances->prev = cwifii;
	}
	ecm_classifier_wifi_instances = cwifii;

	/*
	 * Increment stats
	 */
	ecm_classifier_wifi_count++;
	DEBUG_ASSERT(ecm_classifier_wifi_count > 0, "%px: ecm_classifier_wifi_count wrap\n", cwifii);
	spin_unlock_bh(&ecm_classifier_wifi_lock);

	DEBUG_INFO("Wi-Fi instance alloc: %px\n", cwifii);
	return cwifii;
}

/*
 * ecm_classifier_wifi_callback_register()
 */
int ecm_classifier_wifi_callback_register(struct ecm_classifier_wifi_callbacks *wifi_cb)
{
	spin_lock_bh(&ecm_classifier_wifi_lock);
	if (ecm_wifi.get_wifi_metadata) {
		spin_unlock_bh(&ecm_classifier_wifi_lock);
		DEBUG_ERROR("Wifi classifier callbacks are registered\n");
		return -1;
	}

	if (ecm_wifi.wifi_flowq_setup) {
		spin_unlock_bh(&ecm_classifier_wifi_lock);
		DEBUG_ERROR("Wifi flow queue setup callback is already registered\n");
		return -1;
	}

	ecm_wifi.get_wifi_metadata = wifi_cb->get_wifi_metadata;
	ecm_wifi.wifi_flowq_setup = wifi_cb->wifi_flowq_setup;
	spin_unlock_bh(&ecm_classifier_wifi_lock);
	return 0;
}
EXPORT_SYMBOL(ecm_classifier_wifi_callback_register);

/*
 * ecm_classifier_wifi_callback_unregister()
 */
void ecm_classifier_wifi_callback_unregister(void)
{
	spin_lock_bh(&ecm_classifier_wifi_lock);
	ecm_wifi.get_wifi_metadata = NULL;
	ecm_wifi.wifi_flowq_setup = NULL;
	spin_unlock_bh(&ecm_classifier_wifi_lock);
}
EXPORT_SYMBOL(ecm_classifier_wifi_callback_unregister);

/*
 * ecm_classifier_wifi_init()
 */
int ecm_classifier_wifi_init(struct dentry *dentry)
{
	DEBUG_INFO("Wi-fi classifier Module init\n");

	/*
	 * Register sysctl table for WIFI classifier
	 */
	ecm_classifier_wifi_ctl_table_header = register_sysctl(ECM_CLASSIFIER_WIFI_PATH, ecm_classifier_wifi_ctl_table);
	if (!ecm_classifier_wifi_ctl_table_header) {
		DEBUG_ERROR("Failed to create ecm wifi directory in sysctl\n");
		return -1;
	}

	if (!ecm_debugfs_create_dir("ecm_classifier_wifi", dentry, &ecm_classifier_wifi_dentry)) {
		DEBUG_ERROR("Failed to create ecm wifi directory in debugfs\n");
		unregister_sysctl_table(ecm_classifier_wifi_ctl_table_header);
		return -1;
	}

	ecm_debugfs_create_u32("enabled", S_IRUGO | S_IWUSR, ecm_classifier_wifi_dentry,
					(u32 *)&ecm_classifier_wifi_enabled);

	return 0;
}

/*
 * ecm_classifier_wifi_exit()
 */
void ecm_classifier_wifi_exit(void)
{
	DEBUG_INFO("Wi-Fi classifier Module exit\n");

	spin_lock_bh(&ecm_classifier_wifi_lock);
	ecm_classifier_wifi_terminate_pending = true;
	spin_unlock_bh(&ecm_classifier_wifi_lock);

	/*
	 * Remove the debugfs files recursively.
	 */
	if (ecm_classifier_wifi_dentry) {
		ecm_debugfs_remove_recursive(ecm_classifier_wifi_dentry);
	}

	/*
	 * Unregister sysclt table header
	 */
	if (ecm_classifier_wifi_ctl_table_header) {
		unregister_sysctl_table(ecm_classifier_wifi_ctl_table_header);
	}
}
