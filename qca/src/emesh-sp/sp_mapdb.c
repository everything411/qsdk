/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <linux/version.h>
#include <net/genetlink.h>
#include <linux/notifier.h>

#include "sp_mapdb.h"
#include "sp_types.h"

#define SP_MAPDB_BUCKET_SIZE_MAX 255

/*
 * Callback structure to call into plugin module
 */
static struct emesh_sp_wifi_sawf_callbacks emesh_sp_wifi;

/* Spinlock for SMP updating the rule table. */
DEFINE_SPINLOCK(sp_mapdb_lock);

/* SP Rule manager */
static struct sp_mapdb_rule_manager rule_manager;

/* TSL protection of single writer rule update. */
static unsigned long single_writer = 0;

/* Spm generic netlink family */
static struct genl_family sp_genl_family;

/*
 * Registration/Unregistration methods for SPM rule update/add/delete notifications.
 */
static RAW_NOTIFIER_HEAD(sp_mapdb_notifier_chain);

/*
 * Structures for sync msg
 */
struct net *sync_msg_net;
unsigned int port_id;

/*
 * sp_mapdb_get_hash
 * 	Return hash value for 5 tuple
 */
static uint32_t sp_mapdb_get_hash(struct sp_mapdb_5tuple *tuple)
{
	uint32_t val = 0;

	DEBUG_INFO("sp_mapdb_get_hash 5 tuple info :src   %pI4, %d, %d, %d\
			dest:  %pI4, %d, %d, %d\
			src_port %d\
			dest_port %d\
			proto %d\n",
			&tuple->src_addr[0], tuple->src_addr[1], tuple->src_addr[2], tuple->src_addr[3],
			&tuple->dest_addr[0], tuple->dest_addr[1], tuple->dest_addr[2], tuple->dest_addr[3],
			tuple->src_port, tuple->dest_port, tuple->protocol);

	val ^= tuple->dest_addr[0];
	val ^= tuple->src_addr[0];
	val ^= tuple->dest_addr[1];
	val ^= tuple->src_addr[1];
	val ^= tuple->dest_addr[2];
	val ^= tuple->src_addr[2];
	val ^= tuple->dest_addr[3];
	val ^= tuple->src_addr[3];
	val ^= tuple->dest_port;
	val ^= tuple->src_port;
	val ^= tuple->protocol;

	DEBUG_INFO("hash:  val %d  max %d\n", val, val & (SP_MAPDB_BUCKET_SIZE_MAX - 1));
	return val & (SP_MAPDB_BUCKET_SIZE_MAX - 1);
}

/*
 * sp_mapdb_rule_5tuple_cmp_v4()
 *	Checks if a rule is mapped to a particular ipv4 5-tuple
 */
static inline bool sp_mapdb_rule_5tuple_cmp_v4(struct sp_rule rule, struct sp_mapdb_5tuple *tuple)
{
	if (rule.inner.src_ipv4_addr != tuple->src_addr[0] || rule.inner.dst_ipv4_addr != tuple->dest_addr[0])
		return false;

	return true;
}

/*
 * sp_mapdb_rule_5tuple_cmp_v6()
 *	Checks if a rule is mapped to a particular ipv6 5-tuple
 */
static inline bool sp_mapdb_rule_5tuple_cmp_v6(struct sp_rule rule, struct sp_mapdb_5tuple *tuple)
{
	if (memcmp(rule.inner.src_ipv6_addr, tuple->src_addr, sizeof(uint32_t) * 4) || memcmp(rule.inner.dst_ipv6_addr, tuple->dest_addr, sizeof(uint32_t) * 4))
		return false;

	return true;
}

/*
 * sp_mapdb_rule_5tuple_cmp()
 *	Checks if a rule is mapped to a particular 5-tuple
 */
static bool sp_mapdb_rule_5tuple_cmp(struct sp_rule rule, struct sp_mapdb_5tuple *tuple)
{
	if (rule.inner.src_port != tuple->src_port || rule.inner.dst_port != tuple->dest_port || rule.inner.protocol_number != tuple->protocol) {
		return false;
	}

	if (rule.inner.ip_version_type == 4) {
		if (!sp_mapdb_rule_5tuple_cmp_v4(rule, tuple))
			return false;
	}

	if (rule.inner.ip_version_type == 6) {
		if (!sp_mapdb_rule_5tuple_cmp_v6(rule, tuple))
			return false;
	}

	return true;
}

/*
 * sp_mapdb_rule_match_sawf()
 * 	Performs rule match on received skb.
 *
 * It is called per packet basis and fields are checked and compared with the SP rule (rule).
 */
static inline bool sp_mapdb_rule_match_sawf(struct sp_rule *rule, struct sp_rule_input_params *params)
{
	bool compare_result;
	uint32_t flags = rule->inner.flags_sawf;

	if (flags & SP_RULE_FLAG_MATCH_SAWF_IP_VERSION_TYPE) {
		DEBUG_INFO("Matching IP version type..\n");
		DEBUG_INFO("Input ip version type = 0x%x\n", params->ip_version_type);
		DEBUG_INFO("rule ip version type = 0x%x\n", rule->inner.ip_version_type);
		compare_result = params->ip_version_type == rule->inner.ip_version_type;
		if (!compare_result) {
			DEBUG_WARN("IP version match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SAWF_DST_MAC) {
		DEBUG_INFO("Matching DST..\n");
		DEBUG_INFO("Input dst = %pM\n", params->dst.mac);
		DEBUG_INFO("rule dst = %pM\n", rule->inner.da);
		compare_result = ether_addr_equal(params->dst.mac, rule->inner.da);
		if (!compare_result) {

			/*
			 * If the rule is neither sawf-scs nor legacy scs type, then return false
			 */
			if (rule->classifier_type != SP_RULE_TYPE_SAWF_SCS &&
					rule->classifier_type != SP_RULE_TYPE_SCS) {
				DEBUG_WARN("DST mac address match failed for non-SAWF_SCS ans non-SCS case!\n");
				return false;
			}

			/*
			 * If the rule is sawf-scs or legacy scs type, then we further check for
			 * mac address of the netdevice interfaces.
			 */
			if (flags & SP_RULE_FLAG_MATCH_DST_IFINDEX) {
				compare_result = ether_addr_equal(params->dev_addr, rule->inner.da) &&
					(params->dst_ifindex == rule->inner.dst_ifindex);
			}

			if (!compare_result) {
				DEBUG_WARN("Netdev address and device ID match failed!\n");
				return false;
			}
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SAWF_DST_PORT) {
		DEBUG_INFO("Matching DST PORT..\n");
		DEBUG_INFO("Input dst port = 0x%x\n", params->dst.port);
		DEBUG_INFO("rule dst port = 0x%x\n", rule->inner.dst_port);
		compare_result = params->dst.port == rule->inner.dst_port;
		if (!compare_result) {
			DEBUG_WARN("DST port match failed!\n");
			return false;
		}
	}

	if ((flags & SP_RULE_FLAG_MATCH_SAWF_DST_PORT_RANGE_START) && (flags & SP_RULE_FLAG_MATCH_SAWF_DST_PORT_RANGE_END)) {
		DEBUG_INFO("Matching DST PORT RANGE..\n");
		DEBUG_INFO("skb dst port = 0x%x\n", params->dst.port);
		DEBUG_INFO("rule dst port range start = 0x%x\n", rule->inner.dst_port_range_start);
		DEBUG_INFO("rule dst port range end = 0x%x\n", rule->inner.dst_port_range_end);

		compare_result = ((params->dst.port >= rule->inner.dst_port_range_start) &&
					(params->dst.port <= rule->inner.dst_port_range_end));
		if (!compare_result) {
			DEBUG_WARN("DST port range match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SAWF_DST_IPV4) {
		DEBUG_INFO("Matching DST IP..\n");
		DEBUG_INFO("Input dst ipv4 = %pI4", &params->dst.ip.ipv4_addr);
		DEBUG_INFO("rule dst ipv4 = %pI4", &rule->inner.dst_ipv4_addr);

		if (flags & SP_RULE_FLAG_MATCH_SAWF_DST_IPV4_MASK) {
			params->dst.ip.ipv4_addr &= rule->inner.dst_ipv4_addr_mask;
		}

		compare_result = params->dst.ip.ipv4_addr == rule->inner.dst_ipv4_addr;
		if (!compare_result) {
			DEBUG_WARN("DEST ip match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SAWF_SOURCE_MAC) {
		DEBUG_INFO("Matching SRC..\n");
		DEBUG_INFO("Input src = %pM\n", params->src.mac);
		DEBUG_INFO("rule src = %pM\n", rule->inner.sa);
		compare_result = ether_addr_equal(params->src.mac, rule->inner.sa);
		if (!compare_result) {
			DEBUG_WARN("SRC match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SAWF_SRC_IPV6) {
                DEBUG_INFO("Matching SRC IPv6..\n");
                DEBUG_INFO("Input src IPv6 =  %pI6", &params->src.ip.ipv6_addr);
                DEBUG_INFO("rule src IPv6 =  %pI6", &rule->inner.src_ipv6_addr);

                if (flags & SP_RULE_FLAG_MATCH_SAWF_SRC_IPV6_MASK) {
                        params->src.ip.ipv6_addr[0] &= rule->inner.src_ipv6_addr_mask[0];
                        params->src.ip.ipv6_addr[1] &= rule->inner.src_ipv6_addr_mask[1];
                        params->src.ip.ipv6_addr[2] &= rule->inner.src_ipv6_addr_mask[2];
                        params->src.ip.ipv6_addr[3] &= rule->inner.src_ipv6_addr_mask[3];
                }

                compare_result = memcmp(params->src.ip.ipv6_addr, rule->inner.src_ipv6_addr, sizeof(uint32_t) * 4);
                if (compare_result) {
                        DEBUG_WARN("SRC IPv6 match failed!\n");
                        return false;
                }
        }

        if (flags & SP_RULE_FLAG_MATCH_SAWF_DST_IPV6) {
                DEBUG_INFO("Matching DST IPv6..\n");
                DEBUG_INFO("Input dst IPv6 = %pI6", &params->dst.ip.ipv6_addr);
                DEBUG_INFO("rule dst IPv6 = %pI6", &rule->inner.dst_ipv6_addr);

                if (flags & SP_RULE_FLAG_MATCH_SAWF_DST_IPV6_MASK) {
                        params->dst.ip.ipv6_addr[0] &= rule->inner.dst_ipv6_addr_mask[0];
                        params->dst.ip.ipv6_addr[1] &= rule->inner.dst_ipv6_addr_mask[1];
                        params->dst.ip.ipv6_addr[2] &= rule->inner.dst_ipv6_addr_mask[2];
                        params->dst.ip.ipv6_addr[3] &= rule->inner.dst_ipv6_addr_mask[3];
                }

                compare_result = memcmp(params->dst.ip.ipv6_addr, rule->inner.dst_ipv6_addr, sizeof(uint32_t) * 4);
                if (compare_result) {
                        DEBUG_WARN("DEST IPv6 match failed!\n");
                        return false;
                }
        }

	if (flags & SP_RULE_FLAG_MATCH_SAWF_SRC_PORT) {
		DEBUG_INFO("Matching SRC PORT..\n");
		DEBUG_INFO("Input src port = 0x%x\n", params->src.port);
		DEBUG_INFO("rule srcport = 0x%x\n", rule->inner.src_port);
		compare_result = params->src.port == rule->inner.src_port;
		if (!compare_result) {
			DEBUG_WARN("SRC port match failed!\n");
			return false;
		}
	}

	if ((flags & SP_RULE_FLAG_MATCH_SAWF_SRC_PORT_RANGE_START) && (flags & SP_RULE_FLAG_MATCH_SAWF_SRC_PORT_RANGE_END)) {
		DEBUG_INFO("Matching SRC PORT RANGE..\n");
		DEBUG_INFO("skb src port = 0x%x\n", params->src.port);
		DEBUG_INFO("rule src port range start = 0x%x\n", rule->inner.src_port_range_start);
		DEBUG_INFO("rule src port range end = 0x%x\n", rule->inner.src_port_range_end);

		compare_result = ((params->src.port >= rule->inner.src_port_range_start) &&
					(params->src.port <= rule->inner.src_port_range_end));
		if (!compare_result) {
			DEBUG_WARN("SRC port range match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SAWF_SRC_IPV4) {
		DEBUG_INFO("Matching SRC IP..\n");
		DEBUG_INFO("Input src ipv4 =  %pI4", &params->src.ip.ipv4_addr);
		DEBUG_INFO("rule src ipv4 =  %pI4", &rule->inner.src_ipv4_addr);

		if (flags & SP_RULE_FLAG_MATCH_SAWF_SRC_IPV4_MASK) {
			params->src.ip.ipv4_addr &= rule->inner.src_ipv4_addr_mask;
		}

		compare_result = params->src.ip.ipv4_addr == rule->inner.src_ipv4_addr;
		if (!compare_result) {
			DEBUG_WARN("SRC ip match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SAWF_PROTOCOL) {
		DEBUG_INFO("Matching IP Protocol..\n");
		DEBUG_INFO("Input ip protocol = %u\n", params->protocol);
		DEBUG_INFO("rule ip protocol = %u\n", rule->inner.protocol_number);
		compare_result = params->protocol == rule->inner.protocol_number;
		if (!compare_result) {
			DEBUG_WARN("Protocol match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SAWF_DSCP) {
		DEBUG_INFO("Matching DSCP..\n");
		DEBUG_INFO("Input DSCP = %u\n", params->dscp);
		DEBUG_INFO("rule DSCP = %u\n", rule->inner.dscp);
		compare_result = params->dscp == rule->inner.dscp;
		if (!compare_result) {
			DEBUG_WARN("DSCP match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SAWF_VLAN_PCP) {
		uint8_t vlan_pcp;
		if (params->vlan_tci == SP_RULE_INVALID_VLAN_TCI) {
			DEBUG_WARN("Vlan PCP match failed due to invalid vlan tag!\n");
			return false;
		}

		vlan_pcp = (params->vlan_tci & VLAN_PRIO_MASK) >> VLAN_PRIO_SHIFT;

		DEBUG_INFO("Matching PCP..\n");
		DEBUG_INFO("Input Vlan pcp = %u\n", vlan_pcp);
		DEBUG_INFO("rule Vlan PCP = %u\n", rule->inner.vlan_pcp);
		compare_result = vlan_pcp == rule->inner.vlan_pcp;
		if (!compare_result) {
			DEBUG_WARN("Vlan PCP match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SAWF_VLAN_ID) {
		uint16_t vlan_id;
		if (params->vlan_tci == SP_RULE_INVALID_VLAN_TCI) {
			DEBUG_WARN("Vlan ID match failed due to invalid vlan tag!\n");
			return false;
		}

		vlan_id = params->vlan_tci & VLAN_VID_MASK;
		DEBUG_INFO("Matching Vlan ID..\n");
		DEBUG_INFO("Input Vlan ID = %u\n", vlan_id);
		DEBUG_INFO("rule Vlan ID = %u\n", rule->inner.vlan_id);
		compare_result = vlan_id == rule->inner.vlan_id;
		if (!compare_result) {
			DEBUG_WARN("Vlan ID match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SCS_SPI) {
		DEBUG_INFO("Matching SPI..\n");
		DEBUG_INFO("Input SPI = %u\n", params->spi);
		DEBUG_INFO("rule match pattern value = %x, match pattern mask = %x\n", rule->inner.match_pattern_value, rule->inner.match_pattern_mask);
		params->spi &= rule->inner.match_pattern_mask;
		compare_result = params->spi == rule->inner.match_pattern_value;
		if (!compare_result) {
			DEBUG_WARN("SPI match failed!\n");
			return false;
		}
	}

	return true;
}

/*
 * sp_mapdb_get_ifli_rule()
 * 	Find the hashentry that stores the rule_node by the ruleid and rule_type
 */
static struct sp_mapdb_rule_node *sp_mapdb_get_ifli_rule(struct sp_rule_input_params *params)
{
	struct sp_mapdb_rule_id_hashentry *hashentry_iter;
	struct sp_mapdb_5tuple tuple = {0};
	uint32_t key;

	if (params->ip_version_type == 6) {
		tuple.src_addr[0] = params->src.ip.ipv6_addr[0];
		tuple.src_addr[1] = params->src.ip.ipv6_addr[1];
		tuple.src_addr[2] = params->src.ip.ipv6_addr[2];
		tuple.src_addr[3] = params->src.ip.ipv6_addr[3];
	} else if (params->ip_version_type == 4) {
		tuple.src_addr[0] = params->src.ip.ipv4_addr;
	}

	if (params->ip_version_type == 6) {
		tuple.dest_addr[0] = params->dst.ip.ipv6_addr[0];
		tuple.dest_addr[1] = params->dst.ip.ipv6_addr[1];
		tuple.dest_addr[2] = params->dst.ip.ipv6_addr[2];
		tuple.dest_addr[3] = params->dst.ip.ipv6_addr[3];
	} else if (params->ip_version_type == 4) {
		tuple.dest_addr[0] = params->dst.ip.ipv4_addr;
	}

	tuple.src_port = params->src.port;
	tuple.dest_port = params->dst.port;
	tuple.protocol = params->protocol;

	key = sp_mapdb_get_hash(&tuple);

	hash_for_each_possible(rule_manager.rule_hashmap, hashentry_iter, hlist, key) {
		if (sp_mapdb_rule_match_sawf(&hashentry_iter->rule_node->rule, params) &&
		     (hashentry_iter->rule_node->rule.classifier_type == SP_RULE_TYPE_SAWF_IFLI)) {
			return hashentry_iter->rule_node;
		}
	}


	return NULL;
}

/*
 * sp_mapdb_notifiers_call()
 *	Call registered notifiers.
 */
int sp_mapdb_notifiers_call(struct sp_rule *info, unsigned long val)
{
	return raw_notifier_call_chain(&sp_mapdb_notifier_chain, val, info);
}

/*
 * sp_mapdb_get_classifier_type_str()
 * 	String for classifier type
 */
char *sp_mapdb_get_classifier_type_str(enum sp_rule_classifier_type type)
{
	switch(type) {
	case SP_RULE_TYPE_SAWF_INVALID:
		return "none";
	case SP_RULE_TYPE_MESH:
		return "mesh";
	case SP_RULE_TYPE_SAWF:
		return "sawf";
	case SP_RULE_TYPE_SCS:
		return "scs";
	case SP_RULE_TYPE_MSCS:
		return "mscs";
	case SP_RULE_TYPE_SAWF_SCS:
		return "sawf_scs";
	case SP_RULE_TYPE_SAWF_IFLI:
		return "ifli";
	default:
		return "invalid";
	}
}
EXPORT_SYMBOL(sp_mapdb_get_classifier_type_str);

/*
 * sp_mapdb_notifier_register()
 *	Register SPM rule event notifiers.
 */
void sp_mapdb_notifier_register(struct notifier_block *nb)
{
	raw_notifier_chain_register(&sp_mapdb_notifier_chain, nb);
}
EXPORT_SYMBOL(sp_mapdb_notifier_register);

/*
 * sp_mapdb_notifier_unregister()
 *	Unregister SPM rule event notifiers.
 */
void sp_mapdb_notifier_unregister(struct notifier_block *nb)
{
	raw_notifier_chain_unregister(&sp_mapdb_notifier_chain, nb);
}
EXPORT_SYMBOL(sp_mapdb_notifier_unregister);

/*
 * sp_mapdb_rules_init()
 * 	Initializes prec_map and ruleid_hashmap.
 */
static inline void sp_mapdb_rules_init(void)
{
	int i;

	spin_lock(&sp_mapdb_lock);
	for (i = 0; i < SP_MAPDB_RULE_MAX_PRECEDENCENUM; i++) {
		INIT_LIST_HEAD(&rule_manager.prec_map[i].rule_list);
	}
	spin_unlock(&sp_mapdb_lock);

	hash_init(rule_manager.rule_hashmap);
	rule_manager.rule_count = 0;

	DEBUG_TRACE("%px: Finish Initializing SP ruledb\n", &rule_manager);
}

/*
 * sp_rule_destroy_rcu()
 * 	Destroys a rule node.
 */
static void sp_rule_destroy_rcu(struct rcu_head *head)
{
	struct sp_mapdb_rule_node *node_p =
		container_of(head, struct sp_mapdb_rule_node, rcu);

	DEBUG_INFO("%px: Removed rule id = %d\n", head, node_p->rule.id);
	kfree(node_p);
}

/*
 * sp_mapdb_search_hashentry()
 *	Find the hashentry based on rule id if rule id is valid. Otherwise use 5-tuple match.
 */
static struct sp_mapdb_rule_id_hashentry *sp_mapdb_search_hashentry(uint32_t key, uint32_t ruleid, uint8_t rule_type, struct sp_mapdb_5tuple *tuple)
{
	struct sp_mapdb_rule_id_hashentry *hashentry_iter;

	hash_for_each_possible(rule_manager.rule_hashmap, hashentry_iter, hlist, key) {
		/*
		 * It is possible there are multiple rules without valid rule id
		 * which can only be identified by tuple
		 */
		if (rule_type == SP_RULE_TYPE_SAWF_IFLI) {
			if (!tuple) {
				DEBUG_WARN("Tuple is invalid for IFLI rule hash search\n");
				return NULL;
			}

			if (sp_mapdb_rule_5tuple_cmp(hashentry_iter->rule_node->rule, tuple)) {
				return hashentry_iter;
			}

		} else if ((hashentry_iter->rule_node->rule.id == ruleid) &&
		     (hashentry_iter->rule_node->rule.classifier_type == rule_type)) {
			return hashentry_iter;
		}
	}

	return NULL;
}

/*
 * sp_mapdb_get_tuple_v4()
 *	Fills sp_mapdb_5tuple structure given an ipv4 rule
 */
static inline void sp_mapdb_get_tuple_v4(struct sp_rule *rule, struct sp_mapdb_5tuple *tuple)
{
	tuple->src_addr[0] = rule->inner.src_ipv4_addr;
	tuple->dest_addr[0] = rule->inner.dst_ipv4_addr;
}

/*
 * sp_mapdb_get_tuple_v6()
 *	Fills sp_mapdb_5tuple structure given an ipv6 rule
 */
static inline void sp_mapdb_get_tuple_v6(struct sp_rule *rule, struct sp_mapdb_5tuple *tuple)
{
	memcpy(tuple->src_addr, rule->inner.src_ipv6_addr, sizeof(uint32_t) * 4);
	memcpy(tuple->dest_addr, rule->inner.dst_ipv6_addr, sizeof(uint32_t) * 4);
}

/*
 * sp_mapdb_get_tuple()
 *	Fills sp_mapdb_5tuple structure given a rule
 */
static void sp_mapdb_get_tuple(struct sp_rule *rule, struct sp_mapdb_5tuple *tuple)
{
	if (rule->inner.flags_sawf & SP_RULE_FLAG_MATCH_SAWF_SRC_IPV4 && rule->inner.flags_sawf & SP_RULE_FLAG_MATCH_SAWF_DST_IPV4) {
		sp_mapdb_get_tuple_v4(rule, tuple);
	} else if (rule->inner.flags_sawf & SP_RULE_FLAG_MATCH_SAWF_SRC_IPV6 && rule->inner.flags_sawf & SP_RULE_FLAG_MATCH_SAWF_DST_IPV6) {
		sp_mapdb_get_tuple_v6(rule, tuple);
	}

	tuple->src_port = rule->inner.src_port;
	tuple->dest_port = rule->inner.dst_port;
	tuple->protocol = rule->inner.protocol_number;
}

/*
 * sp_mapdb_is_base_ae_ppe()
 *	Returns if base AE is PPE
 */
static int sp_mapdb_is_base_ae_ppe(struct sp_rule *rule)
{
	if (rule->inner.ae_type == SP_RULE_AE_TYPE_PPE || rule->inner.ae_type == SP_RULE_AE_TYPE_PPE_VP ||
		rule->inner.ae_type == SP_RULE_AE_TYPE_PPE_DS) {
		return true;
	}

	return false;
}

/*
 * sp_mapdb_rule_add()
 * 	Adds (or modifies) the SP rule in the rule table.
 *
 * It will also updating the add_remove_modify and old_prec and field_update argument.
 * old_prec : the precedence of the previous rule.
 * field_update : if fields other than precedence is different from the previous rule.
 */
static sp_mapdb_update_result_t sp_mapdb_rule_add(struct sp_rule *newrule, uint8_t rule_type)
{
	uint32_t key = newrule->id;
	uint8_t newrule_precedence = newrule->rule_precedence;
	struct sp_mapdb_rule_node *cur_rule_node = NULL;
	struct sp_mapdb_rule_id_hashentry *cur_hashentry = NULL;
	struct sp_mapdb_rule_node *new_rule_node;
	struct sp_mapdb_rule_id_hashentry *new_hashentry;
	struct sp_mapdb_5tuple tuple = {0};
	newrule->key = SP_RULE_INVALID_RULE_ID;

	/*
	 * An IFLI rule with PPE AE must be a background flow
	 * there is no need to maintain these rules, but ECM
	 * will still be notified in case AE switch is necessary
	 */
	if (sp_mapdb_is_base_ae_ppe(newrule) && rule_type == SP_RULE_TYPE_SAWF_IFLI) {
		sp_mapdb_get_tuple(newrule, &tuple);
		key = sp_mapdb_get_hash(&tuple);
		newrule->key = key;
		sp_mapdb_notifiers_call(newrule, SP_MAPDB_ADD_RULE);
		return SP_MAPDB_UPDATE_RESULT_SUCCESS_ADD;
	}

	DEBUG_INFO("%px: Try adding rule id = %d with rule_type: %d\n", newrule, newrule->id, rule_type);

	if (rule_manager.rule_count == SP_MAPDB_RULE_MAX) {
		DEBUG_WARN("%px:Ruletable is full. Error adding rule %d, rule_type: %d\n", newrule, newrule->id, rule_type);
		return SP_MAPDB_UPDATE_RESULT_ERR_TBLFULL;
	}

	if (newrule->inner.rule_output >= SP_MAPDB_NO_MATCH) {
		DEBUG_WARN("%px:Invalid rule output value %d (valid range:0-9)\n", newrule, newrule->inner.rule_output);
		return SP_MAPDB_UPDATE_RESULT_ERR_INVALIDENTRY;
	}

	new_rule_node = (struct sp_mapdb_rule_node *)kzalloc(sizeof(struct sp_mapdb_rule_node), GFP_KERNEL);
	if (!new_rule_node) {
		DEBUG_ERROR("%px:Error, allocate rule node failed.\n", newrule);
		return SP_MAPDB_UPDATE_RESULT_ERR_ALLOCNODE;
	}

	memcpy(&new_rule_node->rule, newrule, sizeof(struct sp_rule));
	new_rule_node->rule.classifier_type = rule_type;

	if (newrule_precedence == SP_MAPDB_RULE_MAX_PRECEDENCENUM) {
		new_rule_node->rule.rule_precedence = 0;
		newrule_precedence = new_rule_node->rule.rule_precedence;
	}

	/*
	 * Update the key for IFLI rule type
	 */
	if (rule_type == SP_RULE_TYPE_SAWF_IFLI) {
		sp_mapdb_get_tuple(newrule, &tuple);
		key = sp_mapdb_get_hash(&tuple);
	}

	spin_lock(&sp_mapdb_lock);
	cur_hashentry = sp_mapdb_search_hashentry(key, newrule->id, rule_type, &tuple);
	if (!cur_hashentry) {
		spin_unlock(&sp_mapdb_lock);
		new_hashentry = (struct sp_mapdb_rule_id_hashentry *)kzalloc(sizeof(struct sp_mapdb_rule_id_hashentry), GFP_KERNEL);
		if (!new_hashentry) {
			DEBUG_ERROR("%px:Error, allocate hashentry failed.\n", newrule);
			kfree(new_rule_node);
			return SP_MAPDB_UPDATE_RESULT_ERR_ALLOCHASH;
		}

		/*
		 * Inserting new rule node and hash entry in prec_map
		 * and hashmap respectively.
		 */
		spin_lock(&sp_mapdb_lock);
		new_hashentry->rule_node = new_rule_node;

		list_add_rcu(&new_rule_node->rule_list, &rule_manager.prec_map[newrule_precedence].rule_list);
		hash_add(rule_manager.rule_hashmap, &new_hashentry->hlist,key);
		rule_manager.rule_count++;
		spin_unlock(&sp_mapdb_lock);

		newrule->key = key;
		new_rule_node->rule.key = key;
		DEBUG_INFO("%px:Success %s = %d rule_type: %d \n",
				newrule, (rule_type == SP_RULE_TYPE_SAWF_IFLI) ? "key" : "rule_id",  (rule_type == SP_RULE_TYPE_SAWF_IFLI) ? newrule->key:newrule->id ,rule_type);

		/*
		 * Since this is inserting a new rule, the old precendence
		 * and field update do not possess any meaning.
		 */
		sp_mapdb_notifiers_call(newrule, SP_MAPDB_ADD_RULE);

		return SP_MAPDB_UPDATE_RESULT_SUCCESS_ADD;
	}

	cur_rule_node = cur_hashentry->rule_node;

	if (cur_rule_node->rule.rule_precedence == newrule_precedence) {
		list_replace_rcu(&cur_rule_node->rule_list, &new_rule_node->rule_list);
		cur_hashentry->rule_node = new_rule_node;
		spin_unlock(&sp_mapdb_lock);
		newrule->key = key;
		new_rule_node->rule.key = key;

		DEBUG_INFO("%px:overwrite %s = %x rule_type: %d \n",
				newrule, (rule_type == SP_RULE_TYPE_SAWF_IFLI) ? "key" : "rule_id",  (rule_type == SP_RULE_TYPE_SAWF_IFLI) ? newrule->key:newrule->id ,rule_type);

		/*
		 * If precedence doesn't change then it has to be some fields modified.
		 */
		sp_mapdb_notifiers_call(newrule, SP_MAPDB_MODIFY_RULE);

		call_rcu(&cur_rule_node->rcu, sp_rule_destroy_rcu);

		return SP_MAPDB_UPDATE_RESULT_SUCCESS_MODIFY;
	}

	list_del_rcu(&cur_rule_node->rule_list);
	list_add_rcu(&new_rule_node->rule_list, &rule_manager.prec_map[newrule_precedence].rule_list);
	cur_hashentry->rule_node = new_rule_node;
	newrule->key = key;
	spin_unlock(&sp_mapdb_lock);

	/*
	 * Fields other than rule_precedence can still be updated along with rule_precedence.
	 */
	DEBUG_INFO("%px:Success rule id=%d rule_type: %d\n", newrule, newrule->id, rule_type);
	sp_mapdb_notifiers_call(newrule, SP_MAPDB_MODIFY_RULE);
	call_rcu(&cur_rule_node->rcu, sp_rule_destroy_rcu);

	return SP_MAPDB_UPDATE_RESULT_SUCCESS_MODIFY;
}

/*
 * sp_mapdb_rule_delete()
 * 	Deletes a rule from the rule table by the rule id and rule_type.
 *
 * The memory for the rule node will also be deleted as hash entry will also be freed.
 */
static sp_mapdb_update_result_t sp_mapdb_rule_delete(uint32_t ruleid, uint32_t key, uint8_t rule_type, struct sp_mapdb_5tuple *tuple)
{
	struct sp_mapdb_rule_node *tobedeleted;
	struct sp_mapdb_rule_id_hashentry *cur_hashentry = NULL;

	spin_lock(&sp_mapdb_lock);
	if (rule_manager.rule_count == 0) {
		spin_unlock(&sp_mapdb_lock);
		DEBUG_WARN("rule table is empty\n");
		return SP_MAPDB_UPDATE_RESULT_ERR_TBLEMPTY;
	}

	cur_hashentry = sp_mapdb_search_hashentry(key, ruleid, rule_type, tuple);
	if (!cur_hashentry) {
		spin_unlock(&sp_mapdb_lock);
		DEBUG_WARN("there is no such rule as key = %d, ruleID = %d, rule_type: %d\n", key, ruleid, rule_type);
		return SP_MAPDB_UPDATE_RESULT_ERR_RULENOEXIST;
	}

	tobedeleted = cur_hashentry->rule_node;
	list_del_rcu(&tobedeleted->rule_list);
	hash_del(&cur_hashentry->hlist);
	kfree(cur_hashentry);
	rule_manager.rule_count--;
	spin_unlock(&sp_mapdb_lock);

	DEBUG_INFO("Successful deletion\n");

	/*
	 * There is no point on having old_prec
	 * and field_update in remove rules case.
	 *
	 * Avoid sending remove notification to ECM in case of IFLI.
	 * Because for IFLI delete notification comes from ECM
	 */
	if (rule_type != SP_RULE_TYPE_SAWF_IFLI) {
		sp_mapdb_notifiers_call(&tobedeleted->rule, SP_MAPDB_REMOVE_RULE);
	}

	call_rcu(&tobedeleted->rcu, sp_rule_destroy_rcu);

	return SP_MAPDB_UPDATE_RESULT_SUCCESS_DELETE;
}

/*
 * sp_mapdb_rule_match()
 * 	Performs rule match on received skb.
 *
 * It is called per packet basis and fields are checked and compared with the SP rule (rule).
 */
static bool sp_mapdb_rule_match(struct sk_buff *skb, struct sp_rule *rule, uint8_t *smac, uint8_t *dmac)
{
	struct ethhdr *eth_header;
	struct iphdr *iph;
	struct tcphdr *tcphdr;
	struct udphdr *udphdr;
	uint16_t src_port = 0, dst_port = 0;
	struct vlan_hdr *vhdr;
	int16_t vlan_id;
	bool compare_result, sense;
	uint32_t flags = rule->inner.flags;

	if (flags & SP_RULE_FLAG_MATCH_ALWAYS_TRUE) {
		DEBUG_INFO("Basic match case.\n");
		return true;
	}


	if (flags & SP_RULE_FLAG_MATCH_UP) {
		DEBUG_INFO("Matching UP..\n");
		DEBUG_INFO("skb->up = %d , rule->up = %d\n", skb->priority, rule->inner.user_priority);

		compare_result = (skb->priority == rule->inner.user_priority) ? true:false;
		sense = !!(flags & SP_RULE_FLAG_MATCH_UP_SENSE);
		if (!(compare_result ^ sense)) {
			DEBUG_WARN("Match UP failed\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SOURCE_MAC) {
		DEBUG_INFO("Matching SRC..\n");
		DEBUG_INFO("skb src = %pM\n", smac);
		DEBUG_INFO("rule src = %pM\n", rule->inner.sa);

		compare_result = ether_addr_equal(smac, rule->inner.sa);
		sense = !!(flags & SP_RULE_FLAG_MATCH_SOURCE_MAC_SENSE);
		if (!(compare_result ^ sense)) {
			DEBUG_WARN("SRC match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_DST_MAC) {
		DEBUG_INFO("Matching DST..\n");
		DEBUG_INFO("skb dst = %pM\n", dmac);
		DEBUG_INFO("rule dst = %pM\n", rule->inner.da);

		compare_result = ether_addr_equal(dmac, rule->inner.da);
		sense = !!(flags & SP_RULE_FLAG_MATCH_DST_MAC_SENSE);
		if (!(compare_result ^ sense)) {
			DEBUG_WARN("DST match failed!\n");
			return false;
		}
	}

	eth_header = (struct ethhdr *)skb->data;
	if (flags & SP_RULE_FLAG_MATCH_VLAN_ID) {
		uint16_t ether_type = ntohs(eth_header->h_proto);


		if (ether_type == ETH_P_8021Q) {
			vhdr = (struct vlan_hdr *)(skb->data + ETH_HLEN);
			vlan_id = ntohs(vhdr->h_vlan_TCI);

			DEBUG_INFO("Matching VLAN ID..\n");
			DEBUG_INFO("skb vlan = %u\n", vlan_id);
			DEBUG_INFO("rule vlan = %u\n", rule->inner.vlan_id);

			compare_result = vlan_id == rule->inner.vlan_id;
			sense = !!(flags & SP_RULE_FLAG_MATCH_VLAN_ID_SENSE);
			if (!(compare_result ^ sense)) {
				DEBUG_WARN("SKB vlan match failed!\n");
				return false;
			}
		} else {
			return false;
		}
	}

	if (flags & (SP_RULE_FLAG_MATCH_SRC_IPV4 | SP_RULE_FLAG_MATCH_DST_IPV4 |
				SP_RULE_FLAG_MATCH_SRC_PORT | SP_RULE_FLAG_MATCH_DST_PORT |
				SP_RULE_FLAG_MATCH_DSCP | SP_RULE_FLAG_MATCH_PROTOCOL)) {
		if (skb->protocol == ntohs(ETH_P_IP)) {
			/* Check for ip header */
			if (unlikely(!pskb_may_pull(skb, sizeof(*iph)))) {
				DEBUG_INFO("No ip header in skb\n");
				return false;
			}
			iph = ip_hdr(skb);
		} else {
			DEBUG_INFO("Not ip packet protocol: %x \n", skb->protocol);
			return false;
		}
	} else {
		return true;
	}

	if (flags & SP_RULE_FLAG_MATCH_DSCP) {

		uint16_t dscp;

		dscp = ipv4_get_dsfield(ip_hdr(skb)) >> 2;

		DEBUG_INFO("Matching DSCP..\n");
		DEBUG_INFO("skb DSCP = %u\n", dscp);
		DEBUG_INFO("rule DSCP = %u\n", rule->inner.dscp);

		compare_result = dscp == rule->inner.dscp;
		sense = !!(flags & SP_RULE_FLAG_MATCH_DSCP_SENSE);
		if (!(compare_result ^ sense)) {
			DEBUG_WARN("SRC dscp match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_SRC_IPV4) {
		DEBUG_INFO("Matching SRC IP..\n");
		DEBUG_INFO("skb src ipv4 =  %pI4", &iph->saddr);
		DEBUG_INFO("rule src ipv4 =  %pI4", &rule->inner.src_ipv4_addr);

		compare_result = iph->saddr == rule->inner.src_ipv4_addr;
		sense = !!(flags & SP_RULE_FLAG_MATCH_SRC_IPV4_SENSE);
		if (!(compare_result ^ sense)) {
			DEBUG_WARN("SRC ip match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_DST_IPV4) {
		DEBUG_INFO("Matching DST IP..\n");
		DEBUG_INFO("skb dst ipv4 = %pI4", &iph->daddr);
		DEBUG_INFO("rule dst ipv4 = %pI4", &rule->inner.dst_ipv4_addr);

		compare_result = iph->daddr == rule->inner.dst_ipv4_addr;
		sense = !!(flags & SP_RULE_FLAG_MATCH_DST_IPV4_SENSE);
		if (!(compare_result ^ sense)) {
			DEBUG_WARN("DEST ip match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_PROTOCOL) {
		DEBUG_INFO("Matching IP Protocol..\n");
		DEBUG_INFO("skb ip protocol = %u\n", iph->protocol);
		DEBUG_INFO("rule ip protocol = %u\n", rule->inner.protocol_number);

		compare_result = iph->protocol == rule->inner.protocol_number;
		sense = !!(flags & SP_RULE_FLAG_MATCH_PROTOCOL_SENSE);
		if (!(compare_result ^ sense)) {
			DEBUG_WARN("DEST ip match failed!\n");
			return false;
		}
	}

	if (iph->protocol == IPPROTO_TCP) {
		/* Check for tcp header */
		if (unlikely(!pskb_may_pull(skb, sizeof(*tcphdr)))) {
			DEBUG_INFO("No tcp header in skb\n");
			return false;
		}

		tcphdr = tcp_hdr(skb);
		src_port = tcphdr->source;
		dst_port = tcphdr->dest;
	} else if (iph->protocol == IPPROTO_UDP) {
		/* Check for udp header */
		if (unlikely(!pskb_may_pull(skb, sizeof(*udphdr)))) {
			DEBUG_INFO("No udp header in skb\n");
			return false;
		}

		udphdr = udp_hdr(skb);
		src_port = udphdr->source;
		dst_port = udphdr->dest;
	}

	if (flags & SP_RULE_FLAG_MATCH_SRC_PORT) {
		DEBUG_INFO("Matching SRC PORT..\n");
		DEBUG_INFO("skb src port = 0x%x\n", ntohs(src_port));
		DEBUG_INFO("rule srcport = 0x%x\n", rule->inner.src_port);

		compare_result = ntohs(src_port) == rule->inner.src_port;
		sense = !!(flags & SP_RULE_FLAG_MATCH_SRC_PORT_SENSE);
		if (!(compare_result ^ sense)) {
			DEBUG_WARN("SRC port match failed!\n");
			return false;
		}
	}

	if (flags & SP_RULE_FLAG_MATCH_DST_PORT) {
		DEBUG_INFO("Matching DST PORT..\n");
		DEBUG_INFO("skb dst port = 0x%x\n", ntohs(dst_port));
		DEBUG_INFO("rule dst port = 0x%x\n", rule->inner.dst_port);

		compare_result = ntohs(dst_port) == rule->inner.dst_port;
		sense = !!(flags & SP_RULE_FLAG_MATCH_DST_PORT_SENSE);
		if (!(compare_result ^ sense)) {
			DEBUG_WARN("DST port match failed!\n");
			return false;
		}
	}

	return true;
}

/*
 * sp_mapdb_ruletable_search()
 * 	Performs rules match for a skb over an entire rule table.
 *
 * According to the specification, the rules will be enumerated in a precedence descending order.
 * Once there is a match, the enumeration stops.
 * In details, it enumerates from prec_map[0xFE] to prec_map[0] for each packet, and exits loop if there is a
 * rule match. The output value defined in a matched rule
 * will be used to determine which fields(UP,DSCP) will be used for
 * PCP value.
 */
static uint8_t sp_mapdb_ruletable_search(struct sk_buff *skb, uint8_t *smac, uint8_t *dmac)
{
	uint8_t output = SP_MAPDB_NO_MATCH;
	struct sp_mapdb_rule_node *curnode;
	int i, protocol;

	rcu_read_lock();
	if (rule_manager.rule_count == 0) {
		rcu_read_unlock();
		DEBUG_WARN("rule table is empty\n");
		/*
		 * When rule table is empty, default DSCP based
		 * prioritization should be followed
		 */
		output = SP_MAPDB_USE_DSCP;
		goto set_output;
	}
	rcu_read_unlock();

	/*
	 * The iteration loop goes backward because
	 * rules should be matched in the precedence
	 * descending order.
	 */
	for (i = SP_MAPDB_RULE_MAX_PRECEDENCENUM - 1; i >= 0; i--) {
		list_for_each_entry_rcu(curnode, &(rule_manager.prec_map[i].rule_list), rule_list) {
			DEBUG_INFO("Matching with rid = %d (emesh case)\n", curnode->rule.id);
			if (curnode->rule.classifier_type == SP_RULE_TYPE_MESH) {
				if (sp_mapdb_rule_match(skb, &curnode->rule, smac, dmac)) {
					output = curnode->rule.inner.rule_output;
					goto set_output;
				}
			}
		}
	}

set_output:
	switch (output) {
	case SP_MAPDB_USE_UP:
		output = skb->priority;
		break;

	case SP_MAPDB_USE_DSCP:

		/*
		 * >> 2 first(dscp field) and then >>3 (DSCP->PCP mapping)
		 */
		protocol = ntohs(skb->protocol);

		switch (protocol) {
		case ETH_P_IP:
			output = (ipv4_get_dsfield(ip_hdr(skb))) >> 5;
			break;

		case ETH_P_IPV6:
			output = (ipv6_get_dsfield(ipv6_hdr(skb))) >> 5;
			break;

		default:

			/*
			 * non-IP protocol does not have dscp field, apply DEFAULT_PCP.
			 */
			output = SP_MAPDB_RULE_DEFAULT_PCP;
			break;
		}
		break;

	case SP_MAPDB_NO_MATCH:
		output = SP_MAPDB_RULE_DEFAULT_PCP;
		break;

	default:
		break;
	}

	return output;
}

/*
 * sp_mapdb_rm_sync
 *	Syncs a flow's prioritization information with RM
 */
int sp_mapdb_rm_sync(struct sp_rm_sync_msg *rm_msg)
{
	struct sk_buff *msg;
	void *hdr;

	if (!sync_msg_net || !port_id) {
		DEBUG_WARN("Sync socket is not setup\n");
		return -EINVAL;
	}

	/*
	 * An atomic message must be allocated as this funciton
	 * may be called from an interupt context
	 */
	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_ATOMIC);
	if (!msg) {
		DEBUG_WARN("Failed to allocate netlink message to accomodate rule\n");
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, port_id, 0,
			&sp_genl_family, 0, SPM_CMD_RULE_ACTION);

	if (!hdr) {
		DEBUG_WARN("Failed to put hdr in netlink buffer\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	nla_put(msg, SP_GNL_ATTR_RM_SYNC_MSG, sizeof(struct sp_rm_sync_msg), rm_msg);

	return genlmsg_unicast(sync_msg_net, msg, port_id);
}

EXPORT_SYMBOL(sp_mapdb_rm_sync);

/*
 * sp_mapdb_enum_to_char_ae_type()
 * 	Convert ae type enum to string ae type.
 */
static const char* sp_mapdb_enum_to_char_ae_type(enum sp_rule_ae_type ae_type)
{
	switch (ae_type) {
	case SP_RULE_AE_TYPE_PPE:
		return "ppe";

	case SP_RULE_AE_TYPE_SFE:
		return "sfe";

	case SP_RULE_AE_TYPE_PPE_DS:
		return "ppe-ds";

	case SP_RULE_AE_TYPE_PPE_VP:
		return "ppe-vp";

	case SP_RULE_AE_TYPE_NONE:
		return "none";

	default:
		return "default";
	}
}

/*
 * sp_mapdb_ifli_rule_flush()
 * 	Clear the rule and frees the memory allocated for rule type IFLI.
 */
void sp_mapdb_ifli_rule_flush(struct sp_rule_del_params *del_params)
{
	struct sp_mapdb_5tuple tuple = {0};

	tuple.src_port = del_params->src_port;
	tuple.dest_port = del_params->dest_port;
	tuple.protocol = del_params->protocol;

	if (del_params->ip_version == 4) {
		tuple.src_addr[0] = del_params->src_ip[0];
		tuple.dest_addr[0] = del_params->dest_ip[0];
	} else if (del_params->ip_version == 6) {
		memcpy(tuple.src_addr, del_params->src_ip, sizeof(uint32_t) * 4);
		memcpy(tuple.dest_addr, del_params->dest_ip, sizeof(uint32_t) * 4);
	}

	sp_mapdb_rule_delete(del_params->rule_id, del_params->key, SP_RULE_TYPE_SAWF_IFLI, &tuple);
}
EXPORT_SYMBOL(sp_mapdb_ifli_rule_flush);

/*
 * sp_mapdb_ruletable_flush()
 * 	Clear the rule table and frees the memory allocated for the rules.
 *
 * It will enumerate all the precedence in the prec_map,
 * and start from the head node in each of the precedence in the prec_map,
 * and free all the rule nodes
 * as well as the associated hashentry, with
 * these precedence.
 */
void sp_mapdb_ruletable_flush(void)
{
	int i;

	struct sp_mapdb_rule_node *node_list, *node, *tmp;
	struct sp_mapdb_rule_id_hashentry *hashentry_iter;
	struct hlist_node *hlist_tmp;
	int hash_bkt;

	spin_lock(&sp_mapdb_lock);
	if (rule_manager.rule_count == 0) {
		spin_unlock(&sp_mapdb_lock);
		DEBUG_WARN("The rule table is already empty. No action needed. \n");
		return;
	}

	for (i = 0; i < SP_MAPDB_RULE_MAX_PRECEDENCENUM; i++) {
		node_list = &rule_manager.prec_map[i];
		/*
		 * tmp as a temporary pointer to store the address of next node.
		 * This is required because we are using list_for_each_entry_safe,
		 * which allows in-loop deletion of the node.
		 *
		 */
		list_for_each_entry_safe(node, tmp, &node_list->rule_list, rule_list) {
			list_del_rcu(&node->rule_list);
			call_rcu(&node->rcu, sp_rule_destroy_rcu);
		}
	}

	/* Free hash list. */
	hash_for_each_safe(rule_manager.rule_hashmap, hash_bkt, hlist_tmp, hashentry_iter, hlist) {
		hash_del(&hashentry_iter->hlist);
		kfree(hashentry_iter);
	}
	rule_manager.rule_count = 0;
	spin_unlock(&sp_mapdb_lock);
}
EXPORT_SYMBOL(sp_mapdb_ruletable_flush);

/*
 * sp_mapdb_rule_update()
 * 	Perfoms rule update
 *
 * It will first check the add/remove filter bit of
 * the newrule and pass it to sp_mapdb_rule_add and sp_mapdb_rule_delete acccordingly.
 * sp_mapdb_rule_update will also collect add_remove_modify based on the updated result, as:
 * add = 0, remove = 1, modify = 2
 * and field_update (meaning whether the field(other than precence)
 * is modified), these are useful in perform precise matching in ECM.
 */
sp_mapdb_update_result_t sp_mapdb_rule_update(struct sp_rule *newrule)
{
	sp_mapdb_update_result_t error_code = 0;
	struct sp_mapdb_5tuple tuple = {0};
	uint32_t key;

	if (!newrule) {
		return SP_MAPDB_UPDATE_RESULT_ERR_NEWRULE_NULLPTR;
	}

	if (test_and_set_bit_lock(0, &single_writer)) {
		DEBUG_ERROR("%px: single writer allowed", newrule);
		return SP_MAPDB_UPDATE_RESULT_ERR_SINGLE_WRITER;
	}

	switch (newrule->cmd) {
	case SP_MAPDB_ADD_REMOVE_FILTER_DELETE:
		if (newrule->classifier_type == SP_RULE_TYPE_SAWF_IFLI) {
			sp_mapdb_get_tuple(newrule, &tuple);
			key = sp_mapdb_get_hash(&tuple);
			error_code = sp_mapdb_rule_delete(newrule->id, key, newrule->classifier_type, &tuple);
		} else {
			error_code = sp_mapdb_rule_delete(newrule->id, newrule->id, newrule->classifier_type, NULL);
		}
		break;

	case SP_MAPDB_ADD_REMOVE_FILTER_ADD:
		error_code = sp_mapdb_rule_add(newrule, newrule->classifier_type);
		break;

	default:
		DEBUG_ERROR("%px: Error, unknown Add/Remove filter bit\n", newrule);
		error_code = SP_MAPDB_UPDATE_RESULT_ERR_UNKNOWNBIT;
		break;
	}

	clear_bit_unlock(0, &single_writer);

	return error_code;
}
EXPORT_SYMBOL(sp_mapdb_rule_update);

/*
 * sp_mapdb_enum_radio_band_to_str()
 * 	Convert a sp_rule_vdev_band enum represented band to string
 */
static inline char *sp_mapdb_enum_radio_band_to_str(enum sp_mapdb_radio_band band)
{
	switch (band) {
	case SP_MAPDB_RADIO_BAND_INVALID:
		break;
	case SP_MAPDB_RADIO_BAND_2G:
		return "2G";
	case SP_MAPDB_RADIO_BAND_5G:
		return "5G";
	case SP_MAPDB_RADIO_BAND_5GH:
		return "5GH";
	case SP_MAPDB_RADIO_BAND_5GL:
		return "5GL";
	case SP_MAPDB_RADIO_BAND_6G:
		return "6G";
	}

	return NULL;
}

/*
 * sp_mapdb_enum_radio_bw_to_str()
 * 	Convert a sp_rule_vdev_band enum represented band to string
 */
static inline char *sp_mapdb_enum_radio_bw_to_str(enum sp_mapdb_radio_bandwidth bandwidth)
{
	switch (bandwidth) {
	case SP_MAPDB_RADIO_BANDWIDTH_INVALID:
		break;
	case SP_MAPDB_RADIO_BANDWIDTH_20:
		return "20";
	case SP_MAPDB_RADIO_BANDWIDTH_40:
		return "40";
	case SP_MAPDB_RADIO_BANDWIDTH_80:
		return "80";
	case SP_MAPDB_RADIO_BANDWIDTH_160:
		return "160";
	case SP_MAPDB_RADIO_BANDWIDTH_80_80:
		return "80_80";
	case SP_MAPDB_RADIO_BANDWIDTH_320:
		return "320";
	}

	return NULL;
}

/*
 * sp_mapdb_rule_print_input_wlan_params()
 * 	Print the input wlan parameters of the current rule.
 */
static inline void sp_mapdb_rule_print_input_wlan_params(struct sp_mapdb_rule_node *curnode)
{
	int i;

	printk("WLAN params:\n");
	printk("transmitter_mac: %pM, receiver_mac: %pM", curnode->rule.inner.transmitter_mac, curnode->rule.inner.receiver_mac);
	if (curnode->rule.inner.band_mode == 1) {
		printk("band_mode: %s\n", SP_MAPDB_RADIO_FLAG_AND);
	} else {
		printk("band_mode: %s\n", SP_MAPDB_RADIO_FLAG_OR);
	}

	printk("band: ");
	for (i = 0; i < SP_RULE_MAX_VDEV_PER_ML; i++) {
		if (curnode->rule.inner.radio_band[i] == SP_RULE_BAND_VALID) {
			printk("%s \n", sp_mapdb_enum_radio_band_to_str(i));
		}
	}

	if (curnode->rule.inner.channel_mode == 1) {
		printk("channel_mode: %s\n", SP_MAPDB_RADIO_FLAG_AND);
	} else {
		printk("channel_mode: %s\n", SP_MAPDB_RADIO_FLAG_OR);
	}

	printk("channel: ");
	for (i = 0; i < SP_RULE_MAX_VDEV_PER_ML; i++) {
		printk("%d ", curnode->rule.inner.radio_channel[i]);
	}

	if (curnode->rule.inner.bandwidth_mode == 1) {
		printk("bandwidth_mode: %s\n", SP_MAPDB_RADIO_FLAG_AND);
	} else {
		printk("bandwidth_mode: %s\n", SP_MAPDB_RADIO_FLAG_OR);
	}

	printk("bandwidth: ");
	for (i = 0; i < SP_RULE_MAX_VDEV_PER_ML; i++) {
		printk("%s ", sp_mapdb_enum_radio_bw_to_str(curnode->rule.inner.radio_bandwidth[i]));
	}

	printk("ssid: %s", curnode->rule.inner.ssid);
	printk("ssid_len: 0x%x", curnode->rule.inner.ssid_len);
	printk("bssid: %pM", curnode->rule.inner.bssid);
	printk("access_class: 0x%x", curnode->rule.inner.access_class);
	printk("priority: 0x%x", curnode->rule.inner.priority);
}

/*
 * sp_mapdb_rule_print_input_params()
 * 	Print the input parameters of current rule.
 */
static inline void sp_mapdb_rule_print_input_params(struct sp_mapdb_rule_node *curnode)
{
	printk("\n........INPUT PARAMS........\n");
	printk("src_mac: %pM, dst_mac: %pM, src_port: %d, dst_port: %d, ip_version_type: %d\n",
			curnode->rule.inner.sa, curnode->rule.inner.da, curnode->rule.inner.src_port,
			curnode->rule.inner.dst_port, curnode->rule.inner.ip_version_type);
	printk("dscp: %d, dscp remark: %d, vlan id: %d, vlan pcp: %d, vlan pcp remark: %d, protocol number: %d\n",
			curnode->rule.inner.dscp, curnode->rule.inner.dscp_remark, curnode->rule.inner.vlan_id,
			curnode->rule.inner.vlan_pcp, curnode->rule.inner.vlan_pcp_remark, curnode->rule.inner.protocol_number);

	printk("src_ipv4: %pI4, dst_ipv4: %pI4\n", &curnode->rule.inner.src_ipv4_addr, &curnode->rule.inner.dst_ipv4_addr);

	printk("src_ipv6: %pI6: dst_ipv6: %pI6\n", &curnode->rule.inner.src_ipv6_addr, &curnode->rule.inner.dst_ipv6_addr);

	printk("src_ipv4_mask: %pI4, dst_ipv4_mask: %pI4\n", &curnode->rule.inner.src_ipv4_addr_mask, &curnode->rule.inner.dst_ipv4_addr_mask);

	printk("src_ipv6_mask: %pI6: dst_ipv6_mask: %pI6\n", &curnode->rule.inner.src_ipv6_addr_mask, &curnode->rule.inner.dst_ipv6_addr_mask);
	printk("match pattern value: %x: match pattern mask: %x\n", curnode->rule.inner.match_pattern_value, curnode->rule.inner.match_pattern_mask);
	printk("MSCS TID BITMAP: %x: Priority Limit Value: %x\n", curnode->rule.inner.mscs_tid_bitmap, curnode->rule.inner.priority_limit);
	printk("Destination Interface Index : %d\n", curnode->rule.inner.dst_ifindex);
	printk("src_port: 0x%x, dst_port: 0x%x, src_port_range_start: 0x%x, src_port_range_end: 0x%x, dst_port_range_start: 0x%x, dst_port_range_end: 0x%x\n",
			curnode->rule.inner.src_port, curnode->rule.inner.dst_port, curnode->rule.inner.src_port_range_start,
			curnode->rule.inner.src_port_range_end, curnode->rule.inner.dst_port_range_start,
			curnode->rule.inner.dst_port_range_end);
	printk("Source Interface: %s Destination Interface: %s \n", curnode->rule.inner.src_iface, curnode->rule.inner.dst_iface);

	/*
	 * print the wlan specific input parameters of the current rule
	 */
	if (curnode->rule.inner.wlan_flow == 1) {
		sp_mapdb_rule_print_input_wlan_params(curnode);
	}
}

/*
 * sp_mapdb_ruletable_print()
 *	Print the rule table.
 */
void sp_mapdb_ruletable_print(void)
{
	struct sp_mapdb_rule_id_hashentry *hashentry_iter;
	struct hlist_node *hlist_tmp;
	int hash_bkt;

	rcu_read_lock();
	printk("\n====Rule table start====\nTotal rule count = %d\n", rule_manager.rule_count);
	hash_for_each_safe(rule_manager.rule_hashmap, hash_bkt, hlist_tmp, hashentry_iter, hlist) {
		printk("\nid: %d, classifier_type: %s, precedence: %d\n", hashentry_iter->rule_node->rule.id, sp_mapdb_get_classifier_type_str(hashentry_iter->rule_node->rule.classifier_type), hashentry_iter->rule_node->rule.rule_precedence);
		sp_mapdb_rule_print_input_params(hashentry_iter->rule_node);
		printk("\n........OUTPUT PARAMS........\n");
		printk("dscp_remark: %d, vlan_pcp_remark: %d\n", hashentry_iter->rule_node->rule.inner.dscp_remark, hashentry_iter->rule_node->rule.inner.vlan_pcp_remark);
		printk("output(priority): %d, service_class_id: %d\n ipv4_frag_thresh: %d\n", hashentry_iter->rule_node->rule.inner.rule_output, hashentry_iter->rule_node->rule.inner.service_class_id, hashentry_iter->rule_node->rule.inner.ipv4_frag_thresh);
		printk("MSCS TID BITMAP: %x: Priority Limit Value: %x\n", hashentry_iter->rule_node->rule.inner.mscs_tid_bitmap, hashentry_iter->rule_node->rule.inner.priority_limit);
		printk("acceleration engine type: %s\n", sp_mapdb_enum_to_char_ae_type(hashentry_iter->rule_node->rule.inner.ae_type));
	}


	rcu_read_unlock();
	printk("====Rule table ends====\n");
}

/*
 * sp_mapdb_apply()
 * 	Assign the desired PCP value into skb->priority.
 */
void sp_mapdb_apply(struct sk_buff *skb, uint8_t *smac, uint8_t *dmac)
{
	rcu_read_lock();
	skb->priority = sp_mapdb_ruletable_search(skb, smac, dmac);
	rcu_read_unlock();
}
EXPORT_SYMBOL(sp_mapdb_apply);

/*
 * sp_mapdb_init()
 * 	Initialize ruledb.
 */
void sp_mapdb_init(void)
{
	sp_mapdb_rules_init();
}

/*
 * sp_mapdb_get_wlan_latency_params()
 *  Get latency parameters associated with a sp rule.
 */
void sp_mapdb_get_wlan_latency_params(struct sk_buff *skb,
		uint8_t *service_interval_dl, uint32_t *burst_size_dl,
		uint8_t *service_interval_ul, uint32_t *burst_size_ul,
		uint8_t *smac, uint8_t *dmac)
{
	struct sp_mapdb_rule_node *curnode;
	int i;

	/*
	 * Look up for matching rule and find WiFi latency parameters
	 */
	rcu_read_lock();

	for (i = SP_MAPDB_RULE_MAX_PRECEDENCENUM - 1; i >= 0; i--) {
		list_for_each_entry_rcu(curnode, &(rule_manager.prec_map[i].rule_list), rule_list) {
			DEBUG_INFO("Matching with rid = %d\n", curnode->rule.id);
			if (sp_mapdb_rule_match(skb, &curnode->rule, smac, dmac)) {
				*service_interval_dl = curnode->rule.inner.service_interval_dl;
				*burst_size_dl = curnode->rule.inner.burst_size_dl;
				*service_interval_ul = curnode->rule.inner.service_interval_ul;
				*burst_size_ul = curnode->rule.inner.burst_size_ul;
				rcu_read_unlock();
				return;
			}
		}
	}

	/*
	 * No match found, set both latency parameters to zero
	 * which is invalid value
	 */
	*service_interval_dl = 0;
	*burst_size_dl = 0;
	*service_interval_ul = 0;
	*burst_size_ul = 0;

	rcu_read_unlock();
}
EXPORT_SYMBOL(sp_mapdb_get_wlan_latency_params);

/*
 * sp_mapdb_rule_query_wlan_params()
 * 	API to query WLAN parameters with WLAN driver through plugin
 */
static inline bool sp_mapdb_rule_query_wlan_params(struct sp_rule_wifi_plugin_metadata *wifi_metadata, struct sp_mapdb_rule_node *curnode, struct sp_rule_input_params *params)
{
	ether_addr_copy(wifi_metadata->dest_mac, params->dst.mac);
	ether_addr_copy(wifi_metadata->src_mac, params->src.mac);
	wifi_metadata->band_mode = curnode->rule.inner.band_mode;
	wifi_metadata->channel_mode = curnode->rule.inner.channel_mode;
	wifi_metadata->bandwidth_mode = curnode->rule.inner.bandwidth_mode;
	if (curnode->rule.inner.valid_ac) {
		wifi_metadata->valid_flags |= SP_RULE_AC_VALID;
		wifi_metadata->access_class = curnode->rule.inner.access_class;
	}

	wifi_metadata->priority = curnode->rule.inner.priority;
	wifi_metadata->ssid_len = curnode->rule.inner.ssid_len;
	memcpy(wifi_metadata->ssid, curnode->rule.inner.ssid, strlen(curnode->rule.inner.ssid) + 1);
	ether_addr_copy(wifi_metadata->ta_mac, curnode->rule.inner.transmitter_mac);
	ether_addr_copy(wifi_metadata->ra_mac, curnode->rule.inner.receiver_mac);
	ether_addr_copy(wifi_metadata->bssid, curnode->rule.inner.bssid);
	memcpy(wifi_metadata->radio_band, curnode->rule.inner.radio_band, sizeof(wifi_metadata->radio_band));
	memcpy(wifi_metadata->radio_chan, curnode->rule.inner.radio_channel, sizeof(wifi_metadata->radio_chan));
	memcpy(wifi_metadata->radio_bw, curnode->rule.inner.radio_bandwidth, sizeof(wifi_metadata->radio_bw));

	/*
	 * query metadata with WLAN through plugin
	 */
	if (!emesh_sp_wifi.sawf_rule_query_callback(wifi_metadata)) {

		DEBUG_INFO("WLAN rule match failed for rule_id : %d\n", curnode->rule.id);
		return false;
	}

	DEBUG_INFO("\nMatched with rule_id : %d\n", curnode->rule.id);
	return true;
}

/*
 * sp_mapdb_rule_apply_sawf()
 * 	Assign the desired PCP value into skb->priority,
 * 	return sp_rule_output_params structure
 */
void sp_mapdb_rule_apply_sawf(struct sk_buff *skb, struct sp_rule_input_params *params,
			      struct sp_rule_output_params *rule_output)
{
	int i;
	struct sp_mapdb_rule_node *curnode;
	struct sp_mapdb_rule_node *rule;
	uint8_t dscp_remark = SP_RULE_INVALID_DSCP_REMARK;
	uint8_t vlan_pcp_remark = SP_RULE_INVALID_VLAN_PCP_REMARK;
	uint8_t service_class_id = SP_RULE_INVALID_SERVICE_CLASS_ID;
	uint8_t output = SP_MAPDB_USE_DSCP;
	uint32_t rule_id = SP_RULE_INVALID_RULE_ID;
	uint8_t sawf_rule_type = SP_RULE_TYPE_SAWF_INVALID;
	enum sp_rule_ae_type ae_type = SP_RULE_AE_TYPE_DEFAULT;
	uint16_t ipv4_frag_thresh = SP_RULE_INVALID_IPV4_FRAG_THRESH;
	struct sp_rule_wifi_plugin_metadata wifi_metadata = {0};
	struct net_device *dest_dev, *src_dev;

	dest_dev = params->dest_dev;
	src_dev = params->src_dev;

	if (!dest_dev) {
		goto set_output;
	}

	rcu_read_lock();
	if (rule_manager.rule_count == 0) {
		rcu_read_unlock();
		DEBUG_WARN("rule table is empty\n");
		/*
		 * Rule table is empty.
		 */
		goto set_output;
	}

	rcu_read_unlock();
	wifi_metadata.valid_flags = 0;
	wifi_metadata.dest_dev = dest_dev;
	wifi_metadata.src_dev = src_dev;
	wifi_metadata.skb_prio = skb->priority;

	/*
	 * fill pcp if valid
	 */
	if (params->vlan_tci != SP_RULE_INVALID_VLAN_TCI) {
		wifi_metadata.pcp = (uint8_t)((params->vlan_tci & VLAN_PRIO_MASK) >> VLAN_PRIO_SHIFT);
		wifi_metadata.valid_flags |= SP_RULE_PCP_VALID;
	}

	/*
	 * fill dscp
	 */
	wifi_metadata.dscp = params->dscp;

	/*
	 * The iteration loop goes backward because
	 * rules should be matched in the precedence
	 * descending order.
	 */
	for (i = SP_MAPDB_RULE_MAX_PRECEDENCENUM - 1; i >= 0; i--) {
		list_for_each_entry_rcu(curnode, &(rule_manager.prec_map[i].rule_list), rule_list) {
			DEBUG_INFO("Matching with rule id = %d (sawf case)\n", curnode->rule.id);
			if (curnode->rule.classifier_type == SP_RULE_TYPE_SAWF) {
				if (curnode->rule.inner.wlan_flow) {

					/*
					 * Validate the WLAN parameters if sawf rule query has been registered
					 */
					if (emesh_sp_wifi.sawf_rule_query_callback) {

						/*
						 * current SPM rule has WLAN params
						 * 	validate with WLAN if they are valid or not
						 */
						if (!sp_mapdb_rule_query_wlan_params(&wifi_metadata, curnode, params)) {

							/*
							 * Current WLAN rules are not valid
							 * 	check for next rule.
							 */
							continue;
						}

						/*
						 * Current WLAN rules are valid
						 * 	Fill the output parameters
						 */
						goto fill_output;
					}

					/*
					 * Callback is not registered.
					 * check next rule.
					 */
					continue;
				}

				/*
				 * Current rule is not having any WLAN parameters or
				 * sawf rule query callback has not registered
				 * 	Continue with sp_mapdb_rule_match_sawf().
				 */
				if (!sp_mapdb_rule_match_sawf(&curnode->rule, params)) {
					continue;
				}

fill_output:
				output = curnode->rule.inner.rule_output;
				dscp_remark = curnode->rule.inner.dscp_remark;
				vlan_pcp_remark = curnode->rule.inner.vlan_pcp_remark;
				service_class_id = curnode->rule.inner.service_class_id;
				ipv4_frag_thresh = curnode->rule.inner.ipv4_frag_thresh;
				rule_id = curnode->rule.id;
				ae_type = curnode->rule.inner.ae_type;
				sawf_rule_type = SP_RULE_TYPE_SAWF;
				goto set_output;
			}
		}
	}

	/* Traverse for Legacy SCS rule type */
	for (i = SP_MAPDB_RULE_MAX_PRECEDENCENUM - 1; i >= 0; i--) {
		list_for_each_entry_rcu(curnode, &(rule_manager.prec_map[i].rule_list), rule_list) {
			DEBUG_INFO("Matching with rule id = %d (legacy scs case)\n", curnode->rule.id);
			if (curnode->rule.classifier_type == SP_RULE_TYPE_SAWF_SCS ||
					curnode->rule.classifier_type == SP_RULE_TYPE_SCS) {
				if (sp_mapdb_rule_match_sawf(&curnode->rule, params)) {
					output = curnode->rule.inner.rule_output;
					dscp_remark = curnode->rule.inner.dscp_remark;
					vlan_pcp_remark = curnode->rule.inner.vlan_pcp_remark;
					service_class_id = curnode->rule.inner.service_class_id;
					rule_id = curnode->rule.id;
					sawf_rule_type = SP_RULE_TYPE_SAWF_SCS;
					goto set_output;
				}
			}
		}
	}

	/* Traverse for SAWF-IFLI rule type */
	rule = sp_mapdb_get_ifli_rule(params);
	if (rule) {
		output = rule->rule.inner.rule_output;
		dscp_remark = rule->rule.inner.dscp_remark;
		service_class_id = rule->rule.inner.service_class_id;
		rule_id = rule->rule.id;
		sawf_rule_type = SP_RULE_TYPE_SAWF_IFLI;
		ae_type = rule->rule.inner.ae_type;
		rule_output->key = rule->rule.key;
		goto set_output;
	}

set_output:
	rule_output->service_class_id = service_class_id;
	rule_output->ipv4_frag_thresh = ipv4_frag_thresh;
	rule_output->rule_id = rule_id;
	rule_output->priority = output;
	rule_output->dscp_remark = dscp_remark;
	rule_output->vlan_pcp_remark = vlan_pcp_remark;
	rule_output->sawf_rule_type = sawf_rule_type;
	rule_output->ae_type = ae_type;
}
EXPORT_SYMBOL(sp_mapdb_rule_apply_sawf);

/*
 * sp_mapdb_apply_scs()
 * 	Assign the user priority value into skb->priority on rule match.
 */
void sp_mapdb_apply_scs(struct sk_buff *skb, struct sp_rule_input_params *params, struct sp_rule_output_params *output)
{
	int i;
	struct sp_mapdb_rule_node *curnode;
	uint8_t priority = SP_RULE_INVALID_PRIORITY;
	uint32_t rule_id = SP_RULE_INVALID_RULE_ID;
	rcu_read_lock();
	if (rule_manager.rule_count == 0) {
		rcu_read_unlock();
		DEBUG_WARN("rule table is empty\n");
		/*
		 * Rule table is empty.
		 */
		goto set_output;
	}
	rcu_read_unlock();

	/*
	 * The iteration loop goes backward because
	 * rules should be matched in the precedence
	 * descending order.
	 */
	for (i = SP_MAPDB_RULE_MAX_PRECEDENCENUM - 1; i >= 0; i--) {
		list_for_each_entry_rcu(curnode, &(rule_manager.prec_map[i].rule_list), rule_list) {
			DEBUG_INFO("Matching with rule id = %d (scs case)\n", curnode->rule.id);
			if (curnode->rule.classifier_type == SP_RULE_TYPE_SCS) {
				if (sp_mapdb_rule_match_sawf(&curnode->rule, params)) {
					priority = curnode->rule.inner.rule_output;
					rule_id = curnode->rule.id;
					goto set_output;
				}
			}
		}
	}

set_output:
	output->rule_id = rule_id;
	output->priority = priority;
}
EXPORT_SYMBOL(sp_mapdb_apply_scs);

/*
 * sp_mapdb_apply_mscs()
 *      Assign the user priority value into skb->priority on rule match.
 */
void sp_mapdb_apply_mscs(struct sk_buff *skb, struct sp_rule_input_params *params, struct sp_rule_output_params *output)
{
	int i;
	struct sp_mapdb_rule_node *curnode;
	uint8_t priority = SP_RULE_INVALID_PRIORITY;
	uint8_t mscs_tid_bitmap = SP_RULE_INVALID_MSCS_TID_BITMAP;
	uint32_t rule_id = SP_RULE_INVALID_RULE_ID;
	rcu_read_lock();
	if (rule_manager.rule_count == 0) {
		rcu_read_unlock();
		DEBUG_WARN("rule table is empty\n");
		/*
		 * Rule table is empty.
		 */
		goto set_output;
	}

	rcu_read_unlock();

	/*
	 * The iteration loop goes backward because
	 * rules should be matched in the precedence
	 * descending order.
	 */
	for (i = SP_MAPDB_RULE_MAX_PRECEDENCENUM - 1; i >= 0; i--) {
		list_for_each_entry_rcu(curnode, &(rule_manager.prec_map[i].rule_list), rule_list) {
			DEBUG_INFO("Matching with rule id = %d (mscs case)\n", curnode->rule.id);
			if (curnode->rule.classifier_type == SP_RULE_TYPE_MSCS) {
				if (sp_mapdb_rule_match_sawf(&curnode->rule, params)) {
					mscs_tid_bitmap = curnode->rule.inner.mscs_tid_bitmap;
					/*
					 * Check the priority of the tid bit map received from rule.
					 */
					if (mscs_tid_bitmap != SP_RULE_INVALID_MSCS_TID_BITMAP) {
						if ((1 << skb->priority) & mscs_tid_bitmap) {
							priority = skb->priority;
							rule_id = curnode->rule.id;
							goto set_output;
						}
					}
				}
			}
		}
	}

set_output:
	output->rule_id = rule_id;
	output->priority = priority;
}
EXPORT_SYMBOL(sp_mapdb_apply_mscs);

/*
 * sp_mapdb_rule_receive_status_notify()
 * 	Message reply to userspace about the status of rule addition or failure.
 */
int sp_mapdb_rule_receive_status_notify(struct sk_buff **msg, void **hdr, struct genl_info *info, uint32_t rule_id, uint8_t rule_result)
{
	*msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!*msg) {
		DEBUG_WARN("Failed to allocate netlink message to accomodate rule\n");
		return -ENOMEM;
	}

	*hdr = genlmsg_put(*msg, info->snd_portid, info->snd_seq,
			&sp_genl_family, 0, SPM_CMD_RULE_ACTION);

	if (!*hdr) {
		DEBUG_WARN("Failed to put hdr in netlink buffer\n");
		nlmsg_free(*msg);
		return -ENOMEM;
	}

	if (nla_put_u32(*msg, SP_GNL_ATTR_ID, rule_id) ||
		nla_put_u8(*msg, SP_GNL_ATTR_ADD_DELETE_RULE, rule_result)) {
		goto put_failure;
	}

	return 0;

put_failure:
	genlmsg_cancel(*msg, *hdr);
	nlmsg_free(*msg);
	return -EMSGSIZE;
}

/*
 * sp_mapdb_str_radio_band_to_enum()
 * 	Convert a string represented band to sp_rule_vdev_band enum
 */
static inline enum sp_mapdb_radio_band sp_mapdb_str_radio_band_to_idx(char *str)
{
	if (!(strcasecmp(str, "2G"))) {
		return SP_MAPDB_RADIO_BAND_2G;
	}

	if (!(strcasecmp(str, "5G"))) {
		return SP_MAPDB_RADIO_BAND_5G;
	}

	if (!(strcasecmp(str, "5GH"))) {
		return SP_MAPDB_RADIO_BAND_5GH;
	}

	if (!(strcasecmp(str, "5GL"))) {
		return SP_MAPDB_RADIO_BAND_5GL;
	}

	if (!(strcasecmp(str, "6G"))) {
		return SP_MAPDB_RADIO_BAND_6G;
	}

	return SP_MAPDB_RADIO_BAND_INVALID;
}

/*
 * sp_mapdb_str_radio_bw_to_enum()
 * 	Convert a string represented bandwidth to sp_rule_vdev_band enum
 */
static inline enum sp_mapdb_radio_bandwidth sp_mapdb_str_radio_bw_to_enum(char *str)
{
	if (!(strcasecmp(str, "20"))) {
		return SP_MAPDB_RADIO_BANDWIDTH_20;
	}

	if (!(strcasecmp(str, "40"))) {
		return SP_MAPDB_RADIO_BANDWIDTH_40;
	}

	if (!(strcasecmp(str, "80"))) {
		return SP_MAPDB_RADIO_BANDWIDTH_80;
	}

	if (!(strcasecmp(str, "160"))) {
		return SP_MAPDB_RADIO_BANDWIDTH_160;
	}

	if (!(strcasecmp(str, "80_80"))) {
		return SP_MAPDB_RADIO_BANDWIDTH_80_80;
	}

	if (!(strcasecmp(str, "320"))) {
		return SP_MAPDB_RADIO_BANDWIDTH_320;
	}

	return SP_MAPDB_RADIO_BANDWIDTH_INVALID;
}

/*
 * sp_mapdb_construct_radio_info_map()
 * 	Store the enum converted radio info to sp rule
 */
static inline bool sp_mapdb_construct_radio_info_map(struct sp_rule_radio_info *radio, struct sp_rule_wifi_plugin_metadata *wifi_metadata)
{
	int t;
	enum sp_mapdb_radio_band idx;
	enum sp_mapdb_radio_bandwidth bw;

	for (t = 1; t < SP_RULE_MAX_VDEV_PER_ML; t++) {
		idx = sp_mapdb_str_radio_band_to_idx(radio->radio_band[t]);
		wifi_metadata->radio_band[idx] = SP_RULE_BAND_VALID;
		bw = sp_mapdb_str_radio_bw_to_enum(radio->radio_bandwidth[t]);
		wifi_metadata->radio_bw[idx] = bw;
		wifi_metadata->radio_chan[idx] = radio->radio_channel[t];
	}

	return true;
}

/*
 * sp_mapdb_parse_wlan_rule()
 * 	Extract wlan parameters from netlink message.
 */
static inline bool sp_mapdb_parse_wlan_rule(struct genl_info *info, struct sp_rule *to_sawf_sp)
{
	int i, rem;
	struct sp_rule_radio_info radio = {0};
	struct sp_rule_wifi_plugin_metadata wifi_metadata = {0};
	struct nlattr *wlan_params = NULL, *attr = info->attrs[SP_GNL_ATTR_WLAN_FLOW];

	nla_for_each_nested(wlan_params, attr, rem) {
		switch(nla_type(wlan_params)) {
		case SP_GNL_ATTR_TRANSMITTER_MAC:
			memcpy(wifi_metadata.ta_mac, nla_data(wlan_params), ETH_ALEN);
			break;
		case SP_GNL_ATTR_RECEIVER_MAC:
			memcpy(wifi_metadata.ra_mac, nla_data(wlan_params), ETH_ALEN);
			break;
		case SP_GNL_ATTR_RADIO_BAND:
			memcpy(radio.radio_band, nla_data(wlan_params), sizeof(radio.radio_band));
			break;
		case SP_GNL_ATTR_RADIO_CHANNEL:
			memcpy(radio.radio_channel, nla_data(wlan_params), sizeof(radio.radio_channel));
			break;
		case SP_GNL_ATTR_RADIO_BANDWIDTH:
			memcpy(radio.radio_bandwidth, nla_data(wlan_params), sizeof(radio.radio_bandwidth));
			break;
		case SP_GNL_ATTR_BAND_MODE:
			wifi_metadata.band_mode = nla_get_u8(wlan_params);
			break;
		case SP_GNL_ATTR_CHANNEL_MODE:
			wifi_metadata.channel_mode = nla_get_u8(wlan_params);
			break;
		case SP_GNL_ATTR_BANDWIDTH_MODE:
			wifi_metadata.bandwidth_mode = nla_get_u8(wlan_params);
			break;
		case SP_GNL_ATTR_BSSID:
			memcpy(wifi_metadata.bssid, nla_data(wlan_params), ETH_ALEN);
			break;
		case SP_GNL_ATTR_SSID_LEN:
			wifi_metadata.ssid_len = nla_get_u8(wlan_params);
			break;
		case SP_GNL_ATTR_SSID:
			memcpy(wifi_metadata.ssid, nla_data(wlan_params), sizeof(wifi_metadata.ssid));
			break;
		case SP_GNL_ATTR_ACCESS_CLASS:
			wifi_metadata.access_class = nla_get_u8(wlan_params);
			to_sawf_sp->inner.valid_ac = true;
			wifi_metadata.valid_flags |= SP_RULE_AC_VALID;
			break;
		case SP_GNL_ATTR_PRIORITY:
			wifi_metadata.priority = nla_get_u8(wlan_params);
		}
	}

	sp_mapdb_construct_radio_info_map(&radio, &wifi_metadata);

	if (emesh_sp_wifi.sawf_rule_validate_callback) {

		/*
		 * Plugin callback is registered
		 */
		if (!emesh_sp_wifi.sawf_rule_validate_callback(&wifi_metadata)) {
			DEBUG_ERROR("WLAN params are not valid\n");
			return false;
		}
	} else {
		DEBUG_ERROR("SAWF rule validate callback not registered with Wi-Fi plugin\n");
		return false;
	}

	/*
	 * The rule has valid WLAN params.
	 * 	Add them to SPM.
	 */
	to_sawf_sp->inner.wlan_flow = 1;
	to_sawf_sp->inner.band_mode = wifi_metadata.band_mode;
	to_sawf_sp->inner.channel_mode = wifi_metadata.channel_mode;
	to_sawf_sp->inner.bandwidth_mode = wifi_metadata.bandwidth_mode;
	to_sawf_sp->inner.access_class = wifi_metadata.access_class;
	to_sawf_sp->inner.priority = wifi_metadata.priority;
	to_sawf_sp->inner.ssid_len = wifi_metadata.ssid_len;
	memcpy(to_sawf_sp->inner.ssid, wifi_metadata.ssid, strlen(wifi_metadata.ssid) + 1);
	ether_addr_copy(to_sawf_sp->inner.transmitter_mac, wifi_metadata.ta_mac);
	ether_addr_copy(to_sawf_sp->inner.receiver_mac, wifi_metadata.ra_mac);
	ether_addr_copy(to_sawf_sp->inner.bssid, wifi_metadata.bssid);
	memcpy(to_sawf_sp->inner.radio_band, wifi_metadata.radio_band, sizeof(wifi_metadata.radio_band));
	memcpy(to_sawf_sp->inner.radio_channel, wifi_metadata.radio_chan, sizeof(wifi_metadata.radio_chan));
	memcpy(to_sawf_sp->inner.radio_bandwidth, wifi_metadata.radio_bw, sizeof(wifi_metadata.radio_bw));

	/*
	 * dump the wlan params
	 */
	DEBUG_INFO("WLAN_flow: %d\n", to_sawf_sp->inner.wlan_flow);
	DEBUG_INFO("transmitter_mac : %pM\n", to_sawf_sp->inner.transmitter_mac);
	DEBUG_INFO("receiver_mac : %pM\n", to_sawf_sp->inner.receiver_mac);
	DEBUG_INFO("radio_band:");
	for (i = 1; i < SP_RULE_MAX_VDEV_PER_ML; ++i) {
		DEBUG_INFO("\n%s ", radio.radio_band[i]);
	}

	DEBUG_INFO("radio_channel: ");
	for (i = 1; i < SP_RULE_MAX_VDEV_PER_ML; ++i) {
		DEBUG_INFO("\n%d ", radio.radio_channel[i]);
	}

	DEBUG_INFO("\n");
	DEBUG_INFO("radio_bandwidth:");
	for (i = 1; i < SP_RULE_MAX_VDEV_PER_ML; ++i) {
		DEBUG_INFO("\n%s ", radio.radio_bandwidth[i]);
	}

	DEBUG_INFO("band_mode: %d\n", wifi_metadata.band_mode);
	DEBUG_INFO("channel_mode: %d\n", wifi_metadata.channel_mode);
	DEBUG_INFO("bandwidth_mode: %d\n", wifi_metadata.bandwidth_mode);
	DEBUG_INFO("bssid: %pM\n", wifi_metadata.bssid);
	DEBUG_INFO("ssid_len: %d\n", wifi_metadata.ssid_len);
	DEBUG_INFO("ssid: %s\n", wifi_metadata.ssid);
	DEBUG_INFO("access_class: %d\n", wifi_metadata.access_class);
	DEBUG_INFO("priority: %d\n", wifi_metadata.priority);

	return true;
}

/*
 * sp_mapdb_rule_receive()
 * 	Handles a netlink message from userspace for rule add/delete/update
 */
static inline int sp_mapdb_rule_receive(struct sk_buff *skb, struct genl_info *info)
{
	struct sp_rule to_sawf_sp = {0};
	int rule_cmd;
	uint32_t mask_sawf = 0;
	uint32_t mask_mesh = 0;
	sp_mapdb_update_result_t err;
	int rule_result;
	void *hdr = NULL;
	struct sk_buff *msg = NULL;
	struct net_device *dev;

	/*
	 * Set the invalid output values in rule to avoid these values to be set as 0's in
	 * EMESH-SAWF classifier. If valid values are received from userspace we set the flags
	 * and update the parameters accordingly.
	 */
	to_sawf_sp.inner.service_class_id = SP_RULE_INVALID_SERVICE_CLASS_ID;
	to_sawf_sp.inner.dscp_remark = SP_RULE_INVALID_DSCP_REMARK;
	to_sawf_sp.inner.vlan_pcp_remark = SP_RULE_INVALID_VLAN_PCP_REMARK;
	to_sawf_sp.inner.mscs_tid_bitmap = SP_RULE_INVALID_MSCS_TID_BITMAP;
	to_sawf_sp.inner.ipv4_frag_thresh = SP_RULE_INVALID_IPV4_FRAG_THRESH;

	rcu_read_lock();
	DEBUG_INFO("Recieved rule...\n");

	if (info->attrs[SP_GNL_ATTR_ID]) {
		to_sawf_sp.id = nla_get_u32(info->attrs[SP_GNL_ATTR_ID]);
		DEBUG_INFO("Rule id:  0x%x \n", to_sawf_sp.id);
	}

	if (info->attrs[SP_GNL_ATTR_ADD_DELETE_RULE]) {
		rule_cmd = nla_get_u8(info->attrs[SP_GNL_ATTR_ADD_DELETE_RULE]);
		if (rule_cmd == SP_MAPDB_ADD_REMOVE_FILTER_DELETE) {
			to_sawf_sp.cmd = rule_cmd;
			DEBUG_INFO("Deleting rule \n");
		} else if (rule_cmd == SP_MAPDB_ADD_REMOVE_FILTER_ADD) {
			to_sawf_sp.cmd = rule_cmd;
			DEBUG_INFO("Adding rule \n");
		} else {
			rcu_read_unlock();
			DEBUG_ERROR("Invalid rule cmd\n");
			rule_result = SP_MAPDB_UPDATE_RESULT_ERR_INVALIDENTRY;
			goto status_notify;
		}
	}

	if (info->attrs[SP_GNL_ATTR_RULE_PRECEDENCE]) {
		to_sawf_sp.rule_precedence = nla_get_u8(info->attrs[SP_GNL_ATTR_RULE_PRECEDENCE]);
		DEBUG_INFO("Rule precedence: 0x%x\n", to_sawf_sp.rule_precedence);
	}

	if (info->attrs[SP_GNL_ATTR_RULE_OUTPUT]) {
		to_sawf_sp.inner.rule_output = nla_get_u8(info->attrs[SP_GNL_ATTR_RULE_OUTPUT]);
		DEBUG_INFO("Rule output: 0x%x\n", to_sawf_sp.inner.rule_output);
	}

	if (info->attrs[SP_GNL_ATTR_USER_PRIORITY]) {
		to_sawf_sp.inner.user_priority = nla_get_u8(info->attrs[SP_GNL_ATTR_USER_PRIORITY]);
		DEBUG_INFO("User priority: 0x%x\n", to_sawf_sp.inner.user_priority);
		mask_mesh |= SP_RULE_FLAG_MATCH_UP;
	}

	if (info->attrs[SP_GNL_ATTR_SERVICE_CLASS_ID]) {
		to_sawf_sp.inner.service_class_id = nla_get_u8(info->attrs[SP_GNL_ATTR_SERVICE_CLASS_ID]);
		DEBUG_INFO("Service_class_id: 0x%x\n", to_sawf_sp.inner.service_class_id);
	}

	if (info->attrs[SP_GNL_ATTR_IPV4_FRAG_THRESH]) {
		to_sawf_sp.inner.ipv4_frag_thresh = nla_get_u16(info->attrs[SP_GNL_ATTR_IPV4_FRAG_THRESH]);
		DEBUG_INFO("IPv4_frag_thresh: %d\n", to_sawf_sp.inner.ipv4_frag_thresh);
	}

	if (info->attrs[SP_GNL_ATTR_SRC_PORT]) {
		to_sawf_sp.inner.src_port = nla_get_u16(info->attrs[SP_GNL_ATTR_SRC_PORT]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_SRC_PORT;
		mask_mesh |= SP_RULE_FLAG_MATCH_SRC_PORT;
		DEBUG_INFO("Source port: 0x%x\n", to_sawf_sp.inner.src_port);
	}

	if (info->attrs[SP_GNL_ATTR_DST_PORT]) {
		to_sawf_sp.inner.dst_port = nla_get_u16(info->attrs[SP_GNL_ATTR_DST_PORT]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_DST_PORT;
		mask_mesh |= SP_RULE_FLAG_MATCH_DST_PORT;
		DEBUG_INFO("Destination port: 0x%x\n", to_sawf_sp.inner.dst_port);
	}

	if (info->attrs[SP_GNL_ATTR_SRC_MAC]) {
		memcpy(to_sawf_sp.inner.sa, nla_data(info->attrs[SP_GNL_ATTR_SRC_MAC]), ETH_ALEN);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_SOURCE_MAC;
		mask_mesh |= SP_RULE_FLAG_MATCH_SOURCE_MAC;
		DEBUG_INFO("sa = %pM \n", to_sawf_sp.inner.sa);
	}

	if (info->attrs[SP_GNL_ATTR_DST_MAC]) {
		memcpy(to_sawf_sp.inner.da, nla_data(info->attrs[SP_GNL_ATTR_DST_MAC]), ETH_ALEN);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_DST_MAC;
		mask_mesh |= SP_RULE_FLAG_MATCH_DST_MAC;
		DEBUG_INFO("da = %pM \n", to_sawf_sp.inner.da);
	}

	if (info->attrs[SP_GNL_ATTR_IP_VERSION_TYPE]) {
		to_sawf_sp.inner.ip_version_type = nla_get_u8(info->attrs[SP_GNL_ATTR_IP_VERSION_TYPE]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_IP_VERSION_TYPE;
		DEBUG_INFO("IP Version type: 0x%x\n", to_sawf_sp.inner.ip_version_type);
	}

	if (info->attrs[SP_GNL_ATTR_SRC_IPV4_ADDR]) {
		to_sawf_sp.inner.src_ipv4_addr = nla_get_in_addr(info->attrs[SP_GNL_ATTR_SRC_IPV4_ADDR]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_SRC_IPV4;
		mask_mesh |= SP_RULE_FLAG_MATCH_SRC_IPV4;
		DEBUG_INFO("src_ipv4 = %pI4 \n", &to_sawf_sp.inner.src_ipv4_addr);
	}

	if (info->attrs[SP_GNL_ATTR_SRC_IPV4_ADDR_MASK]) {
		to_sawf_sp.inner.src_ipv4_addr_mask = nla_get_in_addr(info->attrs[SP_GNL_ATTR_SRC_IPV4_ADDR_MASK]);
		DEBUG_INFO("src_ipv4_mask = %pI4 \n", &to_sawf_sp.inner.src_ipv4_addr_mask);
		to_sawf_sp.inner.src_ipv4_addr &= to_sawf_sp.inner.src_ipv4_addr_mask;
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_SRC_IPV4_MASK;
	}

	if (info->attrs[SP_GNL_ATTR_DST_IPV4_ADDR]) {
		to_sawf_sp.inner.dst_ipv4_addr = nla_get_in_addr(info->attrs[SP_GNL_ATTR_DST_IPV4_ADDR]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_DST_IPV4;
		mask_mesh |= SP_RULE_FLAG_MATCH_DST_IPV4;
		DEBUG_INFO("dst_ipv4 = %pI4 \n", &to_sawf_sp.inner.dst_ipv4_addr);
	}

	if (info->attrs[SP_GNL_ATTR_DST_IPV4_ADDR_MASK]) {
		to_sawf_sp.inner.dst_ipv4_addr_mask = nla_get_in_addr(info->attrs[SP_GNL_ATTR_DST_IPV4_ADDR_MASK]);
		DEBUG_INFO("dst_ipv4_mask = %pI4 \n", &to_sawf_sp.inner.dst_ipv4_addr_mask);
		to_sawf_sp.inner.dst_ipv4_addr &= to_sawf_sp.inner.dst_ipv4_addr_mask;
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_DST_IPV4_MASK;
	}

	if (info->attrs[SP_GNL_ATTR_SRC_IPV6_ADDR]) {
		struct in6_addr saddr;
		saddr = nla_get_in6_addr(info->attrs[SP_GNL_ATTR_SRC_IPV6_ADDR]);
		memcpy(to_sawf_sp.inner.src_ipv6_addr, saddr.s6_addr32, sizeof(struct in6_addr));
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_SRC_IPV6;
		mask_mesh |= SP_RULE_FLAG_MATCH_SRC_IPV6;
		DEBUG_INFO("src_ipv6 = %pI6 \n", &to_sawf_sp.inner.src_ipv6_addr);
	}

	if (info->attrs[SP_GNL_ATTR_SRC_IPV6_ADDR_MASK]) {
		struct in6_addr saddr_mask;
		int i;

		saddr_mask = nla_get_in6_addr(info->attrs[SP_GNL_ATTR_SRC_IPV6_ADDR_MASK]);
		memcpy(to_sawf_sp.inner.src_ipv6_addr_mask, saddr_mask.s6_addr32, sizeof(struct in6_addr));
		DEBUG_INFO("src_ipv6_mask = %pI6 \n", &to_sawf_sp.inner.src_ipv6_addr_mask);

		for (i = 0; i < IPV6_ADDR_LEN; i++) {
			to_sawf_sp.inner.src_ipv6_addr[i] &= to_sawf_sp.inner.src_ipv6_addr_mask[i];
		}
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_SRC_IPV6_MASK;
	}

	if (info->attrs[SP_GNL_ATTR_DST_IPV6_ADDR]) {
		struct in6_addr daddr;
		daddr = nla_get_in6_addr(info->attrs[SP_GNL_ATTR_DST_IPV6_ADDR]);
		memcpy(to_sawf_sp.inner.dst_ipv6_addr, daddr.s6_addr32, sizeof(struct in6_addr));
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_DST_IPV6;
		mask_mesh |= SP_RULE_FLAG_MATCH_DST_IPV6;
		DEBUG_INFO("dst_ipv6 = %pI6\n", &to_sawf_sp.inner.dst_ipv6_addr);
	}

	if (info->attrs[SP_GNL_ATTR_DST_IPV6_ADDR_MASK]) {
		struct in6_addr daddr_mask;
		int i;

		daddr_mask = nla_get_in6_addr(info->attrs[SP_GNL_ATTR_DST_IPV6_ADDR_MASK]);
		memcpy(to_sawf_sp.inner.dst_ipv6_addr_mask, daddr_mask.s6_addr32, sizeof(struct in6_addr));
		DEBUG_INFO("dst_ipv6_mask = %pI6 \n", &to_sawf_sp.inner.dst_ipv6_addr_mask);

		for (i = 0; i < IPV6_ADDR_LEN; i++) {
			to_sawf_sp.inner.dst_ipv6_addr[i] &= to_sawf_sp.inner.dst_ipv6_addr_mask[i];
		}

		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_DST_IPV6_MASK;
	}

	if (info->attrs[SP_GNL_ATTR_PROTOCOL_NUMBER]) {
		to_sawf_sp.inner.protocol_number = nla_get_u8(info->attrs[SP_GNL_ATTR_PROTOCOL_NUMBER]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_PROTOCOL;
		mask_mesh |= SP_RULE_FLAG_MATCH_PROTOCOL;
		DEBUG_INFO("protocol_number: 0x%x\n", to_sawf_sp.inner.protocol_number);
	}

	if (info->attrs[SP_GNL_ATTR_VLAN_ID]) {
		to_sawf_sp.inner.vlan_id = nla_get_u16(info->attrs[SP_GNL_ATTR_VLAN_ID]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_VLAN_ID;
		mask_mesh |= SP_RULE_FLAG_MATCH_VLAN_ID;
		DEBUG_INFO("vlan_id: 0x%x\n", to_sawf_sp.inner.vlan_id);
	}

	if (info->attrs[SP_GNL_ATTR_DSCP]) {
		to_sawf_sp.inner.dscp = nla_get_u8(info->attrs[SP_GNL_ATTR_DSCP]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_DSCP;
		mask_mesh |= SP_RULE_FLAG_MATCH_DSCP;
		DEBUG_INFO("dscp: 0x%x\n", to_sawf_sp.inner.dscp);
	}

	if (info->attrs[SP_GNL_ATTR_DSCP_REMARK]) {
		to_sawf_sp.inner.dscp_remark = nla_get_u8(info->attrs[SP_GNL_ATTR_DSCP_REMARK]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_DSCP_REMARK;
		DEBUG_INFO("dscp remark: 0x%x\n", to_sawf_sp.inner.dscp_remark);
	}

	if (info->attrs[SP_GNL_ATTR_VLAN_PCP]) {
		to_sawf_sp.inner.vlan_pcp = nla_get_u8(info->attrs[SP_GNL_ATTR_VLAN_PCP]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_VLAN_PCP;
		DEBUG_INFO("vlan_pcp: 0x%x\n", to_sawf_sp.inner.vlan_pcp);
	}

	if (info->attrs[SP_GNL_ATTR_VLAN_PCP_REMARK]) {
		to_sawf_sp.inner.vlan_pcp_remark = nla_get_u8(info->attrs[SP_GNL_ATTR_VLAN_PCP_REMARK]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_VLAN_PCP_REMARK;
		DEBUG_INFO("vlan_pcp_remark: 0x%x\n", to_sawf_sp.inner.vlan_pcp_remark);
	}

	if (info->attrs[SP_GNL_ATTR_MATCH_PATTERN_VALUE]) {
		mask_sawf |= SP_RULE_FLAG_MATCH_SCS_SPI;
		to_sawf_sp.inner.match_pattern_value = nla_get_u32(info->attrs[SP_GNL_ATTR_MATCH_PATTERN_VALUE]);
	}

	if (info->attrs[SP_GNL_ATTR_MATCH_PATTERN_MASK]) {
		mask_sawf |= SP_RULE_FLAG_MATCH_SCS_SPI;
		to_sawf_sp.inner.match_pattern_mask = nla_get_u32(info->attrs[SP_GNL_ATTR_MATCH_PATTERN_MASK]);
	}

	if (info->attrs[SP_GNL_ATTR_DST_IFINDEX]) {
		mask_sawf |= SP_RULE_FLAG_MATCH_DST_IFINDEX;
		to_sawf_sp.inner.dst_ifindex = nla_get_u8(info->attrs[SP_GNL_ATTR_DST_IFINDEX]);
		DEBUG_INFO("Destination Interface Index: 0x%x\n", to_sawf_sp.inner.dst_ifindex);
	}

	if (info->attrs[SP_GNL_ATTR_TID_BITMAP]) {
		to_sawf_sp.inner.mscs_tid_bitmap = nla_get_u8(info->attrs[SP_GNL_ATTR_TID_BITMAP]);
		DEBUG_INFO("MSCS priority bitmap: 0x%x\n", to_sawf_sp.inner.mscs_tid_bitmap);
	}

	if (info->attrs[SP_GNL_ATTR_PRIORITY_LIMIT]) {
		to_sawf_sp.inner.priority_limit = nla_get_u8(info->attrs[SP_GNL_ATTR_PRIORITY_LIMIT]);
		DEBUG_INFO("Priority limit: 0x%x\n", to_sawf_sp.inner.priority_limit);
	}

	if ((info->attrs[SP_GNL_ATTR_SRC_PORT_RANGE_START] && !(info->attrs[SP_GNL_ATTR_SRC_PORT_RANGE_END])) ||
			(info->attrs[SP_GNL_ATTR_SRC_PORT_RANGE_END] && !(info->attrs[SP_GNL_ATTR_SRC_PORT_RANGE_START]))) {
		rcu_read_unlock();
		DEBUG_ERROR("Invalid input, please enter both start and end value for source port range\n");
		rule_result = SP_MAPDB_UPDATE_RESULT_ERR_INVALIDENTRY;
		goto status_notify;
	}

	if (info->attrs[SP_GNL_ATTR_SRC_PORT_RANGE_START] && info->attrs[SP_GNL_ATTR_SRC_PORT_RANGE_END]) {
		to_sawf_sp.inner.src_port_range_start = nla_get_u16(info->attrs[SP_GNL_ATTR_SRC_PORT_RANGE_START]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_SRC_PORT_RANGE_START;
		DEBUG_INFO("Source port range start: 0x%x\n", to_sawf_sp.inner.src_port_range_start);

		to_sawf_sp.inner.src_port_range_end = nla_get_u16(info->attrs[SP_GNL_ATTR_SRC_PORT_RANGE_END]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_SRC_PORT_RANGE_END;
		DEBUG_INFO("Source port range end: 0x%x\n", to_sawf_sp.inner.src_port_range_end);
	}

	if ((info->attrs[SP_GNL_ATTR_DST_PORT_RANGE_START] && !(info->attrs[SP_GNL_ATTR_DST_PORT_RANGE_END])) ||
			(info->attrs[SP_GNL_ATTR_DST_PORT_RANGE_END] && !(info->attrs[SP_GNL_ATTR_DST_PORT_RANGE_START]))) {
		rcu_read_unlock();
		DEBUG_ERROR("Invalid input, please enter both start and end value for destination port range\n");
		rule_result = SP_MAPDB_UPDATE_RESULT_ERR_INVALIDENTRY;
		goto status_notify;
	}

	if (info->attrs[SP_GNL_ATTR_DST_PORT_RANGE_START] && info->attrs[SP_GNL_ATTR_DST_PORT_RANGE_END]) {
		to_sawf_sp.inner.dst_port_range_start = nla_get_u16(info->attrs[SP_GNL_ATTR_DST_PORT_RANGE_START]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_DST_PORT_RANGE_START;
		DEBUG_INFO("Destination port range start: 0x%x\n", to_sawf_sp.inner.dst_port_range_start);

		to_sawf_sp.inner.dst_port_range_end = nla_get_u16(info->attrs[SP_GNL_ATTR_DST_PORT_RANGE_END]);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_DST_PORT_RANGE_END;
		DEBUG_INFO("Destination port range end: 0x%x\n", to_sawf_sp.inner.dst_port_range_end);
	}

	if (info->attrs[SP_GNL_ATTR_AE_TYPE]) {
		to_sawf_sp.inner.ae_type = nla_get_u8(info->attrs[SP_GNL_ATTR_AE_TYPE]);
		DEBUG_INFO("Ae type: 0x%x\n", to_sawf_sp.inner.ae_type);
	}

	if (info->attrs[SP_GNL_ATTR_SRC_IFACE]) {
		memcpy(to_sawf_sp.inner.src_iface, nla_data(info->attrs[SP_GNL_ATTR_SRC_IFACE]), IFNAMSIZ);
		dev = dev_get_by_name(&init_net, to_sawf_sp.inner.src_iface);
		if (!dev) {
			rcu_read_unlock();
			DEBUG_ERROR("Invalid input, please enter Valid Src Interface %s \n",to_sawf_sp.inner.src_iface);
			rule_result = SP_MAPDB_UPDATE_RESULT_ERR_INVALIDENTRY;
			goto status_notify;
		}

		to_sawf_sp.inner.src_ifindex = dev->ifindex;
		dev_put(dev);
		DEBUG_INFO("Source interface: %s Source interface index: %d \n", to_sawf_sp.inner.src_iface, to_sawf_sp.inner.src_ifindex);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_SRC_IFACE;
	}

	if (info->attrs[SP_GNL_ATTR_DST_IFACE]) {
		memcpy(to_sawf_sp.inner.dst_iface, nla_data(info->attrs[SP_GNL_ATTR_DST_IFACE]), IFNAMSIZ);
		dev = dev_get_by_name(&init_net, to_sawf_sp.inner.dst_iface);
		if (!dev) {
			rcu_read_unlock();
			DEBUG_ERROR("Invalid input, please enter Valid Destination Interface %s \n",to_sawf_sp.inner.dst_iface);
			rule_result = SP_MAPDB_UPDATE_RESULT_ERR_INVALIDENTRY;
			goto status_notify;
		}

		to_sawf_sp.inner.dst_ifindex = dev->ifindex;
		dev_put(dev);
		DEBUG_INFO("Destination interface: %s Destination Interface Index: %d \n", to_sawf_sp.inner.dst_iface, to_sawf_sp.inner.dst_ifindex);
		mask_sawf |= SP_RULE_FLAG_MATCH_SAWF_DST_IFACE;
	}

	/*
	 * Default classifier is SAWF, but if any other rule type is received, then classifier type will be
	 * overwritten by new type.
	 */
	if (info->attrs[SP_GNL_ATTR_SVC_INTERVAL_DL]) {
		to_sawf_sp.inner.service_interval_dl = nla_get_u8(info->attrs[SP_GNL_ATTR_SVC_INTERVAL_DL]);
		DEBUG_INFO("Service interval DL: 0x%x\n", to_sawf_sp.inner.service_interval_dl);
	}

	if (info->attrs[SP_GNL_ATTR_SVC_INTERVAL_UL]) {
		to_sawf_sp.inner.service_interval_ul = nla_get_u8(info->attrs[SP_GNL_ATTR_SVC_INTERVAL_UL]);
		DEBUG_INFO("Service interval UL: 0x%x\n", to_sawf_sp.inner.service_interval_ul);
	}

	if (info->attrs[SP_GNL_ATTR_BURST_SIZE_DL]) {
		to_sawf_sp.inner.burst_size_dl = nla_get_u32(info->attrs[SP_GNL_ATTR_BURST_SIZE_DL]);
		DEBUG_INFO("Burst size DL: 0x%x\n", to_sawf_sp.inner.burst_size_dl);
	}

	if (info->attrs[SP_GNL_ATTR_BURST_SIZE_UL]) {
		to_sawf_sp.inner.burst_size_ul = nla_get_u32(info->attrs[SP_GNL_ATTR_BURST_SIZE_UL]);
		DEBUG_INFO("Burst size UL: 0x%x\n", to_sawf_sp.inner.burst_size_ul);
	}

	if (info->attrs[SP_GNL_ATTR_SENSE_MESH_FLAG_IN]) {
		to_sawf_sp.inner.flags = nla_get_u32(info->attrs[SP_GNL_ATTR_SENSE_MESH_FLAG_IN]);
		DEBUG_INFO("MESH classfiers Flags: 0x%x\n", to_sawf_sp.inner.flags);
	}

	to_sawf_sp.classifier_type = SP_RULE_TYPE_SAWF;
	if (info->attrs[SP_GNL_ATTR_CLASSIFIER_TYPE]) {
		to_sawf_sp.classifier_type = nla_get_u8(info->attrs[SP_GNL_ATTR_CLASSIFIER_TYPE]);
	}

	DEBUG_INFO("classifier type: %d\n", to_sawf_sp.classifier_type);

	if (info->attrs[SP_GNL_ATTR_WLAN_FLOW]) {
		if (!sp_mapdb_parse_wlan_rule(info, &to_sawf_sp)) {
			rcu_read_unlock();
			DEBUG_ERROR("Invalid input for WLAN Params for rule_id : %d\n", to_sawf_sp.id);
			rule_result = SP_MAPDB_UPDATE_RESULT_ERR_INVALIDENTRY;
			goto status_notify;
		}
	}

	rcu_read_unlock();

	/*
	 * Update flag mask for valid rules
	 */
	to_sawf_sp.inner.flags_sawf = mask_sawf;
	to_sawf_sp.inner.flags |= mask_mesh ;

	/*
	 * Update rules in database
	 */
	rule_result = sp_mapdb_rule_update(&to_sawf_sp);

status_notify:
	err = sp_mapdb_rule_receive_status_notify(&msg, &hdr, info, to_sawf_sp.id, rule_result);
	if (err)
		return err;

	genlmsg_end(msg, hdr);
	return genlmsg_unicast(genl_info_net(info), msg, info->snd_portid);
}

/*
 * sp_mapdb_parse_radio_info()
 * 	API converting the radio info
 */
static inline void sp_mapdb_parse_radio_info(struct sp_rule_radio_info *radio, struct sp_rule *rule)
{
	char *band, *bw;
	int t;

	for (t = 1; t < SP_RULE_MAX_VDEV_PER_ML; t++) {
		if (rule->inner.radio_band[t] == SP_RULE_BAND_VALID) {
			band = sp_mapdb_enum_radio_band_to_str(t);
			if (band) {
				memcpy(radio->radio_band[t], band, sizeof(radio->radio_band[t]));
			}

			bw = sp_mapdb_enum_radio_bw_to_str(rule->inner.radio_bandwidth[t]);
			if (bw) {
				memcpy(radio->radio_bandwidth[t], bw, sizeof(radio->radio_bandwidth[t]));
			}

		}
	}
}

/*
 * sp_mapdb_wlan_rule_query()
 * 	API to fill WLAN params to netlink msg
 * 	in the event of rule query from user space
 */
static inline int sp_mapdb_wlan_rule_query(struct sk_buff *msg, struct sp_rule *rule)
{
	struct sp_rule_radio_info radio = {0};
	char ssid[WLAN_SSID_MAX_LEN];

	memcpy(ssid, rule->inner.ssid, sizeof(rule->inner.ssid));
	sp_mapdb_parse_radio_info(&radio, rule);
	if (nla_put_u8(msg, SP_GNL_ATTR_WLAN_FLOW, rule->inner.wlan_flow) ||
		nla_put(msg, SP_GNL_ATTR_TRANSMITTER_MAC, ETH_ALEN, rule->inner.transmitter_mac) ||
		nla_put(msg, SP_GNL_ATTR_RECEIVER_MAC, ETH_ALEN, rule->inner.receiver_mac) ||
		nla_put(msg, SP_GNL_ATTR_RADIO_BAND, sizeof(radio.radio_band), radio.radio_band) ||
		nla_put(msg, SP_GNL_ATTR_RADIO_CHANNEL, SP_RULE_MAX_VDEV_PER_ML, rule->inner.radio_channel) ||
		nla_put(msg, SP_GNL_ATTR_RADIO_BANDWIDTH, sizeof(radio.radio_bandwidth), radio.radio_bandwidth) ||
		nla_put_u8(msg, SP_GNL_ATTR_BAND_MODE, rule->inner.band_mode) ||
		nla_put_u8(msg, SP_GNL_ATTR_CHANNEL_MODE, rule->inner.channel_mode) ||
		nla_put_u8(msg, SP_GNL_ATTR_BANDWIDTH_MODE, rule->inner.bandwidth_mode) ||
		nla_put(msg, SP_GNL_ATTR_BSSID, ETH_ALEN, rule->inner.bssid) ||
		nla_put_u8(msg, SP_GNL_ATTR_SSID_LEN, rule->inner.ssid_len) ||
		nla_put(msg, SP_GNL_ATTR_SSID, sizeof(rule->inner.ssid), ssid) ||
		nla_put_u8(msg, SP_GNL_ATTR_ACCESS_CLASS, rule->inner.access_class) ||
		nla_put_u8(msg, SP_GNL_ATTR_PRIORITY, rule->inner.priority)) {
			return -1;
		}

	return 0;
}

/*
 * sp_mapdb_rule_query()
 * 	Handles a netlink message from userspace for rule query
 */
static inline int sp_mapdb_rule_query(struct sk_buff *skb, struct genl_info *info)
{
	uint32_t rule_id;
	struct sp_rule rule;
	struct sp_mapdb_rule_id_hashentry *cur_hashentry = NULL;
	void *hdr;
	struct sk_buff *msg = NULL;
	struct in6_addr saddr;
	struct in6_addr daddr;
	struct in6_addr saddr_mask;
	struct in6_addr daddr_mask;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg) {
		DEBUG_WARN("Failed to allocate netlink message to accomodate rule\n");
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq,
			  &sp_genl_family, 0, SPM_CMD_RULE_QUERY);
	if (!hdr) {
		DEBUG_WARN("Failed to put hdr in netlink buffer\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	rcu_read_lock();
	rule_id = nla_get_u32(info->attrs[SP_GNL_ATTR_ID]);
	if (!rule_id) {
		DEBUG_WARN("Rule ID does not exist for queried rule\n");
		return -ENOMEM;
	}

	DEBUG_INFO("User requested rule with rule_id: 0x%x \n", rule_id);
	rcu_read_unlock();

	spin_lock(&sp_mapdb_lock);
	if (!rule_manager.rule_count) {
		spin_unlock(&sp_mapdb_lock);
		DEBUG_WARN("Requested rule table is empty\n");
		goto put_failure;
	}

	cur_hashentry = sp_mapdb_search_hashentry(rule_id, rule_id, SP_RULE_TYPE_SAWF, NULL);
	if (!cur_hashentry) {
		spin_unlock(&sp_mapdb_lock);
		DEBUG_WARN("Invalid rule with ruleID = %d, rule_type: %d\n", rule_id, SP_RULE_TYPE_SAWF);
		goto put_failure;
	}

	rule = cur_hashentry->rule_node->rule;
	spin_unlock(&sp_mapdb_lock);

	if (nla_put_u32(msg, SP_GNL_ATTR_ID, rule.id) ||
	    nla_put_u8(msg, SP_GNL_ATTR_RULE_PRECEDENCE, rule.rule_precedence) ||
	    nla_put_u8(msg, SP_GNL_ATTR_RULE_OUTPUT, rule.inner.rule_output) ||
	    nla_put_u8(msg, SP_GNL_ATTR_CLASSIFIER_TYPE, rule.classifier_type) ||
	    nla_put(msg, SP_GNL_ATTR_SRC_MAC, ETH_ALEN, rule.inner.sa) ||
	    nla_put(msg, SP_GNL_ATTR_DST_MAC, ETH_ALEN, rule.inner.da)) {
		goto put_failure;
	}

	if (nla_put_in_addr(msg, SP_GNL_ATTR_SRC_IPV4_ADDR, rule.inner.src_ipv4_addr) ||
	    nla_put_in_addr(msg, SP_GNL_ATTR_DST_IPV4_ADDR, rule.inner.dst_ipv4_addr)) {
		goto put_failure;
	}

	if((nla_put(msg, SP_GNL_ATTR_SRC_IFACE, IFNAMSIZ, rule.inner.src_iface)) ||
	(nla_put(msg, SP_GNL_ATTR_DST_IFACE, IFNAMSIZ, rule.inner.dst_iface))) {
		goto put_failure;
	}

	memcpy(&saddr, rule.inner.src_ipv6_addr, sizeof(struct in6_addr));
	memcpy(&daddr, rule.inner.dst_ipv6_addr, sizeof(struct in6_addr));

	if (nla_put_in6_addr(msg, SP_GNL_ATTR_DST_IPV6_ADDR, &daddr) ||
	    nla_put_in6_addr(msg, SP_GNL_ATTR_SRC_IPV6_ADDR, &saddr)) {
		goto put_failure;
	}
	if (nla_put_in_addr(msg, SP_GNL_ATTR_SRC_IPV4_ADDR_MASK, rule.inner.src_ipv4_addr_mask) ||
	    nla_put_in_addr(msg, SP_GNL_ATTR_DST_IPV4_ADDR_MASK, rule.inner.dst_ipv4_addr_mask)) {
		goto put_failure;
	}

	memcpy(&saddr_mask, rule.inner.src_ipv6_addr_mask, sizeof(struct in6_addr));
	memcpy(&daddr_mask, rule.inner.dst_ipv6_addr_mask, sizeof(struct in6_addr));

	if (nla_put_in6_addr(msg, SP_GNL_ATTR_DST_IPV6_ADDR_MASK, &daddr_mask) ||
	    nla_put_in6_addr(msg, SP_GNL_ATTR_SRC_IPV6_ADDR_MASK, &saddr_mask)) {
		goto put_failure;
	}

	if (nla_put_u16(msg, SP_GNL_ATTR_SRC_PORT, rule.inner.src_port) ||
	    nla_put_u16(msg, SP_GNL_ATTR_DST_PORT, rule.inner.dst_port) ||
	    nla_put_u8(msg, SP_GNL_ATTR_PROTOCOL_NUMBER, rule.inner.protocol_number) ||
	    nla_put_u16(msg, SP_GNL_ATTR_VLAN_ID, rule.inner.vlan_id) ||
	    nla_put_u8(msg, SP_GNL_ATTR_DSCP, rule.inner.dscp) ||
	    nla_put_u8(msg, SP_GNL_ATTR_DSCP_REMARK, rule.inner.dscp_remark) ||
	    nla_put_u8(msg, SP_GNL_ATTR_VLAN_PCP, rule.inner.vlan_pcp) ||
	    nla_put_u8(msg, SP_GNL_ATTR_VLAN_PCP_REMARK, rule.inner.vlan_pcp_remark) ||
	    nla_put_u8(msg, SP_GNL_ATTR_SERVICE_CLASS_ID, rule.inner.service_class_id) ||
	    nla_put_u16(msg, SP_GNL_ATTR_IPV4_FRAG_THRESH, rule.inner.ipv4_frag_thresh) ||
	    nla_put_u8(msg, SP_GNL_ATTR_IP_VERSION_TYPE, rule.inner.ip_version_type) ||
	    nla_put_u32(msg, SP_GNL_ATTR_MATCH_PATTERN_VALUE, rule.inner.match_pattern_value) ||
	    nla_put_u32(msg, SP_GNL_ATTR_MATCH_PATTERN_MASK, rule.inner.match_pattern_mask) ||
	    nla_put_u8(msg, SP_RULE_FLAG_MATCH_MSCS_TID_BITMAP, rule.inner.mscs_tid_bitmap) ||
	    nla_put_u8(msg,  SP_RULE_FLAG_MATCH_PRIORITY_LIMIT, rule.inner.priority_limit) ||
	    nla_put_u8(msg,  SP_RULE_FLAG_MATCH_DST_IFINDEX, rule.inner.dst_ifindex) ||
	    nla_put_u16(msg, SP_GNL_ATTR_SRC_PORT_RANGE_START, rule.inner.src_port_range_start) ||
	    nla_put_u16(msg, SP_GNL_ATTR_SRC_PORT_RANGE_END, rule.inner.src_port_range_end) ||
	    nla_put_u16(msg, SP_GNL_ATTR_DST_PORT_RANGE_START, rule.inner.dst_port_range_start) ||
	    nla_put_u16(msg, SP_GNL_ATTR_DST_PORT_RANGE_END, rule.inner.dst_port_range_end) ||
	    nla_put_u8(msg, SP_GNL_ATTR_AE_TYPE, rule.inner.ae_type) ||
	    nla_put_u8(msg, SP_GNL_ATTR_SVC_INTERVAL_DL, rule.inner.service_interval_dl) ||
	    nla_put_u8(msg, SP_GNL_ATTR_SVC_INTERVAL_UL, rule.inner.service_interval_ul) ||
	    nla_put_u32(msg, SP_GNL_ATTR_BURST_SIZE_DL, rule.inner.burst_size_dl) ||
	    nla_put_u32(msg, SP_GNL_ATTR_BURST_SIZE_UL, rule.inner.burst_size_ul) ||
	    nla_put_u32(msg, SP_GNL_ATTR_SENSE_MESH_FLAG_IN, rule.inner.flags)) {
			goto put_failure;
		}

	if (rule.inner.wlan_flow == 1) {
		if (sp_mapdb_wlan_rule_query(msg, &rule)) {
			goto put_failure;
		}
	}

	genlmsg_end(msg, hdr);
	return genlmsg_reply(msg, info);

put_failure:
	genlmsg_cancel(msg, hdr);
	nlmsg_free(msg);
	return -EMSGSIZE;
}

/*
 * sp_mapdb_rule_query_by_type()
 * 	Handles a netlink message from userspace for rule query
 */
static inline int sp_mapdb_rule_query_by_type(struct sk_buff *skb, struct genl_info *info)
{
	uint8_t type;
	int i;
	struct sp_rule rule;
	void *hdr;
	struct sk_buff *msg = NULL;
	struct in6_addr saddr;
	struct in6_addr daddr;
	struct in6_addr saddr_mask;
	struct in6_addr daddr_mask;
	struct sp_mapdb_rule_node *curnode;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg) {
		DEBUG_WARN("Failed to allocate netlink message to accomodate rule\n");
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq,
			  &sp_genl_family, 0, SPM_CMD_RULE_QUERY);
	if (!hdr) {
		DEBUG_WARN("Failed to put hdr in netlink buffer\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	rcu_read_lock();
	type = nla_get_u8(info->attrs[SP_GNL_ATTR_CLASSIFIER_TYPE]);
	DEBUG_INFO("User requested rule with type: %d \n", type);
	rcu_read_unlock();

	if (type <= SP_RULE_TYPE_SAWF_INVALID || type >= SP_RULE_TYPE_SAWF_MAX) {
		DEBUG_WARN("type is invalid\n");
		goto put_failure;
	}

	if (type == SP_RULE_TYPE_SAWF_IFLI) {
		DEBUG_WARN("Query by type is not supported for IFLI rules\n");
		goto put_failure;
	}

	spin_lock(&sp_mapdb_lock);
	if (!rule_manager.rule_count) {
		spin_unlock(&sp_mapdb_lock);
		DEBUG_WARN("Requested rule table is empty\n");
		goto put_failure;
	}
	spin_unlock(&sp_mapdb_lock);

	for (i = SP_MAPDB_RULE_MAX_PRECEDENCENUM - 1; i >= 0; i--) {
		list_for_each_entry_rcu(curnode, &(rule_manager.prec_map[i].rule_list), rule_list) {
			if (curnode->rule.classifier_type == type) {
				rule = curnode->rule;

				if (nla_put_u32(msg, SP_GNL_ATTR_ID, rule.id) ||
				    nla_put_u8(msg, SP_GNL_ATTR_RULE_PRECEDENCE, rule.rule_precedence) ||
				    nla_put_u8(msg, SP_GNL_ATTR_RULE_OUTPUT, rule.inner.rule_output) ||
				    nla_put_u8(msg, SP_GNL_ATTR_CLASSIFIER_TYPE, rule.classifier_type) ||
				    nla_put(msg, SP_GNL_ATTR_SRC_MAC, ETH_ALEN, rule.inner.sa) ||
				    nla_put(msg, SP_GNL_ATTR_DST_MAC, ETH_ALEN, rule.inner.da)) {
					goto put_failure;
				}

				if (nla_put_in_addr(msg, SP_GNL_ATTR_SRC_IPV4_ADDR, rule.inner.src_ipv4_addr) ||
				    nla_put_in_addr(msg, SP_GNL_ATTR_DST_IPV4_ADDR, rule.inner.dst_ipv4_addr)) {
					goto put_failure;
				}

				memcpy(&saddr, rule.inner.src_ipv6_addr, sizeof(struct in6_addr));
				memcpy(&daddr, rule.inner.dst_ipv6_addr, sizeof(struct in6_addr));

				if (nla_put_in6_addr(msg, SP_GNL_ATTR_DST_IPV6_ADDR, &daddr) ||
				    nla_put_in6_addr(msg, SP_GNL_ATTR_SRC_IPV6_ADDR, &saddr)) {
					goto put_failure;
				}
				if (nla_put_in_addr(msg, SP_GNL_ATTR_SRC_IPV4_ADDR_MASK, rule.inner.src_ipv4_addr_mask) ||
				    nla_put_in_addr(msg, SP_GNL_ATTR_DST_IPV4_ADDR_MASK, rule.inner.dst_ipv4_addr_mask)) {
					goto put_failure;
				}

				memcpy(&saddr_mask, rule.inner.src_ipv6_addr_mask, sizeof(struct in6_addr));
				memcpy(&daddr_mask, rule.inner.dst_ipv6_addr_mask, sizeof(struct in6_addr));

				if (nla_put_in6_addr(msg, SP_GNL_ATTR_DST_IPV6_ADDR_MASK, &daddr_mask) ||
				    nla_put_in6_addr(msg, SP_GNL_ATTR_SRC_IPV6_ADDR_MASK, &saddr_mask)) {
					goto put_failure;
				}

				if (nla_put_u16(msg, SP_GNL_ATTR_SRC_PORT, rule.inner.src_port) ||
				    nla_put_u16(msg, SP_GNL_ATTR_DST_PORT, rule.inner.dst_port) ||
				    nla_put_u8(msg, SP_GNL_ATTR_PROTOCOL_NUMBER, rule.inner.protocol_number) ||
				    nla_put_u16(msg, SP_GNL_ATTR_VLAN_ID, rule.inner.vlan_id) ||
				    nla_put_u8(msg, SP_GNL_ATTR_DSCP, rule.inner.dscp) ||
				    nla_put_u8(msg, SP_GNL_ATTR_DSCP_REMARK, rule.inner.dscp_remark) ||
				    nla_put_u8(msg, SP_GNL_ATTR_VLAN_PCP, rule.inner.vlan_pcp) ||
				    nla_put_u8(msg, SP_GNL_ATTR_VLAN_PCP_REMARK, rule.inner.vlan_pcp_remark) ||
				    nla_put_u8(msg, SP_GNL_ATTR_SERVICE_CLASS_ID, rule.inner.service_class_id) ||
				    nla_put_u16(msg, SP_GNL_ATTR_IPV4_FRAG_THRESH, rule.inner.ipv4_frag_thresh) ||
				    nla_put_u8(msg, SP_GNL_ATTR_IP_VERSION_TYPE, rule.inner.ip_version_type) ||
				    nla_put_u32(msg, SP_GNL_ATTR_MATCH_PATTERN_VALUE, rule.inner.match_pattern_value) ||
				    nla_put_u32(msg, SP_GNL_ATTR_MATCH_PATTERN_MASK, rule.inner.match_pattern_mask) ||
				    nla_put_u8(msg, SP_RULE_FLAG_MATCH_MSCS_TID_BITMAP, rule.inner.mscs_tid_bitmap) ||
				    nla_put_u8(msg,  SP_RULE_FLAG_MATCH_PRIORITY_LIMIT, rule.inner.priority_limit) ||
				    nla_put_u8(msg,  SP_RULE_FLAG_MATCH_DST_IFINDEX, rule.inner.dst_ifindex) ||
				    nla_put_u16(msg, SP_GNL_ATTR_SRC_PORT_RANGE_START, rule.inner.src_port_range_start) ||
				    nla_put_u16(msg, SP_GNL_ATTR_SRC_PORT_RANGE_END, rule.inner.src_port_range_end) ||
				    nla_put_u16(msg, SP_GNL_ATTR_DST_PORT_RANGE_START, rule.inner.dst_port_range_start) ||
				    nla_put_u16(msg, SP_GNL_ATTR_DST_PORT_RANGE_END, rule.inner.dst_port_range_end) ||
				    nla_put_u8(msg, SP_GNL_ATTR_AE_TYPE, rule.inner.ae_type)) {
					goto put_failure;
				}

				genlmsg_end(msg, hdr);
				return genlmsg_reply(msg, info);

			}
		}
	}
put_failure:
	genlmsg_cancel(msg, hdr);
	nlmsg_free(msg);
	return -EMSGSIZE;

}

/*
 * sp_mapdb_ruletable_flush_classifier_type()
 * 	Handles a netlink message from userspace to flush rules
 * 	based on classifier type from spm database.
 */
static inline int sp_mapdb_ruletable_flush_classifier_type(struct sk_buff *skb, struct genl_info *info)
{
	int i;
	struct sp_mapdb_rule_node *node_list, *node, *tmp;
	struct sp_mapdb_rule_id_hashentry *hashentry_iter;
	struct hlist_node *hlist_tmp;
	int hash_bkt;
	uint8_t classifier_type;
	struct hlist_head tmp_head_hashentry;

	rcu_read_lock();

	if (!info->attrs[SP_GNL_ATTR_CLASSIFIER_TYPE]) {
		DEBUG_WARN("classifier type not selected by user, please select classifier type");
		rcu_read_unlock();
		return 0;
	}

	classifier_type = nla_get_u8(info->attrs[SP_GNL_ATTR_CLASSIFIER_TYPE]);
	DEBUG_INFO("User requested flush rules with classifier type : %d\n", classifier_type);

	rcu_read_unlock();

	INIT_HLIST_HEAD(&tmp_head_hashentry);
	spin_lock(&sp_mapdb_lock);
	if (rule_manager.rule_count == 0) {
		spin_unlock(&sp_mapdb_lock);
		DEBUG_WARN("The rule table is already empty. No action needed. \n");
		return 0;
	}

	for (i = 0; i < SP_MAPDB_RULE_MAX_PRECEDENCENUM; i++) {
		node_list = &rule_manager.prec_map[i];

		/*
		 * tmp as a temporary pointer to store the address of next node.
		 * This is required because we are using list_for_each_entry_safe,
		 * which allows in-loop deletion of the node.
		 */
		list_for_each_entry_safe(node, tmp, &node_list->rule_list, rule_list) {
			if (node->rule.classifier_type == classifier_type) {
				list_del_rcu(&node->rule_list);
				call_rcu(&node->rcu, sp_rule_destroy_rcu);
			}
		}
	}

	/* Free hash list. */
	hash_for_each_safe(rule_manager.rule_hashmap, hash_bkt, hlist_tmp, hashentry_iter, hlist) {
		if (hashentry_iter->rule_node->rule.classifier_type == classifier_type) {
			hash_del(&hashentry_iter->hlist);
			hlist_add_head(&hashentry_iter->hlist, &tmp_head_hashentry);
			rule_manager.rule_count--;
		}
	}

	spin_unlock(&sp_mapdb_lock);

	hlist_for_each_entry_safe(hashentry_iter, hlist_tmp, &tmp_head_hashentry, hlist) {
		hash_del(&hashentry_iter->hlist);
		kfree(hashentry_iter);
	}

	return 0;
}

/*
 * sp_mapdb_rule_sync_socket_init
 *	Initialized a global pointer with a socket value which will be used
 *	to send sync messages via netlink
 */
static inline int sp_mapdb_rule_sync_socket_init(struct sk_buff *skb, struct genl_info *info)
{
	sync_msg_net = genl_info_net(info);
	port_id = info->snd_portid;
	return 0;
}

/*
 * sp_mapdb_rule_sync_socket_exit
 *	Reset global variables used for sync message
 */
static inline int sp_mapdb_rule_sync_socket_exit(struct sk_buff *skb, struct genl_info *info)
{
	sync_msg_net = NULL;
	port_id = 0;
	return 0;
}

/*
 * sp_genl_policy
 * 	Policy attributes
 */
static struct nla_policy sp_genl_policy[SP_GNL_MAX + 1] = {
	[SP_GNL_ATTR_ID]		= { .type = NLA_U32, },
	[SP_GNL_ATTR_ADD_DELETE_RULE]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_RULE_PRECEDENCE]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_RULE_OUTPUT]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_USER_PRIORITY]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_SRC_MAC]		= { .len = ETH_ALEN, },
	[SP_GNL_ATTR_DST_MAC]		= { .len = ETH_ALEN, },
	[SP_GNL_ATTR_SRC_IPV4_ADDR]		= { .type = NLA_U32, },
	[SP_GNL_ATTR_SRC_IPV4_ADDR_MASK]	= { .type = NLA_U32, },
	[SP_GNL_ATTR_DST_IPV4_ADDR]		= { .type = NLA_U32, },
	[SP_GNL_ATTR_DST_IPV4_ADDR_MASK]	= { .type = NLA_U32, },
	[SP_GNL_ATTR_SRC_IPV6_ADDR]		= { .len = sizeof(struct in6_addr) },
	[SP_GNL_ATTR_SRC_IPV6_ADDR_MASK]	= { .len = sizeof(struct in6_addr) },
	[SP_GNL_ATTR_DST_IPV6_ADDR]		= { .len = sizeof(struct in6_addr) },
	[SP_GNL_ATTR_DST_IPV6_ADDR_MASK]	= { .len = sizeof(struct in6_addr) },
	[SP_GNL_ATTR_SRC_PORT]		= { .type = NLA_U16, },
	[SP_GNL_ATTR_DST_PORT]		= { .type = NLA_U16, },
	[SP_GNL_ATTR_PROTOCOL_NUMBER]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_VLAN_ID]		= { .type = NLA_U16, },
	[SP_GNL_ATTR_DSCP]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_DSCP_REMARK]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_VLAN_PCP]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_VLAN_PCP_REMARK]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_SERVICE_CLASS_ID]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_IP_VERSION_TYPE]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_CLASSIFIER_TYPE]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_MATCH_PATTERN_VALUE]		= { .type = NLA_U32, },
	[SP_GNL_ATTR_MATCH_PATTERN_MASK]		= { .type = NLA_U32, },
	[SP_GNL_ATTR_TID_BITMAP]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_PRIORITY_LIMIT]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_DST_IFINDEX]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_SRC_PORT_RANGE_START]	= { .type = NLA_U16, },
	[SP_GNL_ATTR_SRC_PORT_RANGE_END]	= { .type = NLA_U16, },
	[SP_GNL_ATTR_DST_PORT_RANGE_START]	= { .type = NLA_U16, },
	[SP_GNL_ATTR_DST_PORT_RANGE_END]	= { .type = NLA_U16, },
	[SP_GNL_ATTR_AE_TYPE]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_SRC_IFACE]		= { .type = NLA_NUL_STRING, },
	[SP_GNL_ATTR_DST_IFACE]		= { .type = NLA_NUL_STRING, },
	[SP_GNL_ATTR_AE_TYPE]			= { .type = NLA_U8, },
	[SP_GNL_ATTR_SVC_INTERVAL_DL]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_SVC_INTERVAL_UL]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_BURST_SIZE_DL]		= { .type = NLA_U32, },
	[SP_GNL_ATTR_BURST_SIZE_UL]		= { .type = NLA_U32, },
	[SP_GNL_ATTR_SENSE_MESH_FLAG_IN]		= { .type = NLA_U32, },
	[SP_GNL_ATTR_IPV4_FRAG_THRESH]          = { .type = NLA_U16, },
	[SP_GNL_ATTR_WLAN_FLOW] 		=	{ .type = NLA_NESTED, },
	[SP_GNL_ATTR_TRANSMITTER_MAC]	=	{ .len = ETH_ALEN, },
	[SP_GNL_ATTR_RECEIVER_MAC]	=	{ .len = ETH_ALEN, },
	[SP_GNL_ATTR_RADIO_BAND]	=	{ .type = NLA_NESTED_ARRAY, },
	[SP_GNL_ATTR_RADIO_CHANNEL]	=	{ .type = NLA_NESTED_ARRAY, },
	[SP_GNL_ATTR_RADIO_BANDWIDTH]	=	{ .type = NLA_NESTED_ARRAY, },
	[SP_GNL_ATTR_BAND_MODE] 	=	{ .type = NLA_U8, },
	[SP_GNL_ATTR_CHANNEL_MODE] 	=	{ .type = NLA_U8, },
	[SP_GNL_ATTR_BANDWIDTH_MODE] 	=	{ .type = NLA_U8, },
	[SP_GNL_ATTR_BSSID]	=	{ .len = ETH_ALEN },
	[SP_GNL_ATTR_SSID_LEN]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_SSID]		= { .type = NLA_NUL_STRING, },
	[SP_GNL_ATTR_ACCESS_CLASS]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_PRIORITY]		= { .type = NLA_U8, },
	[SP_GNL_ATTR_RM_SYNC_MSG]		= { .len = sizeof(struct sp_rm_sync_msg) },
};

/* Spm generic netlink operations */
static const struct genl_ops sp_genl_ops[] = {
	{
		.cmd = SPM_CMD_RULE_ACTION,
		.doit = sp_mapdb_rule_receive,
		.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = SPM_CMD_RULE_QUERY,
		.doit = sp_mapdb_rule_query,
		.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = SPM_CMD_RULE_FLUSH,
		.doit = sp_mapdb_ruletable_flush_classifier_type,
		.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = SPM_CMD_RULE_QUERY_BY_TYPE,
		.doit = sp_mapdb_rule_query_by_type,
		.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = SPM_CMD_SYNC_INIT,
		.doit = sp_mapdb_rule_sync_socket_init,
		.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = SPM_CMD_SYNC_EXIT,
		.doit = sp_mapdb_rule_sync_socket_exit,
		.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.flags = GENL_ADMIN_PERM,
	},
};

/* Spm generic family */
static struct genl_family sp_genl_family = {
	.name           = "spm",
	.version        = 0,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
	.resv_start_op	= SPM_CMD_MAX,
#endif
	.hdrsize        = 0,
	.maxattr        = SP_GNL_MAX,
	.policy 	= sp_genl_policy,
	.netnsok        = true,
	.module         = THIS_MODULE,
	.ops            = sp_genl_ops,
	.n_ops          = ARRAY_SIZE(sp_genl_ops),
};

/*
 * sp_mapdb_fini()
 * 	This is the function called when SPM is unloaded.
 */
void sp_mapdb_fini(void)
{
	sp_mapdb_ruletable_flush();
}

/*
 * sp_mapdb_wifi_plugin_query_wlan_rule_cb_register()
 * 	callback for plugin registration.
 */
int sp_mapdb_wifi_plugin_query_wlan_rule_cb_register(struct emesh_sp_wifi_sawf_callbacks *emesh_sp_wifi_cb)
{
	if (emesh_sp_wifi.sawf_rule_query_callback) {
		DEBUG_ERROR("EMESH_SP_WIFI_PLUGIN_QUERY_WLAN_RULE_CALLBACK already registered!!");
		return -1;
	}

	emesh_sp_wifi.sawf_rule_query_callback = emesh_sp_wifi_cb->sawf_rule_query_callback;
	return 0;
}
EXPORT_SYMBOL(sp_mapdb_wifi_plugin_query_wlan_rule_cb_register);

/*
 * sp_mapdb_wifi_plugin_query_wlan_rule_cb_unregister()
 * 	callback for plugin de-registration.
 */
void sp_mapdb_wifi_plugin_query_wlan_rule_cb_unregister(void)
{
	emesh_sp_wifi.sawf_rule_query_callback = NULL;
}
EXPORT_SYMBOL(sp_mapdb_wifi_plugin_query_wlan_rule_cb_unregister);

/*
 * sp_mapdb_wifi_plugin_validate_wlan_rule_cb_register()
 * 	callback for plugin registration.
 */
int sp_mapdb_wifi_plugin_validate_wlan_rule_cb_register(struct emesh_sp_wifi_sawf_callbacks *emesh_sp_wifi_cb)
{
	if (emesh_sp_wifi.sawf_rule_validate_callback) {
		DEBUG_ERROR("EMESH_SP_WIFI_PLUGIN_VALIDATE_WLAN_RULE_CALLBACK already registered!!");
		return -1;
	}

	emesh_sp_wifi.sawf_rule_validate_callback = emesh_sp_wifi_cb->sawf_rule_validate_callback;
	return 0;
}
EXPORT_SYMBOL(sp_mapdb_wifi_plugin_validate_wlan_rule_cb_register);

/*
 * sp_mapdb_wifi_plugin_validate_wlan_rule_cb_unregister()
 * 	callback for plugin de-registration.
 */
void sp_mapdb_wifi_plugin_validate_wlan_rule_cb_unregister(void)
{
	emesh_sp_wifi.sawf_rule_validate_callback = NULL;
}
EXPORT_SYMBOL(sp_mapdb_wifi_plugin_validate_wlan_rule_cb_unregister);

/*
 * sp_netlink_init()
 * 	Initialize generic netlink
 */
bool sp_netlink_init(void)
{
	int err;
	err = genl_register_family(&sp_genl_family);
	if (err) {
		DEBUG_ERROR("Failed to register sp generic netlink family with error: %d\n", err);
		return false;
	}
	return true;
}

/*
 * sp_netlink_exit()
 * 	Netlink exit
 */
bool sp_netlink_exit(void)
{
	int err;
	err = genl_unregister_family(&sp_genl_family);
	if (err) {
		DEBUG_ERROR("Failed to unregister sp generic netlink family with error: %d\n", err);
		return false;
	}
	return true;
}
