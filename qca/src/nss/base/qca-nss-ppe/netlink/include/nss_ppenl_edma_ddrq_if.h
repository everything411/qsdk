/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file nss_ppenl_edma_ddrq_if.h
 *	NSS EDMA DDRQ Netlink
 */
#ifndef __NSS_PPENL_EDMA_DDRQ_IF_H
#define __NSS_PPENL_EDMA_DDRQ_IF_H

#include <nss_dp_ddrq.h>

/*
 * EDMA DDRQ Configure Family
 */
#define NSS_PPENL_EDMA_DDRQ_FAMILY	"nss_ppenl_ddrq"

#define NSS_PPENL_EDMA_DDRQ_CFG_GET_CNT_SINGLE		1

/**
 * @brief Per DDRQ based configuration
 */
struct nss_ppenl_edma_ddrq_config {
	nss_dp_ddrq_obj_id_t ddrq_obj;			/** DDRQ object */
	nss_dp_ddrq_ac_queue_cfg_tbl_t ddrq_ac_cfg;	/** DDRQ AC configuration */
	bool get_cmd;					/** Knob for getting the current configuration */
	int ret;					/** Return value to userspace */
};

/**
 * @brief DDRQ group configuration
 */
struct nss_ppenl_edma_ddrq_grp_config {
	int8_t grp_id;					/** Group id */
	nss_dp_ddrq_ac_grp_cfg_tbl_t ddrq_ac_grp_cfg;	/** base DDRQ group configuration */
	bool get_cmd;					/** Knob for getting the current configuration */
	int ret;					/** Return value to userspace */
};

/**
 * @brief EDMA rule
 */
struct nss_ppenl_edma_ddrq_rule {
	struct nss_ppenl_cmn cm;				/** Common message header */
	union {
		struct nss_ppenl_edma_ddrq_config ddrq_cfg;		/** Per DDRQ configuration */
		struct nss_ppenl_edma_ddrq_grp_config ddrq_grp_cfg;	/** DDRQ group configuration */
	} msg;
};

/**
 * @brief EDMA DDRQ Message types
 */
enum nss_ppe_edma_ddrq_message_types {
	NSS_PPE_EDMA_DDRQ_CFG_RULE_MSG,			/** EDMA DDRQ configuration message */
	NSS_PPE_EDMA_DDRQ_GRP_CFG_RULE_MSG,		/** EDMA DDRQ group configuration message */
	NSS_PPE_EDMA_DDRQ_MAX_MSG_TYPES,		/** Maximum message type */
};

/**
 * @brief Netlink EDMA DDRQ message init
 *
 * @param rule[IN] NSS NETLINK EDMA DDRQ rule
 * @param type[IN] EDMA DDRQ message type
 */
static inline void nss_ppenl_edma_ddrq_rule_init(struct nss_ppenl_edma_ddrq_rule *rule, enum nss_ppe_edma_ddrq_message_types type)
{
	nss_ppenl_cmn_set_ver(&rule->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&rule->cm, sizeof(struct nss_ppenl_edma_ddrq_rule), type);
}


#endif /* __NSS_PPENL_EDMA_DDRQ_IF_H */
