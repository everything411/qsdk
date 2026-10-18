/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_vlan.h
 *      NSS PPE VLAN definitions.
 */

#ifndef _PPE_VLAN_H_
#define _PPE_VLAN_H_

#include <linux/if.h>
#include <linux/if_ether.h>

/**
 * VLAN rule ID
 */
typedef int16_t ppe_vlan_rule_id_t;

/*
 * Rule flags.
 */
#define PPE_VLAN_RULE_FLAG_RULE_ID                      0x00000001
#define PPE_VLAN_RULE_FLAG_PORT_TYPE			0x00000002
#define PPE_VLAN_RULE_FLAG_PORT_VAL			0x00000004
#define PPE_VLAN_RULE_FLAG_FLOW_DIR			0x00000008
#define PPE_VLAN_RULE_FLAG_STAG_FORMAT			0x00000010
#define PPE_VLAN_RULE_FLAG_SVID_VAL			0x00000020
#define PPE_VLAN_RULE_FLAG_SPCP_VAL			0x00000040
#define PPE_VLAN_RULE_FLAG_SDEI_VAL			0x00000080
#define PPE_VLAN_RULE_FLAG_CTAG_FORMAT			0x00000100
#define PPE_VLAN_RULE_FLAG_CVID_VAL			0x00000200
#define PPE_VLAN_RULE_FLAG_CPCP_VAL			0x00000400
#define PPE_VLAN_RULE_FLAG_CDEI_VAL			0x00000800
#define PPE_VLAN_RULE_FLAG_FTYPE_VAL			0x00001000
#define PPE_VLAN_RULE_FLAG_PROTO_VAL			0x00002000
#define PPE_VLAN_RULE_FLAG_VSI_VAL			0x00004000
#define PPE_VLAN_RULE_FLAG_VNI_RESV_TYP			0x00008000
#define PPE_VLAN_RULE_FLAG_VNI_RESV_VAL			0x00010000
#define PPE_VLAN_RULE_FLAG_STPID				0x00020000
#define PPE_VLAN_RULE_FLAG_CTPID				0x00040000
#define PPE_VLAN_RULE_FLAG_DHCP_TYPE			0x00080000
#define PPE_VLAN_RULE_FLAG_MC_TYPE			0x00100000
#define PPE_VLAN_RULE_FLAG_ACTION			0x00200000
#define PPE_VLAN_RULE_FLAG_MAX				0x00400000

/*
 * Action flags.
 */
#define PPE_VLAN_ACTION_FLAG_VID_SWP			0x00000001
#define PPE_VLAN_ACTION_FLAG_SVID_XLT_CMD		0x00000002
#define PPE_VLAN_ACTION_FLAG_SVID_XLT_VAL		0x00000004
#define PPE_VLAN_ACTION_FLAG_CVID_XLT_CMD		0x00000008
#define PPE_VLAN_ACTION_FLAG_CVID_XLT_VAL		0x00000010
#define PPE_VLAN_ACTION_FLAG_PCP_SWP			0x00000020
#define PPE_VLAN_ACTION_FLAG_SPCP_XLT_CMD		0x00000040
#define PPE_VLAN_ACTION_FLAG_SPCP_XLT_VAL		0x00000080
#define PPE_VLAN_ACTION_FLAG_CPCP_XLT_CMD		0x00000100
#define PPE_VLAN_ACTION_FLAG_CPCP_XLT_VAL		0x00000200
#define PPE_VLAN_ACTION_FLAG_DEI_SWP			0x00000400
#define PPE_VLAN_ACTION_FLAG_SDEI_XLT_CMD		0x00000800
#define PPE_VLAN_ACTION_FLAG_SDEI_XLT_VAL		0x00001000
#define PPE_VLAN_ACTION_FLAG_CDEI_XLT_CMD		0x00002000
#define PPE_VLAN_ACTION_FLAG_CDEI_XLT_VAL		0x00004000
#define PPE_VLAN_ACTION_FLAG_TAGS_TO_REMOVE		0x00008000
#define PPE_VLAN_ACTION_FLAG_STPID_CMD			0x00010000
#define PPE_VLAN_ACTION_FLAG_STPID			0x00020000
#define PPE_VLAN_ACTION_FLAG_CTPID_CMD			0x00040000
#define PPE_VLAN_ACTION_FLAG_CTPID			0x00080000
#define PPE_VLAN_ACTION_FLAG_CNTR_MODE			0x00100000
#define PPE_VLAN_ACTION_FLAG_CNTR_ID			0x00200000
#define PPE_VLAN_ACTION_FLAG_VSI_XLT_VAL		0x00400000
#define PPE_VLAN_ACTION_FLAG_SRC_INFO_TYP		0x00800000
#define PPE_VLAN_ACTION_FLAG_SRC_INFO_VAL		0x01000000
#define PPE_VLAN_ACTION_FLAG_VNI_RESV_VAL		0x02000000
#define PPE_VLAN_ACTION_FLAG_FWD_CMD			0x04000000
#define PPE_VLAN_ACTION_FLAG_SVC_CODE			0x08000000
#define PPE_VLAN_ACTION_FLAG_DEST_INFO			0x10000000
#define PPE_VLAN_ACTION_FLAG_ACTION_MAX			0x20000000

/**
 * ppe_vlan_ret
 *      VLAN return code
 */
typedef enum ppe_vlan_ret {
	PPE_VLAN_RET_SUCCESS = 0,			/**< Success */
	PPE_VLAN_RET_CREATE_FAIL_OOM,			/**< Rule create failed due to out of memory. */
	PPE_VLAN_RET_COUNTER_CTX_FAIL_OOM,		/**< Counter context create failed due to out of memory. */
	PPE_VLAN_RET_COUNTER_CTX_FAIL,			/**< Counter context create failed. */
	PPE_VLAN_RET_CREATE_FAIL_DRV_ALLOC,		/**< Rule create failed due to ppe-drv context alloc failure. */
	PPE_VLAN_RET_VLAN_RULE_NOT_FOUND,		/**< VLAN Rule ID not found. */
	PPE_VLAN_RET_INVALID_RULE_DIR,                  /**< Rule dir failed. */
	PPE_VLAN_RET_CREATE_FAIL_RULE,			/**< Rule create failed due to invalid configuration. */
	PPE_VLAN_RET_CREATE_FAIL_INVALID_CMN,		/**< Rule create failed due to invalid common rule. */
	PPE_VLAN_RET_CREATE_FAIL_RULE_FILL,		/**< Rule create failed due to rule parse failure. */
	PPE_VLAN_RET_CREATE_FAIL_ACTION_FILL,		/**< Rule create failed due to invalid action configuration. */
	PPE_VLAN_RET_CREATE_FAIL_INVALID_ID,		/**< Rule create failed due to invalid rule ID. */
	PPE_VLAN_RET_COUNTER_MODE_NOT_SUPPORTED,	/**< Rule create failed due to counter mode not supported. */
	PPE_VLAN_RET_DESTROY_FAIL_INVALID_ID,		/**< Rule destroy failed due to invalid rule ID. */
	PPE_VLAN_RET_DESTROY_FAIL,			/**< Rule destroy failed. */
} ppe_vlan_ret_t;

/**
 * ppe_vlan_flush_type
 *      Flush type for VLAN rule
 */
typedef enum ppe_vlan_flush_type {
	PPE_VLAN_FLUSH_TYPE_EXCEPT_FIRST,		/**< Flush all the VLAN rules except first. */
	PPE_VLAN_FLUSH_TYPE_ALL,			/**< Flush all the VLAN rules. */
} ppe_vlan_flush_type_t;

/**
 * ppe_vlan_rule_dir
 * 	VLAN rule direction.
 */
typedef enum ppe_vlan_rule_dir {
	PPE_VLAN_RULE_DIR_INGRESS = 1,			/**< Ingress direction. */
	PPE_VLAN_RULE_DIR_EGRESS,			/**< Egress direction. */
} ppe_vlan_rule_dir_t;

/**
 * ppe_vlan_port_type
 * 	VLAN port type.
 */
typedef enum ppe_vlan_port_type {
	PPE_VLAN_PORT_TYPE_BITMAP = 0,			/**< Port type bitmap. */
	PPE_VLAN_PORT_TYPE_PORT,			/**< Port type port. */
	PPE_VLAN_PORT_TYPE_GEM_PORT,			/**< Port type GEM port. */
} ppe_vlan_port_type_t;

/**
 * ppe_vlan_tag_format
 * 	VLAN tag format.
 */
typedef enum ppe_vlan_tag_format {
	PPE_VLAN_TAG_FORMAT_UNTAGGED = 0,		/**< Tag format untagged. */
	PPE_VLAN_TAG_FORMAT_PRIORITY,			/**< Tag format priority. */
	PPE_VLAN_TAG_FORMAT_TAGGED,			/**< Tag format tagged. */
	PPE_VLAN_TAG_FORMAT_PRI_UNTAG,			/**< Tag format priority or untagged. */
	PPE_VLAN_TAG_FORMAT_TAG_UNTAG,			/**< Tag format tagged or untagged. */
	PPE_VLAN_TAG_FORMAT_PRI_TAG,			/**< Tag format priority or tagged. */
	PPE_VLAN_TAG_FORMAT_ALL,			/**< Tag format all. */
} ppe_vlan_tag_format_t;

/**
 * ppe_vlan_frame_type
 * 	VLAN frame type.
 */
typedef enum ppe_vlan_frame_type {
	PPE_VLAN_FRAME_TYPE_ETHERNET = 0,		/**< Frame type Ethernet. */
	PPE_VLAN_FRAME_TYPE_RFC_1024,			/**< Frame type RFC 1024. */
	PPE_VLAN_FRAME_TYPE_LLC_OTHER,			/**< Frame type LLC other. */
	PPE_VLAN_FRAME_TYPE_ETHORRFC1024,		/**< Frame type Ethernet or RFC 1024. */
} ppe_vlan_frame_type_t;

/**
 * ppe_vlan_vni_resv_type
 * 	VLAN VNI reserve type.
 */
typedef enum ppe_vlan_vni_resv_type {
	PPE_VLAN_VNI_RESV_TYPE_VNI_ONLY = 0,		/**< VNI reserve type VNI only. */
	PPE_VLAN_VNI_RESV_TYPE_VNI_RESV,		/**< VNI reserve type VNI reserved. */
} ppe_vlan_vni_resv_type_t;

/**
 * ppe_vlan_dhcp_type
 * 	VLAN DHCP type.
 */
typedef enum ppe_vlan_dhcp_type {
	PPE_VLAN_DHCP_TYPE_NON_DHCP = 0,		/**< DHCP type non-DHCP. */
	PPE_VLAN_DHCP_TYPE_DHCP_V4,			/**< DHCP type DHCP v4. */
	PPE_VLAN_DHCP_TYPE_NON_DHCP_V4,			/**< DHCP type non-DHCP v4. */
	PPE_VLAN_DHCP_TYPE_DHCP_V6,			/**< DHCP type DHCP v6. */
	PPE_VLAN_DHCP_TYPE_NON_DHCP_V6,			/**< DHCP type non-DHCP v6. */
	PPE_VLAN_DHCP_TYPE_DHCP_V4_V6,			/**< DHCP type DHCP v4 or v6. */
	PPE_VLAN_DHCP_TYPE_ALL,				/**< DHCP type all. */
} ppe_vlan_dhcp_type_t;

/**
 * ppe_vlan_mc_type
 * 	VLAN MC type.
 */
typedef enum ppe_vlan_mc_type {
	PPE_VLAN_MC_TYPE_NON_MC = 0,			/**< MC type non-MC. */
	PPE_VLAN_MC_TYPE_IP_MC,				/**< MC type IP MC. */
	PPE_VLAN_MC_TYPE_NON_MC_IP_MC,			/**< MC type non-MC or IP MC. */
	PPE_VLAN_MC_TYPE_NON_IP_MC,			/**< MC type non-IP MC. */
	PPE_VLAN_MC_TYPE_NON_MC_NON_IP_MC,		/**< MC type non-MC or non-IP MC. */
	PPE_VLAN_MC_TYPE_IP_MC_NON_IP_MC,		/**< MC type IP MC or non-IP MC. */
	PPE_VLAN_MC_TYPE_ALL,				/**< MC type all. */
} ppe_vlan_mc_type_t;

/**
 * ppe_vlan_xlt_cmd
 * 	VLAN translation command.
 */
typedef enum ppe_vlan_xlt_cmd {
	PPE_VLAN_XLT_CMD_UNCHANGE = 0,			/**< Translation command unchanged. */
	PPE_VLAN_XLT_CMD_ADD,				/**< Translation command add. */
	PPE_VLAN_XLT_CMD_DEL,				/**< Translation command delete. */
	PPE_VLAN_XLT_CMD_CP_SVID,			/**< Translation command copy SVID. */
	PPE_VLAN_XLT_CMD_CP_CVID,			/**< Translation command copy CVID. */
} ppe_vlan_xlt_cmd_t;

/**
 * ppe_vlan_pcp_xlt_cmd
 * 	VLAN PCP translation command.
 */
typedef enum ppe_vlan_pcp_xlt_cmd {
	PPE_VLAN_PCP_XLT_CMD_UNCHANGE = 0,		/**< PCP translation command unchanged. */
	PPE_VLAN_PCP_XLT_CMD_REPLACE,			/**< PCP translation command replace. */
	PPE_VLAN_PCP_XLT_CMD_CP_SPCP,			/**< PCP translation command copy SPCP. */
	PPE_VLAN_PCP_XLT_CMD_CP_CPCP,			/**< PCP translation command copy CPCP. */
	PPE_VLAN_PCP_XLT_CMD_DSCP_PCP0,			/**< PCP translation command DSCP PCP0. */
	PPE_VLAN_PCP_XLT_CMD_DSCP_PCP1,			/**< PCP translation command DSCP PCP1. */
	PPE_VLAN_PCP_XLT_CMD_ADD_REP_PCP,		/**< PCP translation command add and replace PCP. */
	PPE_VLAN_PCP_XLT_CMD_ADD_CP_SPCP,		/**< PCP translation command add and copy SPCP. */
	PPE_VLAN_PCP_XLT_CMD_ADD_CP_CPCP,		/**< PCP translation command add and copy CPCP. */
	PPE_VLAN_PCP_XLT_CMD_ADD_DSCP_PCP0,		/**< PCP translation command add and DSCP PCP0. */
	PPE_VLAN_PCP_XLT_CMD_ADD_DSCP_PCP1,		/**< PCP translation command add and DSCP PCP1. */
} ppe_vlan_pcp_xlt_cmd_t;

/**
 * ppe_vlan_dei_xlt_cmd
 * 	VLAN DEI translation command.
 */
typedef enum ppe_vlan_dei_xlt_cmd {
	PPE_VLAN_DEI_XLT_CMD_UNCHANGE = 0,		/**< DEI translation command unchanged. */
	PPE_VLAN_DEI_XLT_CMD_REPLACE,			/**< DEI translation command replace. */
	PPE_VLAN_DEI_XLT_CMD_CP_SDEI,			/**< DEI translation command copy SDEI. */
	PPE_VLAN_DEI_XLT_CMD_CP_CDEI,			/**< DEI translation command copy CDEI. */
} ppe_vlan_dei_xlt_cmd_t;

/**
 * ppe_vlan_tpid_cmd
 * 	VLAN TPID command.
 */
typedef enum ppe_vlan_tpid_cmd {
	PPE_VLAN_TPID_CMD_UNCHANGE = 0,			/**< TPID command unchanged. */
	PPE_VLAN_TPID_CMD_REPLACE,			/**< TPID command replace. */
	PPE_VLAN_TPID_CMD_CP_STPID,			/**< TPID command copy STPID. */
	PPE_VLAN_TPID_CMD_CP_CTPID,			/**< TPID command copy CTPID. */
} ppe_vlan_tpid_cmd_t;

/**
 * ppe_vlan_counter_mode
 * 	VLAN counter mode.
 */
typedef enum ppe_vlan_counter_mode {
	PPE_VLAN_COUNTER_MODE_VLAN = 0,			/**< Counter mode VLAN. */
	PPE_VLAN_COUNTER_MODE_PON_PM,			/**< Counter mode PON PM. */
} ppe_vlan_counter_mode_t;

/**
 * ppe_vlan_src_info_type
 * 	VLAN source info type.
 */
typedef enum ppe_vlan_src_info_type {
	PPE_VLAN_SRC_INFO_TYPE_VP = 0,			/**< Source info type VP. */
	PPE_VLAN_SRC_INFO_TYPE_L3_IF,			/**< Source info type L3 IF. */
} ppe_vlan_src_info_type_t;

/**
 * ppe_vlan_fwd_cmd
 * 	VLAN forward command.
 */
typedef enum ppe_vlan_fwd_cmd {
	PPE_VLAN_FWD_CMD_FORWARD = 0,			/**< Forward command forward. */
	PPE_VLAN_FWD_CMD_DROP,				/**< Forward command drop. */
	PPE_VLAN_FWD_CMD_COPY,				/**< Forward command copy. */
	PPE_VLAN_FWD_CMD_REDIRECT,			/**< Forward command redirect. */
} ppe_vlan_fwd_cmd_t;

/**
 * ppe_vlan_rule_action
 *      VLAN rule action
 */
struct ppe_vlan_rule_action {
	/*
	 * Action values.
	 */
	bool swap_svid_cvid;				/**< Swap SVID and CVID. */
	ppe_vlan_xlt_cmd_t svid_xlate_cmd;		/**< SVID translation command. */
	uint32_t svidxlate;				/**< SVID translation value. */
	ppe_vlan_xlt_cmd_t cvid_xlate_cmd;		/**< CVID translation command. */
	uint32_t cvidxlate;				/**< CVID translation value. */
	bool swap_spcp_cpcp;				/**< Swap SPCP and CPCP. */
	ppe_vlan_pcp_xlt_cmd_t spcp_xlate_cmd;		/**< SPCP translation command. */
	uint8_t spcptranslation;			/**< SPCP translation value. */
	ppe_vlan_pcp_xlt_cmd_t cpcp_xlate_cmd;		/**< CPCP translation command. */
	uint8_t cpcptranslation;			/**< CPCP translation value. */
	bool swap_sdei_cdei;				/**< Swap SDEI and CDEI. */
	ppe_vlan_dei_xlt_cmd_t sdei_xlate_cmd;		/**< SDEI translation command. */
	uint8_t sdeitranslation;			/**< SDEI translation value. */
	ppe_vlan_dei_xlt_cmd_t cdei_xlate_cmd;		/**< CDEI translation command. */
	uint8_t cdeitranslation;			/**< CDEI translation value. */
	uint8_t tags_to_remove;				/**< Tags to remove. */
	ppe_vlan_tpid_cmd_t stpid_cmd;			/**< STPID command. */
	uint16_t stpid_action;				/**< STPID action. */
	ppe_vlan_tpid_cmd_t ctpid_cmd;			/**< CTPID command. */
	uint16_t ctpid_action;				/**< CTPID action. */
	uint32_t counter_id;				/**< Counter ID. */
	ppe_vlan_counter_mode_t counter_mode;		/**< Counter mode. */
	uint32_t vsitranslation;			/**< VSI translation. */
	ppe_vlan_src_info_type_t src_info_type;		/**< Source info type. */
	char src_info[IFNAMSIZ];			/**< Source info. */
	uint32_t vni_resv_action;			/**< VNI reserve action. */
	ppe_vlan_fwd_cmd_t fwd_cmd;			/**< Forward command. */
	uint8_t sc;					/**< Service code. */
	char dest_info[IFNAMSIZ];			/**< Destination info. */

	/*
	 * Action control flags.
	 */
	uint32_t action_flags;                         /**< Action control flags. */
};

/**
 * ppe_vlan_rule_match
 *      Fields for VLAN rule
 */
struct ppe_vlan_rule_match {
	/*
	 * Rule values
	 */
	ppe_vlan_port_type_t port_type;			/**< Port type. */
	union {
		uint32_t port_bitmap;
		char dev_name[IFNAMSIZ];
		uint32_t gem_port;
	} port_val;
	ppe_vlan_tag_format_t stag_format;		/**< STAG format. */
	uint32_t svid;					/**< SVID. */
	uint32_t spcp;					/**< SPCP. */
	uint32_t sdei;					/**< SDEI. */
	ppe_vlan_tag_format_t ctag_format;		/**< CTAG format. */
	uint32_t cvid;					/**< CVID. */
	uint32_t cpcp;					/**< CPCP. */
	uint32_t cdei;					/**< CDEI. */
	ppe_vlan_frame_type_t frame_type;		/**< Frame type. */
	uint16_t proto;					/**< Protocol. */
	uint32_t vsi;					/**< VSI. */
	ppe_vlan_vni_resv_type_t vni_resv_type;		/**< VNI reserve type. */
	uint32_t vni_resv;				/**< VNI reserve. */
	uint16_t stpid;					/**< STPID. */
	uint16_t ctpid;					/**< CTPID. */
	ppe_vlan_dhcp_type_t dhcp_type;			/**< DHCP type. */
	ppe_vlan_mc_type_t mc_type;			/**< MC type. */

	/*
	 * Rule control flags.
	 */
	uint32_t rule_flags;                         /**< Rule control flags. */
};

/**
 * ppe_vlan_rule
 *      Multiple VLAN rules.
 */
struct ppe_vlan_rule {
	/*
	 * Request
	 */
	ppe_vlan_rule_id_t rule_id;			/**< Rule ID. */
	ppe_vlan_rule_dir_t rule_dir;			/**< Rule direction for translation rule. */
	char dev_name[IFNAMSIZ];			/**< source dev name. */
	struct ppe_vlan_rule_match rule_f;		/**< VLAN rule object. */
	struct ppe_vlan_rule_action action_f;		/**< VLAN action object. */

	bool except_flag;				/**< Except flag. */

	/*
	 * Response
	 */
	ppe_vlan_ret_t ret;				/**< VLAN return type. */
};


/**
 * ppe_vlan_rule_create()
 *      Create VLAN rule in PPE.
 *
 * @datatypes
 * ppe_vlan_rule
 *
 * @param[IN] rule              VLAN rule information.
 *
 * @return
 * Status of rule create operation.
 */
ppe_vlan_ret_t ppe_vlan_rule_create(struct ppe_vlan_rule *rule);

/**
 * ppe_vlan_rule_destroy()
 *      Destroy VLAN rule in PPE.
 *
 * @datatypes
 * ppe_vlan_rule_id_t
 *
 * @param[IN] id              VLAN rule id.
 *
 * @return
 * Status of rule destroy operation.
 */
ppe_vlan_ret_t ppe_vlan_rule_destroy(ppe_vlan_rule_id_t id);

/**
 * ppe_vlan_rule_flush()
 *      Flush VLAN rule in PPE.
 *
 * @datatypes
 * ppe_vlan_flush_type_t
 *
 * @param[IN] flush_type              VLAN flush type.
 *
 * @return
 * Status of rule flush operation.
 */
ppe_vlan_ret_t ppe_vlan_rule_flush(ppe_vlan_flush_type_t flush_type);

#endif // _PPE_VLAN_H_
