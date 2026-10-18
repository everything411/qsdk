/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_ACL_H
#define __PPECFG_ACL_H

#define PPECFG_ACL_HDR_VERSION 4

/*
 * PPECFG ACL commands
 */
enum ppecfg_acl_error {
	PPECFG_ACL_SUCCESS,		/* flow add */
	PPECFG_ACL_RULE_ID_DEL_FAILED,		/* flow delete */
	PPECFG_ACL_RULE_ID_ADD_FAILED,
	PPECFG_ACL_ERROR_MAX
};

/*
 * PPECFG ACL commands
 */
enum ppecfg_acl_cmd {
	PPECFG_ACL_CMD_RULE_ADD,		/* flow add */
	PPECFG_ACL_CMD_RULE_DEL,		/* flow delete */
	PPECFG_ACL_CMD_RULE_FLUSH,		/* flush rules */
	PPECFG_ACL_CMD_RULE_PRIO_UPD,		/* Update rule priority */
	PPECFG_ACL_CMD_MAX
};

/*
 * PPECFG ACL DEV fields
 */
enum ppecfg_acl_dev {
        PPECFG_ACL_DEV_NAME,		/* Dev name */
        PPECFG_ACL_DEV_TYPE,		/* dev type */
        PPECFG_ACL_DEV_MAX
};

/*
 * PPECFG ACL SMAC fields
 */
enum ppecfg_acl_smac {
	PPECFG_ACL_SMAC_VAL,		/* SMAC Value */
	PPECFG_ACL_SMAC_NVAL,		/* SMAC Not Value */
	PPECFG_ACL_SMAC_MASK,		/* SMAC Mask Value */
	PPECFG_ACL_SMAC_MAX
};

/*
 * PPECFG ACL DMAC fields
 */
enum ppecfg_acl_dmac {
	PPECFG_ACL_DMAC_VAL,		/* DMAC Value */
	PPECFG_ACL_DMAC_NVAL,		/* DMAC Not Value */
	PPECFG_ACL_DMAC_MASK,		/* DMAC Mask Value */
	PPECFG_ACL_DMAC_MAX
};

/*
 * PPECFG ACL CVID fields
 */
enum ppecfg_acl_cvid {
	PPECFG_ACL_CVID_TAGGED,		/* CVID TAG */
	PPECFG_ACL_CVID_TAGGED_MASK,	/* CVID TAG Mask Value*/
	PPECFG_ACL_CVID_VAL,		/* CVID min Value */
	PPECFG_ACL_CVID_MASK,		/* CVID Mask Value */
	PPECFG_ACL_CVID_RANGE,		/* CVID Range Value */
	PPECFG_ACL_CVID_MAX
};

/*
 * PPECFG ACL SVID fields
 */
enum ppecfg_acl_svid {
	PPECFG_ACL_SVID_MIN,		/* SVID min Value */
	PPECFG_ACL_SVID_TAG,		/* SVID TAG */
	PPECFG_ACL_SVID_TAG_MASK,	/* SVID Tag Mask Value */
	PPECFG_ACL_SVID_MASK,		/* SVID Mask Value */
	PPECFG_ACL_SVID_RANGE,		/* SVID Range Value */
	PPECFG_ACL_SVID_MAX
};

/*
 * PPECFG ACL SIP fields
 */
enum ppecfg_acl_sip {
	PPECFG_ACL_SIP_VAL,		/* SIP Value */
	PPECFG_ACL_SIP_TYPE,		/* SIP TYPE V4/V6 */
	PPECFG_ACL_SIP_NVAL,		/* SIP Not Value */
	PPECFG_ACL_SIP_MASK,		/* SIP MASK Value */
	PPECFG_ACL_SIP_MAX
};

/*
 * PPECFG ACL DIP fields
 */
enum ppecfg_acl_dip {
	PPECFG_ACL_DIP_TYPE,		/* DIP TYPE V4/V6 */
	PPECFG_ACL_DIP_VAL,		/* DIP Value */
	PPECFG_ACL_DIP_NVAL,		/* DIP Not Value */
	PPECFG_ACL_DIP_MASK,		/* DIP MASK Value */
	PPECFG_ACL_DIP_MAX
};

/*
 * PPECFG ACL CPCP fields
 */
enum ppecfg_acl_cpcp {
	PPECFG_ACL_CPCP_MIN,		/* CPCP Value */
	PPECFG_ACL_CPCP_MASK,		/* CPCP Mask Value */
	PPECFG_ACL_CPCP_MAX
};

/*
 * PPECFG ACL SPCP Fields
 */
enum ppecfg_acl_spcp {
	PPECFG_ACL_SPCP_MIN,		/* SPCP Value */
	PPECFG_ACL_SPCP_MASK,		/* SPCP Mask Value */
	PPECFG_ACL_SPCP_MAX
};

/*
 * PPECFG ACL PPPoE Fields
 */
enum ppecfg_acl_pppoe {
	PPECFG_ACL_PPPOE_VAL,		/* PPPoE Value */
	PPECFG_ACL_PPPOE_MASK,		/* PPPoE Mask Value */
	PPECFG_ACL_PPPOE_NVAL,		/* PPPoE inverse Value */
	PPECFG_ACL_PPPOE_MAX
};

/*
 * PPECFG ACL Ether Type Fields
 */
enum ppecfg_acl_ether {
	PPECFG_ACL_ETHER_MIN,		/* EtherType Value */
	PPECFG_ACL_ETHER_MASK,		/* EtherType Mask Value */
	PPECFG_ACL_ETHER_NVAL,		/* EtherType inverse Value */
	PPECFG_ACL_ETHER_MAX
};

/*
 * PPECFG ACL TTL fields
 */
enum ppecfg_acl_ttl {
	PPECFG_ACL_TTL_MIN,		/* TTL Value */
	PPECFG_ACL_TTL_MASK,		/* TTL Mask Value */
	PPECFG_ACL_TTL_MAX
};

/*
 * PPECFG ACL SPORT
 */
enum ppecfg_acl_sport {
	PPECFG_ACL_SPORT_RANGE,		/* SPORT Range Value */
	PPECFG_ACL_SPORT_MASK,		/* SPORT Mask Value */
	PPECFG_ACL_SPORT_MIN,		/* SPORT Value */
	PPECFG_ACL_SPORT_NVAL,		/* SPORT inverse Value */
	PPECFG_ACL_SPORT_MAX
};

/*
 * PPECFG ACL DPORT
 */
enum ppecfg_acl_dport {
	PPECFG_ACL_DPORT_RANGE,		/* DPORT Range Value */
	PPECFG_ACL_DPORT_MASK,		/* DPORT Mask Value */
	PPECFG_ACL_DPORT_MIN,		/* DPORT Value */
	PPECFG_ACL_DPORT_NVAL,		/* DPORT inverse Value */
	PPECFG_ACL_DPORT_MAX
};

/*
 * PPECFG ACL TOS/TC
 */
enum ppecfg_acl_tos_tc {
	PPECFG_ACL_TOS_TC_MIN,		/* TOS/TC Value */
	PPECFG_ACL_TOS_TC_MASK,		/* TOS/TC Mask Value */
	PPECFG_ACL_TOS_TC_MAX
};

/*
 * PPECFG ACL L3 Len
 */
enum ppecfg_acl_l3_len {
	PPECFG_ACL_L3_LEN_MIN,		/* L3 length/min  */
	PPECFG_ACL_L3_LEN_MASK,		/* L3 length max/mask */
	PPECFG_ACL_L3_LEN_RANGE,	/* L3 length range enable */
	PPECFG_ACL_L3_LEN_MAX
};

/*
 * PPECFG ACL CTPID
 */
enum ppecfg_acl_ctpid {
	PPECFG_ACL_CTPID_VAL,		/* CTPID val */
	PPECFG_ACL_CTPID_MASK,		/* CTPID mask */
	PPECFG_ACL_CTPID_MAX
};

/*
 * PPECFG ACL STPID
 */
enum ppecfg_acl_stpid {
	PPECFG_ACL_STPID_VAL,		/* STPID val */
	PPECFG_ACL_STPID_MASK,		/* STPID mask */
	PPECFG_ACL_STPID_MAX
};

/*
 * PPECFG ACL CDEI
 */
enum ppecfg_acl_cdei {
	PPECFG_ACL_CDEI_VAL,		/* CDEI val */
	PPECFG_ACL_CDEI_MASK,		/* CDEI mask */
	PPECFG_ACL_CDEI_MAX
};

/*
 * PPECFG ACL SDEI
 */
enum ppecfg_acl_sdei {
	PPECFG_ACL_SDEI_VAL,		/* SDEI val  */
	PPECFG_ACL_SDEI_MASK,		/* SDEI mask */
	PPECFG_ACL_SDEI_MAX
};

/*
 * PPECFG ACL DHCP
 */
enum ppecfg_acl_dhcp_type {
	PPECFG_ACL_DHCP_TYPE,		/* DHCP type */
	PPECFG_ACL_DHCP_TYPE_MASK,	/* DHCP mask */
	PPECFG_ACL_DHCP_MAX
};

/*
 * PPECFG ACL MC
 */
enum ppecfg_acl_mc_type {
	PPECFG_ACL_MC_TYPE,		/* MC type */
	PPECFG_ACL_MC_TYPE_MASK,	/* MC mask */
	PPECFG_ACL_MC_MAX
};

/*
 * PPECFG ACL Actions
 */
enum ppecfg_acl_action {
	PPECFG_ACL_ACTION_FWD_CMD,		/* FWD Command */
	PPECFG_ACL_ACTION_SERVICE_CODE,		/* Service code */
	PPECFG_ACL_ACTION_ENQUEUE_PRI,		/* Enqueue Pri */
	PPECFG_ACL_ACTION_QID,			/* QID Value */
	PPECFG_ACL_ACTION_CTAG_PCP,		/* CTAG PCP */
	PPECFG_ACL_ACTION_STAG_PCP,		/* STAG PCP */
	PPECFG_ACL_ACTION_TOS_TC,		/* TOS TC */
	PPECFG_ACL_ACTION_CVID,			/* CVID Tag */
	PPECFG_ACL_ACTION_SVID,			/* SVID Tag */
	PPECFG_ACL_ACTION_DEST,			/* Dest Flag */
	PPECFG_ACL_ACTION_REDIR_CORE,		/* Redir core */
	PPECFG_ACL_ACTION_POLICER_ID,		/* Policer ID */
	PPECFG_ACL_ACTION_MIRROR_EN,		/* Mirror EN */
#ifdef NSS_PPE_FEATURE_EXCEPTION_EDIT
	PPECFG_ACL_ACTION_EXCEPTION_EDIT_EN,	/* Packet exception edit enable */
#endif
	PPECFG_ACL_ACTION_CTAG_PCP_CMD,		/* CTAG PCP format command */
	PPECFG_ACL_ACTION_STAG_PCP_CMD,		/* STAG PCP format Command */
	PPECFG_ACL_ACTION_CTAG_VID_CMD,		/* CTAG VID format Command */
	PPECFG_ACL_ACTION_STAG_VID_CMD,		/* STAG VID format Command */
	PPECFG_ACL_ACTION_CTAG_PID,		/* CTAG PID */
	PPECFG_ACL_ACTION_CTAG_PID_CMD,		/* CTAG PID format command */
	PPECFG_ACL_ACTION_STAG_PID,		/* STAG PID */
	PPECFG_ACL_ACTION_STAG_PID_CMD,		/* STAG PID format command */
	PPECFG_ACL_ACTION_CTAG_DEI,		/* CTAG DEI */
	PPECFG_ACL_ACTION_CTAG_DEI_CMD,		/* CTAG DEI format command */
	PPECFG_ACL_ACTION_STAG_DEI,		/* STAG DEI */
	PPECFG_ACL_ACTION_STAG_DEI_CMD,		/* STAG DEI format command */
	PPECFG_ACL_ACTION_COUNTER_ID,		/* COUNTER STATS enable */
	PPECFG_ACL_ACTION_COUNTER_MODE,		/* COUNTER MODE */
	PPECFG_ACL_ACTION_TAGS_TO_RMV_CMD,	/* TAGS to remove format command */
	PPECFG_ACL_ACTION_INT_DP,		/* Internal drop precedence. */
	PPECFG_ACL_ACTION_SRC_INFO,		/* SRC info. */
	PPECFG_ACL_ACTION_SRC_INFO_TYPE,	/* SRC info type. */
	PPECFG_ACL_ACTION_MAX
};

/*
 * PPECFG ACL Rule add parameters
 */
enum ppecfg_acl_rule_add {
	PPECFG_ACL_RULE_ADD_RULE_ID,		/* Rule ID */
	PPECFG_ACL_RULE_ADD_DEV,		/* Source Dev */
	PPECFG_ACL_RULE_ADD_POST_ROUTE_EN,	/* Post route enable */
	PPECFG_ACL_RULE_ADD_FLOW_QOS_OVERRIDE,	/* Flow QoS enable */
	PPECFG_ACL_RULE_ADD_PRIORITY,		/* Priority */
	PPECFG_ACL_RULE_ADD_SRC_SC,		/* Source service code */
	PPECFG_ACL_RULE_ADD_OUTER_HEADER,	/* Outer Header */
	PPECFG_ACL_RULE_ADD_METADATA,		/* Metadata */
	PPECFG_ACL_RULE_ADD_GROUP,		/* Group number */
	PPECFG_ACL_RULE_ADD_L4_PROTO,		/* Protocol */
	PPECFG_ACL_RULE_ADD_RULE_DIR,		/* Flow Direction */
	PPECFG_ACL_RULE_ADD_SMAC,		/* SMAC fields */
	PPECFG_ACL_RULE_ADD_DMAC,		/* DMAC fields */
	PPECFG_ACL_RULE_ADD_CVID,		/* CVID fields */
	PPECFG_ACL_RULE_ADD_SVID,		/* SVID fields */
	PPECFG_ACL_RULE_ADD_CPCP,		/* CPCP fields */
	PPECFG_ACL_RULE_ADD_SPCP,		/* SPCP fields */
	PPECFG_ACL_RULE_ADD_PPPOE,		/* PPPoE fields */
	PPECFG_ACL_RULE_ADD_ETHER,		/* EtherType fields */
	PPECFG_ACL_RULE_ADD_SIP,		/* SIP fields */
	PPECFG_ACL_RULE_ADD_DIP,		/* DIP fields */
	PPECFG_ACL_RULE_ADD_SPORT,		/* SPORT fields */
	PPECFG_ACL_RULE_ADD_DPORT,		/* DPORT fields */
	PPECFG_ACL_RULE_ADD_TOS_TC,		/* TOS/TC fields */
	PPECFG_ACL_RULE_ADD_TTL,		/* TTL fields */
	PPECFG_ACL_RULE_ADD_L3_LEN,		/* L3 len fields */
	PPECFG_ACL_RULE_ADD_CTPID,		/* CTPID fields */
	PPECFG_ACL_RULE_ADD_STPID,		/* STPID fields */
	PPECFG_ACL_RULE_ADD_CDEI,		/* CDEI fields */
	PPECFG_ACL_RULE_ADD_SDEI,		/* SDEI fields */
	PPECFG_ACL_RULE_ADD_DHCP_TYPE,		/* DHCP fields */
	PPECFG_ACL_RULE_ADD_MC_TYPE,		/* MC fields */
	PPECFG_ACL_RULE_ADD_ACTION,		/* Action fields */
	PPECFG_ACL_RULE_ADD_MAX
};

/*
 * PPECFG ACL flow del
 */
enum ppecfg_acl_rule_del {
	PPECFG_ACL_RULE_DEL_RULE_ID,		/* Rule ID */
	PPECFG_ACL_RULE_DEL_MAX
};

/*
 * PPECFG ACL flow rule priority update
 */
enum ppecfg_acl_rule_prio_upd {
	PPECFG_ACL_RULE_UPDATE_PRI_RULE_ID,	/* Rule ID */
	PPECFG_ACL_RULE_UPDATE_PRI_PRIORITY,	/* New priority */
	PPECFG_ACL_RULE_UPDATE_PRI_MAX
};

int ppecfg_acl_get_rule_id(uint32_t rule_id_external);
int ppecfg_acl_add_rule_id(uint32_t rule_id_external, uint32_t rule_id);
int ppecfg_acl_del_rule_id(uint32_t rule_id_external);

#endif /* __PPECFG_ACL_H*/
