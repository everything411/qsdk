/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
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
 */

#ifndef _SP_API_H
#define _SP_API_H

#include <linux/types.h>

/*
 * sp_rule_match_flag
 * 	Rule match mask
 */
#define	SP_RULE_FLAG_MATCH_ALWAYS_TRUE		0x01		/* Rule match always true. s(skip field matching) mask */
#define	SP_RULE_FLAG_MATCH_UP			0x02		/* Match up in 802.11 qos control mask */
#define	SP_RULE_FLAG_MATCH_UP_SENSE		0x04 		/* UP in 802.11 qos control match sense mask */
#define	SP_RULE_FLAG_MATCH_SOURCE_MAC		0x08		/* Match source mac address mask */
#define	SP_RULE_FLAG_MATCH_SOURCE_MAC_SENSE	0x10		/* Match source mac address mask */
#define	SP_RULE_FLAG_MATCH_DST_MAC		0x20		/* Match destination mac address mask */
#define	SP_RULE_FLAG_MATCH_DST_MAC_SENSE	0x40		/* Destination mac address match sense mask */
#define	SP_RULE_FLAG_MATCH_SRC_IPV4		0x80		/* Match source ipv4 address match mask */
#define	SP_RULE_FLAG_MATCH_SRC_IPV4_SENSE	0x100		/* Source ipv4 address sense mask */
#define	SP_RULE_FLAG_MATCH_SRC_IPV6		0x200		/* Match source ipv6 address match mask */
#define	SP_RULE_FLAG_MATCH_SRC_IPV6_SENSE	0x400		/* Source ipv6 address sense mask */
#define	SP_RULE_FLAG_MATCH_DST_IPV4		0x800		/* Match destination ipv4 address match mask */
#define	SP_RULE_FLAG_MATCH_DST_IPV4_SENSE	0x1000		/* Destination ipv4 address sense mask */
#define	SP_RULE_FLAG_MATCH_DST_IPV6		0x2000		/* Match destination ipv6 address match mask */
#define	SP_RULE_FLAG_MATCH_DST_IPV6_SENSE	0x4000		/* Destination ipv6 address sense mask */
#define	SP_RULE_FLAG_MATCH_SRC_PORT		0x8000		/* Match source port match mask */
#define	SP_RULE_FLAG_MATCH_SRC_PORT_SENSE	0x10000		/* Source port sense mask */
#define	SP_RULE_FLAG_MATCH_DST_PORT		0x20000		/* Match destination port match mask */
#define	SP_RULE_FLAG_MATCH_DST_PORT_SENSE	0x40000		/* Destination port sense mask */
#define	SP_RULE_FLAG_MATCH_PROTOCOL		0x80000		/* Match protocol match mask */
#define	SP_RULE_FLAG_MATCH_PROTOCOL_SENSE	0x100000	/* Protocol sense mask */
#define	SP_RULE_FLAG_MATCH_VLAN_ID		0x200000	/* Match VLAN id match mask */
#define	SP_RULE_FLAG_MATCH_VLAN_ID_SENSE	0x400000	/* VLAN id sense mask */
#define	SP_RULE_FLAG_MATCH_DSCP			0x800000	/* Match dscp match mask */
#define	SP_RULE_FLAG_MATCH_DSCP_SENSE		0x1000000	/* DSCP sense mask */

/*
 * sp_rule_match_flag_sawf
 * 	Rule match mask for SAWF
 */
#define	SP_RULE_FLAG_MATCH_SAWF_SOURCE_MAC		0x01		/* Match sawf source mac address mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_MAC			0x02		/* Match sawf destination mac address mask */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_IPV4		0x04		/* Match sawf source ipv4 address match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_IPV6		0x08		/* Match sawf source ipv6 address match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_IPV4		0x10		/* Match sawf destination ipv4 address match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_IPV6		0x20		/* Match sawf destination ipv6 address match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_PORT		0x40		/* Match sawf source port match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_PORT		0x80		/* Match sawf destination port match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_PROTOCOL		0x100		/* Match sawf protocol match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_VLAN_ID			0x200		/* Match sawf VLAN id match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DSCP			0x400		/* Match sawf dscp match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_VLAN_PCP		0x800		/* Sawf Vlan priority match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_IPV4_MASK		0x1000		/* Sawf Source ipv4 address mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_IPV4_MASK		0x2000		/* Sawf Destination ipv4 address mask */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_IPV6_MASK		0x4000		/* Sawf Source ipv6 address mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_IPV6_MASK		0x8000		/* Sawf Destination ipv6 address mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DSCP_REMARK		0x10000		/* Sawf Match dscp remark mask */
#define	SP_RULE_FLAG_MATCH_SAWF_VLAN_PCP_REMARK		0x20000		/* Sawf Vlan priority remark mask */
#define	SP_RULE_FLAG_MATCH_SAWF_IP_VERSION_TYPE		0x40000		/* Sawf IP version type match mask */

/*
 * sp_rule_match_flag_sawf
 * 	Rule match mask for SAWF
 */
#define	SP_RULE_FLAG_MATCH_SCS_SPI			0x80000		/* SCS SPI match mask*/
#define SP_RULE_FLAG_MATCH_MSCS_TID_BITMAP		0x100000	/* MSCS Bitmap match*/
#define SP_RULE_FLAG_MATCH_PRIORITY_LIMIT		0x200000	/* Priority Limit value*/
#define	SP_RULE_FLAG_MATCH_IFINDEX			0x400000	/* Interface Index mask */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_PORT_RANGE_START	0x800000	/* Match sawf source port range start */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_PORT_RANGE_END	0x1000000	/* Match sawf source port range end */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_PORT_RANGE_START	0x2000000	/* Match sawf destination port range start */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_PORT_RANGE_END	0x4000000	/* Match sawf destination port range end */

#define IPV6_ADDR_LEN		4

#define	SP_RULE_INVALID_SERVICE_CLASS_ID	0xFF		/* Invalid Service class ID */
#define	SP_RULE_INVALID_DSCP_REMARK		0xFF		/* Invalid DSCP remark */
#define	SP_RULE_INVALID_VLAN_PCP_REMARK		0xFF		/* Invalid vlan PCP remark */
#define	SP_RULE_INVALID_RULE_ID			0xFFFFFFFF	/* Invalid Rule ID */
#define	SP_RULE_INVALID_VLAN_TCI		0xFFFF		/* Invalid vlan tci */
#define	SP_RULE_INVALID_PRIORITY		0xFF		/* Invalid Priority value */
#define SP_RULE_INVALID_MSCS_TID_BITMAP		0x00		/* Invalid MSCS Bitmap */

/*
 * sp_mapdb_update_results
 * 	Result values of rule update.
 */
enum sp_mapdb_update_results {
	SP_MAPDB_UPDATE_RESULT_ERR_TBLFULL, 		/* The rule table is full. */
	SP_MAPDB_UPDATE_RESULT_ERR_TBLEMPTY, 		/* The rule table is empty. */
	SP_MAPDB_UPDATE_RESULT_ERR_ALLOCNODE, 		/* Failed at allocating memory for a rule node. (struct sp_mapdb_rule_node) */
	SP_MAPDB_UPDATE_RESULT_ERR_ALLOCHASH, 		/* Failed at allocating memory for a hashentry. */
	SP_MAPDB_UPDATE_RESULT_ERR_RULENOEXIST, 	/* There is no such rule with the given rule id. */
	SP_MAPDB_UPDATE_RESULT_ERR_UNKNOWNBIT, 		/* Unknown add/remove filter bit. */
	SP_MAPDB_UPDATE_RESULT_ERR_SINGLE_WRITER,	/* Single writer protection violation. */
	SP_MAPDB_UPDATE_RESULT_ERR_INVALIDENTRY,	/* Invalid entry of rule field. */
	SP_MAPDB_UPDATE_RESULT_ERR_NEWRULE_NULLPTR,	/* New rule is a null pointer. */
	SP_MAPDB_UPDATE_RESULT_ERR, 			/* Delimiter */
	SP_MAPDB_UPDATE_RESULT_SUCCESS_ADD, 		/* Successful rule add */
	SP_MAPDB_UPDATE_RESULT_SUCCESS_DELETE, 		/* Successful rule deletion */
	SP_MAPDB_UPDATE_RESULT_SUCCESS_MODIFY, 		/* Successful rule modification */
	SP_MAPDB_UPDATE_RESULT_LAST,
};
typedef enum sp_mapdb_update_results sp_mapdb_update_result_t;

/*
 * sp_mapdb_notify_types
 * 	Types of notification.
 */
enum sp_mapdb_notify_types {
	SP_MAPDB_ADD_RULE,				/* Notify rule has been added. */
	SP_MAPDB_REMOVE_RULE,				/* Notify rule has been removed. */
	SP_MAPDB_MODIFY_RULE,				/* Notify rule has been modified. */
};
typedef enum sp_mapdb_notify_types sp_mapdb_notify_type_t;

/*
 * sp_mapdb_add_remove_filter_types
 * 	Possible value of Add-remove filter bit.
 */
enum sp_mapdb_add_remove_filter_types {
	SP_MAPDB_ADD_REMOVE_FILTER_DELETE, 		/* Delete a rule. */
	SP_MAPDB_ADD_REMOVE_FILTER_ADD, 		/* Add a rule. */
	SP_MAPDB_ADD_REMOVE_FILTER_QUERY,		/* Query rule table */
};
typedef enum sp_mapdb_add_remove_filter_types sp_mapdb_add_remove_filter_type_t;

struct sp_rule_inner {

	/*
	 * Flag bits for rule match
	 */
	uint32_t flags;

	/*
	 * Flag bits for sawf rule match
	 */
	uint32_t flags_sawf;

	/*
	 * The value of rule_output determines how to set the pcp value
	 * to be marked in the matched packet.
	 * Maximum possible value is 7.
	 */
	uint16_t rule_output;

	/*
	 * UP in 802.11 qos control
	 */
	uint8_t user_priority;

	/*
	 * Source mac address
	 * If “match source mac address” flag bit is set,
	 * this field shall be included, otherwise this field shall be omitted.
	 */
	uint8_t sa[ETH_ALEN];

	/*
	 * Destination mac address
	 * If “match destination mac address” flag
	 * bit is set, this field shall be included,
	 * otherwise this field shall be omitted.
	*/
	uint8_t da[ETH_ALEN];

	/*
	 * Source ipv4 address
	 */
	uint32_t src_ipv4_addr;
	/*
	 * Source ipv6 address
	 */
	uint32_t src_ipv6_addr[IPV6_ADDR_LEN];
	/*
	 * Destination ipv4 address
	 */
	uint32_t dst_ipv4_addr;
	/*
	 * Destination ipv6 address
	 */
	uint32_t dst_ipv6_addr[IPV6_ADDR_LEN];
	/*
	 * Source port number
	 */
	uint16_t src_port;
	/*
	 * Destination port number
	 */
	uint16_t dst_port;
	/*
	 * VLAN id
	 */
	uint16_t vlan_id;
	/*
	 * Protocol number
	 */
	uint8_t protocol_number;
	/*
	 * DSCP value
	 */
	uint8_t dscp;
	/*
	 * DSCP remark
	 */
	uint8_t dscp_remark;
	/*
	 * Service interval
	 * Specified msec
	 * This is min latency expectation and is used
	 * by Wi-Fi FW for peer tid queue scheduling
	 */
	uint8_t service_interval_dl;
	uint8_t service_interval_ul;
	/*
	 * Burst size
	 * Specified in bytes
	 * This is used by Wi-Fi FW for peer tid queue
	 * scheduling
	 */
	uint32_t burst_size_dl;
	uint32_t burst_size_ul;
	/*
	 * Vlan Priority
	 */
	uint8_t vlan_pcp;
	/*
	 * Vlan Priority Remark
	 */
	uint8_t vlan_pcp_remark;
	/*
	 * Service class id
	 */
	uint8_t service_class_id;

	/*
	 * Source ipv4 address mask
	 */
	uint32_t src_ipv4_addr_mask;

	/*
	 * Destination ipv4 address mask
	 */
	uint32_t dst_ipv4_addr_mask;

	/*
	 * Source ipv6 address mask
	 */
	uint32_t src_ipv6_addr_mask[IPV6_ADDR_LEN];

	/*
	 * Destination ipv6 address mask
	 */
	uint32_t dst_ipv6_addr_mask[IPV6_ADDR_LEN];

	/*
	 * Match pattern supported for SCS classifier (SPI is supported)
	 */
	uint32_t match_pattern_value;

	/*
	 * Match pattern mask supported for SCS classifier (SPI is supported)
	 */
	uint32_t match_pattern_mask;

	/*
	 * IP version type
	 */
	uint8_t ip_version_type;

	/*
	 * MSCS tid  Bitmap
	 */
	uint8_t mscs_tid_bitmap;

	/*
	 * Priority limit range
	 */
	uint8_t priority_limit;

	/*
	 * Interface Index
	 */
	uint8_t ifindex;

	/*
	 * Source port range start
	 */
	uint16_t src_port_range_start;

	/*
	 * Source port range end
	 */
	uint16_t src_port_range_end;

	/*
	 * Destination port range start
	 */
	uint16_t dst_port_range_start;

	/*
	 * Destination port range end
	 */
	uint16_t dst_port_range_end;
};

/*
 * sp_rule_classifier_type
 * 	This enum defines rule classifier type
 */
enum sp_rule_classifier_type {
	SP_RULE_TYPE_MESH,	/* For non-sawf rule */
	SP_RULE_TYPE_SAWF,	/* For SAWF rule */
	SP_RULE_TYPE_SCS,	/* For SCS rule */
	SP_RULE_TYPE_MSCS,	/* For MSCS rule */
	SP_RULE_TYPE_SAWF_SCS,	/* For SAWF-SCS rule type */
};

/*
 * sp_rule
 *	This struct stores Service Prioritization rule structure.
 */
struct sp_rule {
	u_int32_t id;						/* Service prioritization rule identifier */
	sp_mapdb_add_remove_filter_type_t cmd;			/* Command type. 1 means add 0 means delete. */
	struct sp_rule_inner inner;				/* Inner structure */
	uint8_t rule_precedence;				/* Rule precedence – higher number means higher priority. */
	uint8_t classifier_type;				/* rule_type:0 for Emesh case, 1 for SAWF, 2 for SCS */
};

/*
 * sp_3tuple_info
 * 	This structure lists 3 tuple parameters related to source/destination
 */
struct sp_3tuple_info {
	uint8_t mac[ETH_HLEN];		/* MAC address */
	uint16_t port;			/* Port ID */
	union {
		uint32_t ipv4_addr;		/* IPV4 address */
		uint32_t ipv6_addr[4];		/* IPV6 address */
	} ip;
};

/*
 * sp_rule_input_params
 * 	This structure lists input parameters for ecm query
 */
struct sp_rule_input_params {
	struct sp_3tuple_info src;		/* Source 3 tuple parameters */
	struct sp_3tuple_info dst;		/* Destination 3 tuple parameters */
	uint32_t spi;				/* SPI for ipsec packet */
	uint16_t protocol;			/* Protocol number */
	uint8_t dscp;				/* DSCP value */
	uint8_t ip_version_type;		/* IP Version type */
	uint16_t vlan_tci;			/* Vlan TCI */
	uint8_t ifindex;			/* interface index */
	uint8_t dev_addr[ETH_HLEN];		/* Netdevice address in case of WDS EXT case */
};

/*
 * sp_rule_output_params
 * 	This structure lists output parameters from SPM to ECM
 */
struct sp_rule_output_params {
	uint8_t service_class_id;	/* Service class ID */
	uint8_t priority;		/* Priority */
	uint8_t dscp_remark;		/* DSCP remark */
	uint8_t vlan_pcp_remark;	/* Vlan PCP remark */
	uint32_t rule_id;		/* Rule ID */
};

sp_mapdb_update_result_t sp_mapdb_rule_update(struct sp_rule *newrule);

void sp_mapdb_get_wlan_latency_params(struct sk_buff *skb, uint8_t *service_interval_dl, uint32_t *burst_size_dl, uint8_t *service_interval_ul, uint32_t *burst_size_ul, uint8_t *smac, uint8_t *dmac);
void sp_mapdb_apply(struct sk_buff *skb, uint8_t *smac, uint8_t *dmac);
void sp_mapdb_notifier_register(struct notifier_block *nb);
void sp_mapdb_notifier_unregister(struct notifier_block *nb);
void sp_mapdb_ruletable_flush(void);
void sp_mapdb_rule_apply_sawf(struct sk_buff *skb, struct sp_rule_input_params *params,
			      struct sp_rule_output_params *rule_output);
void sp_mapdb_apply_scs(struct sk_buff *skb, struct sp_rule_input_params *params,
			      struct sp_rule_output_params *rule_output);
void sp_mapdb_apply_mscs(struct sk_buff *skb, struct sp_rule_input_params *params,
			      struct sp_rule_output_params *output);
#endif
