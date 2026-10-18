/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_ACL_H_
#define _PPE_DRV_ACL_H_

/*
 * TODO: Separate internal and external export files
 */

#include <linux/if_ether.h>
#include <ppe_drv.h>

/*
 * General macros.
 */
#define PPE_DRV_ACL_SLICE_PER_ROW 8	/**< Number of ACL slices per row. */
#define PPE_DRV_ACL_MAX_ROW 64		/**< Number of ACL rows. */
#define PPE_DRV_ACL_MAX_ACTION 512	/**< Number of ACL actions. */
#define PPE_DRV_ACL_RULE_CHAIN_MAX 8	/**< Number of maximum ACL slices which can be chained together. */
#define PPE_DRV_ACL_HW_INDEX_MAX 1024	/**< Number of maximum ACL hw indexes. */

/*
 * Rule flags.
 */
#define PPE_DRV_ACL_VLAN_FLAG_VSI_VALID		0x00000001	/**< ACL rule VSI valid is enabled. */
#define PPE_DRV_ACL_VLAN_FLAG_VSI 		0x00000002	/**< ACL rule VSI is valid. */
#define PPE_DRV_ACL_VLAN_FLAG_STAG		0x00000004	/**< ACL rule S-tag format field. */
#define PPE_DRV_ACL_VLAN_FLAG_CTAG		0x00000008	/**< ACL rule C-tag format field. */
#define PPE_DRV_ACL_VLAN_FLAG_SVID		0x00000010	/**< ACL rule S-VID format field. */
#define PPE_DRV_ACL_VLAN_FLAG_CVID		0x00000020	/**< ACL rule C-VID format field. */
#define PPE_DRV_ACL_VLAN_FLAG_SPCP		0x00000040	/**< ACL rule S-PCP match value. */
#define PPE_DRV_ACL_VLAN_FLAG_CPCP		0x00000080	/**< ACL rule C-PCP match value. */
#define PPE_DRV_ACL_VLAN_FLAG_SDEI		0x00000100	/**< ACL rule S-DEI match field. */
#define PPE_DRV_ACL_VLAN_FLAG_CDEI		0x00000200	/**< ACL rule C-DEI match field. */

#define PPE_DRV_ACL_EXT_VLAN_FLAG_CTPID		0x00000001	/**< ACL rule CTPID match field. */
#define PPE_DRV_ACL_EXT_VLAN_FLAG_STPID		0x00000002	/**< ACL rule STPID match field. */
#define PPE_DRV_ACL_EXT_VLAN_FLAG_DHCP		0x00000004	/**< ACL rule DHCP match field. */
#define PPE_DRV_ACL_EXT_VLAN_FLAG_MC		0x00000008	/**< ACL rule MC match field. */
#define PPE_DRV_ACL_EXT_VLAN_FLAG_L2		0x00000010	/**< ACL rule L2 match field. */

#define PPE_DRV_ACL_L2_MISC_FLAG_PPPOE 		0x00000001	/**< ACL rule PPPOE session ID field. */
#define PPE_DRV_ACL_L2_MISC_FLAG_L2PROTO	0x00000002	/**< ACL rule L2 protocol field. */
#define PPE_DRV_ACL_L2_FLAG_SVID		0x00000004	/**< ACL rule L2 S-VID field. */

#define PPE_DRV_ACL_DST_IPV4_FLAG_PKTTYPE	0x00000001	/**< ACL rule destination IPv4 packet type. */
#define PPE_DRV_ACL_DST_IPV4_FLAG_L3FRAG	0x00000002	/**< ACL rule destination IPv4 L3 fragmentation flag. */
#define PPE_DRV_ACL_DST_IPV4_FLAG_L4PORT	0x00000004	/**< ACL rule destination port with IPv4 packet. */
#define PPE_DRV_ACL_DST_IPV4_FLAG_IP		0x00000008	/**< ACL rule destination IPv4 field. */

#define PPE_DRV_ACL_SRC_IPV4_FLAG_PKTTYPE	0x00000001	/**< ACL rule source IPv4 packet type. */
#define PPE_DRV_ACL_SRC_IPV4_FLAG_L3FRAG	0x00000002	/**< ACL rule source IPv4 L3 fragmentation flag. */
#define PPE_DRV_ACL_SRC_IPV4_FLAG_L4PORT	0x00000004	/**< ACL rule source port with IPv4 packet. */
#define PPE_DRV_ACL_SRC_IPV4_FLAG_IP		0x00000008	/**< ACL rule source IPv4 field. */

#define PPE_DRV_ACL_DST_IPV6_FLAG_PKTTYPE	0x00000001	/**< ACL rule destination IPv6 packet type. */
#define PPE_DRV_ACL_DST_IPV6_FLAG_L3FRAG	0x00000002	/**< ACL rule destination IPv6 L3 fragmentation flag. */
#define PPE_DRV_ACL_DST_IPV6_FLAG_L4PORT	0x00000004	/**< ACL rule destination port with IPv6 packet. */
#define PPE_DRV_ACL_DST_IPV6_FLAG_IP		0x00000008	/**< ACL rule destination IPv6 field. */

#define PPE_DRV_ACL_SRC_IPV6_FLAG_PKTTYPE	0x00000001	/**< ACL rule source IPv6 packet type. */
#define PPE_DRV_ACL_SRC_IPV6_FLAG_L3FRAG	0x00000002	/**< ACL rule source IPv6 L3 fragmentation flag. */
#define PPE_DRV_ACL_SRC_IPV6_FLAG_L4PORT	0x00000004	/**< ACL rule source port with IPv6 packet. */
#define PPE_DRV_ACL_SRC_IPV6_FLAG_IP		0x00000008	/**< ACL rule source IPv6 field. */

#define PPE_DRV_ACL_IP_MISC_FLAG_L3FRAG 	0x00000001	/**< ACL rule with L3 fragmentation flag. */
#define PPE_DRV_ACL_IP_MISC_FLAG_OTHEXT_HDR	0x00000002	/**< ACL rule with other extended header. */
#define PPE_DRV_ACL_IP_MISC_FLAG_FRAG_HDR	0x00000004	/**< ACL rule with fragment header. */
#define PPE_DRV_ACL_IP_MISC_FLAG_MOB_HDR	0x00000008	/**< ACL rule with mobility header. */
#define PPE_DRV_ACL_IP_MISC_FLAG_ESP_HDR	0x00000010	/**< ACL rule with ESP header. */
#define PPE_DRV_ACL_IP_MISC_FLAG_AH_HDR		0x00000020	/**< ACL rule with AH header. */
#define PPE_DRV_ACL_IP_MISC_FLAG_TTLHOP		0x00000040	/**< ACL rule with TTL/hop-limit match. */
#define PPE_DRV_ACL_IP_MISC_FLAG_L3STATE	0x00000080	/**< ACL rule with L3 state flag. */
#define PPE_DRV_ACL_IP_MISC_FLAG_TCPFLAG	0x00000100	/**< ACL rule with TCP flags. */
#define PPE_DRV_ACL_IP_MISC_FLAG_L31STFRAG	0x00000200	/**< ACL rule with L3 first fragment. */
#define PPE_DRV_ACL_IP_MISC_FLAG_TOS_TC		0x00000400	/**< ACL rule with TOS or TC value. */
#define PPE_DRV_ACL_IP_MISC_FLAG_L4PROTO	0x00000800	/**< ACL rule with L4 protocol number. */
#define PPE_DRV_ACL_IP_MISC_FLAG_L3LEN		0x00001000	/**< ACL rule with specific L3 length. */

/*
 * ACL action flags.
 */
#define PPE_DRV_ACL_ACTION_FLAG_CC 		0x00000001	/**< ACL action for CPU code change. */
#define PPE_DRV_ACL_ACTION_FLAG_SYN_TOGGLE	0x00000002	/**< ACL action for SYN_TOGGLE bit change. */
#define PPE_DRV_ACL_ACTION_FLAG_SC		0x00000004	/**< ACL action for service code change. */
#define PPE_DRV_ACL_ACTION_FLAG_QID		0x00000008	/**< ACL action for queue ID change. */
#define PPE_DRV_ACL_ACTION_FLAG_ENQUEUE_PRI	0x00000010	/**< ACL action for enqueue priority change. */
#define PPE_DRV_ACL_ACTION_FLAG_CTAG_DEI	0x00000020	/**< ACL action for C-DEI change. */
#define PPE_DRV_ACL_ACTION_FLAG_CTAG_PCP	0x00000040	/**< ACL action for C-PCP change. */
#define PPE_DRV_ACL_ACTION_FLAG_STAG_DEI	0x00000080	/**< ACL action for S-DEI change. */
#define PPE_DRV_ACL_ACTION_FLAG_STAG_PCP	0x00000100	/**< ACL action for C-PCP change. */
#define PPE_DRV_ACL_ACTION_FLAG_TOS_TC		0x00000200	/**< ACL action for TOS or TC change. */
#define PPE_DRV_ACL_ACTION_FLAG_CVID		0x00000400	/**< ACL action for C-VID change. */
#define PPE_DRV_ACL_ACTION_FLAG_SVID		0x00000800	/**< ACL action for S-VID change. */
#define PPE_DRV_ACL_ACTION_FLAG_DST_INFO	0x00001000	/**< ACL action for destination change. */
#define PPE_DRV_ACL_ACTION_FLAG_BYPASS_BITMAP	0x00002000	/**< ACL action with bypass bitmap. */
#define PPE_DRV_ACL_ACTION_FLAG_POLICER_INDEX	0x00004000	/**< ACL action with policer. */
#define PPE_DRV_ACL_ACTION_FLAG_FWD_CMD		0x00008000	/**< ACL action with forward command - fwd/drop/redirect/copy. */
#define PPE_DRV_ACL_ACTION_FLAG_INT_DP		0x00010000	/**< ACL action for internal drop precedence change. */
#define PPE_DRV_ACL_ACTION_FLAG_CTAG_VID_CMD	0x00020000	/**< ACL ctag VID command. */
#define PPE_DRV_ACL_ACTION_FLAG_STAG_VID_CMD	0x00040000	/**< ACL stag VID command. */
#define PPE_DRV_ACL_ACTION_FLAG_MIRROR_EN	0x00080000	/**< ACL action for mirroring. */
#define PPE_DRV_ACL_ACTION_FLAG_METADATA_EN	0x00100000	/**< ACL action with Metadata enable. */

#define PPE_DRV_ACL_ACTION_FLAG_CTAG_PID	0x00200000	/**< ACL action with C-PID change. */
#define PPE_DRV_ACL_ACTION_FLAG_STAG_PID	0x00400000	/**< ACL action with S-PID change. */
#define PPE_DRV_ACL_ACTION_FLAG_COUNTER_EN	0x00800000	/**< ACL action with COUNTER enable. */
#define PPE_DRV_ACL_ACTION_FLAG_TAGS_TO_RMV	0x01000000	/**< ACL action with tags. */
#define PPE_DRV_ACL_ACTION_FLAG_SRC_INFO_EN	0x02000000	/**< ACL action with src info. */
#define PPE_DRV_ACL_ACTION_FLAG_DSCP_PBIT_IDX	0x04000000	/**< ACL action with DSCP and pbit mapping index. */

#define PPE_DRV_ACL_ACTION_FLAG_CTAG_PID_CMD	0x00000001	/**< ACL ctag PID command. */
#define PPE_DRV_ACL_ACTION_FLAG_STAG_PID_CMD	0x00000002	/**< ACL stag PID command. */
#define PPE_DRV_ACL_ACTION_FLAG_CTAG_DEI_CMD	0x00000004	/**< ACL ctag DEI command. */
#define PPE_DRV_ACL_ACTION_FLAG_STAG_DEI_CMD	0x00000008	/**< ACL stag DEI command. */
#define PPE_DRV_ACL_ACTION_FLAG_CTAG_PCP_CMD	0x00000010	/**< ACL ctag PCP command. */
#define PPE_DRV_ACL_ACTION_FLAG_STAG_PCP_CMD	0x00000020	/**< ACL stag PCP command. */

/*
 * ppe_drv_udf_entry
 *	ACL UDF offsets.
 */
enum ppe_drv_udf_entry {
	PPE_DRV_ACL_UDF_0_OFFSET = 0,	/**< ACL UDF0 offset */
	PPE_DRV_ACL_UDF_1_OFFSET,	/**< ACL UDF1 offset */
	PPE_DRV_ACL_UDF_2_OFFSET,	/**< ACL UDF2 offset */
	PPE_DRV_ACL_UDF_3_OFFSET,	/**< ACL UDF3 offset */
	PPE_DRV_ACL_UDF_OFFSET_MAX,	/**< ACL UDF MAX offset */
};

struct ppe_drv_acl_ctx;

/*
 * ppe_drv_acl_slice_type
 *	ACL slice types.
 */
enum ppe_drv_acl_slice_type {
	PPE_DRV_ACL_SLICE_TYPE_DST_MAC,		/**< ACL destination MAC slice type. */
	PPE_DRV_ACL_SLICE_TYPE_SRC_MAC,		/**< ACL source MAC slice type. */
	PPE_DRV_ACL_SLICE_TYPE_VLAN,		/**< ACL VLAN slice type. */
	PPE_DRV_ACL_SLICE_TYPE_L2_MISC,		/**< ACL L2 miscellaneous slice type. */
	PPE_DRV_ACL_SLICE_TYPE_DST_IPV4,	/**< ACL destination IPv4 slice type. */
	PPE_DRV_ACL_SLICE_TYPE_SRC_IPV4,	/**< ACL source IPv4 slice type. */
	PPE_DRV_ACL_SLICE_TYPE_DST_IPV6_0,	/**< ACL destination IPv6 0 slice type. */
	PPE_DRV_ACL_SLICE_TYPE_DST_IPV6_1,	/**< ACL destination IPv6 1 slice type. */
	PPE_DRV_ACL_SLICE_TYPE_DST_IPV6_2,	/**< ACL destination IPv6 2 slice type. */
	PPE_DRV_ACL_SLICE_TYPE_SRC_IPV6_0,	/**< ACL source IPv6 0 slice type. */
	PPE_DRV_ACL_SLICE_TYPE_SRC_IPV6_1,	/**< ACL source IPv6 1 slice type. */
	PPE_DRV_ACL_SLICE_TYPE_SRC_IPV6_2,	/**< ACL source IPv6 2 slice type. */
	PPE_DRV_ACL_SLICE_TYPE_IP_MISC,		/**< ACL IP miscellaneous slice type. */
	PPE_DRV_ACL_SLICE_TYPE_UDF,		/**< ACL UDF with offset slice type */
	PPE_DRV_ACL_SLICE_TYPE_UDF_WITH_IP,	/**< ACL UDF with L3 IP (IPv4/IPv6) slice type */
	PPE_DRV_ACL_SLICE_TYPE_EXT_VLAN,	/**< ACL Extended VLAN slice type. */
	PPE_DRV_ACL_SLICE_TYPE_MAX,		/**< ACL slice type max. */
};

/*
 * ppe_drv_acl_dev_type
 *	ACL dev types.
 */
typedef enum ppe_drv_acl_dev_type {
	PPE_DRV_ACL_DEV_TYPE_PORT_BITMAP,		/**< ACL dev type bitmap. */
	PPE_DRV_ACL_DEV_TYPE_PORT_NUM,			/**< ACL dev type port number. */
	PPE_DRV_ACL_DEV_TYPE_SC,			/**< ACL dev type service code. */
	PPE_DRV_ACL_DEV_TYPE_DST_L3_IF,			/**< ACL dev type destination L3 interface. */
	PPE_DRV_ACL_DEV_TYPE_DEST_L3_PORT,		/**< ACL dev type dst port/virtual port from L3. */
	PPE_DRV_ACL_DEV_TYPE_DEST_L2_PORT,		/**< ACL dev type dst port/Virtual port from L2. */
} ppe_drv_acl_dev_type_t;

/*
 * ppe_drv_acl_dst_type
 *	ACL destination types.
 */
typedef enum ppe_drv_acl_dst_type {
	PPE_DRV_ACL_DST_TYPE_PORT_BITMAP,	/**< ACL destination type port bitmap. */
	PPE_DRV_ACL_DST_TYPE_NEXTHOP,		/**< ACL destination type nexthop index. */
	PPE_DRV_ACL_DST_TYPE_PORT_ID,		/**< ACL destination type port number. */
} ppe_drv_acl_dst_type_t;

/*
 * ppe_drv_acl_vtag_fmt
 *	VLAN tag format.
 */
typedef enum ppe_drv_acl_vtag_fmt {
	PPE_DRV_ACL_VTAG_FMT_UNTAG,		/**< ACL VLAN untag format. */
	PPE_DRV_ACL_VTAG_FMT_TAG,		/**< ACL VLAN tag format. */
	PPE_DRV_ACL_VTAG_FMT_PRI_TAG,		/**< ACL VLAN priority tag format. */
	PPE_DRV_ACL_VTAG_FMT_UNTAG_PRI_TAG,	/**< ACL VLAN untagged and pri tagged format. */
	PPE_DRV_ACL_VTAG_FMT_UNTAG_TAG,		/**< ACL VLAN untag and tag format. */
	PPE_DRV_ACL_VTAG_FMT_TAG_PRITAG,	/**< ACL VLAN tag and pritag format. */
	PPE_DRV_ACL_VTAG_FMT_ALL,		/**< ACL VLAN untag, tag and pri tag format. */
} ppe_drv_acl_vtag_fmt_t;

/*
 * ppe_drv_acl_pkt_type
 *	ACL packet types.
 */
typedef enum ppe_drv_acl_pkt_type {
	PPE_DRV_ACL_PKT_TYPE_TCP = 0,		/**< ACL TCP packet type. */
	PPE_DRV_ACL_PKT_TYPE_UDP = 1,		/**< ACL UDP packet type. */
	PPE_DRV_ACL_PKT_TYPE_UDPLITE = 3,	/**< ACL UDP-Lite packet type. */
	PPE_DRV_ACL_PKT_TYPE_ARP = 5,		/**< ACL ARP packet type. */
	PPE_DRV_ACL_PKT_TYPE_ICMP = 7		/**< ACL ICMP packet type. */
} ppe_drv_acl_pkt_type_t;

/*
 * ppe_drv_acl_ip_ver
 *	ACL rule IP version.
 */
typedef enum ppe_drv_acl_ip_ver {
	PPE_DRV_ACL_IPV4,			/**< ACL IPv4 packet. */
	PPE_DRV_ACL_IPV6			/**< ACL IPv6 packet. */
} ppe_drv_acl_ip_ver_t;

/*
 * ppe_drv_acl_fwd_cmd
 *	ACL action command
 */
typedef enum ppe_drv_acl_fwd_cmd {
	PPE_DRV_ACL_FWD_CMD_FWD,		/**< ACL forward command - forward. */
	PPE_DRV_ACL_FWD_CMD_DROP,		/**< ACL forward command - drop. */
	PPE_DRV_ACL_FWD_CMD_COPY,		/**< ACL forward command - copy to CPU. */
	PPE_DRV_ACL_FWD_CMD_REDIR		/**< ACL forward command - redirect to CPU. */
} ppe_drv_acl_fwd_cmd_t;

/*
 * ppe_drv_acl_dp
 *	ACL drop precedence
 */
typedef enum ppe_drv_acl_dp {
	PPE_DRV_ACL_DP_0,			/**< ACL drop precedence 0. */
	PPE_DRV_ACL_DP_1,			/**< ACL drop precedence 1. */
	PPE_DRV_ACL_DP_2,			/**< ACL drop precedence 2. */
	PPE_DRV_ACL_DP_3,			/**< ACL drop precedence 3. */
} ppe_drv_acl_dp_t;

/*
 * ppe_drv_acl_ipo
 *	ACL IPO type
 */
typedef enum ppe_drv_acl_ipo {
	PPE_DRV_ACL_IPO,			/**< ACL IPO rule. */
	PPE_DRV_ACL_PREIPO,			/**< ACL pre-IPO rule. */
} ppe_drv_acl_ipo_t;

/*
 * ppe_drv_acl_dhcp
 * 	ACL DHCP type
 */
typedef enum ppe_drv_acl_dhcp_type {
	PPE_DRV_ACL_DHCP_TYPE_NON=1,		/**< non dhcp rule. */
	PPE_DRV_ACL_DHCP_TYPE_V4,		/**< V4 dhcp rule. */
	PPE_DRV_ACL_DHCP_TYPE_V4_NON,		/**< V4 and non dhcp rule. */
	PPE_DRV_ACL_DHCP_TYPE_V6,		/**< V6 dhcp rule. */
	PPE_DRV_ACL_DHCP_TYPE_V6_NON,		/**< V6 and non dhcp rules. */
	PPE_DRV_ACL_DHCP_TYPE_V6_V4,		/**< V6 and V4 dhcp rule. */
	PPE_DRV_ACL_DHCP_TYPE_ALL,		/**< all dhcp rule. */
} ppe_drv_acl_dhcp_type_t;

/*
 * ppe_drv_acl_mc
 * 	ACL MC type
 */
typedef enum ppe_drv_acl_mc_type {
	PPE_DRV_ACL_MC_TYPE_NON=1,		/**< no MC rule. */
	PPE_DRV_ACL_MC_TYPE_IP,			/**< IP MC rule. */
	PPE_DRV_ACL_MC_TYPE_IP_NON,		/**< IP MC and No MC rule. */
	PPE_DRV_ACL_MC_TYPE_NONIP,		/**< NONIP MC rule. */
	PPE_DRV_ACL_MC_TYPE_NONIP_NON,		/**< NONIP MC rule and NO MC rule. */
	PPE_DRV_ACL_MC_TYPE_IP_NON_IP,		/**< IP and NON IP MC rule. */
	PPE_DRV_ACL_MC_TYPE_ALL,		/**< all MC rule. */
} ppe_drv_acl_mc_type_t;

/*
 * ppe_drv_acl_pcp_cmd
 * 	VLAN tag pcp command.
 */
typedef enum ppe_drv_acl_pcp_cmd {
	PPE_DRV_ACL_PCP_UNCHANGED,			/**< keep PCP unchanged. */
	PPE_DRV_ACL_PCP_REPLACE,			/**< replace PCP with new value. */
	PPE_DRV_ACL_PCP_CPY_FRM_ORIG_SPCP,		/**< copy PCP from original S-PCP. */
	PPE_DRV_ACL_PCP_CPY_FRM_ORIG_CPCP,		/**< copy PCP from original C-PCP. */
	PPE_DRV_ACL_PCP_DSCP_TO_PBIT,			/**< map DSCP to PCP (p-bit). */
	PPE_DRV_ACL_PCP_ADD_TAG_REPLACE_PCP,		/**< add VLAN tag and replace PCP. */
	PPE_DRV_ACL_PCP_ADD_TAG_CPY_FRM_ORIG_SPCP,	/**< add VLAN tag and copy PCP from original S-PCP. */
	PPE_DRV_ACL_PCP_ADD_TAG_CPY_FRM_ORIG_CPCP,	/**< add VLAN tag and copy PCP from original C-PCP. */
	PPE_DRV_ACL_PCP_ADD_TAG_DSCP_TO_PBIT,		/**< add VLAN tag and map DSCP to PCP. */
} ppe_drv_acl_pcp_cmd_t;

/*
 * ppe_drv_acl_vid_cmd
 *	VLAN tag vid format.
 */
typedef enum ppe_drv_acl_vid_cmd {
	PPE_DRV_ACL_VID_UNCHANGED,		/**< keep VID unchanged. */
	PPE_DRV_ACL_VID_REPLACE,		/**< replace VID with new value. */
	PPE_DRV_ACL_VID_CPY_FRM_ORIG_SVID,	/**< copy VID from original S-VID. */
	PPE_DRV_ACL_VID_CPY_FRM_ORIG_CVID,	/**< copy VID from original C-VID. */
} ppe_drv_acl_vid_cmd_t;

/*
 * ppe_drv_acl_pid_cmd
 *	VLAN tag pid format.
 */
typedef enum ppe_drv_acl_pid_cmd {
	PPE_DRV_ACL_PID_UNCHANGED,		/**< keep PID unchanged. */
	PPE_DRV_ACL_PID_REPLACE,		/**< replace PID with new value. */
	PPE_DRV_ACL_PID_CPY_FRM_ORIG_STPID,	/**< copy PID from original S-TPID. */
	PPE_DRV_ACL_PID_CPY_FRM_ORIG_CTPID,	/**< copy PID from original C-TPID. */
} ppe_drv_acl_pid_cmd_t;

/*
 * ppe_drv_acl_dei_cmd
 * 	VLAN tag dei format.
 */
typedef enum ppe_drv_acl_dei_cmd {
	PPE_DRV_ACL_DEI_UNCHANGED,		/**< keep DEI unchanged. */
	PPE_DRV_ACL_DEI_REPLACE,		/**< replace DEI with new value. */
	PPE_DRV_ACL_DEI_CPY_FRM_ORIG_SVID,	/**< copy DEI from original S-VID. */
	PPE_DRV_ACL_DEI_CPY_FRM_ORIG_CVID,	/**< copy DEI from original C-VID. */
} ppe_drv_acl_dei_cmd_t;

/*
 * ppe_drv_acl_counter_mode
 * 	counter mode for VLAN or PM.
 */
typedef enum ppe_drv_acl_counter_mode {
	PPE_DRV_ACL_VLAN_DEV,			/**< counter mode for VLAN device. */
	PPE_DRV_ACL_PON_PM,			/**< counter mode for PON performance monitoring. */
} ppe_drv_acl_counter_mode_t;

/*
 * ppe_drv_acl_src_info_type
 * 	src is virtual port or L3_IF port for tunnel.
 */
typedef enum ppe_drv_acl_src_info_type {
	PPE_DRV_ACL_SRC_INFO_TYPE_VP,		/**< source is a virtual port. */
	PPE_DRV_ACL_SRC_INFO_TYPE_L3_IF,	/**< source is an L3 interface port (for tunnel). */
} ppe_drv_acl_src_info_type_t;

/*
 * ppe_drv_acl_flow_bind
 *	Structure for flow and ACL binding.
 */
struct ppe_drv_acl_flow_bind {
	/*
	 * Input: ACL rule ID.
	 */
	uint16_t id;				/**< ACL rule ID. */

	/*
	 * Output: Associated service code.
	 */
	uint8_t sc;				/**< returned service code. */
};

/*
 * ppe_drv_acl_rule_match_cmn
 *	Rule info common to all the rules.
 */
struct ppe_drv_acl_rule_match_cmn {
	bool post_routing_en;		/**< Post routing enable flag. */
	uint16_t pri;			/**< ACL rule priority. */
	bool is_ip;			/**< IP packet type flag. */
	uint8_t qos_res_pre;		/**< QOS resolution precedence. */
	uint8_t res_chain;		/**< Chain resolution. */
};

/*
 * ppe_drv_acl_mac
 *	MAC based ACL rule.
 */
struct ppe_drv_acl_mac {
	/*
	 * Rule
	 */
	uint8_t mac[ETH_ALEN];		/**< MAC address for match */

	uint8_t inverse_en;		/**< MAC address match with inverse logic. */
	uint8_t fake_mac_hdr;		/**< Fake mac header flag. */
	uint8_t snap_flag;		/**< SNAP flag. */
	uint8_t ethernet_flag;		/**< Ethernet flag. */
	uint8_t ip_ver;			/**< IP version. */
	uint8_t ip_nonip;		/**< IP no-IP flag. */

	/*
	 * Mask
	 */
	uint8_t fake_mac_hdr_mask;	/**< Fake mac header flag mask. */
	uint8_t snap_flag_mask;		/**< SNAP flag mask. */
	uint8_t ethernet_flag_mask;	/**< Ethernet flag mask. */
	uint8_t ip_ver_mask;		/**< IP version mask. */
	uint8_t ip_nonip_mask;		/**< IP no-IP flag mask. */
	uint8_t mac_mask[ETH_ALEN];	/**< MAC address mask. */
};

/*
 * ppe_drv_acl_vlan
 *	VLAN based ACL rule
 */
struct ppe_drv_acl_vlan {
	uint8_t vsi_valid;			/**< Match based on valid VSI. */
	uint8_t vsi;				/**< VSI value for match. */
	ppe_drv_acl_vtag_fmt_t stag_fmt;	/**< S-tag format. */
	ppe_drv_acl_vtag_fmt_t ctag_fmt;	/**< C-tag format. */
	uint8_t sdei_en;			/**< S-dei based VLAN match. */
	uint8_t spcp;				/**< SPCP based VLAN match. */
	uint16_t svid;				/**< SVID VLAN match. */
	uint8_t cdei_en;			/**< C-dei based VLAN match. */
	uint8_t cpcp;				/**< CPCP based VLAN match. */
	uint16_t cvid_min;			/**< CVID VLAN match. */

	uint8_t inverse_en;			/**< VLAN field match with inverse logic. */
	uint8_t range_en;			/**< VID range match. */
	uint8_t fake_mac_hdr;			/**< Fake mac header flag. */
	uint8_t snap_flag;			/**< SNAP flag. */
	uint8_t ethernet_flag;			/**< Ethernet flag. */
	uint8_t ip_ver;				/**< IP version. */
	uint8_t ip_nonip;			/**< IP no-IP flag. */

	/*
	 * Mask
	 */
	uint8_t fake_mac_hdr_mask;		/**< Fake mac header flag mask. */
	uint8_t snap_flag_mask;			/**< SNAP flag mask. */
	uint8_t ethernet_flag_mask;		/**< Ethernet flag mask. */
	uint8_t ip_ver_mask;			/**< IP version mask. */
	uint8_t ip_nonip_mask;			/**< IP no-IP flag mask. */
	uint8_t vsi_valid_mask;			/**< VSI valid mask. */
	uint8_t vsi_mask;			/**< VSI value mask. */
	uint8_t sdei_en_mask;			/**< S-dei mask. */
	uint8_t spcp_mask;			/**< SPCP mask. */
	uint16_t svid_mask;			/**< SVID mask. */
	uint8_t cdei_en_mask;			/**< C-dei mask. */
	uint8_t cpcp_mask;			/**< CPCP mask. */
	uint16_t cvid_mask_max;			/**< CVID mask or max for range. */
	uint8_t ctag_fmt_mask;			/**< ctag format mask. */
	uint8_t stag_fmt_mask;			/**< stag format mask. */
};

/*
 * ppe_drv_acl_vlan
 *      Extended VLAN based ACL rule
 */
struct ppe_drv_acl_ext_vlan {
	uint16_t ctpid_val;			/**< CTPID based VLAN match. */
	uint16_t stpid_val;			/**< STPID based VLAN match. */
	ppe_drv_acl_dhcp_type_t dhcp_type;	/**< DHCP based VLAN match. */
	ppe_drv_acl_mc_type_t mc_type;		/**< traffic transmission based VLAN match. */
	uint8_t l2_type;			/**< L2 proto type. */
	uint8_t inverse_en;                     /**< EXT VLAN field match with inverse logic. */

	/*
	 * Mask
	 */
	uint8_t ctpid_mask;			/**< ctpid index mask. */
	uint8_t stpid_mask;			/**< stpid index mask. */
	uint8_t dhcp_mask;			/**< dhcp mask. */
	uint8_t mc_mask;			/**< multicast mask. */
	uint8_t l2_mask;			/**< l2 proto mask. */
};

/*
 * ppe_drv_acl_l2_misc
 *
 */
struct ppe_drv_acl_l2_misc {
	uint16_t pppoe_session_id;		/**< PPPOE session ID match. */
	uint16_t l2_proto;			/**< Ether type based match. */
	uint16_t svid_min;			/**< SVID range match. */

	uint8_t inverse_en;			/**< L2 field match with inverse logic. */
	uint8_t range_en;			/**< SVID range match. */
	uint8_t fake_mac_hdr;			/**< Fake mac header flag. */
	uint8_t snap_flag;			/**< SNAP flag. */
	uint8_t ethernet_flag;			/**< Ethernet flag. */
	uint8_t ip_ver;				/**< IP version. */
	uint8_t ip_nonip;			/**< IP no-IP flag. */

	/*
	 * Mask
	 */
	uint8_t fake_mac_hdr_mask;		/**< Fake mac header flag mask. */
	uint8_t snap_flag_mask;			/**< SNAP flag mask. */
	uint8_t ethernet_flag_mask;		/**< Ethernet flag mask. */
	uint8_t ip_ver_mask;			/**< IP version mask. */
	uint8_t ip_nonip_mask;			/**< IP no-IP flag mask. */
	uint16_t pppoe_session_id_mask;		/**< PPPOE session ID mask. */
	uint16_t l2_proto_mask;			/**< Ether type mask. */
	uint16_t svid_mask_max;			/**< SVID max value for range. */
};

/*
 * ppe_drv_acl_ipv4
 *
 */
struct ppe_drv_acl_ipv4 {
	ppe_drv_acl_pkt_type_t pkt_type;	/**< Packet type based match. */
	uint8_t l3_fragment_flag;		/**< L3 fragment flag match. */
	uint16_t l4_port_min;			/**< L4 port based match. */
	uint32_t ip;				/**< IPv4 address match. */

	uint8_t inverse_en;			/**< IPv4 match with inverse logic. */
	uint8_t range_en;			/**< L4 port range match. */
	uint8_t ip_nonip;			/**< IP no-IP flag. */

	/*
	 * Mask
	 */
	uint8_t ip_nonip_mask;			/**< IP no-IP flag mask. */
	uint8_t pkt_type_mask;			/**< packet type mask. */
	uint8_t l3_fragment_flag_mask;		/**< L3 fragment flag mask. */
	uint32_t ip_mask;			/**< IPv4 address mask. */
	uint16_t l4_port_mask_max;		/**< L4 port mask or max value for range. */
};

/*
 * ppe_drv_acl_ipv6
 *
 */
struct ppe_drv_acl_ipv6 {
	ppe_drv_acl_pkt_type_t pkt_type;	/**< Packet type based match. */
	uint8_t l3_fragment_flag;		/**< L3 fragment flag match. */
	uint32_t ip_l32;			/**< Lower 32 bit IP address. */
	union {
		uint16_t ip_u16;		/**< Upper 16 bit IP address. */
		uint16_t l4_port_min;		/**< L4 port based match. */
	};

	uint8_t inverse_en;			/**< IPv6 match with inverse logic. */
	uint8_t range_en;			/**< L4 port range match. */

	/*
	 * Mask
	 */
	uint8_t pkt_type_mask;			/**< packet type mask. */
	uint8_t l3_fragment_flag_mask;		/**< L3 fragment flag mask. */
	uint32_t ip_l32_mask;			/**< Lower 32 bit IP address mask. */
	union {
		uint16_t ip_u16_mask;		/**< Upper 16 bit IP address mask. */
		uint16_t l4_port_mask_max;	/**< L4 port mask or max for range. */
	};
};

/*
 * ppe_drv_acl_ip_misc
 *
 */
struct ppe_drv_acl_ip_misc {
	ppe_drv_acl_ip_ver_t ip_ver;		/**< IP version based match. */
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
 * ppe_drv_acl_rule_udf_op
 *	ACL rule UDF match operation.
 * NOTE: Duplicate of ppe_acl_rule_udf_op in ppe_acl.h; ensure any changes are mirrored in both files.
 */
enum ppe_drv_acl_rule_udf_op {
        PPE_DRV_ACL_UDF_OP_INVAL = 0,	/**< Invalid UDF operation */
        PPE_DRV_ACL_UDF_OP_MASK,		/**< Perform MASK-based matching */
        PPE_DRV_ACL_UDF_OP_RANGE,		/**< Perform RANGE-based matching */
        PPE_DRV_ACL_RULE_OP_LE,		/**< Perform Less-Than-Or-Equal comparison */
        PPE_DRV_ACL_RULE_OP_GE,		/**< Perform Greater-Than-Or-Equal comparison */
        PPE_DRV_ACL_UDF_OP_NE,		/**< Perform NOT-EQUAL matching */
};

/**
 * ppe_drv_acl_udf_rule_base_type
 *	The base layer from which the UDF offset is calculated in an ACL rule.
 *	The offset can be relative to Layer 2, Layer 3, or Layer 4.
 * NOTE: Duplicate of ppe_acl_rule_udf_base_type in ppe_acl.h; ensure any changes are mirrored in both files.
 */
enum ppe_drv_acl_udf_rule_base_type {
	PPE_DRV_ACL_UDF_OFFSET_BASE_L2,	/**< Offset is calculated from the start of Layer 2 */
	PPE_DRV_ACL_UDF_OFFSET_BASE_L3,	/**< Offset is calculated from the start of Layer 3 */
	PPE_DRV_ACL_UDF_OFFSET_BASE_L4,	/**< Offset is calculated from the start of Layer 4 */
};

/**
 * ppe_drv_acl_udf_rule_offset_ref
 *	ACL rule offset protocol type.
 * NOTE: Duplicate of ppe_acl_rule_udf_proto in ppe_acl.h; ensure any changes are mirrored in both files.
 */
enum ppe_drv_acl_udf_proto {
	PPE_DRV_ACL_UDF_NON_IP,	/**< UDF offset for non-ip protocol type */
	PPE_DRV_ACL_UDF_IPV4,	/**< UDF offset for IPv4 protocol type */
	PPE_DRV_ACL_UDF_IPV6,	/**< UDF offset for IPv6 protocol type */
};

/*
 * ppe_drv_acl_udf_offset
 *	UDF base information shared between rule offset and data structures
 */
struct ppe_drv_acl_udf_offset {
	enum ppe_drv_acl_udf_rule_base_type offset_base;	/**< Base layer (L2, L3, or L4) from which the offset is calculated */
	enum ppe_drv_acl_udf_proto proto;			/**< UDF offset Protocol type (non-IP, IPv4, or IPv6) */
	uint16_t offset;					/**< UDF offset for the UDF in bytes */
};

/*
 * ppe_drv_acl_rule_udf_offset
 * 	PPE ACL rule UDF offsets
 */
struct ppe_drv_acl_rule_udf_offset {
	struct ppe_drv_acl_udf_offset base_info;	/**< UDF base information */
	uint32_t udf_index;				/**< UDF offset index */
	bool in_use;					/**< Indicates whether this offset entry is currently in use */
	struct kref ref_cnt;				/**< Reference count of the UDF offset */
};

/*
 * ppe_drv_acl_rule_udf_offset_list
 * 	ACL UDF offset global list.
 */
struct ppe_drv_acl_rule_udf_offset_list {
	struct ppe_drv_acl_rule_udf_offset non_ip[PPE_DRV_ACL_UDF_OFFSET_MAX];	/**< UDF offset list for NON-IPV. */
	struct ppe_drv_acl_rule_udf_offset ipv4[PPE_DRV_ACL_UDF_OFFSET_MAX];	/**< UDF offset list for IPv4 */
	struct ppe_drv_acl_rule_udf_offset ipv6[PPE_DRV_ACL_UDF_OFFSET_MAX];	/**< UDF offset list for IPv4 */
	spinlock_t lock;	/**< lock */
};

/*
 * ppe_drv_acl_udf_data
 * 	ACL UDF offset data.
 */
struct ppe_drv_acl_udf_data {
	struct ppe_drv_acl_rule_udf_offset *udf_offset_entry; /**< Offset entry for the UDF */
	struct ppe_drv_acl_udf_offset base_info;	/**< UDF base information */
	enum ppe_drv_acl_rule_udf_op udf_op;		/**< UDF operation */
	uint16_t udf_min;				/**< UDF value for match. */
	uint16_t udf_mask;				/**< UDF mask. */
	bool udf_valid;					/**< UDF valid for match. */
};

/*
 * ppe_drv_acl_udf
 *	UDF offset data for all udfs
 */
struct ppe_drv_acl_udf {
	struct ppe_drv_acl_udf_data udf0;	/**< First UDF data */
	struct ppe_drv_acl_udf_data udf1;	/**< Second UDF data */
	struct ppe_drv_acl_udf_data udf2;	/**< Third UDF data */
	struct ppe_drv_acl_udf_data udf3;	/**< Forth UDF data */
};

/*
 * ppe_drv_acl_udf_with_ip
 *	UDF with L3 IP version offset data for all udfs.
 *	Sets FAL rule type to FAL_ACL_RULE_IP4 or FAL_ACL_RULE_IP6
 *	based on is_ipv6 flag, instead of FAL_ACL_RULE_UDF.
 *	Optionally matches IP miscellaneous fields (fragment, TTL, TCP
 *	flags, TOS/TC, L4 proto, L3 length, etc.) alongside the UDF
 *	fields by embedding struct ppe_drv_acl_ip_misc.
 */
struct ppe_drv_acl_udf_with_ip {
	struct ppe_drv_acl_udf_data udf0;	/**< First UDF data */
	struct ppe_drv_acl_udf_data udf1;	/**< Second UDF data */
	struct ppe_drv_acl_udf_data udf2;	/**< Third UDF data */
	struct ppe_drv_acl_udf_data udf3;	/**< Fourth UDF data */
	bool is_ipv6;				/**< If true, use FAL_ACL_RULE_IP6 as rule type;
					     otherwise use FAL_ACL_RULE_IP4. */
	struct ppe_drv_acl_ip_misc ip_misc;	/**< Optional IP miscellaneous match fields
					     (fragment, TTL, TCP flags, TOS/TC, L4 proto,
					     L3 length, etc.) to combine with the UDF match. */
	uint32_t ip_misc_flags;			/**< PPE_DRV_ACL_IP_MISC_FLAG_* bitmask indicating
					     which ip_misc fields are valid. Zero means no
					     IP misc matching is applied. */
};


/*
 * ppe_drv_acl_rule_match_one
 *	Single ACL rule info
 */
struct ppe_drv_acl_rule_match_one {
	bool valid;					/**< Slice valid. */
	uint32_t flags;					/**< Slice flag. */
	uint8_t sub_rule_cnt;				/**< Number of sub rules. */
	enum ppe_drv_acl_slice_type type;		/**< Slice type. */
	union {
		struct ppe_drv_acl_mac smac;		/**< SMAC rule slice. */
		struct ppe_drv_acl_mac dmac;		/**< DMAC rule slice. */
		struct ppe_drv_acl_vlan vlan;		/**< VLAN rule slice. */
		struct ppe_drv_acl_l2_misc l2;		/**< L2 field slice. */
		struct ppe_drv_acl_ipv4 sip_v4;		/**< Source IPv4 rule slice. */
		struct ppe_drv_acl_ipv4 dip_v4;		/**< Destination IPv4 rule slice. */
		struct ppe_drv_acl_ipv6 dip_v6_0;	/**< Destination IPv6 rule slice 0. */
		struct ppe_drv_acl_ipv6 dip_v6_1;	/**< Destination IPv6 rule slice 1. */
		struct ppe_drv_acl_ipv6 dip_v6_2;	/**< Destination IPv6 rule slice 2. */
		struct ppe_drv_acl_ipv6 sip_v6_0;	/**< Source IPv6 rule slice 0. */
		struct ppe_drv_acl_ipv6 sip_v6_1;	/**< Source IPv6 rule slice 1. */
		struct ppe_drv_acl_ipv6 sip_v6_2;	/**< Source IPv6 rule slice 2. */
		struct ppe_drv_acl_ip_misc ip;		/**< L3 field slice. */
		struct ppe_drv_acl_udf udf;		/**< UDF with offset */
		struct ppe_drv_acl_udf_with_ip udf_with_ip;	/**< UDF with L3 IP version */
		struct ppe_drv_acl_ext_vlan ext_vlan;	/**< Extended VLAN rule slice. */
	} rule;
};

/*
 * ppe_drv_acl_action
 *	ACL rule action
 */
struct ppe_drv_acl_action {
	/*
	 * Action values.
	 */
	uint8_t cpu_code;			/**< CPU code change action. */
	uint8_t syn_toggle;			/**< SYN_TOGGLE bit change action. */
	uint8_t service_code;			/**< Service code change action. */
	uint8_t qid;				/**< QID change action. */
	uint8_t enqueue_pri;			/**< Enqueue priority change action. */
	uint8_t ctag_dei;			/**< C-DEI change action. */
	uint8_t ctag_pcp;			/**< C-PCP change action. */
	uint8_t stag_dei;			/**< S-DEI change action. */
	uint8_t stag_pcp;			/**< S-PCP change action. */
	uint8_t tos_tc;				/**< TOS/TC change action. */
	uint16_t cvid;				/**< C-VID change action. */
	uint16_t svid;				/**< S-VID change action. */
	ppe_drv_acl_dst_type_t dest_type;	/**< Destination type. */
	uint16_t dest_info;			/**< Destination change action. */
	uint16_t policer_index;			/**< Policer action. */
	uint32_t bypass_bitmap[2];		/**< Bypass bitmap. */
	ppe_drv_acl_fwd_cmd_t fwd_cmd;		/**< Forward command action. */
	enum ppe_drv_acl_dp int_dp;		/**< Internal drop precedence action. */
	uint8_t ctag_pid;			/**< C-TPID change action. */
	uint8_t stag_pid;			/**< S-TPID change action. */
	uint8_t tags_to_rmv;			/**< Tags to remove. */
	uint8_t	dscp_pbit_map_idx;		/**< DSCP to pbit mapping index. */
	uint16_t counter_id;			/**< Counter ID for stats action. */
	ppe_drv_acl_counter_mode_t counter_mode;/**< Counter mode for stats action. */
	uint8_t hw_counter_id;			/**< HW counter mode for FAL action. */
	uint8_t src_info;                       /**< SRC info based action. */
	ppe_drv_acl_src_info_type_t src_info_type;      /**< src info type based action. */
	ppe_drv_acl_pcp_cmd_t ctag_pcp_cmd;	/**< C-tag pcp format action. */
	ppe_drv_acl_pcp_cmd_t stag_pcp_cmd;	/**< S-tag pcp format action. */
	ppe_drv_acl_dei_cmd_t ctag_dei_cmd;	/**< C-tag dei format action. */
	ppe_drv_acl_dei_cmd_t stag_dei_cmd;	/**< S-tag dei format action. */
	ppe_drv_acl_vid_cmd_t ctag_vid_cmd;	/**< C-tag vid format action. */
	ppe_drv_acl_vid_cmd_t stag_vid_cmd;	/**< S-tag vid format action. */
	ppe_drv_acl_pid_cmd_t ctag_pid_cmd;	/**< C-tag pid forward command. */
	ppe_drv_acl_pid_cmd_t stag_pid_cmd;	/**< S-tag pid forward command. */

	/*
	 * Action control flags.
	 */
	uint32_t flags;				/**< Action flags. */
	uint32_t flags_ext;			/**< Action flags extended. */
};

/*
 * ppe_drv_acl_rule
 *	ACL rule and action information.
 */
struct ppe_drv_acl_rule {
	struct ppe_drv_acl_rule_match_cmn cmn;					/**< Common fields for slices. */
	struct ppe_drv_acl_rule_match_one chain[PPE_DRV_ACL_SLICE_TYPE_MAX];	/**< Array of ACL slices. */
	struct ppe_drv_acl_action action;					/**< ACL action information. */
	ppe_drv_acl_dev_type_t dev_type;					/**< Device type for ACL binding. */
	uint8_t dev;								/**< ACL dev binding. */
};

/*
 * ppe_drv_acl_hw_info
 *	ACL hardware rule information
 */
struct ppe_drv_acl_hw_info {
	uint16_t hw_rule_id;		/**< ACL hardware rule ID. */
	uint16_t hw_list_id;		/**< ACL hardware list ID. */
	uint16_t hw_num_slices;		/**< Number of slices occupied. */
};

/**
 * ppe_drv_acl_metadata
 *	ACL rule metadata for packet processing based on ACL rule.
 *	This is a common structure for sending metadata from PPE driver
 *	to PPE rule module for different use cases like mirror, redirect etc.
 */
struct ppe_drv_acl_metadata {
	uint16_t acl_hw_index;		/**< ACL hardware index got from EDMA descriptor. */
	uint16_t cpu_code;		/**< CPU code from EDMA descriptor. */
	uint16_t acl_id;		/**< ACL software index. */
};

/**
 * ppe_drv_acl_rule_callback_t
 *	Function callback type for ACL tagged packet processing.
 *
 * @datatypes
 * ppe_drv_acl_metadata
 *
 * @param[IN] app_data		Pointer to void context from caller.
 * @param[IN] skb		skb pointer.
 * @param[IN] acl_info		ACL information.
 *
 * @return
 * true for success and false for failure.
 */
typedef bool (*ppe_drv_acl_rule_callback_t)(void *app_data, struct sk_buff *skb, struct ppe_drv_acl_metadata *acl_info);

/**
 * ppe_drv_acl_flow_callback_t
 *	Function callback type for flow add and flow delete.
 *
 * @datatypes
 * ppe_drv_acl_flow_bind
 *
 * @param[IN] app_data		Pointer to void context from caller.
 * @param[IN] info 		Flow information object.
 *
 * @return
 * true for success and false for failure.
 */
typedef bool (*ppe_drv_acl_flow_callback_t)(void *app_data, struct ppe_drv_acl_flow_bind *info);

/**
 * ppe_drv_acl_mirror_core_select_cb_t
 *	Function callback type for mirror core selection.
 *
 * @param[IN] core_id		core id for mirrored packets.
 * @param[IN] app_data		Pointer to void context from caller.
 *
 * @return
 * true for success and false for failure.
 */
typedef void (*ppe_drv_acl_mirror_core_select_cb_t)(uint8_t core_id, void *app_data);

/*
 * ppe_drv_acl_process_callback_t
 *	Funtion callback type for processing based on ACL rule.
 */
typedef bool (*ppe_drv_acl_process_callback_t)(void *app_data, struct sk_buff *skb, void *acl_metadata);

/**
 * ppe_drv_acl_sc_return()
 *      Return a flow ACL service code.
 *
 * @param[IN] sc	Service code to be returned to free pool.
 *
 * @return
 * void
 */
void ppe_drv_acl_sc_return(uint8_t sc);

/*
 * ppe_drv_acl_sc_get()
 *      Reserve an unused flow ACL service code.
 *
 * @return
 * Service code
 */
uint8_t ppe_drv_acl_sc_get(void);

/*
 * ppe_drv_acl_get_hw_stats()
 *	Return hardware stats for an ACL rule.
 *
 * @datatype
 * struct ppe_drv_acl_ctx
 *
 * @param[OUT] pkts	Returned hardware packet counters.
 * @param[OUT] bytes	Returned hardware byte counters.
 *
 * @return
 * void
 */
void ppe_drv_acl_get_hw_stats(struct ppe_drv_acl_ctx *ctx, uint64_t *pkts, uint64_t *bytes);

/*
 * ppe_drv_acl_hw_info_get()
 *	Return hardware information associated with an ACL rule.
 *
 * @datatype
 * struct ppe_drv_acl_ctx
 * struct ppe_drv_acl_hw_info
 *
 * @param[IN]  ctx		ACL rule context pointer.
 * @param[OUT] hw_info		Returned hardware rule information.
 *
 * @return
 * void
 */
void ppe_drv_acl_hw_info_get(struct ppe_drv_acl_ctx *ctx, struct ppe_drv_acl_hw_info *hw_info);

/*
 * int ppe_drv_acl_udf_offset_init()
 *	Allocate and initialization acl global offset lists.
 *
 * @datatype
 *
 *
 * @return
 * 0 for success and error code for failure.
 */
int ppe_drv_acl_udf_offset_init(void);

/*
 * ppe_drv_acl_udf_offset_deinit()
 *	Deinitialization ans deallocate acl global offset lists.
 *
 * @datatype
 *
 *
 * @return
 * void.
 */
void ppe_drv_acl_udf_offset_deinit(void);

/*
 * ppe_drv_acl_offset_entry_put()
 *	Put ACL UDF offset entry.
 *
 * @datatype
 * struct ppe_drv_acl_rule_udf
 *
 * @param[IN] udf_offset		ACL UDF offset information
 *
 * @return
 * void
 */
void ppe_drv_acl_offset_entry_put(struct ppe_drv_acl_rule_udf_offset *offset);

/*
 * ppe_drv_acl_offset_entry_get()
 *	Set PPE udf offset profile.
 *
 * @datatype
 * enum ppe_drv_acl_udf_offset_proto_type
 * enum ppe_drv_acl_udf_rule_offset_base
 * enum ppe_drv_acl_rule_udf_op
 * uint32_t
 *
 * @param[IN] type		ACL UDF offset type.
 * @param[IN] offset_from	Offset from.
 * @param[IN] udf_offset	UDF offset.
 * @param[IN] udf_op		UDF match operation
 *
 * @return
 * struct ppe_drv_acl_rule_udf_offset pointer on success, NULL in failuer case.
 */
struct ppe_drv_acl_rule_udf_offset *ppe_drv_acl_offset_entry_get(enum ppe_drv_acl_udf_proto proto,
		enum ppe_drv_acl_udf_rule_base_type offset_base, uint32_t offset, enum ppe_drv_acl_rule_udf_op udf_op);

/**
 * ppe_drv_acl_rule_unregister_cb
 *	Callback unregistration for ACL rule process callback.
 *
 * @return
 * void
 */
void ppe_drv_acl_rule_unregister_cb(void);

/**
 * ppe_drv_acl_rule_register_cb
 *	Register the callback for ACL rule processing.
 *
 * @datatypes
 * ppe_drv_acl_rule_callback_t
 *
 * @param[IN] acl_cb		Pointer to acl callback.
 * @param[IN] app_data		Pointer to void context from caller.
 *
 * @return
 * void.
 */
void ppe_drv_acl_rule_register_cb(ppe_drv_acl_rule_callback_t acl_cb, void *app_data);

/**
 * ppe_drv_acl_flow_unregister_cb
 *	Callback unregistration for flow + ACL combination.
 *
 * @return
 * void
 */
void ppe_drv_acl_flow_unregister_cb(void);

/**
 * ppe_drv_acl_flow_register_cb
 *	Register the callback for flow add and delete
 *
 * @datatypes
 * ppe_drv_acl_flow_callback_t
 * ppe_drv_acl_flow_callback_t
 *
 * @param[IN] add_cb		Pointer to add callback.
 * @param[IN] del_cb		Pointer to delete callback.
 * @param[IN] app_data 		Pointer to void context from caller.
 *
 * @return
 * true for success and false for failure.
 */
void ppe_drv_acl_flow_register_cb(ppe_drv_acl_flow_callback_t add_cb, ppe_drv_acl_flow_callback_t del_cb, void *app_data);

/**
 * ppe_drv_acl_mirror_core_select_unregister_cb
 *	Unegister the mirror core selection API with PPE driver.
 *
 * @return
 * None.
 */
void ppe_drv_acl_mirror_core_select_unregister_cb(void);

/**
 * ppe_drv_acl_mirror_core_select_register_cb
 *	Register the mirror core selection API with PPE driver.
 *
 * @datatypes
 * ppe_drv_acl_mirror_core_select_cb_t
 *
 * @param[IN] cb		Mirror core register callback into DP.
 * @param[IN] app_data		Pointer to void context from caller.
 *
 * @return
 * none.
 */
void ppe_drv_acl_mirror_core_select_register_cb(ppe_drv_acl_mirror_core_select_cb_t cb, void *app_data);

/*
 * ppe_drv_acl_destroy()
 *	Destroy an ACL rule.
 */
void ppe_drv_acl_destroy(struct ppe_drv_acl_ctx *ctx);

/**
 * ppe_drv_acl_rule_configure()
 *	Configure slices for ACL rule.
 *
 * @datatypes
 * ppe_drv_acl_ctx
 * ppe_drv_acl_rule
 *
 * @param[IN] ctx		Pointer to ACL context.
 * @param[IN] info		Pointer to ACL info object.
 *
 * @return
 * Status of configuration operation.
 */
ppe_drv_ret_t ppe_drv_acl_configure(struct ppe_drv_acl_ctx *ctx, struct ppe_drv_acl_rule *info);

/*
 * ppe_drv_acl_alloc()
 *	Allocate slices for ACL rules.
 *
 * @datatypes
 * ppe_drv_acl_ipo_t
 *
 * @param[IN] type		ACL IPO type.
 * @param[IN] num_slices	Number of slices needed for a specific ACL rule.
 * @param[IN] pri		Priority of the rule.
 *
 * @return
 * Status of configuration operation.
 */
struct ppe_drv_acl_ctx *ppe_drv_acl_alloc(ppe_drv_acl_ipo_t type, uint8_t num_slices, uint16_t pri);

/**
 * ppe_drv_acl_enable_mirror_capture_core
 *	Enable capture core for mirrored packets.
 *
 * @param[IN] core_id		Capture core to enable.
 *
 * @return
 * Success or failure of the API.
 */
bool ppe_drv_acl_enable_mirror_capture_core(uint8_t core_id);

/**
 * ppe_drv_acl_process_skbuff
 *	Process packets with a valid ACL id.
 *
 * @datatypes
 * ppe_drv_acl_metadata
 * sk_buff
 *
 * @param[IN] acl_info		ACL rule related information.
 * @param[IN] skb		Socket buffer pointer.
 *
 * @return
 * Status of process operation.
 */
bool ppe_drv_acl_process_skbuff(struct ppe_drv_acl_metadata *acl_info, struct sk_buff *skb);

/**
 * ppe_drv_acl_rule_prio_upd
 * 	update the rule priorty in ppe hw.
 *
 *@datatypes
 * ppe_drv_acl_ctx
 *
 *
 * @param[IN] ctx		PPE driver acl context.
 * @param[IN] priority		New priority of the rule.
 */
ppe_drv_ret_t ppe_drv_acl_rule_prio_upd(struct ppe_drv_acl_ctx *ctx, uint16_t priority);

/**
 * ppe_drv_acl_uni_to_l2vp_sc_map
 * 	 Increments IPO rules count for ACL-bound UNI port.
 *
 *@datatypes
 * ppe_drv_acl_ctx
 *
 * @param[IN] ctx               Pointer to ACL context.
 *
 */
void ppe_drv_acl_uni_to_l2vp_sc_map(struct ppe_drv_acl_ctx *ctx);

/**
 * ppe_drv_acl_uni_to_l2vp_sc_map
 *       Decrements IPO rules count for ACL-bound UNI port.
 *
 *@datatypes
 * ppe_drv_acl_ctx
 *
 * @param[IN] ctx               Pointer to ACL context.
 *
 */
void ppe_drv_acl_uni_to_l2vp_sc_unmap(struct ppe_drv_acl_ctx *ctx);
#endif
