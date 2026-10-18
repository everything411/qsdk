/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_VLAN_H_
#define _PPE_DRV_VLAN_H_

#include <fal/fal_portvlan.h>

struct ppe_drv_iface;

struct ppe_drv_tun_encap;

#define PPE_DRV_MAX_VLAN 2

/**
 * VLAN frame type match in vlan translation
 * These are bit flags that can be combined using bitwise OR to match multiple frame types.
 */
#define PPE_DRV_VLAN_XLT_MATCH_UNTAGGED		(1 << 0)	/**< Match untagged frames. */
#define PPE_DRV_VLAN_XLT_MATCH_PRIORITY		(1 << 1)	/**< Match priority-tagged frames. */
#define PPE_DRV_VLAN_XLT_MATCH_TAGGED		(1 << 2)	/**< Match tagged frames. */

/**
 * VLAN field DHCP type
 * These are bit flags that can be combined using bitwise OR to match multiple DHCP types.
 */
#define PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP		(1 << 0)	/**< Match non-DHCP packets. */
#define PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4		(1 << 1)	/**< Match DHCPv4 packets. */
#define PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6		(1 << 2)	/**< Match DHCPv6 packets. */

/**
 * VLAN field MC type
 * These are bit flags that can be combined using bitwise OR to match multiple multicast types.
 */
#define PPE_DRV_VLAN_MC_TYPE_NON_MC		(1 << 0)	/**< Match non-multicast packets. */
#define PPE_DRV_VLAN_MC_TYPE_IP_MC		(1 << 1)	/**< Match IP multicast packets. */
#define PPE_DRV_VLAN_MC_TYPE_NON_IP_MC		(1 << 2)	/**< Match non-IP multicast packets. */

/**
 * Rule flags.
 */
#define PPE_DRV_VLAN_RULE_FLAG_PORT_TYPE		0x00000002
#define PPE_DRV_VLAN_RULE_FLAG_PORT_VAL			0x00000004
#define PPE_DRV_VLAN_RULE_FLAG_STAG_FORMAT		0x00000010
#define PPE_DRV_VLAN_RULE_FLAG_SVID_VAL			0x00000020
#define PPE_DRV_VLAN_RULE_FLAG_SPCP_VAL			0x00000040
#define PPE_DRV_VLAN_RULE_FLAG_SDEI_VAL			0x00000080
#define PPE_DRV_VLAN_RULE_FLAG_CTAG_FORMAT		0x00000100
#define PPE_DRV_VLAN_RULE_FLAG_CVID_VAL			0x00000200
#define PPE_DRV_VLAN_RULE_FLAG_CPCP_VAL			0x00000400
#define PPE_DRV_VLAN_RULE_FLAG_CDEI_VAL			0x00000800
#define PPE_DRV_VLAN_RULE_FLAG_FTYPE_VAL		0x00001000
#define PPE_DRV_VLAN_RULE_FLAG_PROTO_VAL		0x00002000
#define PPE_DRV_VLAN_RULE_FLAG_VSI_VAL			0x00004000
#define PPE_DRV_VLAN_RULE_FLAG_VNI_RESV_TYP		0x00008000
#define PPE_DRV_VLAN_RULE_FLAG_VNI_RESV_VAL		0x00010000
#define PPE_DRV_VLAN_RULE_FLAG_STPID			0x00020000
#define PPE_DRV_VLAN_RULE_FLAG_CTPID			0x00040000
#define PPE_DRV_VLAN_RULE_FLAG_DHCP_TYPE		0x00080000
#define PPE_DRV_VLAN_RULE_FLAG_MC_TYPE			0x00100000

/**
 * Action flags.
 */
#define PPE_DRV_VLAN_ACTION_FLAG_VID_SWP		0x00000001
#define PPE_DRV_VLAN_ACTION_FLAG_SVID_XLT_CMD		0x00000002
#define PPE_DRV_VLAN_ACTION_FLAG_SVID_XLT_VAL		0x00000004
#define PPE_DRV_VLAN_ACTION_FLAG_CVID_XLT_CMD		0x00000008
#define PPE_DRV_VLAN_ACTION_FLAG_CVID_XLT_VAL		0x00000010
#define PPE_DRV_VLAN_ACTION_FLAG_PCP_SWP		0x00000020
#define PPE_DRV_VLAN_ACTION_FLAG_SPCP_XLT_CMD		0x00000040
#define PPE_DRV_VLAN_ACTION_FLAG_SPCP_XLT_VAL		0x00000080
#define PPE_DRV_VLAN_ACTION_FLAG_CPCP_XLT_CMD		0x00000100
#define PPE_DRV_VLAN_ACTION_FLAG_CPCP_XLT_VAL		0x00000200
#define PPE_DRV_VLAN_ACTION_FLAG_DEI_SWP		0x00000400
#define PPE_DRV_VLAN_ACTION_FLAG_SDEI_XLT_CMD		0x00000800
#define PPE_DRV_VLAN_ACTION_FLAG_SDEI_XLT_VAL		0x00001000
#define PPE_DRV_VLAN_ACTION_FLAG_CDEI_XLT_CMD		0x00002000
#define PPE_DRV_VLAN_ACTION_FLAG_CDEI_XLT_VAL		0x00004000
#define PPE_DRV_VLAN_ACTION_FLAG_TAGS_TO_REMOVE		0x00008000
#define PPE_DRV_VLAN_ACTION_FLAG_STPID_CMD		0x00010000
#define PPE_DRV_VLAN_ACTION_FLAG_STPID			0x00020000
#define PPE_DRV_VLAN_ACTION_FLAG_CTPID_CMD		0x00040000
#define PPE_DRV_VLAN_ACTION_FLAG_CTPID			0x00080000
#define PPE_DRV_VLAN_ACTION_FLAG_CNTR_MODE		0x00100000
#define PPE_DRV_VLAN_ACTION_FLAG_CNTR_ID		0x00200000
#define PPE_DRV_VLAN_ACTION_FLAG_VSI_XLT_VAL		0x00400000
#define PPE_DRV_VLAN_ACTION_FLAG_SRC_INFO_TYP		0x00800000
#define PPE_DRV_VLAN_ACTION_FLAG_SRC_INFO_VAL		0x01000000
#define PPE_DRV_VLAN_ACTION_FLAG_VNI_RESV_VAL		0x02000000
#define PPE_DRV_VLAN_ACTION_FLAG_FWD_CMD		0x04000000
#define PPE_DRV_VLAN_ACTION_FLAG_SVC_CODE		0x08000000
#define PPE_DRV_VLAN_ACTION_FLAG_DEST_INFO		0x10000000

/**
 * ppe_drv_vlan_type
 *	Flag indicating packet is vlan tagged or untagged.
 */
enum ppe_drv_vlan_type {
	PPE_DRV_VLAN_UNTAGGED,	/**< VLAN untagged. */
	PPE_DRV_VLAN_TAGGED	/**< VLAN tagged. */
};

/**
 * ppe_drv_vlan_vid_xlt_cmd
 *	VID translation command.
 */
typedef enum ppe_drv_vlan_vid_xlt_cmd {
	PPE_DRV_VLAN_VID_XLT_CMD_UNCHANGED = 0,		/**< VLAN VID xlate cmd unchanged. */
	PPE_DRV_VLAN_VID_XLT_CMD_ADDORREPLACE = 1,	/**< VLAN VID xlate cmd add or replace. */
	PPE_DRV_VLAN_VID_XLT_CMD_DELETE = 2,		/**< VLAN VID xlate cmd delete. */
	PPE_DRV_VLAN_VID_XLT_CMD_CP_SVID = 3,		/**< VLAN VID xlate cmd copy SVID. */
	PPE_DRV_VLAN_VID_XLT_CMD_CP_CVID = 4,		/**< VLAN VID xlate cmd copy CVID. */
} ppe_drv_vlan_vid_xlt_cmd_t;

/**
 * ppe_drv_vlan_pcp_xlt_cmd
 *	PCP translation commnad.
 */
typedef enum ppe_drv_vlan_pcp_xlt_cmd {
	PPE_DRV_VLAN_PCP_XLT_CMD_UNCHANGED = 0,		/**< VLAN PCP xlate cmd unchanged. */
	PPE_DRV_VLAN_PCP_XLT_CMD_REPLACE = 1,		/**< VLAN PCP xlate cmd replace. */
	PPE_DRV_VLAN_PCP_XLT_CMD_CP_SPCP = 2,		/**< VLAN PCP xlate cmd copy SPCP. */
	PPE_DRV_VLAN_PCP_XLT_CMD_CP_CPCP = 3,		/**< VLAN PCP xlate cmd copy CPCP. */
	PPE_DRV_VLAN_PCP_XLT_CMD_DSCP = 4,		/**< VLAN PCP xlate cmd map from DSCP. */
	PPE_DRV_VLAN_PCP_XLT_CMD_ADD_REP_PCP = 5,	/**< VLAN PCP xlate cmd add and replace PCP. */
	PPE_DRV_VLAN_PCP_XLT_CMD_ADD_CP_SPCP = 6,	/**< VLAN PCP xlate cmd add and copy SPCP. */
	PPE_DRV_VLAN_PCP_XLT_CMD_ADD_CP_CPCP = 7,	/**< VLAN PCP xlate cmd add and copy CPCP. */
	PPE_DRV_VLAN_PCP_XLT_CMD_ADD_DSCP = 8,		/**< VLAN PCP xlate cmd add and map from DSCP. */
} ppe_drv_vlan_pcp_xlt_cmd_t;

/**
 * ppe_drv_vlan_dscp_pbit_index
 *	DSCP_PBIT mapping index.
 */
typedef enum ppe_drv_vlan_dscp_pbit_index {
	PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP0 = 0,		/**< VLAN DSCP map index PCP0. */
	PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP1 = 1,		/**< VLAN DSCP map index PCP1. */
} ppe_drv_vlan_dscp_pbit_index_t;

/**
 * ppe_drv_vlan_dei_xlt_cmd
 *	DEI translation command.
 */
typedef enum ppe_drv_vlan_dei_xlt_cmd {
	PPE_DRV_VLAN_DEI_XLT_CMD_UNCHANGED = 0,		/**< VLAN DEI xlate cmd unchanged. */
	PPE_DRV_VLAN_DEI_XLT_CMD_REPLACE = 1,		/**< VLAN DEI xlate cmd replace. */
	PPE_DRV_VLAN_DEI_XLT_CMD_CP_SDEI = 2,		/**< VLAN DEI xlate cmd copy SDEI. */
	PPE_DRV_VLAN_DEI_XLT_CMD_CP_CDEI = 3,		/**< VLAN DEI xlate cmd copy CDEI. */
} ppe_drv_vlan_dei_xlt_cmd_t;

/**
 * ppe_drv_vlan_tag_cmd
 *	VLAN stpid/ctpid command.
 */
typedef enum ppe_drv_vlan_tag_cmd {
	PPE_DRV_VLAN_TPID_CMD_UNCHANGED = 0,		/**< VLAN tpid cmd unchanged. */
	PPE_DRV_VLAN_TPID_CMD_REPLACE = 1,		/**< VLAN tpid cmd replace. */
	PPE_DRV_VLAN_TPID_CMD_CP_STPID = 2,		/**< VLAN tpid cmd copy stpid. */
	PPE_DRV_VLAN_TPID_CMD_CP_CTPID = 3,		/**< VLAN tpid cmd copy ctpid. */
} ppe_drv_vlan_tag_cmd_t;

/**
 * ppe_drv_vlan_counter_mode
 *	VLAN counter mode.
 */
typedef enum ppe_drv_vlan_counter_mode {
	PPE_DRV_VLAN_COUNTER_MODE_VLAN = 0,		/**< VLAN counter mode VLAN. */
	PPE_DRV_VLAN_COUNTER_MODE_PONPM = 1,		/**< VLAN counter mode PON PM. */
} ppe_drv_vlan_counter_mode_t;

/**
 * ppe_drv_vlan_src_info_type
 *	VLAN source information type.
 */
typedef enum ppe_drv_vlan_src_info_type {
	PPE_DRV_VLAN_SRC_INFO_TYPE_VP = 0,		/**< VLAN source info type VP. */
	PPE_DRV_VLAN_SRC_INFO_TYPE_L3IF = 1,		/**< VLAN source info type L3 IF. */
} ppe_drv_vlan_src_info_type_t;

/**
 * ppe_drv_vlan_fwd_cmd
 *	VLAN forward command.
 */
typedef enum ppe_drv_vlan_fwd_cmd {
	PPE_DRV_VLAN_FWD_CMD_FORWARD = 0,		/**< VLAN forward cmd forward. */
	PPE_DRV_VLAN_FWD_CMD_DROP = 1,			/**< VLAN forward cmd drop. */
	PPE_DRV_VLAN_FWD_CMD_COPY = 2,			/**< VLAN forward cmd copy. */
	PPE_DRV_VLAN_FWD_CMD_REDIRECT = 3,		/**< VLAN forward cmd redirect. */
} ppe_drv_vlan_fwd_cmd_t;

/**
 * ppe_drv_vlan_port_type
 *	VLAN port type.
 */
typedef enum ppe_drv_vlan_port_type {
	PPE_DRV_VLAN_PORT_TYPE_BITMAP = 0,		/**< VLAN port type bitmap. */
	PPE_DRV_VLAN_PORT_TYPE_PORT = 1,		/**< VLAN port type port. */
	PPE_DRV_VLAN_PORT_TYPE_GEMPORT = 2,		/**< VLAN port type GEM port. */
} ppe_drv_vlan_port_type_t;

/**
 * ppe_drv_vlan_frame_type
 *	VLAN frame type.
 */
typedef enum ppe_drv_vlan_frame_type {
	PPE_DRV_VLAN_FRAME_TYPE_ETHERNET = 0,		/**< VLAN frame type Ethernet. */
	PPE_DRV_VLAN_FRAME_TYPE_RFC_1024 = 1,		/**< VLAN frame type RFC 1024. */
	PPE_DRV_VLAN_FRAME_TYPE_LLC_OTHER = 2,		/**< VLAN frame type LLC other. */
	PPE_DRV_VLAN_FRAME_TYPE_ETHORRFC1024 = 3,	/**< VLAN frame type Ethernet or RFC 1024. */
} ppe_drv_vlan_frame_type_t;

/**
 * ppe_drv_vlan_vni_resv_type
 *	VLAN VNI reserve type.
 */
typedef enum ppe_drv_vlan_vni_resv_type {
	PPE_DRV_VLAN_VNI_RESV_TYPE_VNI_ONLY = 0,	/**< VLAN VNI type VNI only. */
	PPE_DRV_VLAN_VNI_RESV_TYPE_VNI_RESV = 1,	/**< VLAN VNI type VNI reserved. */
} ppe_drv_vlan_vni_resv_type_t;

/**
 * ppe_drv_vlan
 *	VLAN information
 */
struct ppe_drv_vlan {
	uint16_t tpid;		/**< TPID in VLAN header. */
	uint16_t tci;		/**< TCI in VLAN header. */
};

/**
 * ppe_drv_vlan_xlate_info()
 *	VLAN translation info
 */
struct ppe_drv_vlan_xlate_info {
	struct ppe_drv_iface *br;				/**< Bridge PPE interface */
	uint32_t port_id;					/**< Port-ID */
	uint32_t cvid;						/**< CVID to program in XLATE tables */
	uint32_t svid;						/**< SVID to program in XLATE tables */
};

/**
 * ppe_drv_vlan_action
 *	VLAN rule action
 */
struct ppe_drv_vlan_action {
	/*
	 * Action values.
	 */
	bool swap_svid_cvid;                 		/**< Swap the internal SVID and CVID */
	ppe_drv_vlan_vid_xlt_cmd_t svid_xlate_cmd;	/**< Command for the translation SVID */
	uint16_t svidxlate;				/**< Translation SVID */
	ppe_drv_vlan_vid_xlt_cmd_t cvid_xlate_cmd;	/**< Command for the translation CVID */
	uint16_t cvidxlate;				/**< Translation CVID */
	bool swap_spcp_cpcp;				/**< Swap the internal SPCP and CPCP */
	ppe_drv_vlan_pcp_xlt_cmd_t spcp_xlate_cmd;	/**< Command for the SPCP translation */
	uint8_t spcptranslation;			/**< Translation SPCP */
	ppe_drv_vlan_pcp_xlt_cmd_t cpcp_xlate_cmd;	/**< Command for the CPCP translation */
	uint8_t cpcptranslation;			/**< Translation CPCP */
	bool swap_sdei_cdei;				/**< Swap the internal SDEI and CDEI */
	ppe_drv_vlan_dei_xlt_cmd_t sdei_xlate_cmd;	/**< Command for the SDEI translation */
	uint8_t sdeitranslation;			/**< Translation SDEI */
	ppe_drv_vlan_dei_xlt_cmd_t cdei_xlate_cmd;	/**< Command for the CDEI translation */
	uint8_t cdeitranslation;			/**< Translation CDEI */
	uint8_t tags_to_remove;				/**< Tags to remove */
	ppe_drv_vlan_tag_cmd_t stpid_cmd;		/**< STPID command */
	int16_t stpid_action;				/**< STPID value for translation */
	ppe_drv_vlan_tag_cmd_t ctpid_cmd;		/**< CTPID command */
	int16_t ctpid_action;				/**< CTPID value for translation */
	ppe_drv_vlan_dscp_pbit_index_t dscp_p_bit_map_ind;/**< DSCP to P bit index */
	uint8_t counter_id;				/**< VLAN device counter ID */
	ppe_drv_vlan_counter_mode_t counter_mode;	/**< Counter mode */
	uint8_t vsitranslation;				/**< Translation VSI */
	ppe_drv_vlan_src_info_type_t src_info_type;	/**< Type of source information */
	bool src_info_enable;				/**< Control source info enablement */
	uint32_t src_info;				/**< Source info */
	bool vni_resv_enable_action;			/**< Enable VNI Resv */
	uint32_t vni_resv_action;			/**< VNI Resv value */
	ppe_drv_vlan_fwd_cmd_t fwd_cmd;			/**< Forward command */
	bool sc_en;					/**< Service code enable field */
	uint8_t sc;					/**< Service code field */
	bool dest_info_valid;				/**< Destination information valid field */
	uint8_t dest_info;				/**< Destination information field */

	/*
	 * Action control flags.
	 */
	uint32_t flags;                                 /**< Action flags. */
};

/**
 * ppe_vlan_rule_match
 *	Fields for VLAN rule match
 */
struct ppe_drv_vlan_rule_match {
	/*
	 * Rule values.
	 */
	ppe_drv_vlan_port_type_t port_type;		/**< Port Type: 0 = Port bitmap, 1 = Port/VP; 2 = VP profile; 3 = GEM port */
	uint8_t port_val;				/**< Port Value */
	uint8_t stag_format;				/**< SVID Format: if the frame is untagged, priority tagged, or tagged */
	uint32_t svid;					/**< SVID value */
	uint8_t spcp;					/**< Source Priority Code Point */
	uint8_t sdei;					/**< Source Drop Eligible Indicator */
	uint8_t ctag_format;				/**< CVID Format: if the frame is untagged, priority tagged, or tagged */
	uint32_t cvid;					/**< CVID value */
	uint8_t cpcp;					/**< Customer Priority Code Point */
	uint8_t cdei;					/**< Customer Drop Eligible Indicator */
	ppe_drv_vlan_frame_type_t frame_type;		/**< Frame type for protocol-based VLAN assignment */
	uint16_t proto;					/**< Protocol value */
	uint32_t vsi;					/**< VSI value */
	ppe_drv_vlan_vni_resv_type_t vni_resv_type;	/**< Type of VNI or GRE key field */
	uint32_t vni_resv;				/**< VNI or GRE key field */
	int16_t stpid;					/**< STPID Value: For inner outer TPID match */
	int16_t ctpid;					/**< CTPID Value: For inner outer TPID match */
	uint8_t dhcp_type;				/**< DHCP Type */
	uint8_t mc_type;				/**< Multicast Type */

	/*
	 * Rule control flags.
	 */
	uint32_t flags;					/**< Rule flags. */
};

/**
 * ppe_drv_vlan_cfg
 *	Fields for VLAN rule
 */
struct ppe_drv_vlan_cfg {
	struct net_device *src_dev;			/**< Source Net device */
	struct net_device *dst_dev;			/**< Destination Net device */
	uint32_t  rule_id;				/**< Unique Identifier for VLAN Rule */
	ppe_drv_rule_dir_t rule_dir;			/**< VLAN rule direction: 1:Ingress 2:Egress */
	struct ppe_drv_vlan_rule_match rule_f;		/**< VLAN rule informations */
	struct ppe_drv_vlan_action action_f;            /**< VLAN action information */
};

/**
 * ppe_drv_vlan_tpid_set
 *	Set VLAN tpid in PPE vlan tables.
 *
 * @param[in] tpid_arr	TPID Array.
 * @param[in] mask    Mask for Egress and Ingress vlan enable.
 * @param[in] port_role Vlan port role.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_tpid_set(uint16_t *tpid_arr, uint32_t mask, fal_qinq_port_role_t port_role);

/**
 * ppe_drv_vlan_port_role_set
 *	Set VLAN port role.
 *
 * @datatypes
 * ppe_drv_iface
 *
 * @param[in] iface  PPE interface for vlan device.
 * @param[in] port_id  Port index number.
 * @param[in] mode    Ingress and/or egress mode.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_port_role_set(struct ppe_drv_iface *iface, uint32_t port_id, fal_port_qinq_role_t *mode);

/**
 * ppe_drv_vlan_del_xlate_rule
 *	Delete vlan translation rules.
 *
 * @datatypes
 * ppe_drv_iface
 * ppe_drv_vlan_xlate_info
 *
 * @param[in] iface  PPE interface for vlan device.
 * @param[in] ppe_drv_vlan_xlate_info	Translation info.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_del_xlate_rule(struct ppe_drv_iface *iface, struct ppe_drv_vlan_xlate_info *info);

/*
 * ppe_drv_vlan_over_bridge_del_ig_rule
 * 	Deleting ingress xlate rules for the given iface
 *
 * @datatypes
 * ppe_drv_iface
 *
 * @param[in] iface PPE interface of the slave
 * @param[in] iface PPE interface of the VLAN
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_over_bridge_del_ig_rule(struct ppe_drv_iface *slave_iface,
						   struct ppe_drv_iface *vlan_iface);
/*
 * ppe_drv_vlan_over_bridge_add_ig_rule
 * 	Installing ingress xlate rules for the given iface
 *
 * @datatypes
 * ppe_drv_iface
 *
 * @param[in] iface PPE interface of the slave
 * @param[in] iface PPE interface of the VLAN
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_over_bridge_add_ig_rule(struct ppe_drv_iface *slave_iface,
						   struct ppe_drv_iface *vlan_iface);
/**
 * ppe_drv_vlan_as_vp_del_xlate_rules
 *	Delete vlan translation rules with VP.
 *
 * @datatypes
 * ppe_drv_iface
 * ppe_drv_vlan_xlate_info
 *
 * @param[in] iface  PPE interface for vlan device.
 * @param[in] ppe_drv_vlan_xlate_info Translation info.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_as_vp_del_xlate_rules(struct ppe_drv_iface *iface, struct ppe_drv_vlan_xlate_info *info);

/**
 * ppe_drv_vlan_add_xlate_rule
 *	Add vlan translation rules.
 *
 * @datatypes
 * ppe_drv_iface
 * ppe_drv_vlan_xlate_info
 *
 * @param[in] iface  PPE interface for vlan device.
 * @param[in] ppe_drv_vlan_xlate_info Translation info.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_add_xlate_rule(struct ppe_drv_iface *iface, struct ppe_drv_vlan_xlate_info *info);

/**
 * ppe_drv_vlan_as_vp_add_xlate_rules
 *	Add vlan translation rules with VP.
 *
 * @datatypes
 * ppe_drv_iface
 * ppe_drv_vlan_xlate_info
 *
 * @param[in] iface  PPE interface for vlan device.
 * @param[in] ppe_drv_vlan_xlate_info Translation info.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_as_vp_add_xlate_rules(struct ppe_drv_iface *iface, struct ppe_drv_vlan_xlate_info *info);

/**
 * ppe_drv_vlan_fdb_learn_disable
 *	Configure VLAN based VSI FDB learning.
 *
 * @datatypes
 * ppe_drv_iface
 *
 * @param[in] iface  PPE interface for vlan device.
 * @param[in] vlan_fdb_learn_dis  FDB learning disable.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_fdb_learn_disable(struct ppe_drv_iface *ppe_iface, bool vlan_fdb_learn_dis);

/**
 * ppe_drv_vlan_deinit
 *	Deinitialize vlan.
 *
 * @datatypes
 * ppe_drv_iface
 *
 * @param[in] iface  PPE interface for vlan device.
 *
 * @return
 * Status of the operation.
 */
void ppe_drv_vlan_deinit(struct ppe_drv_iface *iface);

/**
 * ppe_drv_vlan_init
 *	Initialize vlan.
 *
 * @datatypes
 * ppe_drv_iface
 * net_device
 *
 * @param[in] iface  PPE interface for vlan device.
 * @param[in] base_dev  Base net device on which vlan is created.
 * @param[in] vlan_id  vlan_id.
 * @param[in] vlan_over_bridge VLAN interface is created over bridge
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_init(struct ppe_drv_iface *iface, struct net_device *base_dev, uint32_t vlan_id,
				bool vlan_over_bridge);

/**
 * ppe_drv_vlan_lag_slave_join
 *	slave dev inside lag join vlan.
 *
 * @datatypes
 * ppe_drv_iface
 * net_device
 *
 * @param[in] vlan_iface  PPE interface for vlan device.
 * @param[in] slave_dev  Slave net device
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_lag_slave_join(struct ppe_drv_iface *vlan_iface, struct net_device *slave_dev);

/**
 * ppe_drv_vlan_lag_slave_leave
 *	slave dev inside lag leave vlan.
 *
 * @datatypes
 * ppe_drv_iface
 * net_device
 *
 * @param[in] vlan_iface  PPE interface for vlan device.
 * @param[in] slave_dev  Slave net device
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_lag_slave_leave(struct ppe_drv_iface *vlan_iface, struct net_device *slave_dev);

/**
 * ppe_drv_vlan_wlanif_vp_tun_enc_ctx_attach
 *	Attach WLAN VP to the tunnel encap context.
 *
 * @datatypes
 * ppe_drv_tun_encap
 *
 * @param[in] ptec  PPE tunnel encap entry
 * @param[in] vp_num  VP number to attach to tunnel encap entry.
 *
 * @return
 * Status of the operation.
 */
bool ppe_drv_vlan_wlanif_vp_tun_enc_ctx_attach(struct ppe_drv_tun_encap *ptec, int16_t vp_num);

/**
 * ppe_drv_vlan_wlanif_vp_tun_enc_ctx_detach()
 *	Detach WLAN VP from tunnel encap context.
 *
 * @datatypes
 * ppe_drv_tun_encap
 *
 * @param[in] ptec  PPE tunnel encap entry
 * @param[in] vp_num  VP number to detach from tunnel encap context.
 *
 * @return
 * Status of the operation.
 */
bool ppe_drv_vlan_wlanif_vp_tun_enc_ctx_detach(struct ppe_drv_tun_encap *ptec, int16_t vp_num);

/**
 * ppe_drv_vlan_wlanif_vp_tun_enc_destroy()
 *	Destroy WLANIF tunnel encap context.
 *
 * @datatypes
 * ppe_drv_tun_encap
 *
 * @param[in] ptec  PPE tunnel encap entry
 */
void ppe_drv_vlan_wlanif_vp_tun_enc_destroy(struct ppe_drv_tun_encap *ptec);

/**
 * ppe_drv_vlan_wlanif_vp_tun_enc_setup()
 *	Allocate and configure WLANIF tunnel encap context.
 *
 * @datatypes
 * net_device
 *
 * @param[in] dev  WLAN realdev net device.
 * @param[in] vp_num  VP number associated with WLAN realdev.
 *
 * @return
 * A pointer to PPE tunnel encapsulation entry.
 */
struct ppe_drv_tun_encap *ppe_drv_vlan_wlanif_tun_enc_setup(struct net_device *dev, int16_t vp_num);

/**
 * ppe_drv_vlan_alloc()
 *      Allocate and initialize a VLAN context.
 *
 * @datatypes
 * ppe_drv_rule_dir_t
 *
 * @param[in] rule_dir  Direction of the rule (ingress or egress).
 *
 * @return
 * A pointer to the allocated VLAN context.
 */
struct ppe_drv_vlan_ctx *ppe_drv_vlan_alloc(ppe_drv_rule_dir_t rule_dir);

/**
 * ppe_drv_vlan_ctx_iface_get()
 *      Fetch the PPE interface associated with a VLAN context.
 *
 * @datatypes
 * ppe_drv_vlan_ctx
 *
 * @param[in] ctx  Pointer to the VLAN context.
 *
 * @return
 * PPE interface pointer for the VLAN context or NULL.
 */
struct ppe_drv_iface *ppe_drv_vlan_ctx_iface_get(struct ppe_drv_vlan_ctx *ctx);

/**
 * ppe_drv_vlan_rule_create()
 *      Create a VLAN rule based on the provided configuration.
 *
 * @datatypes
 * ppe_drv_vlan_ctx
 * ppe_drv_vlan_cfg
 *
 * @param[in] ctx  Pointer to the VLAN context.
 * @param[in] rule  Pointer to the VLAN rule configuration.
 *
 * @return
 * Status of the rule creation operation.
 */
ppe_drv_ret_t  ppe_drv_vlan_rule_create(struct ppe_drv_vlan_ctx *ctx, struct ppe_drv_vlan_cfg *rule);

/**
 * ppe_drv_vlan_destroy()
 *      Destroy a VLAN rule based on the provided configuration.
 *
 * @datatypes
 * ppe_drv_vlan_ctx
 *
 * @param[in] ctx  Pointer to the VLAN context.
 *
 * @return
 * Status of the rule destroy operation.
 */
void ppe_drv_vlan_destroy(struct ppe_drv_vlan_ctx *ctx);

/**
 * ppe_drv_vlan_as_veip_add_xlate_rules
 *      Add vlan translation rules with VP.
 *
 * @datatypes
 * ppe_drv_iface
 * ppe_drv_vlan_xlate_info
 *
 * @param[in] iface  PPE interface for vlan device.
 * @param[in] ppe_drv_vlan_xlate_info Translation info.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_as_veip_add_xlate_rules(struct ppe_drv_iface *iface, struct ppe_drv_vlan_xlate_info *info);

/**
 * ppe_drv_vlan_as_veip_del_xlate_rules
 *      Delete vlan translation rules with VP.
 *
 * @datatypes
 * ppe_drv_iface
 * ppe_drv_vlan_xlate_info
 *
 * @param[in] iface  PPE interface for vlan device.
 * @param[in] ppe_drv_vlan_xlate_info Translation info.
 *
 * @return
 * Status of the operation.
 */
ppe_drv_ret_t ppe_drv_vlan_as_veip_del_xlate_rules(struct ppe_drv_iface *iface, struct ppe_drv_vlan_xlate_info *info);

/**
 * ppe_drv_vlan_hgu_rule_create - Create VLAN rules for HGU mode.
 *
 * This API programs VLAN rules required for HGU (Home Gateway Unit) operation.
 * Based on the provided configuration, the function installs necessary PPE rules
 * for upstream/downstream VLAN handling, classification, and tagging behavior.
 *
 * @datatypes
 *  ppe_drv_vlan_cfg
 *  ppe_drv_vlan_ctx
 *
 * @param[in]  info   VLAN configuration describing rule parameters.
 * @param[out] ctx    Context to store internal rule identifiers for later deletion.
 *
 * @return
 *  ppe_drv_ret_t     Status of rule creation.
 *                    PPE_DRV_RET_SUCCESS on success.
 *                    Appropriate error code on failure.
 */
ppe_drv_ret_t ppe_drv_vlan_hgu_rule_create(struct ppe_drv_vlan_cfg *info, struct ppe_drv_vlan_ctx *ctx);

/**
 * ppe_drv_vlan_hgu_rule_destroy - Remove previously created HGU VLAN rules.
 *
 * This API deletes VLAN rules created via ppe_drv_vlan_hgu_rule_create().
 * It uses the stored rule context to clean up all associated PPE rule entries,
 * ensuring proper rollback of HGU VLAN configuration.
 *
 * @datatypes
 *  ppe_drv_vlan_ctx
 *
 * @param[in] ctx   Context containing rule identifiers to remove.
 *
 * @return
 *  ppe_drv_ret_t   Status of rule deletion.
 *                  PPE_DRV_RET_SUCCESS on success.
 *                  Appropriate error code on failure.
 */
ppe_drv_ret_t ppe_drv_vlan_hgu_rule_destroy(struct ppe_drv_vlan_ctx *ctx);

/**
 * ppe_drv_vlan_dsa_cfg_set
 *      Map VID to VP for ingress and egress; program global private TPID on first DSA interface.
 *
 * @param[in] port_id   VP port number of the DSA interface.
 * @param[in] tpid      Private TPID to program (e.g. PPE_DRV_DSA_QINQ_PRIVATE_TPID).
 * @param[in] vid       VLAN ID to map to this port.
 *
 * @return
 *  ppe_drv_ret_t   Status of DSA VLAN configuration.
 *                  PPE_DRV_RET_SUCCESS on success.
 *                  Appropriate error code on failure.
 */
ppe_drv_ret_t ppe_drv_vlan_dsa_cfg_set(uint32_t port_id, uint32_t tpid, uint32_t vid);

/**
 * ppe_drv_vlan_dsa_cfg_reset
 *      Clear VID-to-VP mapping for this interface; clear global private TPID on last DSA interface removal.
 *
 * @param[in] port_id   VP port number of the DSA interface.
 * @param[in] vid       VLAN ID that was mapped to this port.
 *
 * @return
 *  0 on success, -1 on failure.
 */
int ppe_drv_vlan_dsa_cfg_reset(uint32_t port_id, uint32_t vid);

/**
 * ppe_drv_vlan_dsa_vp_egress_map_set
 *      Set egress private VLAN mapping (VP -> VID) for a VP created for a VLAN
 *      over a DSA slave (e.g. lan3.10 or lan3.10.20).
 *
 * @param[in] port_id   VP port number allocated for the VLAN-over-DSA interface.
 * @param[in] vid       VLAN ID (from the base DSA interface) to map to this VP.
 *
 * @return
 *  ppe_drv_ret_t   Status of DSA VP egress map configuration.
 *                  PPE_DRV_RET_SUCCESS on success.
 *                  Appropriate error code on failure.
 */
ppe_drv_ret_t ppe_drv_vlan_dsa_vp_egress_map_set(uint32_t port_id, uint32_t vid);

/**
 * ppe_drv_vlan_dsa_vp_egress_map_reset
 *      Clear egress private VLAN mapping for a VP created for a VLAN over a DSA slave.
 *
 * @param[in] port_id   VP port number allocated for the VLAN-over-DSA interface.
 * @param[in] vid       VLAN ID that was mapped at set time.
 *
 * @return
 *  0 on success, -1 on failure.
 */
int ppe_drv_vlan_dsa_vp_egress_map_reset(uint32_t port_id, uint32_t vid);

#endif /* _PPE_DRV_VLAN_H_ */
