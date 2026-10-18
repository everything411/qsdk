/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_edma_ddrq.c
 * NSS Netlink EDMA DDRQ Handler
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/if.h>
#include <linux/in.h>
#include <linux/netlink.h>
#include <linux/rcupdate.h>
#include <linux/etherdevice.h>
#include <linux/if_addr.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/if_vlan.h>
#include <linux/completion.h>
#include <linux/semaphore.h>
#include <linux/in.h>

#include <net/arp.h>
#include <net/genetlink.h>
#include <net/neighbour.h>
#include <net/net_namespace.h>
#include <net/route.h>
#include <net/sock.h>

#include <nss_ppenl_cmn_if.h>
#include <nss_ppenl_edma_ddrq_if.h>
#include "nss_ppenl.h"
#include "nss_ppenl_edma_ddrq.h"
#include <nss_dp_ddrq.h>

/*
 * Function prototypes
 */
static int nss_ppenl_edma_ddrq_ops_cfg_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_edma_ddrq_ops_grp_cfg_rule(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_edma_ddrq_ops[] = {
	{.cmd = NSS_PPE_EDMA_DDRQ_CFG_RULE_MSG, .doit = nss_ppenl_edma_ddrq_ops_cfg_rule,},
	{.cmd = NSS_PPE_EDMA_DDRQ_GRP_CFG_RULE_MSG, .doit = nss_ppenl_edma_ddrq_ops_grp_cfg_rule,},

};

/*
 * EDMA DDRQ family definition
 */
static struct genl_family nss_ppenl_edma_ddrq_family = {
	.name = NSS_PPENL_EDMA_DDRQ_FAMILY,		/* family name string */
	.hdrsize = sizeof(struct nss_ppenl_edma_ddrq_rule),	/* NSS NETLINK DDRQ rule */
	.version = NSS_PPENL_VER,			/* Set it to NSS_PPENL_VER version */
	.maxattr = NSS_PPE_EDMA_DDRQ_MAX_MSG_TYPES,		/* maximum commands supported */
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_ppenl_edma_ddrq_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_edma_ddrq_ops),
};

/*
 * nss_ppenl_edma_ddrq_ops_cfg_rule()
 *	API to handle EDMA DDRQ configuration rule
 */
static int nss_ppenl_edma_ddrq_ops_cfg_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_edma_ddrq_rule *nl_edma_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	nss_dp_ddrq_ret_t ret = DDRQ_RET_ERR;
	struct nss_ppenl_edma_ddrq_config * ddrq_cfg;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_edma_ddrq_family, info, NSS_PPE_EDMA_DDRQ_CFG_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract EDMA DDRQ config data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_edma_rule = container_of(nl_cm, struct nss_ppenl_edma_ddrq_rule, cm);
	ddrq_cfg = &nl_edma_rule->msg.ddrq_cfg;
	pid = nl_cm->pid;

	if (ddrq_cfg->ddrq_obj.cfg_type == NSS_DP_DDRQ_CFG_TYPE_NONE) {
		nss_ppenl_warn("Neither port id or the queue id is provided to extract EDMA DDRQ configuration\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Check whether the DDRQ configurations are required to be retrieved or not
	 */
	if (ddrq_cfg->get_cmd) {
		nss_dp_ddrq_ac_queue_cfg_tbl_t ddrq_ac_queue_cfg = {0};

		/*
		 * copy the NL message for response
		 */
		resp = nss_ppenl_copy_msg(skb);
		if (!resp) {
			nss_ppenl_warn("%d:unable to save response data from NL buffer\n", pid);
			nss_ppenl_ucast_resp(skb);
			return -ENOMEM;
		}

		/*
		 * Get DDRQ configuration from NSS DP module
		 */
		ret = nss_dp_ddrq_cfg_get(&ddrq_cfg->ddrq_obj, &ddrq_ac_queue_cfg, NSS_PPENL_EDMA_DDRQ_CFG_GET_CNT_SINGLE);
		if (ret == DDRQ_RET_SUCCESS) {
			nss_ppenl_trace("Configuration dump for DDRQ[%d]:\n", ddrq_cfg->ddrq_obj.cfg_id);
			nss_ppenl_trace("ac_cfg_gap_grn_grn_min: %d\n", ddrq_ac_queue_cfg.ac_cfg_gap_grn_grn_min);
			nss_ppenl_trace("ac_cfg_gap_grn_red_max: %d\n", ddrq_ac_queue_cfg.ac_cfg_gap_grn_red_max);
			nss_ppenl_trace("ac_cfg_gap_grn_red_min: %d\n", ddrq_ac_queue_cfg.ac_cfg_gap_grn_red_min);
			nss_ppenl_trace("ac_cfg_gap_grn_yel_max: %d\n", ddrq_ac_queue_cfg.ac_cfg_gap_grn_yel_max);
			nss_ppenl_trace("ac_cfg_gap_grn_yel_min: %d\n", ddrq_ac_queue_cfg.ac_cfg_gap_grn_yel_min);
			nss_ppenl_trace("ac_cfg_grn_resume_offset: %d\n", ddrq_ac_queue_cfg.ac_cfg_grn_resume_offset);
			nss_ppenl_trace("ac_cfg_pre_alloc_limit: %d\n", ddrq_ac_queue_cfg.ac_cfg_pre_alloc_limit);
			nss_ppenl_trace("ac_cfg_red_resume_offset: %d\n", ddrq_ac_queue_cfg.ac_cfg_red_resume_offset);
			nss_ppenl_trace("ac_cfg_shared_ceiling: %d\n", ddrq_ac_queue_cfg.ac_cfg_shared_ceiling);
			nss_ppenl_trace("ac_cfg_yel_resume_offset: %d\n", ddrq_ac_queue_cfg.ac_cfg_yel_resume_offset);
			nss_ppenl_trace("ac_cfg_ac_en: %d\n", ddrq_ac_queue_cfg.ac_cfg_ac_en);
			nss_ppenl_trace("ac_cfg_bp_en: %d\n", ddrq_ac_queue_cfg.ac_cfg_bp_en);
			nss_ppenl_trace("ac_cfg_color_aware: %d\n", ddrq_ac_queue_cfg.ac_cfg_color_aware);
			nss_ppenl_trace("ac_cfg_ecn_mark_en: %d\n", ddrq_ac_queue_cfg.ac_cfg_ecn_mark_en);
			nss_ppenl_trace("ac_cfg_grp_id: %d\n", ddrq_ac_queue_cfg.ac_cfg_grp_id);
			nss_ppenl_trace("ac_cfg_shared_dynamic: %d\n", ddrq_ac_queue_cfg.ac_cfg_shared_dynamic);
			nss_ppenl_trace("ac_cfg_shared_weight: %d\n", ddrq_ac_queue_cfg.ac_cfg_shared_weight);
			nss_ppenl_trace("ac_cfg_wred_en: %d\n", ddrq_ac_queue_cfg.ac_cfg_wred_en);
			nss_ppenl_trace("ddrq enable state: %d\n", ddrq_ac_queue_cfg.ddrq_state);
			nss_ppenl_trace("EDMA DDRQ configuration get operation completed\n");
		} else {
			nss_ppenl_warn("%d: Error in getting the DDRQ configuration\n", pid);
		}

		nl_edma_rule = nss_ppenl_get_data(resp);
		nl_edma_rule->msg.ddrq_cfg.ret = ret;
		nss_ppenl_ucast_resp(resp);
		return 0;
	}

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	/*
	 * Set DDRQ configuration from NSS DP module
	 */
	ret = nss_dp_ddrq_cfg_set(&ddrq_cfg->ddrq_obj, &ddrq_cfg->ddrq_ac_cfg);
	if (ret != DDRQ_RET_SUCCESS) {
		nss_ppenl_warn("%d:Error in setting the DDRQ configuration set operation\n", pid);
	} else {
		nss_ppenl_info("EDMA DDRQ cfg set operation completed\n");
	}

	nl_edma_rule = nss_ppenl_get_data(resp);
	nl_edma_rule->msg.ddrq_cfg.ret = ret;
	nss_ppenl_ucast_resp(resp);
	nss_ppenl_info("EDMA DDRQ cfg set configuration completed\n");
	return 0;
}

/*
 * nss_ppenl_edma_ddrq_ops_grp_cfg_rule()
 *	API to handle EDMA DDRQ group configuration rule
 */
static int nss_ppenl_edma_ddrq_ops_grp_cfg_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_edma_ddrq_rule *nl_edma_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	nss_dp_ddrq_ret_t ret = DDRQ_RET_ERR;
	struct nss_ppenl_edma_ddrq_grp_config *ddrq_grp_cfg;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_edma_ddrq_family, info, NSS_PPE_EDMA_DDRQ_GRP_CFG_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract EDMA DDRQ GRP config data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_edma_rule = container_of(nl_cm, struct nss_ppenl_edma_ddrq_rule, cm);
	ddrq_grp_cfg = &nl_edma_rule->msg.ddrq_grp_cfg;
	pid = nl_cm->pid;

	if (ddrq_grp_cfg->grp_id == NSS_DP_DDRQ_INV_VAL) {
		nss_ppenl_warn("DDRQ group id for the configuration handling is not provided\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Check whether the DDRQ group configurations are required to be retrieved or not
	 */
	if (ddrq_grp_cfg->get_cmd) {
		nss_dp_ddrq_ac_grp_cfg_tbl_t ddrq_ac_grp_cfg = {0};

		/*
		 * copy the NL message for response
		 */
		resp = nss_ppenl_copy_msg(skb);
		if (!resp) {
			nss_ppenl_warn("%d:unable to save response data from NL buffer\n", pid);
			nss_ppenl_ucast_resp(skb);
			return -ENOMEM;
		}

		/*
		 * Get DDRQ group configuration from the NSS DP module
		 */
		ret = nss_dp_ddrq_grp_cfg_get(ddrq_grp_cfg->grp_id, &ddrq_ac_grp_cfg);
		if (ret == DDRQ_RET_SUCCESS) {
			nss_ppenl_trace("DDRQ group Configuration dump for DDRQ group[%d]:\n", ddrq_grp_cfg->grp_id);
			nss_ppenl_trace("ac_grp_dp_thrd: %d\n", ddrq_ac_grp_cfg.ac_grp_dp_thrd);
			nss_ppenl_trace("ac_grp_gap_grn_red: %d\n", ddrq_ac_grp_cfg.ac_grp_gap_grn_red);
			nss_ppenl_trace("ac_grp_gap_grn_yel: %d\n", ddrq_ac_grp_cfg.ac_grp_gap_grn_yel);
			nss_ppenl_trace("ac_grp_grn_resume_offset: %d\n", ddrq_ac_grp_cfg.ac_grp_grn_resume_offset);
			nss_ppenl_trace("ac_grp_red_resume_offset: %d\n", ddrq_ac_grp_cfg.ac_grp_red_resume_offset);
			nss_ppenl_trace("ac_grp_gap_shrd_limit: %d\n", ddrq_ac_grp_cfg.ac_grp_gap_shrd_limit);
			nss_ppenl_trace("ac_grp_yel_resume_offset: %d\n", ddrq_ac_grp_cfg.ac_grp_yel_resume_offset);
			nss_ppenl_trace("ac_cfg_ac_en: %d\n", ddrq_ac_grp_cfg.ac_cfg_ac_en);
			nss_ppenl_trace("ac_cfg_color_aware: %d\n", ddrq_ac_grp_cfg.ac_cfg_color_aware);
			nss_ppenl_trace("EDMA DDRQ group configuration get operation completed\n");
		} else {
			nss_ppenl_warn("%d:Error in getting the DDRQ group[%d] configuration operation\n", pid,
						 ddrq_grp_cfg->grp_id);
		}

		nl_edma_rule = nss_ppenl_get_data(resp);
		nl_edma_rule->msg.ddrq_grp_cfg.ret = ret;
		nss_ppenl_ucast_resp(resp);
		return 0;
	}

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	/*
	 * Set the DDRQ group configuration from the NSS DP module
	 */
	ret = nss_dp_ddrq_grp_cfg_set(ddrq_grp_cfg->grp_id, &ddrq_grp_cfg->ddrq_ac_grp_cfg);
	if (ret != DDRQ_RET_SUCCESS) {
		nss_ppenl_warn("%d:Error in setting the DDRQ group configuration operation\n", pid);
	} else {
		nss_ppenl_info("EDMA DDRQ group configuration set operation completed\n");
	}

	nl_edma_rule = nss_ppenl_get_data(resp);
	nl_edma_rule->msg.ddrq_grp_cfg.ret = ret;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_edma_ddrq_init()
 *	EDMA DDRQ init handler
 */
bool nss_ppenl_edma_ddrq_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE EDMA DDRQ handler\n");

	/*
	 * Register NETLINK ops with the family
	 */
	error = genl_register_family(&nss_ppenl_edma_ddrq_family);
	if (error != 0) {
		nss_ppenl_info_always("Error: unable to register EDMA DDRQ family\n");
		return false;
	}

	return true;
}

/*
 * nss_ppenl_edma_ddrq_exit()
 *	EDMA DDRQ exit handler
 */
bool nss_ppenl_edma_ddrq_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS netlink EDMA DDRQ handler\n");

	/*
	 * Unregister the ops family
	 */
	error = genl_unregister_family(&nss_ppenl_edma_ddrq_family);
	if (error != 0) {
		nss_ppenl_info_always("unable to unregister EDMA DDRQ NETLINK family\n");
		return false;
	}

	return true;
}
