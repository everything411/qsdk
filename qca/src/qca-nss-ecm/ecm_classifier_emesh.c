/*
 ***************************************************************************
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/netfilter/xt_dscp.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_dscpremark_ext.h>
#include <net/ip.h>
#include <linux/inet.h>
#include <sp_api.h>

/*
 * Debug output levels
 * 0 = OFF
 * 1 = ASSERTS / ERRORS
 * 2 = 1 + WARN
 * 3 = 2 + INFO
 * 4 = 3 + TRACE
 */
#define DEBUG_LEVEL ECM_CLASSIFIER_EMESH_DEBUG_LEVEL

#include "ecm_types.h"
#include "ecm_db_types.h"
#include "ecm_state.h"
#include "ecm_tracker.h"
#include "ecm_classifier.h"
#include "ecm_front_end_types.h"
#include "ecm_db.h"
#include "ecm_interface.h"
#include "ecm_classifier_emesh_public.h"
#include "ecm_front_end_ipv4.h"
#include "ecm_front_end_ipv6.h"
#include "ecm_front_end_common.h"

/*
 * Magic numbers
 */
#define ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC 0xFECA
#define ECM_CLASSIFIER_EMESH_SAWF_INVALID_SPI 0xff

/*
 * Latency parameter operation
 */
#define ECM_CLASSIFIER_EMESH_ADD_LATENCY_PARAMS 0x1
#define ECM_CLASSIFIER_EMESH_SUB_LATENCY_PARAMS 0x2

/*
 * Flag to enable SPM rule lookup
 */
#define ECM_CLASSIFIER_EMESH_ENABLE_SPM_RULE_LOOKUP 0x1
#define ECM_CLASSIFIER_EMESH_ENABLE_LATENCY_UPDATE 0x2

/*
 * SAWF information.
 */
#define ECM_CLASSIFIER_EMESH_SAWF_TAG_SHIFT             8
#define ECM_CLASSIFIER_EMESH_SAWF_SERVICE_CLASS_SHIFT   16
#define ECM_CLASSIFIER_EMESH_SAWF_VALID_TAG             0xAA
#define ECM_CLASSIFIER_EMESH_SAWF_INVALID_SERVICE_CLASS 0xff
#define ECM_CLASSIFIER_EMESH_SAWF_INVALID_MSDUQ         0xffff
#define ECM_CLASSIFIER_EMESH_SAWF_INVALID_RULE_LOOKUP   0xffffffff
#define ECM_CLASSIFIER_EMESH_SAWF_CAKE_HANDLE_SHIFT     16
#define ECM_CLASSIFIER_EMESH_SAWF_CAKE_INVALID_HANDLE   0xffff
#define ECM_CLASSIFIER_EMESH_SAWF_CAKE_PRIORITY_MASK    0xffff
#define ECM_CLASSIFIER_EMESH_SAWF_ADD_FLOW              1
#define ECM_CLASSIFIER_EMESH_SAWF_SUB_FLOW              2
#define ECM_CLASSIFIER_EMESH_SAWF_SERVICE_CLASS_MASK    0xff
#define ECM_CLASSIFIER_EMESH_SAWF_TAG_GET(sawf_meta)    ((sawf_meta >> 24) & 0xFF)
#define ECM_CLASSIFIER_EMESH_SAWF_TAG_IS_VALID(sawf_meta) \
		((ECM_CLASSIFIER_EMESH_SAWF_TAG_GET(sawf_meta) == ECM_CLASSIFIER_EMESH_SAWF_VALID_TAG) ? true : false)

/*
 * EMESH classifier type.
 */
enum ecm_classifier_emesh_sawf_types {
	ECM_CLASSIFIER_EMESH = 1,
	ECM_CLASSIFIER_SAWF,
};

/*
 * EMESH classifier type.
 */
enum ecm_classifier_emesh_ul_params_sync_modes {
	ECM_CLASSIFIER_EMESH_MODE_ACCEL = 0,
	ECM_CLASSIFIER_EMESH_MODE_DECEL,
	ECM_CLASSIFIER_EMESH_MODE_MAX,
};

/*
 * struct ecm_classifier_emesh_sawf_instance
 * 	State to allow tracking of dynamic qos for a connection
 */
struct ecm_classifier_emesh_sawf_instance {
	struct ecm_classifier_instance base;			/* Base type */

	struct ecm_classifier_emesh_sawf_instance *next;		/* Next classifier state instance (for accouting and reporting purposes) */
	struct ecm_classifier_emesh_sawf_instance *prev;		/* Next classifier state instance (for accouting and reporting purposes) */

	uint32_t ci_serial;					/* RO: Serial of the connection */
	uint32_t pcp[ECM_CONN_DIR_MAX];				/* PCP values for the connections */
	uint32_t dscp[ECM_CONN_DIR_MAX];			/* DSCP values for the connections */
	struct ecm_classifier_process_response process_response;/* Last process response computed */

	int refs;						/* Integer to trap we never go negative */
	uint8_t packet_seen[ECM_CONN_DIR_MAX];				/* Per direction packet seen flag */
	uint8_t ul_parameters_sync[ECM_CLASSIFIER_EMESH_MODE_MAX];	/* SAWF UL parameters sync flag on connection accel as well as decel time. */
	uint32_t service_interval_dl;		/* wlan downlink latency parameter: Service interval associated with this connection */
	uint32_t burst_size_dl;			/* wlan downlink latency parameter: Burst Size associated with this connection */
	uint32_t service_interval_ul;		/* wlan uplink latency parameter: Service interval associated with this connection */
	uint32_t burst_size_ul;			/* wlan uplink latency parameter: Burst Size associated with this connection */
	enum ecm_classifier_emesh_sawf_types type;		/* Flag for which type of classifier classified the connection */
	uint16_t flow_rule_id;			/* Rule id that matches the connection */
	uint16_t return_rule_id;		/* Rule id that matches the connection for reverse direction */

#if (DEBUG_LEVEL > 0)
	uint16_t magic;
#endif
};

/*
 * Operational control
 */
static uint32_t ecm_classifier_emesh_enabled;			/* Operational behaviour */
static uint32_t ecm_classifier_emesh_latency_config_enabled;	/* Mesh Latency profile enable flag */
static uint32_t ecm_classifier_sawf_enabled;			/* SAWF Mode */
static uint32_t ecm_classifier_sawf_cake_enabled;		/* CAKE Qdisc enable flag for SAWF */
static int ecm_classifier_sawf_emesh_udp_ipsec_port = 4500;	/* UDP ipsec port */

/*
 * Management thread control
 */
static bool ecm_classifier_emesh_sawf_terminate_pending = false;	/* True when the user wants us to terminate */

/*
 * Debugfs dentry object.
 */
static struct dentry *ecm_classifier_emesh_sawf_dentry;

/*
 * Locking of the classifier structures
 */
static DEFINE_SPINLOCK(ecm_classifier_emesh_sawf_lock);			/* Protect SMP access. */

/*
 * List of our classifier instances
 */
static struct ecm_classifier_emesh_sawf_instance *ecm_classifier_emesh_sawf_instances = NULL;
								/* list of all active instances */
static int ecm_classifier_emesh_sawf_count = 0;			/* Tracks number of instances allocated */

/*
 * Callback structure to support Mesh latency param config in WLAN driver
 */
static struct ecm_classifier_emesh_sawf_callbacks ecm_emesh;

/*
 * Message coming from userspace
 */
static struct ecm_front_end_flowsawf_msg flowsawf_msg;

/*
 * debugfs file object
 */
static uint32_t ecm_classifier_emesh_sawf_flowsawf;

/*
 * ecm_classifier_emesh_sawf_mark_set()
 */
static void ecm_classifier_emesh_sawf_mark_set(
				uint32_t flow_service_class_id, uint32_t return_service_class_id,
				uint16_t msduq_forward, uint16_t msduq_reverse,
				struct ecm_front_end_flowsawf_msg *msg)
{
	if (msduq_forward != ECM_CLASSIFIER_EMESH_SAWF_INVALID_MSDUQ) {
		msg->flow_mark = ECM_CLASSIFIER_EMESH_SAWF_VALID_TAG;
		msg->flow_mark <<= ECM_CLASSIFIER_EMESH_SAWF_TAG_SHIFT;
		msg->flow_mark |= (flow_service_class_id & 0xff) ;
		msg->flow_mark <<= ECM_CLASSIFIER_EMESH_SAWF_SERVICE_CLASS_SHIFT;
		msg->flow_mark |= msduq_forward;
	} else {
		msg->flow_mark = ECM_CLASSIFIER_EMESH_SAWF_INVALID_MSDUQ;
	}

	if (msduq_reverse != ECM_CLASSIFIER_EMESH_SAWF_INVALID_MSDUQ) {
		msg->return_mark = ECM_CLASSIFIER_EMESH_SAWF_VALID_TAG;
		msg->return_mark <<= ECM_CLASSIFIER_EMESH_SAWF_TAG_SHIFT;
		msg->return_mark |= (return_service_class_id & 0xff);
		msg->return_mark <<= ECM_CLASSIFIER_EMESH_SAWF_SERVICE_CLASS_SHIFT;
		msg->return_mark |= msduq_reverse;
	} else {
		msg->return_mark = ECM_CLASSIFIER_EMESH_SAWF_INVALID_MSDUQ;
	}
}

/*
 * ecm_classifier_emesh_sawf_flowsawf_set()
 *	Set sawf mark
 * Note: IP addrs and ports in msg are in network order.
 */
static void ecm_classifier_emesh_sawf_flowsawf_set(struct ecm_front_end_flowsawf_msg *msg)
{
	struct ecm_db_connection_instance *ci;
	ip_addr_t src_ip, dest_ip, match_addr;
	ecm_tracker_sender_type_t sender;
	struct net_device *src_dev = NULL;
	struct net_device *dest_dev = NULL;
	uint16_t msduq_forward = ECM_CLASSIFIER_EMESH_SAWF_INVALID_MSDUQ;
	uint16_t msduq_reverse = ECM_CLASSIFIER_EMESH_SAWF_INVALID_MSDUQ;
	uint8_t dmac[ETH_ALEN];
	uint8_t smac[ETH_ALEN];

	if (msg->ip_version == 4) {
		DEBUG_TRACE("%px: flow/return service_class_id=%u/%u %pI4n:%u -> %pI4n:%u protocol=%d\n", msg,
				msg->flow_service_class_id, msg->return_service_class_id,
				msg->flow_src_ip, ntohs(msg->flow_src_port),
				msg->return_src_ip, ntohs(msg->return_src_port), msg->protocol);
		ECM_NIN4_ADDR_TO_IP_ADDR(src_ip, msg->flow_src_ip[0]);
		ECM_NIN4_ADDR_TO_IP_ADDR(dest_ip, msg->return_src_ip[0]);
	} else {
		DEBUG_TRACE("%px: flow/return service_class_id=%u/%u %pI6c@%u -> %pI6c@%u protocol=%d\n", msg,
				msg->flow_service_class_id, msg->return_service_class_id,
				msg->flow_src_ip, ntohs(msg->flow_src_port),
				msg->return_src_ip, ntohs(msg->return_src_port), msg->protocol);
		ECM_NET_IPV6_ADDR_TO_IP_ADDR(src_ip, msg->flow_src_ip);
		ECM_NET_IPV6_ADDR_TO_IP_ADDR(dest_ip, msg->return_src_ip);
	}

	ci = ecm_db_connection_find_and_ref(src_ip,
					    dest_ip,
					    msg->protocol,
					    ntohs(msg->flow_src_port),
					    ntohs(msg->return_src_port));
	if (unlikely(!ci)) {
		DEBUG_WARN("no ci\n");
		return;
	}

	if (!ci->feci->update_rule) {
		DEBUG_WARN("frontend update_rule callback is not registered\n");
		goto end;
	}

	/* TODO: need to recheck if the logic to get sender is correct or not */
	ecm_db_connection_address_get(ci, ECM_DB_OBJ_DIR_FROM, match_addr);
	if (ECM_IP_ADDR_MATCH(src_ip, match_addr)) {
		sender = ECM_TRACKER_SENDER_TYPE_SRC;
		ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_FROM, smac);
		ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_TO, dmac);
	} else {
		sender = ECM_TRACKER_SENDER_TYPE_DEST;
		ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_TO, smac);
		ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_FROM, dmac);
	}

	ecm_db_netdevs_get_and_hold(ci, sender, &src_dev, &dest_dev);

	/*
	 * Get the bidirectional msduq by calling wlan driver API (qca_sawf_get_msdu_queue())
	 * using the service id, netdev, and peer's mac address.
	 * TODO: Can we call qca_sawf_get_msduq(netdev, peer_mac, service_id) instead of
	 *       qca_sawf_get_msdu_queue(netdev, peer_mac, service_id, dscp, rule_id)?
	 */
	if (ecm_emesh.update_service_id_get_msduq) {
		if (dest_dev) {
			msduq_forward = ecm_emesh.update_service_id_get_msduq(dest_dev, dmac, msg->flow_service_class_id, 0, 0);
		}
		if (src_dev) {
			msduq_reverse = ecm_emesh.update_service_id_get_msduq(src_dev, smac, msg->return_service_class_id, 0, 0);
		}
	}

	DEBUG_TRACE("ci=%px: sender=%d src_dev=%s smac=%pM dest_dev=%s dmac=%pM "
		    "svcid_f=%u svcid_r=%u msduq_f=0x%x msduq_r=0x%x\n",
			ci, sender, src_dev->name, smac, dest_dev->name, dmac,
			msg->flow_service_class_id, msg->return_service_class_id,
			msduq_forward, msduq_reverse);

	/*
	 * Set msg's flow/return marks to sawf_meta created from service ids and msduqs
	 */
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		ecm_classifier_emesh_sawf_mark_set(msg->flow_service_class_id, msg->return_service_class_id,
				msduq_forward, msduq_reverse, msg);
	} else {
		ecm_classifier_emesh_sawf_mark_set(msg->return_service_class_id, msg->flow_service_class_id,
				msduq_reverse, msduq_forward, msg);
	}

	/*
	 * Update frontend's mark rule
	 */
	ci->feci->update_rule(ci->feci, ECM_RULE_UPDATE_TYPE_SAWFMARK, msg);

	/*
	 * All done
	 */
	if (src_dev) {
		dev_put(src_dev);
	}

	if (dest_dev) {
		dev_put(dest_dev);
	}

end:
	ecm_db_connection_deref(ci);

	return;
}

/*
 * ecm_classifier_emesh_sawf_file_read()
 */
static ssize_t ecm_classifier_emesh_sawf_file_read(struct file *filp, char __user *userbuf,
                                size_t count, loff_t *ppos)
{
	return 0;
}

/*
 * ecm_classifier_emesh_sawf_file_write()
 *      ECM creates 'flowsawf' file in debugfs.
 *      Userspace app will open() 'flowsawf' and write() 'flowsawf_msg' to the file.
 */
static ssize_t ecm_classifier_emesh_sawf_file_write(struct file *filp, const char __user *userbuf,
                                size_t count, loff_t *ppos)
{
	unsigned long not_copied;

	if (count != offsetof(struct ecm_front_end_flowsawf_msg, flow_mark)) {
		DEBUG_WARN("bytes_from_user=%zu offsetof(struct ecm_front_end_flowsawf_msg, flow_mark)=%zu\n",
				count, offsetof(struct ecm_front_end_flowsawf_msg, flow_mark));
		return -EINVAL;
	}

	memset(&flowsawf_msg, 0, sizeof(flowsawf_msg));
	not_copied = copy_from_user((char *)&flowsawf_msg, userbuf, count);

	DEBUG_TRACE("bytes_from_user=%zu bytes_not_copied=%lu pos=%llu\n",
			count, not_copied, *ppos);

	if (not_copied) {
		return -EFAULT;
	}

	/*
	 * Update the mark
	 * IP addrs and ports in ecm_sfe_ifli_msg are in network order.
	 */
	ecm_classifier_emesh_sawf_flowsawf_set(&flowsawf_msg);

	return count;
}

static const struct file_operations ecm_classifier_emesh_sawf_flowsawf_fops = {
        .read = ecm_classifier_emesh_sawf_file_read,
        .write = ecm_classifier_emesh_sawf_file_write,
};

/*
 * ecm_classifier_emesh_sawf_ref()
 *	Ref
 */
static void ecm_classifier_emesh_sawf_ref(struct ecm_classifier_instance *ci)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	cemi = (struct ecm_classifier_emesh_sawf_instance *)ci;

	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed\n", cemi);
	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	cemi->refs++;
	DEBUG_TRACE("%px: cemi ref %d\n", cemi, cemi->refs);
	DEBUG_ASSERT(cemi->refs > 0, "%px: ref wrap\n", cemi);
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
}

/*
 * ecm_classifier_emesh_sawf_deref()
 *	Deref
 */
static int ecm_classifier_emesh_sawf_deref(struct ecm_classifier_instance *ci)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	cemi = (struct ecm_classifier_emesh_sawf_instance *)ci;

	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed\n", cemi);
	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	cemi->refs--;
	DEBUG_ASSERT(cemi->refs >= 0, "%px: refs wrapped\n", cemi);
	DEBUG_TRACE("%px: EMESH classifier deref %d\n", cemi, cemi->refs);
	if (cemi->refs) {
		int refs = cemi->refs;
		spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
		return refs;
	}

	/*
	 * Object to be destroyed
	 */
	ecm_classifier_emesh_sawf_count--;
	DEBUG_ASSERT(ecm_classifier_emesh_sawf_count >= 0, "%px: ecm_classifier_emesh_sawf_count wrap\n", cemi);

	/*
	 * UnLink the instance from our list
	 */
	if (cemi->next) {
		cemi->next->prev = cemi->prev;
	}

	if (cemi->prev) {
		cemi->prev->next = cemi->next;
	} else {
		DEBUG_ASSERT(ecm_classifier_emesh_sawf_instances == cemi, "%px: list bad %px\n", cemi, ecm_classifier_emesh_sawf_instances);
		ecm_classifier_emesh_sawf_instances = cemi->next;
	}
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);

	/*
	 * Final
	 */
	DEBUG_INFO("%px: Final EMESH classifier instance\n", cemi);
	kfree(cemi);

	return 0;
}

/*
 * ecm_classifier_emesh_sawf_is_bidi_packet_seen()
 *	Return true if both direction packets are seen.
 */
static inline bool ecm_classifier_emesh_sawf_is_bidi_packet_seen(struct ecm_classifier_emesh_sawf_instance *cemi)
{
	return ((cemi->packet_seen[ECM_CONN_DIR_FLOW] == true) && (cemi->packet_seen[ECM_CONN_DIR_RETURN] == true));
}

/*
 * ecm_classifier_emesh_sawf_fill_dscp_info()
 *	Save the DSCP values in classifier instance for SPM rule match.
 */
static void ecm_classifier_emesh_sawf_fill_dscp_info(uint16_t dscp, struct ecm_classifier_emesh_sawf_instance *cemi,
							ecm_tracker_sender_type_t sender,
							struct sp_rule_input_params *flow_input_params,
							struct sp_rule_input_params *return_input_params)
{
	/*
	 * Save the dscp values and fill the flow and return
	 * input parameters accordingly.
	 */
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		cemi->dscp[ECM_CONN_DIR_FLOW] = dscp;
		/*
		 * In case of UDP acceleration delay packets disabled, we save the same
		 * DSCP value in the return direction. Also we avoid saving the value if
		 * we have already seen reverse direction packet and saved the dscp in reverse direction.
		 */
		if (!cemi->dscp[ECM_CONN_DIR_RETURN]) {
			cemi->dscp[ECM_CONN_DIR_RETURN] = dscp;
		}

		flow_input_params->dscp = cemi->dscp[ECM_CONN_DIR_FLOW];
		return_input_params->dscp = cemi->dscp[ECM_CONN_DIR_RETURN];
	} else {
		cemi->dscp[ECM_CONN_DIR_RETURN] = dscp;
		flow_input_params->dscp = cemi->dscp[ECM_CONN_DIR_RETURN];
		return_input_params->dscp = cemi->dscp[ECM_CONN_DIR_FLOW];
	}
}
/*
 * ecm_classifier_emesh_sawf_fill_pcp()
 *	Save the PCP value in the classifier instance.
 */
static void ecm_classifier_emesh_sawf_fill_pcp(struct ecm_classifier_emesh_sawf_instance *cemi,
		 ecm_tracker_sender_type_t sender, struct sk_buff *skb, uint16_t cake_handle)
{
	/*
	 * If CAKE Qdisc is enabled on the destination interface, put Qdisc handle as the
	 * major number in the priority field (in skb->priority, upper 16 bits are interpreted as
	 * major number and the lower 16 as minor number). CAKE will use the major number for
	 * Qdisc match and the minor number will be used for priority classification inside CAKE.
	 */
	if (cake_handle != ECM_CLASSIFIER_EMESH_SAWF_CAKE_INVALID_HANDLE) {
		/*
		 * First clear out upper 16 bits and then put the
		 * handle in case of CAKE is enabled.
		 */
		skb->priority &= ECM_CLASSIFIER_EMESH_SAWF_CAKE_PRIORITY_MASK;
		skb->priority |= (cake_handle << ECM_CLASSIFIER_EMESH_SAWF_CAKE_HANDLE_SHIFT);
	}

	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		cemi->pcp[ECM_CONN_DIR_FLOW] = skb->priority;
		cemi->packet_seen[ECM_CONN_DIR_FLOW] = true;
	} else {
		cemi->pcp[ECM_CONN_DIR_RETURN] = skb->priority;
		cemi->packet_seen[ECM_CONN_DIR_RETURN] = true;
	}
}

/*
 * ecm_classifier_emesh_sawf_check_cake_qdisc()
 *	Here we check if CAKE Qdisc is enabled on the given interface or not.
 *	If yes, we can fetch the Qdisc handle so that it will be placed inside skb->priority
 *	for CAKE priority classification.
 */
static void ecm_classifier_emesh_sawf_check_cake_qdisc(struct net_device *dev, uint32_t *cake_handle)
{
	/*
	 * Check if given dev has a qdisc attached or not, if yes, return the handle.
	 */
	if (dev && ecm_classifier_sawf_cake_enabled) {
		if (dev->qdisc && (!strcmp(dev->qdisc->ops->id, "cake"))) {
			DEBUG_INFO("%px: CAKE Qdisc is attached on dev %s\n", dev, dev->name);
			*cake_handle = dev->qdisc->handle >> ECM_CLASSIFIER_EMESH_SAWF_CAKE_HANDLE_SHIFT;
		}
	}
}

/*
 * ecm_classifier_emesh_sawf_fill_vlan_info()
 *	Get the VLAN info in respective directions by iterating over the ECM's
 *	'to' and 'from' list and getting the source and the destination vlan net devices
 *	(if vlan is configured in the respective direction) for vlan_pcp remark support for SAWF.
 */
static void ecm_classifier_emesh_sawf_fill_vlan_info(struct ecm_db_connection_instance *ci,
						ecm_tracker_sender_type_t sender,
						struct sp_rule_input_params *flow_input_params,
						struct sp_rule_input_params *return_input_params)
{
	uint32_t first_index;
	uint32_t i;
	ecm_db_obj_dir_t dir;
	struct net_device *dev = NULL;
	struct ecm_db_iface_instance *interfaces[ECM_DB_IFACE_HEIRARCHY_MAX];

	/*
	 * Obtained destination vlan dev from ECM's 'to' or 'from' interface list
	 * according to the type of sender.
	 */
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		first_index = ecm_db_connection_interfaces_get_and_ref(ci, interfaces, ECM_DB_OBJ_DIR_TO);
		dir = ECM_DB_OBJ_DIR_TO;
	} else {
		first_index = ecm_db_connection_interfaces_get_and_ref(ci, interfaces, ECM_DB_OBJ_DIR_FROM);
		dir = ECM_DB_OBJ_DIR_FROM;
	}

	if (first_index == ECM_DB_IFACE_HEIRARCHY_MAX) {
		DEBUG_WARN("%px: Failed to get %s interfaces list\n", ci, ecm_db_obj_dir_strings[dir]);
		goto get_source_vlan_dev;
        }

	for (i = first_index; i < ECM_DB_IFACE_HEIRARCHY_MAX; i++) {
		dev = dev_get_by_index(&init_net, ecm_db_iface_interface_identifier_get(interfaces[i]));
		if (!dev) {
			DEBUG_WARN("%px: Failed to get net device with %d index\n", ci, first_index);
			ecm_db_connection_interfaces_deref(interfaces, first_index);
			goto get_source_vlan_dev;
		}

		if (is_vlan_dev(dev)) {
			flow_input_params->vlan_tci = vlan_dev_vlan_id(dev);
			ecm_db_connection_interfaces_deref(interfaces, first_index);
			dev_put(dev);
			goto get_source_vlan_dev;
		}

		dev_put(dev);
	}

	ecm_db_connection_interfaces_deref(interfaces, first_index);

get_source_vlan_dev:
	/*
	 * Obtained source vlan netdev form ECM's 'to' or 'from' interface list
	 * according to the type of sender.
	 */
	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		first_index = ecm_db_connection_interfaces_get_and_ref(ci, interfaces, ECM_DB_OBJ_DIR_FROM);
		dir = ECM_DB_OBJ_DIR_FROM;
	} else {
		first_index = ecm_db_connection_interfaces_get_and_ref(ci, interfaces, ECM_DB_OBJ_DIR_TO);
		dir = ECM_DB_OBJ_DIR_TO;
	}

	if (first_index == ECM_DB_IFACE_HEIRARCHY_MAX) {
		DEBUG_WARN("%px: Failed to get %s interfaces list\n", ci, ecm_db_obj_dir_strings[dir]);
		return;
        }

	for (i = first_index; i < ECM_DB_IFACE_HEIRARCHY_MAX; i++) {
		dev = dev_get_by_index(&init_net, ecm_db_iface_interface_identifier_get(interfaces[i]));
		if (!dev) {
			DEBUG_WARN("%px: Failed to get net device with %d index\n", ci, first_index);
			ecm_db_connection_interfaces_deref(interfaces, first_index);
			return;
		}

		if (is_vlan_dev(dev)) {
			return_input_params->vlan_tci = vlan_dev_vlan_id(dev);
			ecm_db_connection_interfaces_deref(interfaces, first_index);
			dev_put(dev);
			return;
		}

		dev_put(dev);
	}

	ecm_db_connection_interfaces_deref(interfaces, first_index);
}

/*
 * ecm_classifier_emesh_mark_sawf_metadata()
 *	Fills the sawf metadata in skb->mark.
 */
static void ecm_classifier_emesh_mark_sawf_metadata(struct sk_buff *skb,
		struct sp_rule_output_params *flow_output_params, uint32_t msduq_forward)
{
	if (flow_output_params->service_class_id != ECM_CLASSIFIER_EMESH_SAWF_INVALID_SERVICE_CLASS) {
		skb->mark = ECM_CLASSIFIER_EMESH_SAWF_VALID_TAG;
		skb->mark <<= ECM_CLASSIFIER_EMESH_SAWF_TAG_SHIFT;
		skb->mark |= flow_output_params->service_class_id;
		skb->mark <<= ECM_CLASSIFIER_EMESH_SAWF_SERVICE_CLASS_SHIFT;
		skb->mark |= msduq_forward;
	}
}

/*
 * ecm_classifier_emesh_sawf_fill_sawf_metadata()
 *	Save the sawf metadata in the classifier instance.
 */
static void ecm_classifier_emesh_sawf_fill_sawf_metadata(struct ecm_classifier_emesh_sawf_instance *cemi,
		struct sp_rule_output_params *flow_output_params, struct sp_rule_output_params *return_output_params,
		uint32_t msduq_forward, uint32_t msduq_reverse)
{
	/*
	 * Update the flow_sawf_metadata in process response,
	 * if the service class id is valid. Tag bits are used to
	 * distinguish between valid and invalid sawf_metadata.
	 */
	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	if (flow_output_params->service_class_id != ECM_CLASSIFIER_EMESH_SAWF_INVALID_SERVICE_CLASS) {
		cemi->process_response.flow_sawf_metadata = ECM_CLASSIFIER_EMESH_SAWF_VALID_TAG;
		cemi->process_response.flow_sawf_metadata <<= ECM_CLASSIFIER_EMESH_SAWF_TAG_SHIFT;
		cemi->process_response.flow_sawf_metadata |= flow_output_params->service_class_id;
		cemi->process_response.flow_sawf_metadata <<= ECM_CLASSIFIER_EMESH_SAWF_SERVICE_CLASS_SHIFT;
		cemi->process_response.flow_sawf_metadata |= msduq_forward;
		cemi->flow_rule_id = flow_output_params->rule_id;
	}

	/*
	 * Update the return_sawf_metadata in process response,
	 * if the service class id is valid. Tag bits are used to
	 * distinguish between valid and invalid sawf_metadata.
	 */
	if (return_output_params->service_class_id != ECM_CLASSIFIER_EMESH_SAWF_INVALID_SERVICE_CLASS) {
		cemi->process_response.return_sawf_metadata = ECM_CLASSIFIER_EMESH_SAWF_VALID_TAG;
		cemi->process_response.return_sawf_metadata <<= ECM_CLASSIFIER_EMESH_SAWF_TAG_SHIFT;
		cemi->process_response.return_sawf_metadata |= return_output_params->service_class_id;
		cemi->process_response.return_sawf_metadata <<= ECM_CLASSIFIER_EMESH_SAWF_SERVICE_CLASS_SHIFT;
		cemi->process_response.return_sawf_metadata |= msduq_reverse;
		cemi->return_rule_id = return_output_params->rule_id;
	}

	/*
	 * Indicates response contains SAWF information.
	 */
	cemi->process_response.process_actions |= ECM_CLASSIFIER_PROCESS_ACTION_EMESH_SAWF_TAG;

	/*
	 * While updating the DSCP remark values, even if one direction rule matches and we have one sided dscp remark
	 * coming from userspace, we will apply the same for both direction. Does not apply for vlan
	 * pcp remark as vlan id could be different in the other direction.
	 */
	if (flow_output_params->dscp_remark != SP_RULE_INVALID_DSCP_REMARK &&
			return_output_params->dscp_remark != SP_RULE_INVALID_DSCP_REMARK) {
		cemi->process_response.flow_dscp = flow_output_params->dscp_remark;
		cemi->process_response.return_dscp = return_output_params->dscp_remark;
		cemi->process_response.process_actions |= ECM_CLASSIFIER_PROCESS_ACTION_DSCP;
	} else if (flow_output_params->dscp_remark != SP_RULE_INVALID_DSCP_REMARK) {
		cemi->process_response.flow_dscp = flow_output_params->dscp_remark;
		cemi->process_response.return_dscp = flow_output_params->dscp_remark;
		cemi->process_response.process_actions |= ECM_CLASSIFIER_PROCESS_ACTION_DSCP;
	} else if (return_output_params->dscp_remark != SP_RULE_INVALID_DSCP_REMARK) {
		cemi->process_response.flow_dscp = return_output_params->dscp_remark;
		cemi->process_response.return_dscp = return_output_params->dscp_remark;
		cemi->process_response.process_actions |= ECM_CLASSIFIER_PROCESS_ACTION_DSCP;
	}

	/*
	 * Fill VLAN pcp remark coming from user.
	 */
	cemi->process_response.flow_vlan_pcp = flow_output_params->vlan_pcp_remark;
	cemi->process_response.return_vlan_pcp = return_output_params->vlan_pcp_remark;

	/*
	 * Set VLAN PCP values if SAWF rules has provided the remark in atleast one direction.
	 */
	if ((flow_output_params->vlan_pcp_remark != SP_RULE_INVALID_VLAN_PCP_REMARK) ||
					(return_output_params->vlan_pcp_remark != SP_RULE_INVALID_VLAN_PCP_REMARK)) {
		cemi->process_response.process_actions |= ECM_CLASSIFIER_PROCESS_ACTION_EMESH_SAWF_VLAN_PCP_REMARK;
	}

	cemi->type = ECM_CLASSIFIER_SAWF;

	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
}

/*
 * ecm_classifier_sawf_fill_input_params()
 *	fill sawf input params for sp rule lookup.
 */
static bool ecm_classifier_sawf_fill_input_params(struct sk_buff *skb, struct ecm_db_connection_instance *ci,
							struct ecm_classifier_emesh_sawf_instance *cemi,
							ecm_tracker_sender_type_t sender,
							uint8_t *smac, uint8_t *dmac,
							struct sp_rule_input_params *flow_input_params,
							struct sp_rule_input_params *return_input_params,
							struct net_device *src_dev, struct net_device *dest_dev)
{
	struct iphdr *iph;
	struct ipv6hdr *ip6h;
	struct tcphdr *tcphdr;
	struct udphdr *udphdr;
	uint16_t dscp;
	struct ip_esp_hdr *esp;
	uint16_t version;

	/*
	 * Return false if any of src or dest dev is not present.
	 * Because, we need both devs to call the msduq callback.
	 */
	if (!src_dev || !dest_dev)
		return false;

	if (skb->protocol == ntohs(ETH_P_IP)) {
		version = ntohs(ETH_P_IP);
		if (unlikely(!pskb_may_pull(skb, sizeof(*iph)))) {
			/*
			 * Check for ip header
			 */
			DEBUG_INFO("No ip header in skb\n");
			return false;
		}

		iph = ip_hdr(skb);
		flow_input_params->protocol = iph->protocol;
		return_input_params->protocol = iph->protocol;
		flow_input_params->src.ip.ipv4_addr = return_input_params->dst.ip.ipv4_addr = iph->saddr;
		flow_input_params->dst.ip.ipv4_addr = return_input_params->src.ip.ipv4_addr = iph->daddr;
		flow_input_params->ip_version_type = return_input_params->ip_version_type = 4;
		dscp = ipv4_get_dsfield(iph) >> XT_DSCP_SHIFT;
		ecm_classifier_emesh_sawf_fill_dscp_info(dscp, cemi, sender, flow_input_params, return_input_params);
	} else if (skb->protocol == ntohs(ETH_P_IPV6)) {
		version = ntohs(ETH_P_IPV6);
		if (unlikely(!pskb_may_pull(skb, sizeof(*ip6h)))) {
			/*
			 * Check for ipv6 header
			 */
			DEBUG_INFO("No ipv6 header in skb\n");
			return false;
		}

		ip6h = ipv6_hdr(skb);
		flow_input_params->protocol = ip6h->nexthdr;
		return_input_params->protocol = ip6h->nexthdr;
		memcpy(&flow_input_params->src.ip.ipv6_addr, &ip6h->saddr, sizeof(struct in6_addr));
		memcpy(&flow_input_params->dst.ip.ipv6_addr, &ip6h->daddr, sizeof(struct in6_addr));
		memcpy(&return_input_params->src.ip.ipv6_addr, &ip6h->daddr, sizeof(struct in6_addr));
		memcpy(&return_input_params->dst.ip.ipv6_addr, &ip6h->saddr, sizeof(struct in6_addr));
		flow_input_params->ip_version_type = return_input_params->ip_version_type = 6;
		dscp = ipv6_get_dsfield(ip6h) >> XT_DSCP_SHIFT;
		ecm_classifier_emesh_sawf_fill_dscp_info(dscp, cemi, sender, flow_input_params, return_input_params);
	} else {
		DEBUG_INFO("Not ip packet protocol: %x \n", skb->protocol);
		return false;
	}

	flow_input_params->spi = ECM_CLASSIFIER_EMESH_SAWF_INVALID_SPI;
	if (flow_input_params->protocol == IPPROTO_TCP) {
		/*
		 * Check for tcp header
		 */
		if (unlikely(!pskb_may_pull(skb, sizeof(*tcphdr)))) {
			DEBUG_INFO("No tcp header in skb\n");
			return false;
		}

		tcphdr = tcp_hdr(skb);
		flow_input_params->src.port = return_input_params->dst.port = ntohs(tcphdr->source);
		flow_input_params->dst.port = return_input_params->src.port = ntohs(tcphdr->dest);
	} else if (flow_input_params->protocol == IPPROTO_UDP) {
		/*
		 * Check for udp header
		 */
		if (unlikely(!pskb_may_pull(skb, sizeof(*udphdr)))) {
			DEBUG_INFO("No udp header in skb\n");
			return false;
		}

		udphdr = udp_hdr(skb);
		flow_input_params->src.port = return_input_params->dst.port = ntohs(udphdr->source);
		flow_input_params->dst.port = return_input_params->src.port = ntohs(udphdr->dest);

		/*
		 * Check for UDP encapsulated IPSEC packet.
		 */
		if (flow_input_params->dst.port == ecm_classifier_sawf_emesh_udp_ipsec_port) {
			esp = (struct ip_esp_hdr *)((uint8_t *)udphdr + sizeof(*udphdr));
			flow_input_params->spi = ntohl(esp->spi);
		}
	} else if (flow_input_params->protocol == IPPROTO_ESP) {

		/*
		 * Get the SPI for IPSEC packets.
		 */
		if (version == ntohs(ETH_P_IP)) {
			esp = (struct ip_esp_hdr *)((uint8_t *)iph + sizeof(*iph));
			flow_input_params->spi = ntohl(esp->spi);
		} else {
			esp = (struct ip_esp_hdr *)((uint8_t *)ip6h + sizeof(*ip6h));
			flow_input_params->spi = ntohl(esp->spi);
		}
	} else {
		DEBUG_INFO("Not a ported protocol \n");
		return false;
	}

	flow_input_params->vlan_tci = return_input_params->vlan_tci = SP_RULE_INVALID_VLAN_TCI;

	/*
	 * Get the source and destination VLAN information.
	 */
	ecm_classifier_emesh_sawf_fill_vlan_info(ci, sender, flow_input_params, return_input_params);

	flow_input_params->ifindex = dest_dev->ifindex;
	/*
	 *  Get the netdevice addres in case of wds repeater cases.
	 */
	ether_addr_copy((uint8_t *)flow_input_params->dev_addr, (uint8_t *)dest_dev->dev_addr);
	ether_addr_copy(flow_input_params->src.mac, smac);
	ether_addr_copy(flow_input_params->dst.mac, dmac);
	ether_addr_copy(return_input_params->src.mac, dmac);
	ether_addr_copy(return_input_params->dst.mac, smac);
	return true;
}

/*
 * ecm_classifier_emesh_sawf_update()
 *	Called from the frontend files to update the classifier instance.
 */
void ecm_classifier_emesh_sawf_update(struct ecm_classifier_instance *aci, enum ecm_rule_update_type type, void *arg)
{
	struct ecm_front_end_flowsawf_msg *msg = (struct ecm_front_end_flowsawf_msg *)arg;
	struct nf_conn *ct;
	struct nf_conntrack_tuple tuple;
	struct nf_conntrack_tuple_hash *h;
	struct nf_ct_dscpremark_ext *dscpcte;

	if (type != ECM_RULE_UPDATE_TYPE_SAWFMARK) {
		DEBUG_WARN("%px: unsupported update type: %d\n", aci, type);
		return;
	}

	/*
	 * Create a tuple so as to be able to look up a conntrack connection
	 */
	memset(&tuple, 0, sizeof(tuple));
	tuple.src.u.all = msg->flow_src_port;
	tuple.dst.u.all = msg->flow_dest_port;
	tuple.dst.protonum = (uint8_t)msg->protocol;
	tuple.dst.dir = IP_CT_DIR_ORIGINAL;
	if (msg->ip_version == 4) {
		tuple.src.l3num = AF_INET;
		tuple.src.u3.ip = msg->flow_src_ip[0];
		tuple.dst.u3.ip = msg->flow_dest_ip[0];
		DEBUG_TRACE("%px: Lookup ct using Protocol=%d src_addr=%pI4:%d dest_addr=%pI4:%d\n",
				aci, (int)tuple.dst.protonum,
				tuple.src.u3.all, (int)(ntohs(tuple.src.u.all)),
				tuple.dst.u3.all, (int)(ntohs(tuple.dst.u.all)));
	} else {
		tuple.src.l3num = AF_INET6;
		ECM_IP_ADDR_COPY(tuple.src.u3.ip6, msg->flow_src_ip);
		ECM_IP_ADDR_COPY(tuple.dst.u3.ip6, msg->flow_dest_ip);
		DEBUG_TRACE("%px: Lookup ct using Protocol=%d src_addr=%pI6c@%d dest_addr=%pI6c@%d\n",
				aci, (int)tuple.dst.protonum,
				tuple.src.u3.all, (int)(ntohs(tuple.src.u.all)),
				tuple.dst.u3.all, (int)(ntohs(tuple.dst.u.all)));
	}
	h = nf_conntrack_find_get(&init_net, &nf_ct_zone_dflt, &tuple);
	if (!h) {
		DEBUG_WARN("%px: no ct\n", aci);
		return;
	}

	ct = nf_ct_tuplehash_to_ctrack(h);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
	NF_CT_ASSERT(ct->timeout.data == (unsigned long)ct);
#endif

	spin_lock_bh(&ct->lock);
	dscpcte = nf_ct_dscpremark_ext_find(ct);
	if (!dscpcte) {
		spin_unlock_bh(&ct->lock);
		DEBUG_WARN("%px: ct=%px: no dscpcte\n", aci, ct);
		return;
	}

	if (ECM_CLASSIFIER_EMESH_SAWF_TAG_GET(msg->flow_mark) == ECM_CLASSIFIER_EMESH_SAWF_VALID_TAG) {
		dscpcte->flow_sawf_meta = msg->flow_mark;
		dscpcte->flow_set_flags |= NF_CT_DSCPREMARK_EXT_SAWF;
	} else {
		dscpcte->flow_set_flags &= ~NF_CT_DSCPREMARK_EXT_SAWF;
	}
	if (ECM_CLASSIFIER_EMESH_SAWF_TAG_GET(msg->return_mark) == ECM_CLASSIFIER_EMESH_SAWF_VALID_TAG) {
		dscpcte->return_sawf_meta = msg->return_mark;
		dscpcte->return_set_flags |= NF_CT_DSCPREMARK_EXT_SAWF;
	} else {
		dscpcte->return_set_flags &= ~NF_CT_DSCPREMARK_EXT_SAWF;
	}
	spin_unlock_bh(&ct->lock);

	/*
	 * Release connection
	 */
	nf_ct_put(ct);

	return;
}

/*
 * ecm_classifier_emesh_sawf_process()
 *	Process new data for connection
 */
static void ecm_classifier_emesh_sawf_process(struct ecm_classifier_instance *aci, ecm_tracker_sender_type_t sender,
						struct ecm_tracker_ip_header *ip_hdr, struct sk_buff *skb,
						struct ecm_classifier_process_response *process_response)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	ecm_classifier_relevence_t relevance;
	struct ecm_db_connection_instance *ci = NULL;
	struct ecm_front_end_connection_instance *feci;
	ecm_front_end_acceleration_mode_t accel_mode;
	uint32_t became_relevant = 0;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	int protocol;
	uint64_t slow_pkts;
	uint8_t dmac[ETH_ALEN];
	uint8_t smac[ETH_ALEN];
	uint32_t cake_flow_handle = ECM_CLASSIFIER_EMESH_SAWF_CAKE_INVALID_HANDLE;
	uint32_t cake_return_handle = ECM_CLASSIFIER_EMESH_SAWF_CAKE_INVALID_HANDLE;
	struct net_device *src_dev = NULL;
	struct net_device *dest_dev = NULL;
	struct sp_rule_input_params flow_input_params;
	struct sp_rule_input_params return_input_params;
	struct sp_rule_output_params flow_output_params;
	struct sp_rule_output_params return_output_params;
	bool is_sawf_relevant = false;

	cemi = (struct ecm_classifier_emesh_sawf_instance *)aci;
	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed\n", cemi);

	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	relevance = cemi->process_response.relevance;

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
		goto sawf_emesh_classifier_out;
	}

	/*
	 * Yes or maybe relevant.
	 *
	 * Need to decide our relevance to this connection.
	 * We are only relevant to a connection if:
	 * 1. We are enabled.
	 * 2. Connection can be accelerated.
	 * Any other condition and we are not and will stop analysing this connection.
	 */
	if (!ecm_classifier_emesh_enabled && !ecm_classifier_sawf_enabled) {
		/*
		 * Lock still held
		 */
		cemi->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_NO;
		goto sawf_emesh_classifier_out;
	}
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);

	/*
	 * Is there a valid conntrack?
	 */
	ct = nf_ct_get(skb, &ctinfo);
	if (!ct) {
		spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
		cemi->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_NO;
		goto sawf_emesh_classifier_out;
	}

	/*
	 * Can we accelerate?
	 */
	ci = ecm_db_connection_serial_find_and_ref(cemi->ci_serial);
	if (!ci) {
		DEBUG_TRACE("%px: No ci found for %u\n", cemi, cemi->ci_serial);
		spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
		cemi->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_NO;
		goto sawf_emesh_classifier_out;
	}

	/*
	 * Check if SAWF is enabled or SPM rule lookup flag is enabled
	 */
	if (!ecm_classifier_sawf_enabled &&
			!(ecm_classifier_emesh_latency_config_enabled & ECM_CLASSIFIER_EMESH_ENABLE_SPM_RULE_LOOKUP)) {
		goto sawf_classifier_out;
	}

	if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
		DEBUG_TRACE("%px: sender is SRC\n", aci);
		ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_FROM, smac);
		ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_TO, dmac);
	} else {
		DEBUG_TRACE("%px: sender is DEST\n", aci);
		ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_TO, smac);
		ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_FROM, dmac);
	}

	/*
	 * Fetch the src and dest net devices required to get the msduq for SAWF
	 * and to check for the CAKE Qdisc as well.
	 */
	ecm_db_netdevs_get_and_hold(ci, sender, &src_dev, &dest_dev);

	/*
	 * SAWF does support ported protocols.
	 */
	protocol = ecm_db_connection_protocol_get(ci);
	if ((protocol != IPPROTO_UDP) && (protocol != IPPROTO_TCP)) {
		goto check_emesh_classifier;
	}

	/*
	 * Invoke SPM rule lookup API for skb priority update
	 * For bridging traffic, it will be matched with the rule table on SPM prerouting hook
	 * emesh-sawf takes precedence over the emesh classifier.
	 * emesh classifer SPM lookup will be done in case sawf is not enabled.
	 */
	flow_output_params.rule_id = ECM_CLASSIFIER_EMESH_SAWF_INVALID_RULE_LOOKUP;
	return_output_params.rule_id = ECM_CLASSIFIER_EMESH_SAWF_INVALID_RULE_LOOKUP;
	if (ecm_classifier_sawf_enabled) {
		uint32_t msduq_forward = ECM_CLASSIFIER_EMESH_SAWF_INVALID_MSDUQ;
		uint32_t msduq_reverse = ECM_CLASSIFIER_EMESH_SAWF_INVALID_MSDUQ;

		DEBUG_INFO("ecm classifier sawf is enabled\n");
		if (!ecm_classifier_sawf_fill_input_params(skb, ci, cemi, sender, smac, dmac,
								&flow_input_params, &return_input_params, src_dev, dest_dev)) {
			DEBUG_TRACE("%px: failed to fill in sawf input params\n", ci);
			/*
			 * If SAWF fails to fill input parameters,
			 * emesh classifier should get precedence.
			 */
			goto check_emesh_classifier;
		}

		sp_mapdb_rule_apply_sawf(skb, &flow_input_params, &flow_output_params);
		sp_mapdb_rule_apply_sawf(skb, &return_input_params, &return_output_params);

		/*
		 * If sawf SPM rule lookup fails for both directions,
		 * SAWF classifier is no longer valid and check for emesh classifier.
		 */
		if (flow_output_params.rule_id == ECM_CLASSIFIER_EMESH_SAWF_INVALID_RULE_LOOKUP
			&& return_output_params.rule_id == ECM_CLASSIFIER_EMESH_SAWF_INVALID_RULE_LOOKUP) {

			/*
			 * We then use sawf_meta stored in the dscp extension if sawf_meta is valid.
			 * Sawf_meta is stored in the dscp extentension when the update callback is called.
			 */
			struct nf_ct_dscpremark_ext *dscpcte;

			dscpcte = nf_ct_dscpremark_ext_find(ct);
			if (dscpcte && (dscpcte->flow_set_flags & NF_CT_DSCPREMARK_EXT_SAWF)) {
				cemi->process_response.flow_sawf_metadata = dscpcte->flow_sawf_meta;
				DEBUG_TRACE("%px: use dscpcte's flow_sawf_meta=%x\n", cemi, cemi->process_response.flow_sawf_metadata);
			}

			if (dscpcte && (dscpcte->return_set_flags & NF_CT_DSCPREMARK_EXT_SAWF)) {
				cemi->process_response.return_sawf_metadata = dscpcte->return_sawf_meta;
				DEBUG_TRACE("%px: use dscpcte's return_sawf_meta=%x\n", cemi, cemi->process_response.return_sawf_metadata);
			}

			goto check_emesh_classifier;
		}

		/*
		 * Get the bidirectional msduq from wlan driver using the service id
		 * (received from spm rule lookup), netdev, and mac address.
		 */
		if (ecm_emesh.update_service_id_get_msduq) {
			if (dest_dev) {
				msduq_forward = ecm_emesh.update_service_id_get_msduq(dest_dev, dmac, flow_output_params.service_class_id, cemi->dscp[ECM_CONN_DIR_FLOW], flow_output_params.rule_id);

				/*
				 * Mark the skb with SAWF meta data for flow creation packet.
				 */
				ecm_classifier_emesh_mark_sawf_metadata(skb,
									&flow_output_params,
									msduq_forward);
			}
			if (src_dev) {
				msduq_reverse = ecm_emesh.update_service_id_get_msduq(src_dev, smac, return_output_params.service_class_id, cemi->dscp[ECM_CONN_DIR_RETURN], return_output_params.rule_id);
			}
		}

		/*
		 * Update skb->priority with the priority sent by SPM-SAWF rule lookup in case of
		 * successful rule lookup. Also if SAWF has updated the priority value, CAKE priority classification
		 * should take over the dscp classification (if enabled), so we check if CAKE is enabled on
		 * the interface or not and put the cake handle in skb->priority.
		 */
		if (flow_output_params.rule_id != ECM_CLASSIFIER_EMESH_SAWF_INVALID_RULE_LOOKUP) {
			skb->priority = flow_output_params.priority;
			ecm_classifier_emesh_sawf_check_cake_qdisc(dest_dev, &cake_flow_handle);
			/*
			 * This is for UDP traffic with accel delay packets disabled.
			 */
			ecm_classifier_emesh_sawf_check_cake_qdisc(src_dev, &cake_return_handle);
		}

		if (sender == ECM_TRACKER_SENDER_TYPE_SRC) {
			ecm_classifier_emesh_sawf_fill_sawf_metadata(cemi, &flow_output_params, &return_output_params,
				msduq_forward, msduq_reverse);
		} else {
			ecm_classifier_emesh_sawf_fill_sawf_metadata(cemi, &return_output_params, &flow_output_params,
				msduq_reverse, msduq_forward);
		}

		is_sawf_relevant = true;

		/*
		 * For IPSEC protocol, we update both side priority values and let it go via slow path.
		 * TODO: FIx the IPSEC acceleration issue.
		 */
		if (protocol == IPPROTO_ESP || (protocol == IPPROTO_UDP &&
			flow_input_params.dst.port == ecm_classifier_sawf_emesh_udp_ipsec_port)) {
			ecm_db_connection_deref(ci);
			spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
			cemi->process_response.process_actions |= ECM_CLASSIFIER_PROCESS_ACTION_ACCEL_MODE;
			cemi->process_response.accel_mode = ECM_CLASSIFIER_ACCELERATION_MODE_NO;
			cemi->process_response.flow_qos_tag = skb->priority;
			cemi->process_response.return_qos_tag = skb->priority;
			cemi->process_response.process_actions |= ECM_CLASSIFIER_PROCESS_ACTION_QOS_TAG;
			ecm_classifier_emesh_mark_sawf_metadata(skb,
								&flow_output_params,
								msduq_forward);
			goto sawf_emesh_classifier_out;
		}
	}

	/*
	 * If SAWF classifier is not enabled or failed,
	 * we should check for emesh classifier.
	 */
check_emesh_classifier:
	if (!ecm_classifier_emesh_enabled && !is_sawf_relevant) {
		ecm_db_connection_deref(ci);
		spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
		cemi->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_NO;
		goto sawf_emesh_classifier_out;
	}

	if (ecm_classifier_emesh_enabled &&
			(ecm_classifier_emesh_latency_config_enabled & ECM_CLASSIFIER_EMESH_ENABLE_SPM_RULE_LOOKUP)) {
		/*
		 * We will apply the SP rules if the incoming interface
		 * of this packet is different and the outgoing interface in case of EMESH classifer.
		 */
		if (skb->skb_iif == skb->dev->ifindex) {
			goto sawf_classifier_out;
		}

		/*
		 * Update skb->priority with emesh SPM rule lookup if
		 * 1. If sawf classifier is not relevant or.
		 * 2. SAWF rule match for this direction fails, then the emesh priority has to be used.
		 * Also, if emesh classifier is updating priority, we apply CAKE priority classification if enabled.
		 */
		if (!is_sawf_relevant || (flow_output_params.rule_id == ECM_CLASSIFIER_EMESH_SAWF_INVALID_RULE_LOOKUP)) {
			sp_mapdb_apply(skb, smac, dmac);
			ecm_classifier_emesh_sawf_check_cake_qdisc(dest_dev, &cake_flow_handle);
			/*
			 * This is for UDP traffic with accel delay packets disabled.
			 */
			ecm_classifier_emesh_sawf_check_cake_qdisc(src_dev, &cake_return_handle);
		}

		/*
		 * Classifier type is emesh if
		 * 1. SAWF is not enabled or
		 * 2. Both direction SAWF rule match fails
		 */
		spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
		if (!is_sawf_relevant) {
			cemi->type = ECM_CLASSIFIER_EMESH;
		}
		spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
	}

sawf_classifier_out:
	feci = ecm_db_connection_front_end_get_and_ref(ci);
	accel_mode = ecm_front_end_connection_accel_state_get(feci);
	slow_pkts = ecm_front_end_get_slow_packet_count(feci);
	ecm_front_end_connection_deref(feci);
	protocol = ecm_db_connection_protocol_get(ci);
	ecm_db_connection_deref(ci);

	if (ECM_FRONT_END_ACCELERATION_NOT_POSSIBLE(accel_mode)) {
		spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
		cemi->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_NO;
		goto sawf_emesh_classifier_out;
	}

	/*
	 * We are relevant to the connection.
	 * Set the process response to its default value, that is, to
	 * allow the acceleration.
	 */
	became_relevant = ecm_db_time_get();

	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	cemi->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_YES;
	cemi->process_response.became_relevant = became_relevant;

	cemi->process_response.process_actions |= ECM_CLASSIFIER_PROCESS_ACTION_ACCEL_MODE;
	cemi->process_response.accel_mode = ECM_CLASSIFIER_ACCELERATION_MODE_ACCEL;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);

	if (protocol == IPPROTO_TCP) {
		/*
		 * Stop the processing if both side packets are already seen.
		 * Above the process response is already set to allow the acceleration.
		 */
		if (ecm_classifier_emesh_sawf_is_bidi_packet_seen(cemi)) {
			spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
			goto sawf_emesh_classifier_out;
		}

		/*
		 * Store the PCP value in the classifier instance and deny the
		 * acceleration if both side PCP value is not yet available.
		 */
		ecm_classifier_emesh_sawf_fill_pcp(cemi, sender, skb, cake_flow_handle);
		if (!ecm_classifier_emesh_sawf_is_bidi_packet_seen(cemi)) {
			DEBUG_TRACE("%px: Both side PCP value is not yet picked\n", cemi);
			spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
			cemi->process_response.accel_mode = ECM_CLASSIFIER_ACCELERATION_MODE_NO;
			goto sawf_emesh_classifier_out;
		}
	} else {
		/*
		 * If the acceleration delay option is enabled, we will wait
		 * until seeing both side traffic.
		 *
		 * There are 2 options:
		 * Option 1: Wait forever until to see the reply direction traffic
		 * Option 2: Wait for seeing N number of packets. If we still don't see reply,
		 * set the uni-directional values.
		 */
		if (ecm_classifier_accel_delay_pkts) {
			/*
			 * Stop the processing if both side packets are already seen.
			 * Above the process response is already set to allow the
			 * acceleration.
			 */
			if (ecm_classifier_emesh_sawf_is_bidi_packet_seen(cemi)) {
				spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
				goto sawf_emesh_classifier_out;
			}

			/*
			 * Store the PCP value in the classifier instance and allow the
			 * acceleration if both side PCP value is not yet available.
			 */
			ecm_classifier_emesh_sawf_fill_pcp(cemi, sender, skb, cake_flow_handle);
			if (ecm_classifier_emesh_sawf_is_bidi_packet_seen(cemi)) {
				DEBUG_TRACE("%px: Both side PCP value is picked\n", cemi);
				goto done;
			}

			/*
			 * Deny the acceleration if any of the below options holds true.
			 * For option 1, we wait forever
			 * For option 2, we wait until seeing ecm_classifier_accel_delay_pkts.
			 */
			if ((ecm_classifier_accel_delay_pkts == 1) || (slow_pkts < ecm_classifier_accel_delay_pkts)) {
				DEBUG_TRACE("%px: accel_delay_pkts: %d slow_pkts: %llu accel is not allowed yet\n",
						cemi, ecm_classifier_accel_delay_pkts, slow_pkts);
				spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
				cemi->process_response.accel_mode = ECM_CLASSIFIER_ACCELERATION_MODE_NO;
				goto sawf_emesh_classifier_out;
			}
		}

		/*
		 * If we didn't see both direction traffic during the acceleration
		 * delay time, we can allow the acceleration by setting the uni-directional
		 * values to both flow and return PCP.
		 */
		cemi->pcp[ECM_CONN_DIR_FLOW] = skb->priority;
		cemi->pcp[ECM_CONN_DIR_RETURN] = skb->priority;

		/*
		 * In case if SAWF rule matches with reverse direction,
		 * use that priority value.
		 */
		if (is_sawf_relevant && (return_output_params.rule_id != ECM_CLASSIFIER_EMESH_SAWF_INVALID_RULE_LOOKUP)) {
			cemi->pcp[ECM_CONN_DIR_RETURN] = return_output_params.priority;
			/*
			 * Check for CAKE handle for return direction as sawf has updated priority.
			 */
			ecm_classifier_emesh_sawf_check_cake_qdisc(src_dev, &cake_return_handle);
		}

		/*
		 * In case of acceleration delay option is disabled,
		 * CASE 1 : CAKE is enabled on flow direction, put the handle in skb->priority (current
		 * packet) and store the same to pass it to the acceleration engine.
		 * CASE 2 : CAKE is enabled on the return direction, store the handle to pass it to the
		 * acceleration engine.
		 * First clear out upper 16 bits and then put the handle in case of CAKE is enabled.
		 */
		if (cake_flow_handle != ECM_CLASSIFIER_EMESH_SAWF_CAKE_INVALID_HANDLE) {

			skb->priority &= ECM_CLASSIFIER_EMESH_SAWF_CAKE_PRIORITY_MASK;
			skb->priority |= (cake_flow_handle << ECM_CLASSIFIER_EMESH_SAWF_CAKE_HANDLE_SHIFT);
			cemi->pcp[ECM_CONN_DIR_FLOW] &= ECM_CLASSIFIER_EMESH_SAWF_CAKE_PRIORITY_MASK;
			cemi->pcp[ECM_CONN_DIR_FLOW] |= (cake_flow_handle << ECM_CLASSIFIER_EMESH_SAWF_CAKE_HANDLE_SHIFT);
		}

		if (cake_return_handle != ECM_CLASSIFIER_EMESH_SAWF_CAKE_INVALID_HANDLE) {
			cemi->pcp[ECM_CONN_DIR_RETURN] &= ECM_CLASSIFIER_EMESH_SAWF_CAKE_PRIORITY_MASK;
			cemi->pcp[ECM_CONN_DIR_RETURN] |= (cake_return_handle << ECM_CLASSIFIER_EMESH_SAWF_CAKE_HANDLE_SHIFT);
		}
	}

done:
	DEBUG_TRACE("Protocol: %d, Flow Priority: %d, Return priority: %d, sender: %d\n",
			protocol, cemi->pcp[ECM_CONN_DIR_FLOW],
			cemi->pcp[ECM_CONN_DIR_RETURN], sender);

	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);

	cemi->process_response.process_actions |= ECM_CLASSIFIER_PROCESS_ACTION_QOS_TAG;

	if (((sender == ECM_TRACKER_SENDER_TYPE_SRC) && (IP_CT_DIR_ORIGINAL == CTINFO2DIR(ctinfo))) ||
			((sender == ECM_TRACKER_SENDER_TYPE_DEST) && (IP_CT_DIR_REPLY == CTINFO2DIR(ctinfo)))) {
		cemi->process_response.flow_qos_tag = cemi->pcp[ECM_CONN_DIR_FLOW];
		cemi->process_response.return_qos_tag = cemi->pcp[ECM_CONN_DIR_RETURN];
	} else {
		cemi->process_response.flow_qos_tag = cemi->pcp[ECM_CONN_DIR_RETURN];
		cemi->process_response.return_qos_tag = cemi->pcp[ECM_CONN_DIR_FLOW];
	}
sawf_emesh_classifier_out:

	/*
	 * Return our process response
	 * Release the source and destination dev count
	 */
	if (src_dev) {
		dev_put(src_dev);
	}

	if (dest_dev) {
		dev_put(dest_dev);
	}

	*process_response = cemi->process_response;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
}

/*
 * ecm_classifier_emesh_sawf_update_fse_flow()
 *	Update fse flow parameters to wlan host driver when
 *	emesh-sawf connection is accelerated as well as decelerated.
 */
void ecm_classifier_emesh_sawf_update_fse_flow(struct ecm_classifier_instance *aci,
						ecm_classifier_fse_connection_state_t state)
{
	ip_addr_t src_ip;
	ip_addr_t dest_ip;
	struct ecm_classifier_fse_info fse_info;
	struct ecm_classifier_emesh_sawf_instance *cemi;
	struct ecm_db_connection_instance *ci;

	/*
	 * Return if fse callback is not registered.
	 */
	if (!ecm_emesh.update_fse_flow_info) {
		DEBUG_WARN("fse callback is not registered\n");
		return;
	}

	cemi = (struct ecm_classifier_emesh_sawf_instance *)aci;
	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed", cemi);

	/*
	 * Return if the connection does not have emesh SAWF classifier.
	 */
	if (cemi->type != ECM_CLASSIFIER_SAWF) {
		DEBUG_WARN("%px: No emesh SAWF classifier present\n", cemi);
		return;
	}

	ci = ecm_db_connection_serial_find_and_ref(cemi->ci_serial);
	if (!ci) {
		DEBUG_WARN("%px: No ci found for %u\n", cemi, cemi->ci_serial);
		return;
	}

	/*
	 * Get the five tuple information.
	 */
	fse_info.ip_version = ecm_db_connection_ip_version_get(ci);
	fse_info.protocol = ecm_db_connection_protocol_get(ci);
	fse_info.src_port = htons(ecm_db_connection_port_get(ci, ECM_DB_OBJ_DIR_FROM));
	fse_info.dest_port = htons(ecm_db_connection_port_get(ci, ECM_DB_OBJ_DIR_TO));
	ecm_db_connection_address_get(ci, ECM_DB_OBJ_DIR_FROM, src_ip);
	ecm_db_connection_address_get(ci, ECM_DB_OBJ_DIR_TO, dest_ip);

	ecm_db_connection_deref(ci);
	if (fse_info.ip_version == 4) {
		ECM_IP_ADDR_TO_NIN4_ADDR(fse_info.src.v4_addr, src_ip);
		ECM_IP_ADDR_TO_NIN4_ADDR(fse_info.dest.v4_addr, dest_ip);
	} else if (fse_info.ip_version == 6) {
		ECM_IP_ADDR_TO_NIN6_ADDR(fse_info.src.v6_addr, src_ip);
		ECM_IP_ADDR_TO_NIN6_ADDR(fse_info.dest.v6_addr, dest_ip);
	} else {
		DEBUG_WARN("Wrong IP protocol: %d\n", fse_info.ip_version);
		return;
	}

	ecm_emesh.update_fse_flow_info(&fse_info, state);
}

/*
 * ecm_classifier_emesh_sawf_update_ul_param_on_conn_decel()
 *	Update SAWF uplink parameters to wlan host driver when a connection gets decelerated in ECM
 */
void ecm_classifier_emesh_sawf_update_ul_param_on_conn_decel(struct ecm_classifier_instance *aci, struct ecm_classifier_rule_sync *sync)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	struct ecm_db_connection_instance *ci;
	uint8_t dmac[ETH_ALEN], smac[ETH_ALEN];
	uint8_t forward_service_id, reverse_service_id;

	cemi = (struct ecm_classifier_emesh_sawf_instance *)aci;
	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed", cemi);

	/*
	 * Return if SAWF functionality is not enabled or we have already sent
	 * the parameters to host in one of the previous sync calls
	 */
	if (!ecm_classifier_sawf_enabled || cemi->ul_parameters_sync[ECM_CLASSIFIER_EMESH_MODE_DECEL]) {
		return;
	}

	ci = ecm_db_connection_serial_find_and_ref(cemi->ci_serial);
	if (!ci) {
		DEBUG_WARN("%px: No ci found for %u\n", cemi, cemi->ci_serial);
		return;
	}

	/*
	 * Get mac address for destination node
	 */
	ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_TO, dmac);

	/*
	 * Get mac address for source node
	 */
	ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_FROM, smac);

	/*
	 * Config sawf uplink parameters.
	 * Service ID is in 16-23 bits of flow_sawf_metadata and return_sawf_metadata.
	 */
	if (ecm_emesh.update_sawf_ul) {
		forward_service_id = cemi->process_response.flow_sawf_metadata >> ECM_CLASSIFIER_EMESH_SAWF_SERVICE_CLASS_SHIFT;
		reverse_service_id = cemi->process_response.return_sawf_metadata >> ECM_CLASSIFIER_EMESH_SAWF_SERVICE_CLASS_SHIFT;
		DEBUG_INFO("%px: SAWF UL forward service id : %x reverse service id : %x\n", cemi, forward_service_id, reverse_service_id);
		ecm_emesh.update_sawf_ul(dmac, smac, forward_service_id, reverse_service_id,
					ECM_CLASSIFIER_EMESH_SAWF_SUB_FLOW);
		cemi->ul_parameters_sync[ECM_CLASSIFIER_EMESH_MODE_DECEL] = true;
		cemi->ul_parameters_sync[ECM_CLASSIFIER_EMESH_MODE_ACCEL] = false;
	}

	ecm_db_connection_deref(ci);
}


/*
 * ecm_classifier_emesh_sawf_update_latency_param_on_conn_decel()
 *	Update mesh latency parameters to wlan host driver when a connection gets decelerated in ECM
 */
void ecm_classifier_emesh_sawf_update_latency_param_on_conn_decel(struct ecm_classifier_instance *aci, struct ecm_classifier_rule_sync *sync)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	struct ecm_db_connection_instance *ci;
	uint8_t peer_mac[ETH_ALEN];

	cemi = (struct ecm_classifier_emesh_sawf_instance *)aci;
	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed", cemi);

	/*
	 * Return if E-Mesh functionality is not enabled.
	 */
	if (!ecm_classifier_emesh_enabled) {
		return;
	}

	if (!(ecm_classifier_emesh_latency_config_enabled
				& ECM_CLASSIFIER_EMESH_ENABLE_LATENCY_UPDATE)) {
		return;
	}

	if (!ecm_emesh.update_peer_mesh_latency_params) {
		return;
	}

	ci = ecm_db_connection_serial_find_and_ref(cemi->ci_serial);
	if (!ci) {
		DEBUG_WARN("%px: No ci found for %u\n", cemi, cemi->ci_serial);
		return;
	}

	/*
	 * Get mac address for destination node
	 */
	ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_TO, peer_mac);
	ecm_emesh.update_peer_mesh_latency_params(peer_mac,
			cemi->service_interval_dl, cemi->burst_size_dl, cemi->service_interval_ul, cemi->burst_size_ul,
			cemi->pcp[ECM_CONN_DIR_FLOW], ECM_CLASSIFIER_EMESH_SUB_LATENCY_PARAMS);

	/*
	 * Get mac address for source node
	 */
	ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_FROM, peer_mac);
	ecm_emesh.update_peer_mesh_latency_params(peer_mac,
			cemi->service_interval_dl, cemi->burst_size_dl, cemi->service_interval_ul, cemi->burst_size_ul,
			cemi->pcp[ECM_CONN_DIR_FLOW], ECM_CLASSIFIER_EMESH_SUB_LATENCY_PARAMS);

	ecm_db_connection_deref(ci);
}

/*
 * ecm_classifier_emesh_sawf_sync_to_v4()
 *	Front end is pushing accel engine state to us
 */
static void ecm_classifier_emesh_sawf_sync_to_v4(struct ecm_classifier_instance *aci, struct ecm_classifier_rule_sync *sync)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	cemi = (struct ecm_classifier_emesh_sawf_instance *)aci;
	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed", cemi);

	switch(sync->reason) {
	case ECM_FRONT_END_IPV4_RULE_SYNC_REASON_FLUSH:
	case ECM_FRONT_END_IPV4_RULE_SYNC_REASON_EVICT:
	case ECM_FRONT_END_IPV4_RULE_SYNC_REASON_DESTROY:
		ecm_classifier_emesh_sawf_update_latency_param_on_conn_decel(aci, sync);
		ecm_classifier_emesh_sawf_update_ul_param_on_conn_decel(aci, sync);
		ecm_classifier_emesh_sawf_update_fse_flow(aci, ECM_CLASSIFIER_SAWF_FSE_CONNECTION_STATE_DECEL);
		break;
	default:
		break;
	}
}

/*
 * ecm_classifier_emesh_sawf_update_ul_params_on_conn_accel()
 *	Update SAWF uplink parameters associated with SP rule to wlan host driver
 *	when a connection getting accelerated in ECM
 */
static void ecm_classifier_emesh_sawf_update_ul_params_on_conn_accel(struct ecm_classifier_instance *aci,
		struct ecm_classifier_rule_create *ecrc)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	struct ecm_db_connection_instance *ci;
	uint8_t dmac[ETH_ALEN];
	uint8_t smac[ETH_ALEN];
	uint8_t forward_service_id, reverse_service_id;

	cemi = (struct ecm_classifier_emesh_sawf_instance *)aci;
	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed", cemi);

	/*
	 * Return if SAWF functionality is not enabled or we have already sent
	 * the parameters to host in one of the previous sync calls
	 */
	if (!ecm_classifier_sawf_enabled || cemi->ul_parameters_sync[ECM_CLASSIFIER_EMESH_MODE_ACCEL]) {
		return;
	}

	ci = ecm_db_connection_serial_find_and_ref(cemi->ci_serial);
	if (!ci) {
		DEBUG_WARN("%px: No ci found for %u\n", cemi, cemi->ci_serial);
		return;
	}

	ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_FROM, smac);
	ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_TO, dmac);

	/*
	 * Config sawf uplink parameters.
	 * Service ID is in 16-23 bits of flow_sawf_metadata and return_sawf_metadata
	 */
	if (ecm_emesh.update_sawf_ul) {
		forward_service_id = cemi->process_response.flow_sawf_metadata >> ECM_CLASSIFIER_EMESH_SAWF_SERVICE_CLASS_SHIFT;
		reverse_service_id = cemi->process_response.return_sawf_metadata >> ECM_CLASSIFIER_EMESH_SAWF_SERVICE_CLASS_SHIFT;
		DEBUG_INFO("%px: SAWF UL forward service id : %x reverse service id : %x\n", cemi, forward_service_id, reverse_service_id);
		ecm_emesh.update_sawf_ul(dmac, smac, forward_service_id, reverse_service_id,
					ECM_CLASSIFIER_EMESH_SAWF_ADD_FLOW);
		cemi->ul_parameters_sync[ECM_CLASSIFIER_EMESH_MODE_ACCEL] = true;
		cemi->ul_parameters_sync[ECM_CLASSIFIER_EMESH_MODE_DECEL] = false;
	}

	ecm_db_connection_deref(ci);
}


/*
 * ecm_classifier_emesh_sawf_update_wlan_latency_params_on_conn_accel()
 *	Update wifi latency parameters associated with SP rule to wlan host driver
 *	when a connection getting accelerated in ECM
 */
static void ecm_classifier_emesh_sawf_update_wlan_latency_params_on_conn_accel(struct ecm_classifier_instance *aci,
		struct ecm_classifier_rule_create *ecrc)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	struct ecm_db_connection_instance *ci;
	uint8_t service_interval_dl;
	uint32_t burst_size_dl;
	uint8_t service_interval_ul;
	uint32_t burst_size_ul;
	struct sk_buff *skb;
	uint8_t dmac[ETH_ALEN];
	uint8_t smac[ETH_ALEN];

	/*
	 * Return if E-Mesh functionality is not enabled.
	 */
	if (!ecm_classifier_emesh_enabled) {
		return;
	}

	if (!(ecm_classifier_emesh_latency_config_enabled
			& ECM_CLASSIFIER_EMESH_ENABLE_LATENCY_UPDATE)) {
		/*
		 * Flow based latency parameter updation to WLAN host driver not enabled
		 */
		return;
	}

	/*
	 * When mesh low latency feature flags is enabled, ECM gets
	 * latency config parameters associated with a SPM rule and send
	 * to WLAN host driver invoking callback
	 */
	if (!ecm_emesh.update_peer_mesh_latency_params) {
		return;
	}

	skb = ecrc->skb;

	cemi = (struct ecm_classifier_emesh_sawf_instance *)aci;
	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed", cemi);

	ci = ecm_db_connection_serial_find_and_ref(cemi->ci_serial);
	if (!ci) {
		DEBUG_WARN("%px: No ci found for %u\n", cemi, cemi->ci_serial);
		return;
	}

	/*
	 * Invoke SPM rule lookup API to update skb priority
	 * When latency config is enabled, fetch latency parameter
	 * associated with a SPM rule.Since we do not know direction of
	 * connection, we get src and destination mac address of both
	 * connection and let wlan driver find corresponding wlan peer
	 * connected
	 */
	ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_FROM, smac);
	ecm_db_connection_node_address_get(ci, ECM_DB_OBJ_DIR_TO, dmac);
	sp_mapdb_get_wlan_latency_params(skb, &service_interval_dl, &burst_size_dl,
			&service_interval_ul, &burst_size_ul, smac, dmac);

	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);

	/*
	 * Update latency parameters to accelerated connection
	 */
	cemi->service_interval_dl = service_interval_dl;
	cemi->burst_size_dl = burst_size_dl;
	cemi->service_interval_ul = service_interval_ul;
	cemi->burst_size_ul = burst_size_ul;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);

	/*
	 * If one of the latency parameters are zero, there could be
	 * 2 possibilities - 1. no rule match 2. sp rule does not have
	 * latency parameter configured.
	 */
	if ((service_interval_ul && burst_size_ul) || (service_interval_dl && burst_size_dl)) {
		/*
		 * Send destination mac address of this connection
		 */
		ecm_emesh.update_peer_mesh_latency_params(dmac,
				service_interval_dl, burst_size_dl, service_interval_ul, burst_size_ul,
				skb->priority, ECM_CLASSIFIER_EMESH_ADD_LATENCY_PARAMS);
	}

	/*
	 * Get latency parameter for other direction
	 */
	sp_mapdb_get_wlan_latency_params(skb, &service_interval_dl, &burst_size_dl,
			&service_interval_ul, &burst_size_ul, dmac, smac);

	if ((service_interval_ul && burst_size_ul) || (service_interval_dl && burst_size_dl)) {
		/*
		 * Send source mac address of this connection
		 */
		ecm_emesh.update_peer_mesh_latency_params(smac,
				service_interval_dl, burst_size_dl, service_interval_ul, burst_size_ul,
				skb->priority, ECM_CLASSIFIER_EMESH_ADD_LATENCY_PARAMS);
	}

	ecm_db_connection_deref(ci);
}

/*
 * ecm_classifier_emesh_sawf_sync_from_v4()
 *	Front end is retrieving accel engine state from us
 */
static void ecm_classifier_emesh_sawf_sync_from_v4(struct ecm_classifier_instance *aci, struct ecm_classifier_rule_create *ecrc)
{
	ecm_classifier_emesh_sawf_update_wlan_latency_params_on_conn_accel(aci, ecrc);
	ecm_classifier_emesh_sawf_update_ul_params_on_conn_accel(aci, ecrc);
	ecm_classifier_emesh_sawf_update_fse_flow(aci, ECM_CLASSIFIER_SAWF_FSE_CONNECTION_STATE_ACCEL);
}

/*
 * ecm_classifier_emesh_sawf_sync_to_v6()
 *	Front end is pushing accel engine state to us
 */
static void ecm_classifier_emesh_sawf_sync_to_v6(struct ecm_classifier_instance *aci, struct ecm_classifier_rule_sync *sync)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	cemi = (struct ecm_classifier_emesh_sawf_instance *)aci;
	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed", cemi);

	switch(sync->reason) {
	case ECM_FRONT_END_IPV6_RULE_SYNC_REASON_FLUSH:
	case ECM_FRONT_END_IPV6_RULE_SYNC_REASON_EVICT:
	case ECM_FRONT_END_IPV6_RULE_SYNC_REASON_DESTROY:
		ecm_classifier_emesh_sawf_update_latency_param_on_conn_decel(aci, sync);
		ecm_classifier_emesh_sawf_update_ul_param_on_conn_decel(aci, sync);
		ecm_classifier_emesh_sawf_update_fse_flow(aci, ECM_CLASSIFIER_SAWF_FSE_CONNECTION_STATE_DECEL);
		break;
	default:
		break;
	}
}

/*
 * ecm_classifier_emesh_sawf_sync_from_v6()
 *	Front end is retrieving accel engine state from us
 */
static void ecm_classifier_emesh_sawf_sync_from_v6(struct ecm_classifier_instance *aci, struct ecm_classifier_rule_create *ecrc)
{
	ecm_classifier_emesh_sawf_update_wlan_latency_params_on_conn_accel(aci, ecrc);
	ecm_classifier_emesh_sawf_update_ul_params_on_conn_accel(aci, ecrc);
	ecm_classifier_emesh_sawf_update_fse_flow(aci, ECM_CLASSIFIER_SAWF_FSE_CONNECTION_STATE_ACCEL);
}

/*
 * ecm_classifier_emesh_sawf_type_get()
 *	Get type of classifier this is
 */
static ecm_classifier_type_t ecm_classifier_emesh_sawf_type_get(struct ecm_classifier_instance *ci)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	cemi = (struct ecm_classifier_emesh_sawf_instance *)ci;

	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed\n", cemi);
	return ECM_CLASSIFIER_TYPE_EMESH;
}

/*
 * ecm_classifier_emesh_sawf_last_process_response_get()
 *	Get result code returned by the last process call
 */
static void ecm_classifier_emesh_sawf_last_process_response_get(struct ecm_classifier_instance *ci,
							struct ecm_classifier_process_response *process_response)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;

	cemi = (struct ecm_classifier_emesh_sawf_instance *)ci;
	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed\n", cemi);

	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	*process_response = cemi->process_response;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
}

/*
 * ecm_classifier_emesh_sawf_reclassify_allowed()
 *	Indicate if reclassify is allowed
 */
static bool ecm_classifier_emesh_sawf_reclassify_allowed(struct ecm_classifier_instance *ci)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	cemi = (struct ecm_classifier_emesh_sawf_instance *)ci;
	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed\n", cemi);

	return true;
}

/*
 * ecm_classifier_emesh_sawf_reclassify()
 *	Reclassify
 */
static void ecm_classifier_emesh_sawf_reclassify(struct ecm_classifier_instance *ci)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	cemi = (struct ecm_classifier_emesh_sawf_instance *)ci;
	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed\n", cemi);

	/*
	 * Revert back to MAYBE relevant - we will evaluate when we get the next process() call.
	 */
	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	cemi->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_MAYBE;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
}

#ifdef ECM_STATE_OUTPUT_ENABLE
/*
 * ecm_classifier_emesh_sawf_state_get()
 *	Return state
 */
static int ecm_classifier_emesh_sawf_state_get(struct ecm_classifier_instance *ci, struct ecm_state_file_instance *sfi)
{
	int result;
	struct ecm_classifier_emesh_sawf_instance *cemi;
	struct ecm_classifier_process_response process_response;

	cemi = (struct ecm_classifier_emesh_sawf_instance *)ci;
	DEBUG_CHECK_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC, "%px: magic failed", cemi);

	if ((result = ecm_state_prefix_add(sfi, "emesh"))) {
		return result;
	}

	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	process_response = cemi->process_response;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);

	/*
	 * Output our last process response
	 */
	if ((result = ecm_classifier_process_response_state_get(sfi, &process_response))) {
		return result;
	}

	return ecm_state_prefix_remove(sfi);
}
#endif

/*
 * ecm_classifier_emesh_sawf_should_keep_connection()
 *	In case of SAWF, we defunct the connection based on MAC at the time of STA join event
 *	as well, so will return false in case if the event is STA join and the connection is SAWF.
 */
static void ecm_classifier_emesh_sawf_should_keep_connection(struct ecm_classifier_instance *aci,
								struct ecm_db_connection_defunct_info *info)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;
	if (info->type != ECM_DB_CONNECTION_DEFUNCT_TYPE_STA_JOIN) {
		/*
		 * Classifier does not care about the connection deletion.
		 */
		return;
	}

	/*
	 * In case of STA join event, we will let only SAWF connections be defuncted
	 * and not the EMESH connections.
	 */
	cemi = (struct ecm_classifier_emesh_sawf_instance *)aci;
	if (cemi->type == ECM_CLASSIFIER_SAWF) {
		info->should_keep_connection = false;
	}
}

/*
 * ecm_classifier_emesh_sawf_instance_alloc()
 *	Allocate an instance of the EMESH classifier
 */
struct ecm_classifier_emesh_sawf_instance *ecm_classifier_emesh_sawf_instance_alloc(struct ecm_db_connection_instance *ci)
{
	struct ecm_classifier_emesh_sawf_instance *cemi;

	/*
	 * Allocate the instance
	 */
	cemi = (struct ecm_classifier_emesh_sawf_instance *)kzalloc(sizeof(struct ecm_classifier_emesh_sawf_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!cemi) {
		DEBUG_WARN("Failed to allocate EMESH instance\n");
		return NULL;
	}

	DEBUG_SET_MAGIC(cemi, ECM_CLASSIFIER_EMESH_INSTANCE_MAGIC);
	cemi->refs = 1;
	cemi->base.process = ecm_classifier_emesh_sawf_process;
	cemi->base.sync_from_v4 = ecm_classifier_emesh_sawf_sync_from_v4;
	cemi->base.sync_to_v4 = ecm_classifier_emesh_sawf_sync_to_v4;
	cemi->base.sync_from_v6 = ecm_classifier_emesh_sawf_sync_from_v6;
	cemi->base.sync_to_v6 = ecm_classifier_emesh_sawf_sync_to_v6;
	cemi->base.type_get = ecm_classifier_emesh_sawf_type_get;
	cemi->base.last_process_response_get = ecm_classifier_emesh_sawf_last_process_response_get;
	cemi->base.reclassify_allowed = ecm_classifier_emesh_sawf_reclassify_allowed;
	cemi->base.reclassify = ecm_classifier_emesh_sawf_reclassify;
#ifdef ECM_STATE_OUTPUT_ENABLE
	cemi->base.state_get = ecm_classifier_emesh_sawf_state_get;
#endif
	cemi->base.ref = ecm_classifier_emesh_sawf_ref;
	cemi->base.deref = ecm_classifier_emesh_sawf_deref;
	cemi->base.update = ecm_classifier_emesh_sawf_update;
	cemi->base.should_keep_connection = ecm_classifier_emesh_sawf_should_keep_connection;
	cemi->ci_serial = ecm_db_connection_serial_get(ci);
	cemi->process_response.process_actions = 0;
	cemi->process_response.relevance = ECM_CLASSIFIER_RELEVANCE_MAYBE;

	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);

	/*
	 * Final check if we are pending termination
	 */
	if (ecm_classifier_emesh_sawf_terminate_pending) {
		spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
		DEBUG_INFO("%px: Terminating\n", ci);
		kfree(cemi);
		return NULL;
	}

	/*
	 * Link the new instance into our list at the head
	 */
	cemi->next = ecm_classifier_emesh_sawf_instances;
	if (ecm_classifier_emesh_sawf_instances) {
		ecm_classifier_emesh_sawf_instances->prev = cemi;
	}
	ecm_classifier_emesh_sawf_instances = cemi;

	/*
	 * Increment stats
	 */
	ecm_classifier_emesh_sawf_count++;
	DEBUG_ASSERT(ecm_classifier_emesh_sawf_count > 0, "%px: ecm_classifier_emesh_sawf_count wrap\n", cemi);
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);

	DEBUG_INFO("EMESH instance alloc: %px\n", cemi);
	return cemi;
}
EXPORT_SYMBOL(ecm_classifier_emesh_sawf_instance_alloc);

/*
 * ecm_db_connection_make_defunct_sawf_connections()
 *      Make defunct all connections that are currently assigned to a classifier of the given type
 */
void ecm_db_connection_make_defunct_sawf_connections(uint16_t rule_id)
{
	struct ecm_db_connection_instance *ci;
	DEBUG_INFO("Make defunct all connections assigned to SAWF\n");
	ci = ecm_db_connections_get_and_ref_first();
	while (ci) {
		struct ecm_db_connection_instance *cin;
		struct ecm_classifier_instance *eci;
		struct ecm_classifier_emesh_sawf_instance *cemi;

		eci = ecm_db_connection_assigned_classifier_find_and_ref(ci, ECM_CLASSIFIER_TYPE_EMESH);
		if (!eci) {
			goto next_ci;
		}

		cemi = (struct ecm_classifier_emesh_sawf_instance *)eci;
		if (cemi->type == ECM_CLASSIFIER_SAWF &&
				(cemi->flow_rule_id == rule_id || cemi->return_rule_id == rule_id)) {
			DEBUG_INFO("%px Defuncting the connection\n", ci);
			ecm_db_connection_make_defunct(ci);
		}

		eci->deref(eci);
next_ci:
		cin = ecm_db_connection_get_and_ref_next(ci);
		ecm_db_connection_deref(ci);
		ci = cin;
	}
}

/*
 * ecm_classifier_sawf_emesh_get_udp_ipsec_port()
 */
static int ecm_classifier_sawf_emesh_get_udp_ipsec_port(void *data, u64 *val)
{
	*val = ecm_classifier_sawf_emesh_udp_ipsec_port;

	return 0;
}

/*
 * ecm_classifier_mscs_scs_set_udp_ipsec_port()
 */
static int ecm_classifier_sawf_emesh_set_udp_ipsec_port(void *data, u64 val)
{
	DEBUG_TRACE("ecm_classifier_sawf_emesh_udp_ipsec_port = %u\n", (uint32_t)val);

	if (val != 5200) {
		DEBUG_WARN("Invalid value: %u. Valid value is 5200.\n", (uint32_t)val);
		return -EINVAL;
	}

	ecm_classifier_sawf_emesh_udp_ipsec_port = (uint32_t)val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(ecm_classifier_sawf_emesh_udp_ipsec_port_fops, ecm_classifier_sawf_emesh_get_udp_ipsec_port, ecm_classifier_sawf_emesh_set_udp_ipsec_port, "%llu\n");

/*
 * ecm_classifier_emesh_sawf_spm_notifier_callback()
 *	Callback for service prioritization notification update.
 */
static int ecm_classifier_emesh_sawf_spm_notifier_callback(struct notifier_block *nb, unsigned long event, void *data)
{
	ip_addr_t ip_addr;
	struct in6_addr ipv6addr = IN6ADDR_ANY_INIT;
	struct sp_rule *r = (struct sp_rule *)data;
	uint32_t valid_flag_emesh = r->inner.flags;
	uint32_t valid_flag_sawf = r->inner.flags_sawf;

	/*
	 * Return if E-Mesh or SAWF functionality is not enabled or the rule
	 * classifier type is SCS.
	 */
	if (r->classifier_type == SP_RULE_TYPE_SCS || r->classifier_type == SP_RULE_TYPE_MSCS) {
		DEBUG_INFO("Not an EMESH / SAWF rule notification !\n");
		return NOTIFY_DONE;
	}

	if (!ecm_classifier_emesh_enabled && !ecm_classifier_sawf_enabled) {
		return NOTIFY_DONE;
	}

	DEBUG_INFO("SP rule update notification received\n");
	if (ecm_classifier_sawf_enabled || r->classifier_type != SP_RULE_TYPE_MESH) {

		switch(event) {
		case SP_MAPDB_REMOVE_RULE:
		case SP_MAPDB_MODIFY_RULE:
			ecm_db_connection_make_defunct_sawf_connections(r->id);
			break;
		case SP_MAPDB_ADD_RULE:

			/*
			 * At add rule notification, defunct already exsisting connections in
			 * matching rule fields in certain order of priority of rule fields.
			 */
			goto defunct_by_priority;
		}

		return NOTIFY_DONE;
	}

defunct_by_priority:
	/*
	 * Order of priority of rule fields to match and flush connections:
	 * Port ---> IP address ---> Mac Address ---> Protocol
	 * Flush connections for both directions as ECM creates reverse
	 * direction rule as well
	 */
	if ((valid_flag_emesh & SP_RULE_FLAG_MATCH_SRC_PORT) || (valid_flag_sawf & SP_RULE_FLAG_MATCH_SAWF_SRC_PORT)) {
		uint16_t src_port = r->inner.src_port;
		if (r->classifier_type != SP_RULE_TYPE_MESH)
			src_port = htons(r->inner.src_port);

		ecm_db_connection_defunct_by_port(src_port, ECM_DB_OBJ_DIR_FROM);
		ecm_db_connection_defunct_by_port(src_port, ECM_DB_OBJ_DIR_TO);
		return NOTIFY_DONE;
	}

	if ((valid_flag_emesh & SP_RULE_FLAG_MATCH_DST_PORT) || (valid_flag_sawf & SP_RULE_FLAG_MATCH_SAWF_DST_PORT)) {
		uint16_t dst_port = r->inner.dst_port;
		if (r->classifier_type != SP_RULE_TYPE_MESH)
			dst_port = htons(r->inner.dst_port);

		ecm_db_connection_defunct_by_port(dst_port, ECM_DB_OBJ_DIR_FROM);
		ecm_db_connection_defunct_by_port(dst_port, ECM_DB_OBJ_DIR_TO);
		return NOTIFY_DONE;
	}

	if ((valid_flag_emesh & SP_RULE_FLAG_MATCH_SRC_IPV4) || (valid_flag_sawf & SP_RULE_FLAG_MATCH_SAWF_SRC_IPV4)) {
		ECM_NIN4_ADDR_TO_IP_ADDR(ip_addr, r->inner.src_ipv4_addr);
		ecm_db_host_connections_defunct_by_dir(ip_addr, ECM_DB_OBJ_DIR_FROM);
		ecm_db_host_connections_defunct_by_dir(ip_addr, ECM_DB_OBJ_DIR_TO);
		return NOTIFY_DONE;
	}

	if ((valid_flag_emesh & SP_RULE_FLAG_MATCH_DST_IPV4) || (valid_flag_sawf & SP_RULE_FLAG_MATCH_SAWF_DST_IPV4)) {
		ECM_NIN4_ADDR_TO_IP_ADDR(ip_addr, r->inner.dst_ipv4_addr);
		ecm_db_host_connections_defunct_by_dir(ip_addr, ECM_DB_OBJ_DIR_FROM);
		ecm_db_host_connections_defunct_by_dir(ip_addr, ECM_DB_OBJ_DIR_TO);
		return NOTIFY_DONE;
	}

	if ((valid_flag_emesh & SP_RULE_FLAG_MATCH_SRC_IPV6) || (valid_flag_sawf & SP_RULE_FLAG_MATCH_SAWF_SRC_IPV6)) {
		memcpy(ipv6addr.s6_addr32, r->inner.src_ipv6_addr, sizeof(struct in6_addr));
		ECM_NIN6_ADDR_TO_IP_ADDR(ip_addr, ipv6addr);
		ecm_db_host_connections_defunct_by_dir(ip_addr, ECM_DB_OBJ_DIR_FROM);
		ecm_db_host_connections_defunct_by_dir(ip_addr, ECM_DB_OBJ_DIR_TO);
		return NOTIFY_DONE;
	}

	if ((valid_flag_emesh & SP_RULE_FLAG_MATCH_DST_IPV6) || (valid_flag_sawf & SP_RULE_FLAG_MATCH_SAWF_DST_IPV6)) {
		memcpy(ipv6addr.s6_addr32, r->inner.dst_ipv6_addr, sizeof(struct in6_addr));
		ECM_NIN6_ADDR_TO_IP_ADDR(ip_addr, ipv6addr);
		ecm_db_host_connections_defunct_by_dir(ip_addr, ECM_DB_OBJ_DIR_FROM);
		ecm_db_host_connections_defunct_by_dir(ip_addr, ECM_DB_OBJ_DIR_TO);
		return NOTIFY_DONE;
	}

	if ((valid_flag_emesh & SP_RULE_FLAG_MATCH_SOURCE_MAC) || (valid_flag_sawf & SP_RULE_FLAG_MATCH_SAWF_SOURCE_MAC)) {
		ecm_interface_node_connections_defunct((uint8_t *)r->inner.sa, ECM_DB_IP_VERSION_IGNORE);
		return NOTIFY_DONE;
	}

	if ((valid_flag_emesh & SP_RULE_FLAG_MATCH_DST_MAC) || (valid_flag_sawf & SP_RULE_FLAG_MATCH_SAWF_DST_MAC)) {
		ecm_interface_node_connections_defunct((uint8_t *)r->inner.da, ECM_DB_IP_VERSION_IGNORE);
		return NOTIFY_DONE;
	}

	if ((valid_flag_emesh & SP_RULE_FLAG_MATCH_PROTOCOL) || (valid_flag_sawf & SP_RULE_FLAG_MATCH_SAWF_PROTOCOL)) {
		ecm_db_connection_defunct_by_protocol(r->inner.protocol_number);
		return NOTIFY_DONE;
	}

	/*
	 * Destroy all the connections that are currently assigned to Emesh classifier
	 * The usage of the incoming parameters in this service prioritization
	 * callback will be done in future to perform more refined flush of
	 * connections.
	 */
	ecm_db_connection_make_defunct_by_assignment_type(ECM_CLASSIFIER_TYPE_EMESH);
	return NOTIFY_DONE;
}

/*
 * ecm_classifier_emesh_latency_config_callback_register()
 */
int ecm_classifier_emesh_latency_config_callback_register(struct ecm_classifier_emesh_sawf_callbacks *emesh_cb)
{
	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	if (ecm_emesh.update_peer_mesh_latency_params) {
		spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
		DEBUG_ERROR("EMESH latency config callbacks are registered\n");
		return -1;
	}

	ecm_emesh.update_peer_mesh_latency_params = emesh_cb->update_peer_mesh_latency_params;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
	return 0;
}
EXPORT_SYMBOL(ecm_classifier_emesh_latency_config_callback_register);

/*
 * ecm_classifier_emesh_latency_config_callback_unregister()
 */
void ecm_classifier_emesh_latency_config_callback_unregister(void)
{
	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	ecm_emesh.update_peer_mesh_latency_params = NULL;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
}
EXPORT_SYMBOL(ecm_classifier_emesh_latency_config_callback_unregister);

/*
 * ecm_classifier_emesh_sawf_msduq_callback_register()
 */
int ecm_classifier_emesh_sawf_msduq_callback_register(struct ecm_classifier_emesh_sawf_callbacks *emesh_cb)
{
	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	if (ecm_emesh.update_service_id_get_msduq) {
		spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
		DEBUG_ERROR("SAWF EMESH msduq callbacks are registered\n");
		return -1;
	}

	ecm_emesh.update_service_id_get_msduq = emesh_cb->update_service_id_get_msduq;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
	return 0;
}
EXPORT_SYMBOL(ecm_classifier_emesh_sawf_msduq_callback_register);

/*
 * ecm_classifier_emesh_sawf_msduq_callback_unregister()
 */
void ecm_classifier_emesh_sawf_msduq_callback_unregister(void)
{
	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	ecm_emesh.update_service_id_get_msduq = NULL;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
}
EXPORT_SYMBOL(ecm_classifier_emesh_sawf_msduq_callback_unregister);

/*
 * ecm_classifier_emesh_sawf_config_ul_callback_register()
 */
int ecm_classifier_emesh_sawf_config_ul_callback_register(struct ecm_classifier_emesh_sawf_callbacks *emesh_cb)
{
	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	if (ecm_emesh.update_sawf_ul) {
		spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
		DEBUG_ERROR("SAWF EMESH config uplink callbacks are registered\n");
		return -1;
	}

	ecm_emesh.update_sawf_ul = emesh_cb->update_sawf_ul;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
	return 0;
}
EXPORT_SYMBOL(ecm_classifier_emesh_sawf_config_ul_callback_register);

/*
 * ecm_classifier_emesh_sawf_config_ul_callback_unregister()
 */
void ecm_classifier_emesh_sawf_config_ul_callback_unregister(void)
{
	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	ecm_emesh.update_sawf_ul = NULL;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
}
EXPORT_SYMBOL(ecm_classifier_emesh_sawf_config_ul_callback_unregister);

/*
 * ecm_classifier_emesh_sawf_update_fse_flow_callback_register()
 */
int ecm_classifier_emesh_sawf_update_fse_flow_callback_register(struct ecm_classifier_emesh_sawf_callbacks *emesh_cb)
{
	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	if (ecm_emesh.update_fse_flow_info) {
		spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
		DEBUG_ERROR("SAWF EMESH update fse flow callbacks are registered\n");
		return -1;
	}

	ecm_emesh.update_fse_flow_info = emesh_cb->update_fse_flow_info;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
	return 0;
}
EXPORT_SYMBOL(ecm_classifier_emesh_sawf_update_fse_flow_callback_register);

/*
 * ecm_classifier_emesh_sawf_update_fse_flow_callback_unregister()
 */
void ecm_classifier_emesh_sawf_update_fse_flow_callback_unregister(void)
{
	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	ecm_emesh.update_fse_flow_info = NULL;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);
}
EXPORT_SYMBOL(ecm_classifier_emesh_sawf_update_fse_flow_callback_unregister);

/*
 * ecm_classifier_emesh_sawf_spm_notifier
 *	Registration for SPM rule update events
 */
static struct notifier_block ecm_classifier_emesh_sawf_spm_notifier __read_mostly = {
	.notifier_call = ecm_classifier_emesh_sawf_spm_notifier_callback,
};

/*
 * ecm_classifier_emesh_sawf_init()
 */
int ecm_classifier_emesh_sawf_init(struct dentry *dentry)
{
	DEBUG_INFO("SAWF EMESH classifier Module init\n");

	ecm_classifier_emesh_sawf_dentry = debugfs_create_dir("ecm_classifier_emesh", dentry);
	if (!ecm_classifier_emesh_sawf_dentry) {
		DEBUG_ERROR("Failed to create ecm emesh directory in debugfs\n");
		return -1;
	}

	if (!debugfs_create_u32("enabled", S_IRUGO | S_IWUSR, ecm_classifier_emesh_sawf_dentry,
				(u32 *)&ecm_classifier_emesh_enabled)) {
		DEBUG_ERROR("Failed to create ecm emesh classifier enabled file in debugfs\n");
		debugfs_remove_recursive(ecm_classifier_emesh_sawf_dentry);
		return -1;
	}

	if (!debugfs_create_u32("latency_config_enabled", S_IRUGO | S_IWUSR, ecm_classifier_emesh_sawf_dentry,
				(u32 *)&ecm_classifier_emesh_latency_config_enabled)) {
		DEBUG_ERROR("Failed to create ecm emesh classifier latency config enabled file in debugfs\n");
		debugfs_remove_recursive(ecm_classifier_emesh_sawf_dentry);
		return -1;
	}

	if (!debugfs_create_u32("sawf_enabled", S_IRUGO | S_IWUSR, ecm_classifier_emesh_sawf_dentry,
				(u32 *)&ecm_classifier_sawf_enabled)) {
		DEBUG_ERROR("Failed to create ecm sawf classifier  enabled file in debugfs\n");
		debugfs_remove_recursive(ecm_classifier_emesh_sawf_dentry);
		return -1;
	}

	if (!debugfs_create_u32("cake_enabled", S_IRUGO | S_IWUSR, ecm_classifier_emesh_sawf_dentry,
				(u32 *)&ecm_classifier_sawf_cake_enabled)) {
		DEBUG_ERROR("Failed to create ecm sawf cake enabled file in debugfs\n");
		debugfs_remove_recursive(ecm_classifier_emesh_sawf_dentry);
		return -1;
	}

	if (!debugfs_create_file("udp_ipsec_port", S_IRUGO | S_IWUSR, ecm_classifier_emesh_sawf_dentry,
				NULL, &ecm_classifier_sawf_emesh_udp_ipsec_port_fops)) {
		DEBUG_ERROR("Failed to create ecm sawf udp ipsec port file in debugfs for adding port number\n");
		debugfs_remove_recursive(ecm_classifier_emesh_sawf_dentry);
		return -1;
	}


	/*
	 * Create /sys/kernel/debug/ecm/ecm_classifier_emesh/flowsawf
	 */
	if (!debugfs_create_file("flowsawf", S_IRUGO | S_IWUSR, ecm_classifier_emesh_sawf_dentry,
				&ecm_classifier_emesh_sawf_flowsawf, &ecm_classifier_emesh_sawf_flowsawf_fops)) {
		DEBUG_ERROR("Failed to create flowsawf file in debugfs\n");
		debugfs_remove_recursive(ecm_classifier_emesh_sawf_dentry);
		return -1;
	}

	/*
	 * Register for service prioritization notification update.
	 */
	sp_mapdb_notifier_register(&ecm_classifier_emesh_sawf_spm_notifier);

	return 0;
}
EXPORT_SYMBOL(ecm_classifier_emesh_sawf_init);

/*
 * ecm_classifier_emesh_sawf_exit()
 */
void ecm_classifier_emesh_sawf_exit(void)
{
	DEBUG_INFO("Emesh classifier Module exit\n");

	spin_lock_bh(&ecm_classifier_emesh_sawf_lock);
	ecm_classifier_emesh_sawf_terminate_pending = true;
	spin_unlock_bh(&ecm_classifier_emesh_sawf_lock);

	/*
	 * Remove the debugfs files recursively.
	 */
	if (ecm_classifier_emesh_sawf_dentry) {
		debugfs_remove_recursive(ecm_classifier_emesh_sawf_dentry);
	}

	/*
	 * Unregister service prioritization notification update.
	 */
	sp_mapdb_notifier_unregister(&ecm_classifier_emesh_sawf_spm_notifier);
}
EXPORT_SYMBOL(ecm_classifier_emesh_sawf_exit);
