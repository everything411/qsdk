/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_acl.h
 *	NSS PPE ACL definitions.
 */

#ifndef _PPE_ACL_H_
#define _PPE_ACL_H_

#include <linux/if.h>
#include <linux/if_ether.h>

#define PPE_ACL_PRI_NOMINAL 1
#define PPE_ACL_INVALID_HW_INDEX 0xFFFF
#define PPE_ACL_HW_INDEX_MAX 1024
#define PPE_ACL_GROUP_MAX 2
#define PPE_ACL_GROUP_DEFAULT 0

/**
 * ACL rule ID
 */
typedef int16_t ppe_acl_rule_id_t;

/**
 * ppe_acl_rule_match_type
 *	ACL rule types.
 */
typedef enum ppe_acl_rule_match_type {
	PPE_ACL_RULE_MATCH_TYPE_INVALID,		/**< Invalid rule type. */
	PPE_ACL_RULE_MATCH_TYPE_SMAC,			/**< SMAC rule type. */
	PPE_ACL_RULE_MATCH_TYPE_DMAC,			/**< DMAC rule type. */
	PPE_ACL_RULE_MATCH_TYPE_SVID,			/**< SVID rule type. */
	PPE_ACL_RULE_MATCH_TYPE_CVID,			/**< CVID rule type. */
	PPE_ACL_RULE_MATCH_TYPE_SPCP,			/**< SPCP rule type. */
	PPE_ACL_RULE_MATCH_TYPE_CPCP,			/**< CPCP rule type. */
	PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS,		/**< PPPOE session rule type. */
	PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE,		/**< Ether type rule */
	PPE_ACL_RULE_MATCH_TYPE_L3_1ST_FRAG,		/**< First IP fragment rule type. */
	PPE_ACL_RULE_MATCH_TYPE_IP_LEN,			/**< IP length rule type. */
	PPE_ACL_RULE_MATCH_TYPE_TTL_HOPLIMIT,		/**< TTL/hop-limit rule type. */
	PPE_ACL_RULE_MATCH_TYPE_TOS_TC,			/**< TOS/traffic-class rule type. */
	PPE_ACL_RULE_MATCH_TYPE_PROTO_NEXTHDR,		/**< L4 proto rule type. */
	PPE_ACL_RULE_MATCH_TYPE_TCP_FLAG,		/**< TCP flags rule type. */
	PPE_ACL_RULE_MATCH_TYPE_SIP,			/**< Source IP rule type. */
	PPE_ACL_RULE_MATCH_TYPE_DIP,			/**< Destination rule type. */
	PPE_ACL_RULE_MATCH_TYPE_SPORT,			/**< L4 source port rule type. */
	PPE_ACL_RULE_MATCH_TYPE_DPORT,			/**< L4 destination port rule type. */
	PPE_ACL_RULE_MATCH_TYPE_IP_GEN,			/**< General IP fields rule type. */
	PPE_ACL_RULE_MATCH_TYPE_UDF,			/**< User Defined fields (UDF) rule type. */
	PPE_ACL_RULE_MATCH_TYPE_UDF_WITH_IP,		/**< UDF with L3 IP (IPv4/IPv6) rule type.
							     Uses FAL_ACL_RULE_IP4 or FAL_ACL_RULE_IP6
							     as rule type instead of FAL_ACL_RULE_UDF. */
	PPE_ACL_RULE_MATCH_TYPE_CTPID,			/**< CTPID Index rule type. */
	PPE_ACL_RULE_MATCH_TYPE_STPID,			/**< STPID Index rule type. */
	PPE_ACL_RULE_MATCH_TYPE_CDEI,			/**< CDEI rule type. */
	PPE_ACL_RULE_MATCH_TYPE_SDEI,			/**< SDEI rule type. */
	PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE,		/**< DHCP rule type. */
	PPE_ACL_RULE_MATCH_TYPE_MC_TYPE,		/**< MC rule type. */
	PPE_ACL_RULE_MATCH_TYPE_DEFAULT,		/**< Default rule type. */
	PPE_ACL_RULE_MATCH_TYPE_MAX,			/**< Maximum rule type. */
} ppe_acl_rule_match_type_t;

/*
 * Rule type valid flag
 */
#define PPE_ACL_RULE_MATCH_TYPE_SMAC_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_SMAC)
#define PPE_ACL_RULE_MATCH_TYPE_DMAC_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_DMAC)
#define PPE_ACL_RULE_MATCH_TYPE_SVID_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_SVID)
#define PPE_ACL_RULE_MATCH_TYPE_CVID_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_CVID)
#define PPE_ACL_RULE_MATCH_TYPE_SPCP_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_SPCP)
#define PPE_ACL_RULE_MATCH_TYPE_CPCP_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_CPCP)
#define PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS_VALID	(1 << PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS)
#define PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE_VALID	(1 << PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE)
#define PPE_ACL_RULE_MATCH_TYPE_L3_1ST_FRAG_VALID	(1 << PPE_ACL_RULE_MATCH_TYPE_L3_1ST_FRAG)
#define PPE_ACL_RULE_MATCH_TYPE_IP_LEN_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_IP_LEN)
#define PPE_ACL_RULE_MATCH_TYPE_TTL_HOPLIMIT_VALID	(1 << PPE_ACL_RULE_MATCH_TYPE_TTL_HOPLIMIT)
#define PPE_ACL_RULE_MATCH_TYPE_TOS_TC_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_TOS_TC)
#define PPE_ACL_RULE_MATCH_TYPE_PROTO_NEXTHDR_VALID	(1 << PPE_ACL_RULE_MATCH_TYPE_PROTO_NEXTHDR)
#define PPE_ACL_RULE_MATCH_TYPE_TCP_FLAG_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_TCP_FLAG)
#define PPE_ACL_RULE_MATCH_TYPE_SIP_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_SIP)
#define PPE_ACL_RULE_MATCH_TYPE_DIP_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_DIP)
#define PPE_ACL_RULE_MATCH_TYPE_SPORT_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_SPORT)
#define PPE_ACL_RULE_MATCH_TYPE_DPORT_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_DPORT)
#define PPE_ACL_RULE_MATCH_TYPE_IP_GEN_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_IP_GEN)
#define PPE_ACL_RULE_MATCH_TYPE_UDF_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_UDF)
#define PPE_ACL_RULE_MATCH_TYPE_UDF_WITH_IP_VALID	(1 << PPE_ACL_RULE_MATCH_TYPE_UDF_WITH_IP)
#define PPE_ACL_RULE_MATCH_TYPE_DEFAULT_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_DEFAULT)
#define PPE_ACL_RULE_MATCH_TYPE_CTPID_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_CTPID)
#define PPE_ACL_RULE_MATCH_TYPE_STPID_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_STPID)
#define PPE_ACL_RULE_MATCH_TYPE_CDEI_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_CDEI)
#define PPE_ACL_RULE_MATCH_TYPE_SDEI_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_SDEI)
#define PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE)
#define PPE_ACL_RULE_MATCH_TYPE_MC_TYPE_VALID		(1 << PPE_ACL_RULE_MATCH_TYPE_MC_TYPE)

/*
 * ACL rule flag common
 */
#define PPE_ACL_RULE_CMN_FLAG_SNAP			0x00000001	/**< Rule common flag for SNAP packets. */
#define PPE_ACL_RULE_CMN_FLAG_FAKE_MAC			0x00000002	/**< Rule common flag for packets with fake mac header. */
#define PPE_ACL_RULE_CMN_FLAG_ETHERNET			0x00000004	/**< Rule common flag for ACL match for ethernet packet. */
#define PPE_ACL_RULE_CMN_FLAG_IPV4			0x00000008	/**< Rule common flag for ACL match for IPv4 packets. */
#define PPE_ACL_RULE_CMN_FLAG_IPV6			0x00000010	/**< Rule common flag for ACL match for IPv6 packets. */
#define PPE_ACL_RULE_CMN_FLAG_IP			0x00000020	/**< Rule common flag for ACL match for IP packets. */
#define PPE_ACL_RULE_CMN_FLAG_PRI_EN			0x00000040	/**< Rule common flag to indicate priority configuration. */
#define PPE_ACL_RULE_CMN_FLAG_POST_RT_EN		0x00000080	/**< Rule common flag to enable match for post routing fields. */
#define PPE_ACL_RULE_CMN_FLAG_OUTER_HDR_MATCH		0x00000100	/**< Rule common flag to enable match on tunnel outer headers fields. */
#define PPE_ACL_RULE_CMN_FLAG_NO_RULEID			0x00000200	/**< Rule common flag to indicate rule ID is not passed by caller. */
#define PPE_ACL_RULE_CMN_FLAG_FLOW_QOS_OVERRIDE		0x00000400	/**< Rule common flag to override QOS parameters from flow entry. */
#define PPE_ACL_RULE_CMN_FLAG_METADATA_EN		0x00000800	/**< Rule common flag to indicate METADATA ENABLE in ACL rule. */
#define PPE_ACL_RULE_CMN_FLAG_GROUP_EN			0x00001000	/**< Rule common flag to indicate priority configuration. */
#define PPE_ACL_RULE_CMN_FLOW_DIR_TYPE_US		0x00002000	/**< Rule commaon flag to indicate the flow direction type as upstream. */

/*
 * ACL rule flag general - applicable for each rule separately.
 */
#define PPE_ACL_RULE_GEN_FLAG_INVERSE_EN        0x00000001				/**< General flag for inverse matching. */
#define PPE_ACL_RULE_GEN_FLAG_LAST		PPE_ACL_RULE_GEN_FLAG_INVERSE_EN	/**< Last general flag. */

/*
 * VLAN ACL rule flag
 */
#define PPE_ACL_RULE_FLAG_CDEI_EN		(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with DEI bit in 1p header. */
#define PPE_ACL_RULE_FLAG_SDEI_EN		(PPE_ACL_RULE_GEN_FLAG_LAST << 2)	/**< Rule match with DEI bit in 1p header. */
#define PPE_ACL_RULE_FLAG_SVID_RANGE		(PPE_ACL_RULE_GEN_FLAG_LAST << 3)	/**< Rule match with S-VID range. */
#define PPE_ACL_RULE_FLAG_CVID_RANGE		(PPE_ACL_RULE_GEN_FLAG_LAST << 4)	/**< Rule match with C-VID range. */
#define PPE_ACL_RULE_FLAG_CVID_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 5)	/**< Rule match with VID mask. */
#define PPE_ACL_RULE_FLAG_SVID_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 6)	/**< Rule match with VID mask. */
#define PPE_ACL_RULE_FLAG_CPCP_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 7)	/**< Rule match with 802.1p mask. */
#define PPE_ACL_RULE_FLAG_SPCP_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 8)	/**< Rule match with 802.1p mask. */
#define PPE_ACL_RULE_FLAG_CTAG_FMT		(PPE_ACL_RULE_GEN_FLAG_LAST << 9)	/**< Rule match with ctagged format type. */
#define PPE_ACL_RULE_FLAG_STAG_FMT		(PPE_ACL_RULE_GEN_FLAG_LAST << 10)	/**< Rule match with stagged format type. */

/*
 * EXT VLAN ACL rule flag
 */
#define PPE_ACL_RULE_FLAG_STPID_EN		(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with PID bit. */
#define PPE_ACL_RULE_FLAG_CTPID_EN		(PPE_ACL_RULE_GEN_FLAG_LAST << 2)	/**< Rule match with PID bit. */
#define PPE_ACL_RULE_FLAG_DHCP_TYPE_EN		(PPE_ACL_RULE_GEN_FLAG_LAST << 3)	/**< Rule match with DHCP type. */
#define PPE_ACL_RULE_FLAG_MC_TYPE_EN		(PPE_ACL_RULE_GEN_FLAG_LAST << 4)	/**< Rule match with MC type. */

/*
 * L3 length rule flag.
 */
#define PPE_ACL_RULE_FLAG_IPLEN_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with L3 length mask. */
#define PPE_ACL_RULE_FLAG_IPLEN_RANGE		(PPE_ACL_RULE_GEN_FLAG_LAST << 2)	/**< Rule match with L3 length range. */

/*
 * MAC ACL rule flag.
 */
#define PPE_ACL_RULE_FLAG_MAC_MASK 		(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with MAC mask. */

/*
 * PPPOE session rule flag.
 */
#define PPE_ACL_RULE_FLAG_PPPOE_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with PPPOE session mask. */

/*
 * Ethertype ACL rule flag.
 */
#define PPE_ACL_RULE_FLAG_ETHTYPE_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with ethertype mask. */
#define PPE_ACL_RULE_EXT_ETH_TYPE_EN 		(PPE_ACL_RULE_GEN_FLAG_LAST << 2)	/**< Rule match for ext slices. */

/*
 * TTL/HOPLIMIT rule flag.
 */
#define PPE_ACL_RULE_FLAG_TTL_HOPLIMIT_MASK	(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with TTL/hop-limit mask. */

/*
 * DSCP/TC rule flag.
 */
#define PPE_ACL_RULE_FLAG_TOS_TC_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with TOS/TC mask. */

/*
 * L4 protocol rule flag.
 */
#define PPE_ACL_RULE_FLAG_PROTO_NEXTHDR_MASK	(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with proto/next-header mask. */

/*
 * IP general flags.
 */
#define PPE_ACL_RULE_FLAG_L3_FRAG		(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with IP fragment header. */
#define PPE_ACL_RULE_FLAG_ESP_HDR		(PPE_ACL_RULE_GEN_FLAG_LAST << 2)	/**< Rule match with ESP header. */
#define PPE_ACL_RULE_FLAG_AH_HDR		(PPE_ACL_RULE_GEN_FLAG_LAST << 3)	/**< Rule match with auth header. */
#define PPE_ACL_RULE_FLAG_MOBILITY_HDR		(PPE_ACL_RULE_GEN_FLAG_LAST << 4)	/**< Rule match with mobility header. */
#define PPE_ACL_RULE_FLAG_OTHER_EXT_HDR		(PPE_ACL_RULE_GEN_FLAG_LAST << 5)	/**< Rule match with other IPv6 header. */
#define PPE_ACL_RULE_FLAG_FRAG_HDR		(PPE_ACL_RULE_GEN_FLAG_LAST << 6)	/**< Rule match with fragment IPv6 header. */
#define PPE_ACL_RULE_FLAG_IPV4_OPTION		(PPE_ACL_RULE_GEN_FLAG_LAST << 7)	/**< Rule match with IPv4 option. */

/*
 * Tcp-flags rule flag.
 */
#define PPE_ACL_RULE_FLAG_TCP_FLG_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with TCP flag mask. */

/*
 * IP rule flag.
 */
#define PPE_ACL_RULE_FLAG_SIP_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with source IP mask. */
#define PPE_ACL_RULE_FLAG_DIP_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 2)	/**< Rule match with destination IP mask. */

/*
 * L4 port rule flag.
 */
#define PPE_ACL_RULE_FLAG_SPORT_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 1)	/**< Rule match with source port mask. */
#define PPE_ACL_RULE_FLAG_SPORT_RANGE		(PPE_ACL_RULE_GEN_FLAG_LAST << 2)	/**< Rule match with source port range. */
#define PPE_ACL_RULE_FLAG_DPORT_MASK		(PPE_ACL_RULE_GEN_FLAG_LAST << 3)	/**< Rule match with destination port mask. */
#define PPE_ACL_RULE_FLAG_DPORT_RANGE		(PPE_ACL_RULE_GEN_FLAG_LAST << 4)	/**< Rule match with destination port range. */

/*
 * ACL action flags.
 */
#define PPE_ACL_RULE_ACTION_FLAG_SERVICE_CODE_EN		0x00000001	/**< Rule action to change service code. */
#define PPE_ACL_RULE_ACTION_FLAG_QID_EN				0x00000002	/**< Rule action to change queue ID. */
#define PPE_ACL_RULE_ACTION_FLAG_ENQUEUE_PRI_CHANGE_EN		0x00000004	/**< Rule action to change enqueue priority. */
#define PPE_ACL_RULE_ACTION_FLAG_CTAG_DEI_CHANGE_EN		0x00000008	/**< Rule action to change C-DEI bit in 1p. */
#define PPE_ACL_RULE_ACTION_FLAG_CTAG_PCP_CHANGE_EN		0x00000010	/**< Rule action to change C-PCP marking. */
#define PPE_ACL_RULE_ACTION_FLAG_STAG_DEI_CHANGE_EN		0x00000020	/**< Rule action to change S-DEI bit in 1p. */
#define PPE_ACL_RULE_ACTION_FLAG_STAG_PCP_CHANGE_EN		0x00000040	/**< Rule action to change S-PCP marking. */
#define PPE_ACL_RULE_ACTION_FLAG_TOS_TC_CHANGE_EN		0x00000080	/**< Rule action to change TOS/traffic-class in IP header. */
#define PPE_ACL_RULE_ACTION_FLAG_CVID_CHANGE_EN			0x00000100	/**< Rule action to change C-VID. */
#define PPE_ACL_RULE_ACTION_FLAG_SVID_CHANGE_EN			0x00000200	/**< Rule action to change S-VID. */
#define PPE_ACL_RULE_ACTION_FLAG_DEST_INFO_CHANGE_EN		0x00000400	/**< Rule action to change destination. */
#define PPE_ACL_RULE_ACTION_FLAG_MIRROR_EN			0x00000800	/**< Rule action to enable mirroring. */
#define PPE_ACL_RULE_ACTION_FLAG_CTAG_VID_CMD			0x00001000	/**< ACL ctag VID command. */
#define PPE_ACL_RULE_ACTION_FLAG_STAG_VID_CMD			0x00002000	/**< ACL stag VID command. */
#define PPE_ACL_RULE_ACTION_FLAG_REDIR_TO_CORE_EN		0x00004000	/**< Rule action to enable redirection to a specific core. */
#define PPE_ACL_RULE_ACTION_FLAG_FW_CMD				0x00008000	/**< Rule action to enable special forwarding - forward/drop/copu/redirect. */
#define PPE_ACL_RULE_ACTION_FLAG_POLICER_EN			0x00010000	/**< Rule action to enable police. */
#define PPE_ACL_RULE_ACTION_FLAG_REDIR_EDIT_EN			0x00020000	/**< Rule action to enable redirect with packet editing enable. */
#define PPE_ACL_RULE_ACTION_FLAG_EXCEPTION_EDIT_EN		0x00040000	/**< Rule action to enable commit for packet edits. */
#define PPE_ACL_RULE_ACTION_FLAG_CTAG_PID_CHANGE_EN		0x00040000	/**< Rule action to change C-TPID. */
#define PPE_ACL_RULE_ACTION_FLAG_STAG_PID_CHANGE_EN		0x00080000	/**< Rule action to change S-TPID. */
#define PPE_ACL_RULE_ACTION_FLAG_DSCP_PBIT_MAP_IDX		0x00100000	/**< Rule action to change PCPs by mapping it to DSCP. */
#define PPE_ACL_RULE_ACTION_FLAG_COUNTER_EN			0x00200000	/**< Rule action to enable the COUNTER ID */
#define PPE_ACL_RULE_ACTION_FLAG_TAGS_TO_RMV_EN			0x00800000	/**< Rule action to remove tags. */
#define PPE_ACL_RULE_ACTION_FLAG_INT_DP_CHANGE_EN		0x01000000	/**< Rule action for internal drop precedence. */
#define PPE_ACL_RULE_ACTION_FLAG_SRC_INFO			0x02000000	/**< Rule action for src info. */
#define PPE_ACL_RULE_ACTION_FLAG_CTAG_VID_CHANGE_EN		0x04000000	/**< Rule action to change C-VID. */
#define PPE_ACL_RULE_ACTION_FLAG_STAG_VID_CHANGE_EN		0x08000000	/**< Rule action to change S-VID. */

#define PPE_ACL_RULE_ACTION_FLAG_CTAG_PID_CMD			0x00000001	/**< ACL ctag PID command. */
#define PPE_ACL_RULE_ACTION_FLAG_STAG_PID_CMD			0x00000002	/**< ACL stag PID command. */
#define PPE_ACL_RULE_ACTION_FLAG_CTAG_DEI_CMD			0x00000004	/**< ACL ctag DEI command. */
#define PPE_ACL_RULE_ACTION_FLAG_STAG_DEI_CMD			0x00000008	/**< ACL stag DEI command. */
#define PPE_ACL_RULE_ACTION_FLAG_CTAG_PCP_CMD			0x00000010	/**< ACL ctag PCP command. */
#define PPE_ACL_RULE_ACTION_FLAG_STAG_PCP_CMD			0x00000020	/**< ACL stag PCP command. */

/**
 * ppe_acl_rule_vlan_fmt
 *	VLAN format
 */
typedef enum ppe_acl_vtag_fmt {
	PPE_ACL_VTAG_FMT_UNTAGGED,		/**< VLAN untagged format. */
	PPE_ACL_VTAG_FMT_TAGGED,		/**< VLAN tagged format. */
	PPE_ACL_VTAG_FMT_PRI_TAGGED,		/**< VLAN priority tagged format. */
	PPE_ACL_VTAG_FMT_UNTAG_PRITAG,		/**< VLAN untagged and priority tagged format. */
	PPE_ACL_VTAG_FMT_UNTAG_TAG,		/**< VLAN untagged and tagged format. */
	PPE_ACL_VTAG_FMT_TAG_PRIORITY,		/**< VLAN tagged and priority tagged format */
	PPE_ACL_VTAG_FMT_ALL,			/**< VLAN untagged, tagged and priority tagged format. */
} ppe_acl_vtag_fmt_t;

/**
 * ppe_acl_rule_ip_type
 *	ACL rule IP type.
 */
typedef enum ppe_acl_ip_type {
	PPE_ACL_IP_TYPE_V4,			/**< Packet with IPv4 header. */
	PPE_ACL_IP_TYPE_V6			/**< Packet with IPv6 header. */
} ppe_acl_ip_type_t;

/**
 * ppe_acl_fwd_cmd
 *	PPE ACL forward action
 */
typedef enum ppe_acl_fwd_cmd {
	PPE_ACL_FWD_CMD_FWD,			/**< Forward command to allow regular forwarding. */
	PPE_ACL_FWD_CMD_DROP,			/**< Forward command to drop matching packets. */
	PPE_ACL_FWD_CMD_COPY,			/**< Forward command to copy the matching packets. */
	PPE_ACL_FWD_CMD_REDIR,			/**< Forward command to redirecting the matching packets. */
} ppe_acl_fwd_cmd_t;

/**
 * ppe_acl_rule_dev_type
 *	PPE ACL rule device tyoe
 */
typedef enum ppe_acl_rule_dev_type {
	PPE_ACL_RULE_DEV_TYPE_SRC_DEV = 1,	/**< Rule device is a net-device. */
	PPE_ACL_RULE_DEV_TYPE_SC,		/**< Rule device is a service code. */
	PPE_ACL_RULE_DEV_TYPE_FLOW,		/**< Rule device is flow. */
	PPE_ACL_RULE_DEV_TYPE_DEST_L2_PORT,	/**< Rule device is dest port. */
	PPE_ACL_RULE_DEV_TYPE_DEST_L3_PORT,	/**< Rule device is L3 dest port. */
} ppe_acl_rule_dev_type_t;

/*
 * ppe_acl_rule_dhcp_type
 *      ACL DHCP type
 */
typedef enum ppe_acl_rule_dhcp_type {
	PPE_ACL_RULE_DHCP_TYPE_NON=1,		/**< non dhcp rule. */
	PPE_ACL_RULE_DHCP_TYPE_V4,		/**< V4 dhcp rule. */
	PPE_ACL_RULE_DHCP_TYPE_V4_NON,		/**< V4 and non dhcp rule. */
	PPE_ACL_RULE_DHCP_TYPE_V6,		/**< V6 dhcp rule. */
	PPE_ACL_RULE_DHCP_TYPE_V6_NON,		/**< V6 and non dhcp rules. */
	PPE_ACL_RULE_DHCP_TYPE_V6_V4,		/**< V6 and V4 dhcp rule. */
	PPE_ACL_RULE_DHCP_TYPE_ALL,		/**< all dhcp rule. */
} ppe_acl_rule_dhcp_type_t;

/*
 * ppe_acl_rule_mc
 *      ACL MC type
 */
typedef enum ppe_acl_rule_mc_type {
	PPE_ACL_RULE_MC_TYPE_NON=1,		/**< no MC rule. */
	PPE_ACL_RULE_MC_TYPE_IP,		/**< IP MC rule. */
	PPE_ACL_RULE_MC_TYPE_IP_NON,		/**< IP MC and No MC rule. */
	PPE_ACL_RULE_MC_TYPE_NONIP,		/**< NONIP MC rule. */
	PPE_ACL_RULE_MC_TYPE_NONIP_NON,		/**< NONIP MC rule and NO MC rule. */
	PPE_ACL_RULE_MC_TYPE_IP_NONIP,		/**< IP and NON IP MC rule. */
	PPE_ACL_RULE_MC_TYPE_ALL,		/**< all MC rule. */
} ppe_acl_rule_mc_type_t;

/* ppe_acl_src_info_type
 * 	Source type: virtual port or L3 interface.
 */
typedef enum ppe_acl_src_info_type {
	PPE_ACL_SRC_INFO_TYPE_VP,		/**< Virtual port. */
	PPE_ACL_SRC_INFO_TYPE_L3_IF,		/**< L3 interface. */
} ppe_acl_src_info_type_t;

/* ppe_acl_dei_cmd
 * 	DEI (Drop Eligibility Indicator) command.
 */
typedef enum ppe_acl_dei_cmd {
	PPE_ACL_DEI_UNCHANGED,			/**< Keep DEI unchanged. */
	PPE_ACL_DEI_REPLACE,			/**< Replace DEI. */
	PPE_ACL_DEI_CPY_FRM_ORIG_SDEI,		/**< Copy from S‑Tag DEI. */
	PPE_ACL_DEI_CPY_FRM_ORIG_CDEI,		/**< Copy from C‑Tag DEI. */
} ppe_acl_dei_cmd_t;

/* ppe_acl_pcp_cmd
 * 	PCP (priority) command.
 */
typedef enum ppe_acl_pcp_cmd {
	PPE_ACL_PCP_UNCHANGED,			/**< Keep PCP unchanged. */
	PPE_ACL_PCP_REPLACE,			/**< Replace PCP. */
	PPE_ACL_PCP_CPY_FRM_ORIG_SPCP,		/**< Copy from S‑Tag PCP. */
	PPE_ACL_PCP_CPY_FRM_ORIG_CPCP,		/**< Copy from C‑Tag PCP. */
	PPE_ACL_PCP_DSCP_TO_PBIT,		/**< Map DSCP to PCP. */
	PPE_ACL_PCP_ADD_TAG_REPLACE_PCP,	/**< Add tag, replace PCP. */
	PPE_ACL_PCP_ADD_TAG_CPY_FRM_ORIG_SPCP,	/**< Add tag, copy S‑Tag PCP. */
	PPE_ACL_PCP_ADD_TAG_CPY_FRM_ORIG_CPCP,	/**< Add tag, copy C‑Tag PCP. */
	PPE_ACL_PCP_ADD_TAG_DSCP_TO_PBIT,	/**< Add tag, map DSCP. */
} ppe_acl_pcp_cmd_t;

/* ppe_acl_vid_cmd
 * 	VID (VLAN ID) command.
 */
typedef enum ppe_acl_vid_cmd {
	PPE_ACL_VID_UNCHANGED,			/**< Keep VID unchanged. */
	PPE_ACL_VID_REPLACE,			/**< Replace VID. */
	PPE_ACL_VID_CPY_FRM_ORIG_SVID,		/**< Copy S‑Tag VID. */
	PPE_ACL_VID_CPY_FRM_ORIG_CVID,		/**< Copy C‑Tag VID. */
} ppe_acl_vid_cmd_t;

/* ppe_acl_pid_cmd
 * 	TPID command.
 */
typedef enum ppe_acl_pid_cmd {
	PPE_ACL_PID_UNCHANGED,			/**< Keep TPID unchanged. */
	PPE_ACL_PID_REPLACE,			/**< Replace TPID. */
	PPE_ACL_PID_CPY_FRM_ORIG_STPID,		/**< Copy S‑Tag TPID. */
	PPE_ACL_PID_CPY_FRM_ORIG_CTPID,		/**< Copy C‑Tag TPID. */
} ppe_acl_pid_cmd_t;

/*
 * ppe_acl_counter_mode
 * 	counter mode for VLAN or PM.
 */
typedef enum ppe_acl_counter_mode {
	PPE_ACL_VLAN_DEV,			/**< VLAN dev type. */
	PPE_ACL_PON_PM,				/**< PON dev type. */
} ppe_acl_counter_mode_t;

/*
 * ppe_acl_dp_cmd
 *      ACL drop precedence
 */
typedef enum ppe_acl_dp_cmd {
	PPE_ACL_DP_0,                       /**< ACL drop precedence 0. */
	PPE_ACL_DP_1,                       /**< ACL drop precedence 1. */
	PPE_ACL_DP_2,                       /**< ACL drop precedence 2. */
	PPE_ACL_DP_3,                       /**< ACL drop precedence 3. */
} ppe_acl_dp_t;

/**
 * ppe_acl_ret
 *	ACL return code
 */
typedef enum ppe_acl_ret {
	PPE_ACL_RET_SUCCESS = 0,			/**< Success */
	PPE_ACL_RET_CREATE_FAIL_OOM,			/**< Rule create failed due to out of memory. */
	PPE_ACL_RET_CREATE_FAIL_MAX_SLICES,		/**< Rule create failed because rule require more than max slices. */
	PPE_ACL_RET_CREATE_FAIL_DRV_ALLOC,		/**< Rule create failed due to ppe-drv context alloc failure. */
	PPE_ACL_RET_CREATE_FAIL_RULE_CONFIG,		/**< Rule create failed due to invalid configuration. */
	PPE_ACL_RET_CREATE_FAIL_INVALID_SRC,		/**< Rule create failed due to invalid source. */
	PPE_ACL_RET_CREATE_FAIL_INVALID_CMN,		/**< Rule create failed due to invalid common rule. */
	PPE_ACL_RET_CREATE_FAIL_RULE_PARSE,		/**< Rule create failed due to rule parse failure. */
	PPE_ACL_RET_CREATE_FAIL_SC_ALLOC,		/**< Rule create failed due to service code allocation failure. */
	PPE_ACL_RET_CREATE_FAIL_ACTION_CONFIG,		/**< Rule create failed due to invalid action configuration. */
	PPE_ACL_RET_CREATE_FAIL_PM_CTX,			/**< Rule create failed due to pm context failure. */
	PPE_ACL_RET_CREATE_FAIL_INVALID_ID,		/**< Rule create failed due to invalid rule ID. */
	PPE_ACL_RET_DESTROY_FAIL_INVALID_ID,		/**< Rule destroy failed due to invalid rule ID. */
	PPE_ACL_RET_FLUSH_FAIL,				/**< Rule flush failed. */
	PPE_ACL_RET_UPDATE_PRI_FAIL_INVALID_ID,		/**< Rule update priority failed due to invalid ID. */
	PPE_ACL_RET_UPDATE_PRI_FAIL_INVALID_PRIORITY,	/**< Rule update priority failed due to out of bound priority. */
} ppe_acl_ret_t;

/**
 * ppe_acl_flush_type
 *	Flush type for ACL rule
 */
typedef enum ppe_acl_flush_type {
	PPE_ACL_FLUSH_TYPE_USERSPACE = 0,		/**< Flush userspace ACL rules. */
	PPE_ACL_FLUSH_TYPE_KERNELSPACE,			/**< Flush kernel ACL rules. */
	PPE_ACL_FLUSH_TYPE_ALL,				/**< Flush all the rules. */
} ppe_acl_flush_type_t;

/**
 * ppe_acl_rule_udf_base_type
 *	The base layer from which the UDF offset is calculated in an ACL rule.
 *	The offset can be relative to Layer 2, Layer 3, or Layer 4.
 * NOTE: Duplicate of ppe_drv_acl_udf_rule_base_type in ppe_drv_acl.h; ensure any changes are mirrored in both files.
 */
enum ppe_acl_rule_udf_base_type {
	PPE_ACL_UDF_OFFSET_BASE_L2,	/**< Offset is calculated from the start of Layer 2 */
	PPE_ACL_UDF_OFFSET_BASE_L3,	/**< Offset is calculated from the start of Layer 3 */
	PPE_ACL_UDF_OFFSET_BASE_L4,	/**< Offset is calculated from the start of Layer 4 */
};

/**
 * ppe_acl_rule_udf_proto
 *	ACL rule offset protocol type.
 * NOTE: Duplicate of ppe_drv_acl_udf_proto in ppe_drv_acl.h; ensure any changes are mirrored in both files.
 */
enum ppe_acl_rule_udf_proto {
	PPE_ACL_UDF_NON_IP,	/**< UDF offset for non-ip protocol type */
	PPE_ACL_UDF_IPV4,	/**< UDF offset for IPv4 protocol type */
	PPE_ACL_UDF_IPV6,	/**< UDF offset for IPv6 protocol type */
};

/**
 * ppe_acl_rule_udf_op
 *	ACL rule UDF match operation.
 * NOTE: Duplicate of ppe_drv_acl_rule_udf_op in ppe_drv_acl.h; ensure any changes are mirrored in both files.
 */
enum ppe_acl_rule_udf_op {
        PPE_ACL_UDF_OP_INVAL = 0,	/**< Invalid UDF operation */
        PPE_ACL_UDF_OP_MASK,		/**< Perform MASK-based matching */
        PPE_ACL_UDF_OP_RANGE,		/**< Perform RANGE-based matching */
        PPE_ACL_RULE_OP_LE,		/**< Perform Less-Than-Or-Equal comparison */
        PPE_ACL_RULE_OP_GE,		/**< Perform Greater-Than-Or-Equal comparison */
        PPE_ACL_UDF_OP_NE,		/**< Perform NOT-EQUAL matching */
};

/**
 * ppe_acl_rule_match_mac
 *	MAC based ACL rule.
 */
struct ppe_acl_rule_match_mac {
	/*
	 * Rule
	 */
	uint8_t mac[ETH_ALEN];			/**< MAC address to match. */
	uint8_t mac_mask[ETH_ALEN];		/**< MAC address mask to be used with match. */
};

/**
 * ppe_acl_rule_match_vlan
 *	VLAN based ACL rule.
 */
struct ppe_acl_rule_match_vlan {
	ppe_acl_vtag_fmt_t tag_fmt;		/**< VLAN tag format for ACL match. */
	uint8_t tag_fmt_mask;			/**< VLAN tag format mask for ACL match. */
	uint16_t vid_min;			/**< VLAN ID for ACL match. */
	uint16_t vid_mask_max;			/**< VLAN ID mask or maximum value for range. */
};

/**
 * ppe_acl_rule_match_tpid
 *	Tpid val based ACL rule.
 */
struct ppe_acl_rule_match_tpid {
	uint8_t tpid_val;			/**< TPID index for ACL match. */
	uint8_t tpid_mask;			/**< TPID index mask for ACL match.  */
};

/**
 * ppe_acl_rule_match_dei
 *	DEI based ACL rule.
 */
struct ppe_acl_rule_match_dei {
	uint8_t dei;				/**< DEI for ACL match. */
	uint8_t dei_mask;			/**< DEI mask for ACL match.  */
};
/**
 * ppe_acl_rule_match_dscp_type
 * 	Dscp type based ACL rule.
 */
struct ppe_acl_rule_match_dhcp_type {
	ppe_acl_rule_dhcp_type_t dhcp_type;	/**< DHCP type for ACL match. */
	uint8_t dhcp_mask;			/**< DHCP mask for ACL match. */
};

/**
 * ppe_acl_rule_match_mc_type
 * 	MC type based ACL rule.
 */
struct ppe_acl_rule_match_mc_type {
	ppe_acl_rule_mc_type_t mc_type;		/**< MC type for ACL match. */
	uint8_t mc_mask;			/**< MC mask for ACL match. */
};

/**
 * ppe_acl_rule_match_1p
 *	802.1p based ACL rule.
 */
struct ppe_acl_rule_match_1p {
	uint8_t pcp;				/**< PCP value for ACL match. */
	uint8_t pcp_mask;			/**< PCP mask for ACL match. */
};

/**
 * ppe_acl_rule_match_pppoe_session
 *	PPPOE session based ACL rule
 */
struct ppe_acl_rule_match_pppoe_session {
	uint16_t pppoe_session_id;		/**< PPPOE session ID for ACL match. */
	uint16_t pppoe_session_id_mask;		/**< PPPOE session ID mask for ACL match. */
};

/**
 * ppe_acl_rule_match_ether_type
 *	Ether-type based ACL rule.
 */
struct ppe_acl_rule_match_ether_type {
	uint16_t l2_proto;			/**< Ether type value for ACL match. */
	uint16_t l2_proto_mask;			/**< Ether type mask for ACL match. */
};

/**
 * ppe_acl_rule_match_ip
 *	IP based ACL rule
 */
struct ppe_acl_rule_match_ip {
	ppe_acl_ip_type_t ip_type;		/**< IP type for IP based ACL rule. */
	uint32_t ip[4];				/**< IP address for ACL match. */
	uint32_t ip_mask[4];			/**< IP address mask for ACL match. */
};

/**
 * ppe_acl_rule_match_l4_port
 *	L4 port based ACL rule
 */
struct ppe_acl_rule_match_l4_port {
	uint16_t l4_port_min;			/**< L4 port value for ACL match. */
	uint16_t l4_port_max_mask;		/**< L4 port mask or maximum value for range. */
};

/**
 * ppe_acl_rule_match_1st_frag
 *	ACL rule for first IP fragment
 */
struct ppe_acl_rule_match_1st_frag {
	uint8_t reserved;			/**< Reserved field - not used. */
};

/**
 * ppe_acl_rule_match_tcp_flag
 *	TCP flags based ACL rule.
 */
struct ppe_acl_rule_match_tcp_flag {
	uint8_t tcp_flags;			/**< TCP flags for ACL match. */
	uint8_t tcp_flags_mask;			/**< TCP flags mask for ACL match. */
};

/**
 * ppe_acl_rule_match_l4_proto
 *	L4 proto based ACL rule.
 */
struct ppe_acl_rule_match_l4_proto {
	uint8_t l3_v4proto_v6nexthdr;		/**< L4 proto for ACL match. */
	uint8_t l3_v4proto_v6nexthdr_mask;	/**< L4 proto mask for ACL match. */
};

/**
 * ppe_acl_rule_match_tos_tc
 *	ToS/TC based ACL rule.
 *	IPv4 header: 8-bit ToS [DSCP(6) & ECN(2)]
 *	IPv6 header: 8-bit TC [DSCP(6) & ECN(2)]
 */
struct ppe_acl_rule_match_tos_tc {
	uint8_t l3_tos_tc;			/**< ToS/TC value for ACL match. */
	uint8_t l3_tos_tc_mask;			/**< ToS/TC mask for ACL match. */
};


/**
 * ppe_acl_rule_match_l3_len
 *	ACL rule based on IP length.
 */
struct ppe_acl_rule_match_l3_len {
	uint16_t l3_length_min;			/**< L3 length for ACL match. */
	uint16_t l3_length_mask_max;		/**< L3 length mask of maximum value for range. */
};

/**
 * ppe_acl_rule_match_ttl_hop
 *	TTL/HOP limit based ACL rule.
 */
struct ppe_acl_rule_match_ttl_hop {
	uint8_t hop_limit;			/**< TTL/hop-limit for ACL match. */
	uint8_t hop_limit_mask;			/**< TTL/hop-limit mask for ACL match. */
};

/**
 * ppe_acl_rule_udf
 *	Single UDF configuration for ACL rule
 */
struct ppe_acl_rule_udf {
	enum ppe_acl_rule_udf_op valid_op;		/**< UDF valid operation for ACL match. */
	enum ppe_acl_rule_udf_proto proto;		/**< UDF offset prototype (IPv4, IPv6 or NON-IP). */
	enum ppe_acl_rule_udf_base_type offset_base;	/**< UDF offset reference (L2, L3 or L4). */
	uint32_t offset;				/**< UDF offset. */
	uint16_t min;					/**< UDF min/value for ACL match. */
	uint16_t mask_max;				/**< UDF max/mask for ACL match. */
};

/**
 * ppe_acl_rule_match_udf
 *	UDF based ACL rule
 *	NOTE: UDF-D is supporting only mask match operation.
 */
struct ppe_acl_rule_match_udf {
	struct ppe_acl_rule_udf udf_a;		/**< UDF-A configuration. */
	struct ppe_acl_rule_udf udf_b;		/**< UDF-B configuration. */
	struct ppe_acl_rule_udf udf_c;		/**< UDF-C configuration. */
	struct ppe_acl_rule_udf udf_d;		/**< UDF-D configuration. */
};

/*
 * IP miscellaneous match flags for ppe_acl_rule_match_ip_misc.flags.
 * Set the corresponding bit to indicate which fields are valid/enabled.
 */
enum ppe_acl_ip_misc_bit {
	PPE_ACL_IP_MISC_BIT_L3_FRAG = 0,	/**< Bit position for l3_fragment_flag. */
	PPE_ACL_IP_MISC_BIT_ESP_HDR,		/**< Bit position for esp_hdr_flag. */
	PPE_ACL_IP_MISC_BIT_AH_HDR,		/**< Bit position for ah_hdr_flag. */
	PPE_ACL_IP_MISC_BIT_MOBILITY_HDR,	/**< Bit position for mobility_hdr_flag. */
	PPE_ACL_IP_MISC_BIT_OTHER_EXT_HDR,	/**< Bit position for other_extension_hdr_flag. */
	PPE_ACL_IP_MISC_BIT_FRAG_HDR,		/**< Bit position for frag_hdr_flag. */
	PPE_ACL_IP_MISC_BIT_IPV4_OPTION,	/**< Bit position for l3_state_option_flag. */
	PPE_ACL_IP_MISC_BIT_HOP_LIMIT,		/**< Bit position for hop_limit. */
	PPE_ACL_IP_MISC_BIT_TCP_FLAGS,		/**< Bit position for tcp_flags. */
	PPE_ACL_IP_MISC_BIT_L3_1ST_FRAG,	/**< Bit position for l3_1st_fragment_flag. */
	PPE_ACL_IP_MISC_BIT_TOS_TC,		/**< Bit position for l3_tos_tc. */
	PPE_ACL_IP_MISC_BIT_PROTO_NEXTHDR,	/**< Bit position for l3_v4proto_v6nexthdr. */
	PPE_ACL_IP_MISC_BIT_L3_LEN,		/**< Bit position for l3_length. */
	PPE_ACL_IP_MISC_BIT_IP_VER,		/**< Bit position for ip_ver. */
};

#define PPE_ACL_IP_MISC_FLAG_L3_FRAG		BIT(PPE_ACL_IP_MISC_BIT_L3_FRAG)	/**< l3_fragment_flag field is valid. */
#define PPE_ACL_IP_MISC_FLAG_ESP_HDR		BIT(PPE_ACL_IP_MISC_BIT_ESP_HDR)	/**< esp_hdr_flag field is valid. */
#define PPE_ACL_IP_MISC_FLAG_AH_HDR		BIT(PPE_ACL_IP_MISC_BIT_AH_HDR)		/**< ah_hdr_flag field is valid. */
#define PPE_ACL_IP_MISC_FLAG_MOBILITY_HDR	BIT(PPE_ACL_IP_MISC_BIT_MOBILITY_HDR)	/**< mobility_hdr_flag field is valid. */
#define PPE_ACL_IP_MISC_FLAG_OTHER_EXT_HDR	BIT(PPE_ACL_IP_MISC_BIT_OTHER_EXT_HDR)	/**< other_extension_hdr_flag field is valid. */
#define PPE_ACL_IP_MISC_FLAG_FRAG_HDR		BIT(PPE_ACL_IP_MISC_BIT_FRAG_HDR)	/**< frag_hdr_flag field is valid. */
#define PPE_ACL_IP_MISC_FLAG_IPV4_OPTION	BIT(PPE_ACL_IP_MISC_BIT_IPV4_OPTION)	/**< l3_state_option_flag field is valid. */
#define PPE_ACL_IP_MISC_FLAG_HOP_LIMIT		BIT(PPE_ACL_IP_MISC_BIT_HOP_LIMIT)	/**< hop_limit field is valid. */
#define PPE_ACL_IP_MISC_FLAG_TCP_FLAGS		BIT(PPE_ACL_IP_MISC_BIT_TCP_FLAGS)	/**< tcp_flags field is valid. */
#define PPE_ACL_IP_MISC_FLAG_L3_1ST_FRAG	BIT(PPE_ACL_IP_MISC_BIT_L3_1ST_FRAG)	/**< l3_1st_fragment_flag field is valid. */
#define PPE_ACL_IP_MISC_FLAG_TOS_TC		BIT(PPE_ACL_IP_MISC_BIT_TOS_TC)		/**< l3_tos_tc field is valid. */
#define PPE_ACL_IP_MISC_FLAG_PROTO_NEXTHDR	BIT(PPE_ACL_IP_MISC_BIT_PROTO_NEXTHDR)	/**< l3_v4proto_v6nexthdr field is valid. */
#define PPE_ACL_IP_MISC_FLAG_L3_LEN		BIT(PPE_ACL_IP_MISC_BIT_L3_LEN)		/**< l3_length field is valid. */
#define PPE_ACL_IP_MISC_FLAG_IP_VER		BIT(PPE_ACL_IP_MISC_BIT_IP_VER)		/**< ip_ver field is valid. */

/**
 * ppe_acl_rule_match_ip_misc
 *	IP miscellaneous match fields for ACL rule.
 *	Mirrors ppe_drv_acl_ip_misc in ppe_drv_acl.h.
 *	Set the corresponding PPE_ACL_IP_MISC_FLAG_* bit in 'flags' to
 *	indicate which fields are valid/enabled before copying to the driver.
 */
struct ppe_acl_rule_match_ip_misc {
	uint32_t flags;			/**< Bitmask of PPE_ACL_IP_MISC_FLAG_* indicating which fields are valid. */
	ppe_acl_ip_type_t ip_ver;		/**< IP version based match. */
	uint8_t l3_fragment_flag;		/**< L3 fragment flag match. */
	uint8_t other_extension_hdr_flag;	/**< Other extension header flag match. */
	uint8_t frag_hdr_flag;			/**< Frag header flag match. */
	uint8_t mobility_hdr_flag;		/**< Mobility header flag match. */
	uint8_t esp_hdr_flag;			/**< ESP header flag match. */
	uint8_t ah_hdr_flag;			/**< AH header flag match. */
	uint8_t hop_limit;			/**< TTL/Hop-limit based match. */
	uint8_t l3_state_option_flag;		/**< L3 state option flag match. */
	uint8_t tcp_flags;			/**< TCP flags based match. */
	uint8_t l3_1st_fragment_flag;		/**< L3 first fragment flag match. */
	uint8_t l3_tos_tc;			/**< L3 ToS/TC based match. */
	uint8_t l3_v4proto_v6nexthdr;		/**< L4 proto/nexth-header based match. */
	uint16_t l3_length;			/**< L3 length based match. */

	uint8_t inverse_en;			/**< L3 fields match with inverse logic. */
	uint8_t range_en;			/**< L3 length range match. */

	/*
	 * Mask
	 */
	uint8_t ip_ver_mask;			/**< IP version mask. */
	uint8_t l3_fragment_flag_mask;		/**< L3 fragment flag mask. */
	uint8_t other_extension_hdr_flag_mask;	/**< Other extension header flag mask. */
	uint8_t frag_hdr_flag_mask;		/**< Frag header flag mask. */
	uint8_t mobility_hdr_flag_mask;		/**< Mobility header flag mask. */
	uint8_t esp_hdr_flag_mask;		/**< ESP header flag mask. */
	uint8_t ah_hdr_flag_mask;		/**< AH header flag mask. */
	uint8_t hop_limit_mask;			/**< TTL/Hop-limit based mask. */
	uint8_t l3_state_option_flag_mask;	/**< L3 state option flag mask. */
	uint8_t tcp_flags_mask;			/**< TCP flags mask. */
	uint8_t l3_1st_fragment_flag_mask;	/**< L3 first fragment flag mask. */
	uint8_t l3_tos_tc_mask;			/**< L3 ToS/TC mask. */
	uint8_t l3_v4proto_v6nexthdr_mask;	/**< L4 proto/nexth-header mask. */
	uint8_t reserved;			/**< Reserved for explicit 2-byte alignment of l3_length_mask_max. */
	uint16_t l3_length_mask_max;		/**< L3 length mask or max for range. */
};

/**
 * ppe_acl_rule_match_udf_with_ip
 *	UDF with L3 IP version based ACL rule.
 *	Matches UDF fields and sets the FAL rule type to IP4 or IP6
 *	(instead of UDF) based on the specified IP version.
 *	Optionally matches IP general/miscellaneous fields alongside
 *	the UDF fields using the same PPE_ACL_RULE_FLAG_* bitmask as
 *	PPE_ACL_RULE_MATCH_TYPE_IP_GEN.
 *	NOTE: UDF-D is supporting only mask match operation.
 */
struct ppe_acl_rule_match_udf_with_ip {
	struct ppe_acl_rule_udf udf_a;		/**< UDF-A configuration. */
	struct ppe_acl_rule_udf udf_b;		/**< UDF-B configuration. */
	struct ppe_acl_rule_udf udf_c;		/**< UDF-C configuration. */
	struct ppe_acl_rule_udf udf_d;		/**< UDF-D configuration. */
	bool is_ipv6;				/**< If true, use FAL_ACL_RULE_IP6 as rule type;
					     otherwise use FAL_ACL_RULE_IP4. */
	struct ppe_acl_rule_match_ip_misc ip_misc;
					/**< IP miscellaneous match fields (L3_FRAG, ESP_HDR,
					     AH_HDR, MOBILITY_HDR, OTHER_EXT_HDR, FRAG_HDR,
					     IPV4_OPTION). Zero flags = no IP misc matching. */
};


/**
 * ppe_acl_rule_match_default
 *	Default ACL rule.
 */
struct ppe_acl_rule_match_default {
	uint8_t reserved;			/**< Reserved field - not used. */
};

/**
 * ppe_acl_rule_action
 *	ACL rule action
 */
struct ppe_acl_rule_action {
	/*
	 * Action values.
	 */
	uint8_t service_code;			/**< Changed service code. */
	uint8_t qid;				/**< Changed queue ID. */
	uint8_t enqueue_pri;			/**< Changed enqueue priority. */
	uint8_t ctag_pcp;			/**< Changed CPCP. */
	uint8_t stag_pcp;			/**< Changed SPCP. */
	uint8_t ctag_dei;                       /**< C-DEI change action. */
	uint8_t stag_dei;                       /**< S-DEI change action. */
	uint8_t tos_tc;				/**< Changed TOS/TC. */
	uint16_t cvid;				/**< Changed CVID. */
	uint16_t svid;				/**< Changed SVID. */
	union {
		char dev_name[IFNAMSIZ];	/**< Changed destination device. */
	} dst;
	uint8_t redir_core;			/**< Redirect core. */
	uint16_t policer_id;			/**< Policer ID. */
	ppe_acl_fwd_cmd_t fwd_cmd;		/**< Forward action for matched packets. */
	uint8_t ctag_pid;                       /**< C-TPID change action. */
	uint8_t stag_pid;                       /**< S-TPID change action. */
	uint8_t dscp_pbit_map_idx;		/**< DSCP to pbit mapping index. */
	uint8_t counter_id;                     /**< Counter ID for stats action. */
	uint8_t hw_counter_id;			/**< HW Counter ID. */
	uint8_t tags_to_rmv;                    /**< Tags to remove. */
	uint8_t	src_info;			/**< source info. */
	ppe_acl_src_info_type_t src_info_type;	/**< source info type. */
	ppe_acl_counter_mode_t counter_mode;	/**< Counter mode for stats action. */
	ppe_acl_dp_t int_dp;			/**< Internal Drop Precedence. */
	ppe_acl_pcp_cmd_t ctag_pcp_cmd;		/**< C-tag pcp format action. */
	ppe_acl_pcp_cmd_t stag_pcp_cmd;		/**< S-tag pcp format action. */
	ppe_acl_dei_cmd_t ctag_dei_cmd;		/**< C-tag dei format action. */
	ppe_acl_dei_cmd_t stag_dei_cmd;		/**< S-tag dei format action. */
	ppe_acl_vid_cmd_t ctag_vid_cmd;		/**< C-tag vid format action. */
	ppe_acl_vid_cmd_t stag_vid_cmd;		/**< S-tag vid format action. */
	ppe_acl_pid_cmd_t ctag_pid_cmd;		/**< C-tag pid forward command. */
	ppe_acl_pid_cmd_t stag_pid_cmd;		/**< S-tag pid forward command. */

	/*
	 * Action control flags.
	 */
	uint32_t flags;				/**< Action control flags. */
	uint32_t flags_ext;			/**< Action control flags extended for PON */
};

/**
 * ppe_acl_rule_match_one
 *	Single ACL rule info
 */
struct ppe_acl_rule_match_one {
	uint32_t rule_flags;				/**< Rule flags. */
	union {
		struct ppe_acl_rule_match_mac smac;	/**< Rule with source-MAC address match. */
		struct ppe_acl_rule_match_mac dmac;	/**< Rule with destination-MAC address match. */
		struct ppe_acl_rule_match_vlan cvid;	/**< Rule with C-VLAN ID match. */
		struct ppe_acl_rule_match_vlan svid;	/**< Rule with S-VLAN ID match. */
		struct ppe_acl_rule_match_1p cpcp;	/**< Rule with C-PCP match. */
		struct ppe_acl_rule_match_1p spcp;	/**< Rule with S-PCP match. */
		struct ppe_acl_rule_match_pppoe_session pppoe_sess;
							/**< Rule with PPPOE session ID match. */
		struct ppe_acl_rule_match_ether_type ether_type;
							/**< Rule with ethertype match. */
		struct ppe_acl_rule_match_ip sip;	/**< Rule with source IP match. */
		struct ppe_acl_rule_match_ip dip;	/**< Rule with destination IP match. */
		struct ppe_acl_rule_match_l4_port sport;
							/**< Rule with source port match. */
		struct ppe_acl_rule_match_l4_port dport;
							/**< Rule with destination port match. */
		struct ppe_acl_rule_match_1st_frag first_frag;
							/**< Rule with first frag match. */
		struct ppe_acl_rule_match_tcp_flag tcp_flag;
							/**< Rule with tcp flag match. */
		struct ppe_acl_rule_match_l4_proto proto_nexthdr;
							/**< Rule with L4 proto match. */
		struct ppe_acl_rule_match_tos_tc tos_tc;
							/**< Rule with ToS/traffic-class match. */
		struct ppe_acl_rule_match_l3_len l3_len;
							/**< Rule with L3 length match. */
		struct ppe_acl_rule_match_ttl_hop ttl_hop;
							/**< Rule with TTL/hop-limit match. */
		struct ppe_acl_rule_match_udf udf;		/**< Rule with user-defined match. */
		struct ppe_acl_rule_match_udf_with_ip udf_with_ip;	/**< Rule with UDF and L3 IP version match. */
		struct ppe_acl_rule_match_default def;	/**< Rule with default match. */
		struct ppe_acl_rule_match_tpid ctpid;
							/**< Rule with CTPID val match. */
		struct ppe_acl_rule_match_tpid stpid;
							/**< Rule with STPID val match. */
		struct ppe_acl_rule_match_dei cdei;	/**< Rule with CDEI match. */
		struct ppe_acl_rule_match_dei sdei;	/**< Rule with SDEI match. */
		struct ppe_acl_rule_match_dhcp_type dhcp_type;
							/**< Rule with DHCP type match. */
		struct ppe_acl_rule_match_mc_type mc_type;
							/**< Rule with MC type match. */
	} rule;
};

/**
 * ppe_acl_rule_flow_policer
 *	ACL rule info for flow policer.
 *
 * TODO: Move this to internal header file.
 */
struct ppe_acl_rule_flow_policer {
	/*
	 * Request
	 */
	uint16_t hw_policer_idx;			/**< Policer index to be used for flow + policer combination. */
	ppe_acl_rule_id_t rule_id;			/**< ACL rule ID associated with flow + policer. */
	bool pkt_noedit;				/**< Indication if policing is required in packet non-edit mode */

	/*
	 * Response
	 */
	uint8_t sc;					/**< Flow service code to be used to bind flow with ACL. */
	ppe_acl_ret_t ret;				/**< Return status. */
};

/**
 * ppe_acl_rule_match_cmn
 *	Common fields for ACL rule
 */
struct ppe_acl_rule_match_cmn {
	uint32_t cmn_flags;				/**< Rule match common flag. */
	uint16_t pri;					/**< ACL rule priority. */
	uint8_t group;					/**< ACL rule chain resolution. */
};

/**
 * ppe_acl_rule
 *	Multiple ACL rules.
 */
struct ppe_acl_rule {
	/*
	 * Request
	 */
	ppe_acl_rule_id_t rule_id;			/**< Rule ID. */
	uint32_t valid_flags;				/**< Bits indicating valid rule types in the rule. */
	bool userspace_rule;                            /**< Flag indicating userspace rule */
	struct ppe_acl_rule_match_cmn cmn;		/**< Common match rule. */
	struct ppe_acl_rule_match_one rules[PPE_ACL_RULE_MATCH_TYPE_MAX];
							/**< Multiple single rule. */
	struct ppe_acl_rule_action action;		/**< ACL action object. */
	ppe_acl_rule_dev_type_t dev_type;		/**< ACL dev type. */
	union {
		uint8_t sc;				/**< ACL dev a service code. */
		char dev_name[IFNAMSIZ];		/**< ACL dev dev name. */
	} dev;

	/*
	 * Response
	 */
	ppe_acl_ret_t ret;				/**< ACL return type. */

};

/*
 * TOOD: Move flow_policer APIs to intenral .h file.
 */

/**
 * ppe_acl_rule_process_callback_t
 *	External callback to be registered with ACL for certain actions.
 */
typedef bool (*ppe_acl_rule_process_callback_t)(void *app_data, void *skb);

/**
 * ppe_acl_rule_flow_policer_create()
 *	Create ACL rule for a flow & policer combination. This is API is used
 *	when user want to enable policer per flow and provide necessary details
 *	in rule create via classifier in connection manager.
 *
 * @datatypes
 * ppe_acl_rule_flow_policer
 *
 * @param[IN] rule	ACL rule information.
 *
 * @return
 * Status of flow policer create rule operation.
 */
ppe_acl_ret_t ppe_acl_rule_flow_policer_create(struct ppe_acl_rule_flow_policer *rule);

/**
 * ppe_acl_rule_destroy()
 *	Destroy ACL rule in PPE.
 *
 * @datatypes
 * ppe_acl_rule_id_t
 *
 * @param[IN] id	ACL rule ID which needs to be deleted.
 *
 * @return
 * Status of rule destroy operation.
 */
ppe_acl_ret_t ppe_acl_rule_destroy(ppe_acl_rule_id_t id);

/**
 * ppe_acl_rule_create()
 *	Create ACL rule in PPE.
 *
 * @datatypes
 * ppe_acl_rule
 *
 * @param[IN] rule		ACL rule information.
 *
 * @return
 * Status of rule create operation.
 */
ppe_acl_ret_t ppe_acl_rule_create(struct ppe_acl_rule *rule);

/**
 * ppe_acl_rule_flush()
 *	Flush all ACL rules in PPE.
 *
 * @datatypes
 * ppe_acl_flush_type_t
 *
 * @return
 * Status of rule flush operation.
 */
ppe_acl_ret_t ppe_acl_rule_flush(ppe_acl_flush_type_t flush_type);

/**
 * ppe_acl_rule_get_acl_hw_index()
 *	Get ACL hardware index.
 *
 * @datatypes
 * ppe_acl_rule_id_t
 *
 * @param[IN] acl_id		ACL rule ID.
 *
 * @return
 * Return the hw index.
 */
uint16_t ppe_acl_rule_get_acl_hw_index(ppe_acl_rule_id_t acl_id);

/**
 * ppe_acl_rule_ref()
 *	Take a reference on an ACL rule.
 *
 * @datatypes
 * ppe_acl_rule_id_t
 *
 * @param[IN] acl_id		ACL rule ID.
 *
 * @return
 * True or False.
 */
bool ppe_acl_rule_ref(ppe_acl_rule_id_t acl_id);

/**
 * ppe_acl_rule_deref()
 *	Dereference on an ACL rule.
 *
 * @datatypes
 * ppe_acl_rule_id_t
 *
 * @param[IN] acl_id		ACL rule ID.
 *
 * @return
 * True or False.
 */
bool ppe_acl_rule_deref(ppe_acl_rule_id_t acl_id);

/**
 * ppe_acl_rule_callback_register()
 *	Register callbacks for ACL rules for certain actions to be taken.
 *
 * @datatypes
 * ppe_acl_rule_id_t
 * ppe_acl_rule_callback_t_cb
 *
 * @param[IN] acl_id		ACL rule information.
 * @param[IN] cb		callback.
 * @param[IN] app_data		app_data.
 *
 * @return
 * Status of register operation.
 */
bool ppe_acl_rule_callback_register(ppe_acl_rule_id_t acl_id, ppe_acl_rule_process_callback_t cb, void *app_data);

/**
 * ppe_acl_rule_callback_unregister()
 *	Unregister callbacks for ACL rules.
 *
 * @datatypes
 * ppe_acl_rule_id_t
 *
 * @param[IN] acl_id		ACL rule information.
 *
 * @return
 * void.
 */
void ppe_acl_rule_callback_unregister(ppe_acl_rule_id_t acl_id);

/**
 * ppe_acl_rule_prio_upd()
 * 	Updates the priority of the existing ACL rules.
 *
 * @datatypes
 * ppe_acl_rule_id_t
 *
 * @param[IN] acl_id		ACL rule information.
 * @param[IN] priority		ACL rule priority.
 *
 * @return
 * status of rule priority update.
 */
ppe_acl_ret_t ppe_acl_rule_prio_upd(ppe_acl_rule_id_t id, uint16_t priority);

#endif /* _PPE_ACL_H_ */
