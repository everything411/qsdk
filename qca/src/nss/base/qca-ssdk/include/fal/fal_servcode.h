/*
 * Copyright (c) 2016-2018, 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @defgroup fal_servcode FAL_SERVCODE
 * @{
 */
#ifndef _FAL_SERVCODE_H_
#define _FAL_SERVCODE_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"
#include "fal_athtag.h"

#if defined(SW_API_LOCK) && (!defined(HSL_STANDALONG))
#define FAL_SERVCODE_API_LOCK FAL_API_LOCK
#define FAL_SERVCODE_API_UNLOCK FAL_API_UNLOCK
#else
#define FAL_SERVCODE_API_LOCK
#define FAL_SERVCODE_API_UNLOCK
#endif

#define SERVICE_BYP_NUM 4
#define FAL_SERVCODE_INVALID 0xffff

/* field_update_bitmap in EG_SERVICE_TBL */
enum {
	/* 32bit FIELD_UPDATE_ACTION field */
	FLD_UPDATE_CAPWAP_EN = 0, /*only for IP197*/
	FLD_UPDATE_DIRECTION, /*only for IP197*/
	FLD_UPDATE_DEST_INFO,
	FLD_UPDATE_SRC_INFO,
	FLD_UPDATE_INT_PRI,
	FLD_UPDATE_SERVICE_CODE,
	FLD_UPDATE_HASH_FLOW_INDEX,
	FLD_UPDATE_FAKE_L2_PROT_EN, /*only for IP197*/
	/* new add for IPQ95xx */
	FLD_UPDATE_EIP_FLOW_LABEL = 8, /*only for IP197*/
	FLD_UPDATE_ALLOWPADDING,       /*only for IP197*/
	FLD_UPDATE_STRIPPADDING,       /*only for IP197*/
	FLD_UPDATE_KEEPOUTER,          /*only for IP197*/
	FLD_UPDATE_L4CHECKSUM,         /*only for IP197*/
	FLD_UPDATE_IPV4CHECKSUM,       /*only for IP197*/
	FLD_UPDATE_ENCLASTDEST,        /*only for IP197*/
	FLD_UPDATE_WIFI_QOS,
	FLD_UPDATE_TREE_QOS,
	FLD_UPDATE_FLOW_IDX,
	FLD_UPDATE_CONTEXT_TYPE0 = 18, /*new add for IPQ53xx*/
	FLD_UPDATE_CONTEXT_TYPE1, /*new add for IPQ53xx*/
	FLD_UPDATE_FLOW_COOKIE_DIS, /*new add for IPQ53xx*/
	FLD_UPDATE_DECAP_DIS, /*new add for IPQ54xx*/
	FLD_UPDATE_ENCAP_DIS, /*new add for IPQ54xx*/
	FLD_UPDATE_SRC_INFO_BYPASS = 24,
	FLD_UPDATE_DST_INFO_BYPASS,
	FLD_UPDATE_MAC_HDR_BYPASS,
	FLD_UPDATE_FAKE_MAC_CLEAR,

	/* New add 24bit FIELD_UPDATE_ACTION_EXT field for IPQ96xx */
	FLD_UPDATE_VLAN_XLT_DROP_BYPASS = 32,
	FLD_UPDATE_CPU_EDIT_EN,
	FLD_UP_SRC_WITH_DST,
	EPE_PASSTHROUGH_SERV_CODE_SEL_BIT0,
	FLD_UP_PPPOE_BYPASS,
	FLD_UP_TTL_BYPASS,
	FLD_UP_DSCP_BYPASS,
	FLD_UP_SIP_BYPASS,
	FLD_UP_DIP_BYPASS,
	FLD_UP_L4_PORT_BYPASS,
	EPE_VLAN_XLT_BYPASS,
	EPE_VLAN_FMT_BYPASS,
	EPE_LOOPBACK_VLAN_START,
	EPE_LOOPBACK_VLAN_END_BYPASS,
	EPE_PASSTHROUGH_SERV_CODE_SEL_BIT1,
	FLD_UP_DST_INFO_WITH_GEM,
	FLD_UP_DST_INFO_FORCE,
	EPE_PRE_HEADER_VALID,
	EPE_DST_INFO_WITH_PROFILE,
	FLD_UP_DDRQ_FLOW_INDEX,
	FLD_UP_SRC_INFO_GEM_BYPASS,
	FLD_UP_MACSEC_OFFSET_SEL,
	FLD_UP_SERVICE_CODE_DDRQ,
	FLD_UP_DDRQ_DIRECT_ENQ,

	/* New add 16bit FIELD_UPDATE_ACTION_EXT1 field for IPQ96xx */
	FLD_UP_EG_CNT_VSI_BYPASS = 56,
	FLD_UP_EG_CNT_VLAN_DEV_BYPASS,
	FLD_UP_EG_CNT_VP_BYPASS,
	FLD_UP_EG_CNT_PM_BYPASS,
	FLD_UP_DDRQ_VLD_SC_SEL_BIT0,
	FLD_UP_DDRQ_VLD_SC_SEL_BIT1,
	FLD_UP_SPEC_SC_CONVERT,

	FLD_UP_MAX = 72,
};

/* athtag bit location in field_update_bitmap */
#define ATHTAG_INSERT_UPDATE		BIT(23)
#define ATHTAG_ACTION_UPDATE		BIT(28)
#define ATHTAG_BYPASS_FWD_EN_UPDATE	BIT(29)
#define ATHTAG_DEST_PORT_UPDATE		BIT(30)
#define ATHTAG_FIELD_DISABLE_UPDATE	BIT(31)
#define ATHTAG_UPDATE			(GENMASK(31, 28) | BIT(23))

/* athtag field update bitmap, new add for IPQ53xx */
enum {
	FLD_UPDATE_ATH_TAG_INSERT, /*update insert athtag or not*/
	FLD_UPDATE_ATH_TAG_ACTION, /*update athtag action*/
	FLD_UPDATE_ATH_TAG_BYPASS_FWD_EN, /*update bypass fwd en*/
	FLD_UPDATE_ATH_TAG_DEST_PORT, /*update dest portid or portmap*/
	FLD_UPDATE_ATH_TAG_FIELD_DISABLE, /*update field disable*/
};

/* bypss_bitmap_0: bypass bitmap in IN_SERVICE_TBL */
enum {
	IN_VLAN_TAG_FMT_CHECK_BYP = 0,
	IN_VLAN_MEMBER_CHECK_BYP,
	IN_VLAN_XLT_BYP,
	MY_MAC_CHECK_BYP,
	DIP_LOOKUP_BYP,
	FLOW_LOOKUP_BYP = 5,
	FLOW_ACTION_BYP,
	ACL_BYP,
	FAKE_MAC_HEADER_BYP,
	SERVICE_CODE_BYP,
	WRONG_PKT_FMT_L2_BYP = 10,
	WRONG_PKT_FMT_L3_IPV4_BYP,
	WRONG_PKT_FMT_L3_IPV6_BYP,
	WRONG_PKT_FMT_L4_BYP,
	FLOW_SERVICE_CODE_BYP,
	ACL_SERVICE_CODE_BYP = 15,
	FAKE_L2_PROTO_BYP,
	PPPOE_TERMINATION_BYP,
	DEFAULT_VLAN_BYP,
	DEFAULT_PCP_BYP, /* new add for IPQ95xx */
	VSI_ASSIGN_BYP, /* new add for IPQ95xx */
	WRONG_PKT_FMT_TUNNEL_BYP,
	L3_MY_MAC_DST_BYP, /* new add for IPQ96xx */
	WRONG_PKT_FMT_L4_CSM_BYP, /* new add for IPQ96xx */
	IN_VLAN_ASSIGN_FAIL_BYP = 24,
	SOURCE_GUARD_BYP,
	MRU_MTU_CHECK_BYP,
	FLOW_SRC_CHECK_BYP,
	FLOW_QOS_BYP,
	/* New add for IPQ96xx */
	L3_ROUTING_BYP,
	PASSTHROUGH_SERV_CODE_SEL_BIT0,
	PASSTHROUGH_SERV_CODE_SEL_BIT1 = 31,
};

/* bypss_bitmap_1: bypass bitmap in IN_L2_SERVICE_TBL */
enum {
	/* 24bit BYPASS_BITMAP field */
	EG_VLAN_MEMBER_CHECK_BYP = 0,
	EG_VLAN_XLT_BYP,
	EG_VLAN_TAG_FMT_CTRL_BYP,
	FDB_LEARN_BYP,
	FDB_REFRESH_BYP,
	L2_SOURCE_SEC_BYP = 5,
	MANAGEMENT_FWD_BYP,
	BRIDGING_FWD_BYP,
	IN_STP_FLTR_BYP,
	EG_STP_FLTR_BYP,
	SOURCE_FLTR_BYP = 10,
	POLICER_BYP,
	L2_PKT_EDIT_BYP,
	L3_PKT_EDIT_BYP,
	ACL_POST_ROUTING_CHECK_BYP,
	PORT_ISOLATION_BYP = 15,
	/* new add Qos bypass for IPQ60xx*/
	PRE_ACL_QOS_BYP,
	POST_ACL_QOS_BYP,
	DSCP_QOS_BYP,
	PCP_QOS_BYP,
	PREHEADER_QOS_BYP = 20,
	/* new add for IPQ95xx */
	FAKE_MAC_DROP_BYP,
	TUNL_CONTEXT_BYP,
	/* new add for IPQ53xx */
	FLOW_POLICER_BYP,

	/* 8bit BYPASS_BITMAP_EXT field */
	/* new add for IPQ54xx */
	SMAC_MC_DROP_BYP = 24,
	L2_FLOODING_BYP,
	TUNNEL_EXP_CNT_CTRL,
	/* new add for IPQ96xx */
	VLAN_PORT_ISOLATION_BYP,
	L2_VP_SERVICE_CODE_ENQ_BYP,
	DOT1P_VLAN_FROM_LOOPBACK,
	DOT1P_MAPPER_BYP,
	IPMC_FWD_BYP = 31,

	/* 16bit BYPASS_BITMAP_NEW field */
	DOT1P_EG_PORT_OVERRIDE = 32,
	DOT1P_UNI_LOOKUP_REVERSE,
	DOT1P_DST_LOOKUP_BYPASS,
	L2_PASSTHROUGH_SERV_CODE_SEL_BIT0,
	L2_PASSTHROUGH_SERV_CODE_SEL_BIT1,
	SERVICE_HIGH_PRIORITY,
	L2_PASSTHROUGH_SERV_CODE_SEL_BIT2,
	L2_FLOW_QOS_BYP,
	IN_MIRROR_BYP,
	EG_MIRROR_BYP,

	/* 8bit POST_BYPASS_BITMAP field */
	QM_QID_MISMATCH_BYPASS = 48,
	QM_DDRQ_ID_GEN_BYPASS,
	QM_DDRQ_ID_GEN_FORCE,
	SAWF_BYP,
	QM_PASSTHROUGH_CPU_CODE_0_SEL,
	QM_PASSTHROUGH_CPU_CODE_1_SEL,
	DROP_CPUCODE_CNT_BYP,
};

/* bypss_bitmap_2 */
enum {
	RX_VLAN_COUNTER_BYP = 0,
	RX_COUNTER_BYP,
	TX_VLAN_COUNTER_BYP,
	TX_COUNTER_BYP,
};

/* bypass_bitmap_3, new add for IPQ95xx */
enum {
	TL_SERVICE_CODE_BYP = 0,
	TL_BYP,
	TL_L3_IF_CHECK_BYP,
	TL_VLAN_CHECK_BYP,
	TL_DMAC_CHECK_BYP,
	TL_UDP_CSUM_0_CHECK_BYP = 5,
	TL_TBL_DE_ACCE_CHECK_BYP,
	TL_PPPOE_MC_TERM_CHECK_BYP,
	TL_TTL_EXCEED_CHECK_BYP,
	TL_MAP_SRC_CHECK_BYP,
	TL_MAP_DST_CHECK_BYP = 10,
	TL_LPM_DST_LOOKUP_BYP,
	TL_LPM_LOOKUP_BYP,
	TL_WRONG_PKT_FMT_L2_BYP,
	TL_WRONG_PKT_FMT_L3_IPV4_BYP,
	TL_WRONG_PKT_FMT_L3_IPV6_BYP = 15,
	TL_WRONG_PKT_FMT_L4_BYP,
	TL_WRONG_PKT_FMT_TUNNEL_BYP,
	TL_WRONG_PKT_FMT_L4_CSM_BYP, /* new add for IPQ96xx */
	TL_PASSTHROUGH_SERV_CODE_SEL_BIT0, /* new add for IPQ96xx */
	TL_PRE_IPO_BYP = 20,
	TL_PASSTHROUGH_SERV_CODE_SEL_BIT1, /* new add for IPQ96xx */
	TL_PASSTHROUGH_SERV_CODE_SEL_BIT2, /* new add for IPQ96xx */
	TL_INNER_IP_PADDING_EXP_BYP,
};

#define FAL_SERVCODE_TYPE_NORMAL		0
#define FAL_SERVCODE_TYPE_PASSTHROUGH		1
#define FAL_SERVCODE_TYPE(servcode_index)	(((servcode_index)>>24)&0xff)
#define FAL_SERVCODE_VALUE(servcode_index)	((servcode_index)&0xffffff)
#define FAL_SERVCODE(type, value)		(((type)<<24)|(value))

typedef struct {
	a_bool_t dest_port_valid; /* dest_port_id valid or not, passthrough servcode not support */
	fal_port_t dest_port_id; /* dest physical port id, passthrough servcode not support */
	a_uint64_t  bypass_bitmap[SERVICE_BYP_NUM]; /* refer to enum IN_VLAN_TAG_FMT_CHECK_BYP... */
	a_uint32_t  direction; /* if dest is vp, fill it in dest_info or src_info, 0:dest, 1:src
				* passthrough servcode not support
				*/
	a_uint64_t field_update_bitmap[BITS_TO_U64(FLD_UP_MAX)]; /* refer to enum FLD_UPDATE_CAPWAP_EN... */
	a_uint32_t  next_service_code; /* next service code, passthrough servcode not support */
	a_uint32_t  hw_services; /* HW_SERVICES to IP-197, passthrough servcode not support */
	a_uint32_t  offset_sel; /* Select the offset value to IP-197:0: l3_offset, 1:l4_offset
				 * passthrough servcode not support
				 */
} fal_servcode_config_t;

typedef struct {
	a_uint32_t athtag_update_bitmap;
	a_bool_t athtag_en; /*updated athtag_en*/
	fal_athtag_action_t action; /*updated action field*/
	a_bool_t bypass_fwd_en; /*updated bypass fwd en field*/
	a_bool_t field_disable; /*updated ver3 field disable*/
	a_uint8_t dest_port; /*updated dest portid or portmap field*/
} fal_servcode_athtag_t;

sw_error_t
fal_servcode_config_set(a_uint32_t dev_id, a_uint32_t servcode_index, fal_servcode_config_t *entry);
sw_error_t
fal_servcode_config_get(a_uint32_t dev_id, a_uint32_t servcode_index, fal_servcode_config_t *entry);
sw_error_t fal_servcode_loopcheck_en(a_uint32_t dev_id, a_bool_t enable);
sw_error_t fal_servcode_loopcheck_status_get(a_uint32_t dev_id, a_bool_t *enable);
sw_error_t fal_port_servcode_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t servcode_index);
sw_error_t fal_port_servcode_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t *servcode_index);
sw_error_t
fal_servcode_athtag_set(a_uint32_t dev_id, a_uint32_t servcode_index, fal_servcode_athtag_t *entry);
sw_error_t
fal_servcode_athtag_get(a_uint32_t dev_id, a_uint32_t servcode_index, fal_servcode_athtag_t *entry);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_SERVCODE_H_ */
/**
 * @}
 */

