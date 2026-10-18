/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_VLAN_H
#define __PPECFG_VLAN_H

/*
 * PPECFG VLAN commands
 */
enum ppecfg_vlan_cmd {
	PPECFG_VLAN_CMD_RULE_ADD,			/* flow add */
	PPECFG_VLAN_CMD_RULE_DEL,			/* flow delete */
	PPECFG_VLAN_CMD_RULE_FLUSH,			/* flow flush */
	PPECFG_VLAN_CMD_MAX
};

/*
 * PPECFG VLAN Actions
 */
enum ppecfg_vlan_action {
	PPECFG_VLAN_ACTION_VID_SWP,			/* Swap SVID and CVID */
	PPECFG_VLAN_ACTION_SVID_XLT_CMD,		/* SVID Translation Command */
	PPECFG_VLAN_ACTION_SVID_XLT_VAL,		/* SVID Translation Value */
	PPECFG_VLAN_ACTION_CVID_XLT_CMD,		/* CVID Translation Command */
	PPECFG_VLAN_ACTION_CVID_XLT_VAL,		/* CVID Translation Value */
	PPECFG_VLAN_ACTION_PCP_SWP,			/* Swap SPCP and CPCP */
	PPECFG_VLAN_ACTION_SPCP_XLT_CMD,		/* SPCP Translation Command */
	PPECFG_VLAN_ACTION_SPCP_XLT_VAL,		/* SPCP Translation Value */
	PPECFG_VLAN_ACTION_CPCP_XLT_CMD,		/* CPCP Translation Command */
	PPECFG_VLAN_ACTION_CPCP_XLT_VAL,		/* CPCP Translation Value */
	PPECFG_VLAN_ACTION_DEI_SWP,			/* Swap SDEI and CDEI */
	PPECFG_VLAN_ACTION_SDEI_XLT_CMD,		/* SDEI Translation Command */
	PPECFG_VLAN_ACTION_SDEI_XLT_VAL,		/* SDEI Translation Value */
	PPECFG_VLAN_ACTION_CDEI_XLT_CMD,		/* CDEI Translation Command */
	PPECFG_VLAN_ACTION_CDEI_XLT_VAL,		/* CDEI Translation Value */
	PPECFG_VLAN_ACTION_TAGS_TO_REMOVE,		/* Tags To Remove */
	PPECFG_VLAN_ACTION_STPID_CMD,			/* STPID Command */
	PPECFG_VLAN_ACTION_STPID,			/* STPID Value */
	PPECFG_VLAN_ACTION_CTPID_CMD,			/* CTPID Command */
	PPECFG_VLAN_ACTION_CTPID,			/* CTPID Value */
	PPECFG_VLAN_ACTION_CNTR_ID,			/* Counter ID */
	PPECFG_VLAN_ACTION_CNTR_MODE,			/* Counter Mode */
	PPECFG_VLAN_ACTION_VSI_XLT_VAL,			/* VSI Translation Value */
	PPECFG_VLAN_ACTION_SRC_INFO_TYP,		/* Source Information Type */
	PPECFG_VLAN_ACTION_SRC_INFO_VAL,		/* Source Information Value */
	PPECFG_VLAN_ACTION_VNI_RESV_VAL,		/* VNI and Reserved Value */
	PPECFG_VLAN_ACTION_FWD_CMD,			/* Forward Command */
	PPECFG_VLAN_ACTION_SERVICE_CODE,		/* Service Code */
	PPECFG_VLAN_ACTION_DEST_INFO,			/* Destination Information */
	PPECFG_VLAN_ACTION_MAX				/* Maximum VLAN Action */
};

/*
 * PPECFG VLAN Rule add parameters
 */
enum ppecfg_vlan_rule_add {
	PPECFG_VLAN_RULE_ADD_RULE_ID,			/* Rule Identifier */
	PPECFG_VLAN_RULE_ADD_PORT_TYPE,			/* Port Type */
	PPECFG_VLAN_RULE_ADD_PORT_VAL,			/* Port Value */
	PPECFG_VLAN_RULE_ADD_RULE_DIR,			/* Rule Direction */
	PPECFG_VLAN_RULE_ADD_STAG_FORMAT,		/* S-Tag Format */
	PPECFG_VLAN_RULE_ADD_SVID_VAL,			/* SVID Value */
	PPECFG_VLAN_RULE_ADD_SPCP_VAL,			/* SPCP Value */
	PPECFG_VLAN_RULE_ADD_SDEI_VAL,			/* SDEI Value */
	PPECFG_VLAN_RULE_ADD_CTAG_FORMAT,		/* C-Tag Format */
	PPECFG_VLAN_RULE_ADD_CVID_VAL,			/* CVID Value */
	PPECFG_VLAN_RULE_ADD_CPCP_VAL,			/* CPCP Value */
	PPECFG_VLAN_RULE_ADD_CDEI_VAL,			/* CDEI Value */
	PPECFG_VLAN_RULE_ADD_FTYPE_VAL,			/* Frame Type Value */
	PPECFG_VLAN_RULE_ADD_PROTO_VAL,			/* Protocol Value */
	PPECFG_VLAN_RULE_ADD_VSI_VAL,			/* VSI Value */
	PPECFG_VLAN_RULE_ADD_VNI_RESV_TYP,		/* VNI Reserved Type */
	PPECFG_VLAN_RULE_ADD_VNI_RESV_VAL,		/* VNI Reserved Value */
	PPECFG_VLAN_RULE_ADD_STPID,			/* STPID Value */
	PPECFG_VLAN_RULE_ADD_CTPID,			/* CTPID Value */
	PPECFG_VLAN_RULE_ADD_DHCP_TYPE,			/* DHCP Type */
	PPECFG_VLAN_RULE_ADD_MC_TYPE,			/* MC Type */
	PPECFG_VLAN_RULE_ADD_ACTION,			/* Action Fields */
	PPECFG_VLAN_RULE_ADD_MAX			/* Maximum Rule Add Parameter */
};

/*
 * PPECFG VLAN flow del
 */
enum ppecfg_vlan_rule_del {
	PPECFG_VLAN_RULE_DEL_RULE_ID,			/* Rule ID */
	PPECFG_VLAN_RULE_DEL_MAX
};

/*
 * PPECFG VLAN flow flush
 */
enum ppecfg_vlan_rule_flush {
        PPECFG_VLAN_RULE_FLUSH_FLAG,			/* Flush flag */
        PPECFG_VLAN_RULE_FLUSH_MAX
};

#endif /* __PPECFG_VLAN_H*/
