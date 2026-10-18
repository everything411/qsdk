/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppe_drv.h"

/*
 * ppe_drv_dot1p_ctx_alloc()
 *	Allocate memory for a DOT1P rule.
 */
static inline struct ppe_drv_dot1p_ctx *ppe_drv_dot1p_ctx_alloc(void)
{
	/*
	 * Allocate memory for a new rule
	 *
	 * kzalloc with GFP_ATOMIC is used assuming DOT1P rules can be created
	 * in softirq context as well while processing an SKB in the system.
	 */
	return kzalloc(sizeof(struct ppe_drv_dot1p_ctx), GFP_ATOMIC);
}

/*
 * ppe_drv_dot1p_ctx_free()
 *	Free DOT1P context.
 */
static inline void ppe_drv_dot1p_ctx_free(struct ppe_drv_dot1p_ctx *ctx)
{
	kfree(ctx);
}

/*
 * ppe_drv_dot1p_list_id_get()
 *	Get a free list id.
 */
static int16_t ppe_drv_dot1p_list_id_get(void)
{
	int16_t id;
	uint16_t list_id_start = PPE_DRV_DOT1P_LIST_ID_START;
	uint16_t list_id_end = PPE_DRV_DOT1P_LIST_ID_MAX;
	struct ppe_drv_dot1p *dot1p = ppe_drv_gbl->dot1p;

	for (id = list_id_start; id < list_id_end; id++) {
		if (dot1p->list_id[id].list_id_state == PPE_DRV_DOT1P_LIST_ID_FREE) {
			dot1p->list_id[id].list_id_state = PPE_DRV_DOT1P_LIST_ID_USED;
			return id;
		}
	}

	return -1;
}

/*
 * ppe_drv_dot1p_list_id_free()
 *	Return a list id to free pool.
 */
static void ppe_drv_dot1p_list_id_free(int16_t id)
{
	struct ppe_drv_dot1p *dot1p = ppe_drv_gbl->dot1p;

	dot1p->list_id[id].ctx = NULL;
	dot1p->list_id[id].list_id_state = PPE_DRV_DOT1P_LIST_ID_FREE;
}

/*
 * ppe_drv_dot1p_policer_ctx_alloc()
 *	Allocate memory for a DOT1P policer rule.
 */
static inline struct ppe_drv_dot1p_policer_ctx *ppe_drv_dot1p_policer_ctx_alloc(void)
{
	/*
	 * Allocate memory for a new rule
	 *
	 * kzalloc with GFP_ATOMIC is used assuming DOT1P policer rules can be created
	 * in softirq context as well while processing an SKB in the system.
	 */
	return kzalloc(sizeof(struct ppe_drv_dot1p_policer_ctx), GFP_ATOMIC);
}

/*
 * ppe_drv_dot1p_policer_ctx_free()
 *	Free DOT1P policer context.
 */
static inline void ppe_drv_dot1p_policer_ctx_free(struct ppe_drv_dot1p_policer_ctx *ctx)
{
	kfree(ctx);
}

/*
 * ppe_drv_dot1p_policer_list_id_get()
 *	Get a free list id.
 */
static int16_t ppe_drv_dot1p_policer_list_id_get(void)
{
	int16_t id;
	uint16_t list_id_start = PPE_DRV_DOT1P_LIST_ID_START;
	uint16_t list_id_end = PPE_DRV_DOT1P_LIST_ID_MAX;
	struct ppe_drv_dot1p_policer *dot1p = ppe_drv_gbl->dot1p_policer;

	for (id = list_id_start; id < list_id_end; id++) {
		if (dot1p->list_id[id].list_id_state == PPE_DRV_DOT1P_LIST_ID_FREE) {
			dot1p->list_id[id].list_id_state = PPE_DRV_DOT1P_LIST_ID_USED;
			return id;
		}
	}

	return -1;
}

/*
 * ppe_drv_dot1p_policer_list_id_free()
 *	Return a list id to free pool.
 */
static void ppe_drv_dot1p_policer_list_id_free(int16_t id)
{
	struct ppe_drv_dot1p_policer *dot1p = ppe_drv_gbl->dot1p_policer;

	dot1p->list_id[id].ctx = NULL;
	dot1p->list_id[id].list_id_state = PPE_DRV_DOT1P_LIST_ID_FREE;
}

/*
 * ppe_drv_dot1p_rule_fill()
 *	Fill rule related information.
 */
static bool ppe_drv_dot1p_rule_fill(struct ppe_drv_dot1p_ctx *ctx, struct ppe_drv_dot1p_rule *info)
{
	fal_gemport_gen_t *fal_rule = &ctx->fal_gen_rule;
	struct ppe_drv_dot1p_rule_match *rule_f = &info->rule;

	if (rule_f->rule_flags & PPE_DRV_DOT1P_RULE_FLAG_SRC_INFO) {
		fal_port_t fal_port;

		switch(rule_f->src_port_type) {
		case PPE_DRV_DOT1P_PORT_TYPE_BITMAP:
			fal_port = FAL_PORT_ID(FAL_PORT_TYPE_PPORT, rule_f->src_port);
			fal_rule->src_info = fal_port;
			break;

		case PPE_DRV_DOT1P_PORT_TYPE_PORT:
			fal_port = PPE_DRV_VIRTUAL_PORT_CHK(rule_f->src_port) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, rule_f->src_port)
				: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, rule_f->src_port);

			if (PPE_DRV_VIRTUAL_PORT_CHK(rule_f->src_port)) {
				fal_rule->src_info = fal_port;
			} else {
				fal_rule->src_info = (1ULL << fal_port);
			}
			break;

		}

		fal_rule->src_info_valid = (a_bool_t)true;
	}

	if (rule_f->rule_flags & PPE_DRV_DOT1P_RULE_FLAG_DST_INFO) {
		fal_dest_info_t fal_dest_info;
		fal_rule->dest_info_valid = (a_bool_t)true;

		switch(rule_f->dst_port_type) {
		case PPE_DRV_DOT1P_PORT_TYPE_BITMAP:
			fal_dest_info.dest_info_type = FAL_DEST_INFO_PORT_BMP;
			break;

		case PPE_DRV_DOT1P_PORT_TYPE_PORT:
			fal_dest_info.dest_info_type = FAL_DEST_INFO_PORT_ID;
			break;
		}
		fal_dest_info.dest_info_value = rule_f->dst_port;
		fal_rule->dest_info = fal_dest_info;
	}

	if (rule_f->rule_flags & PPE_DRV_DOT1P_RULE_FLAG_VID) {
		fal_rule->vlan_id_valid = (a_bool_t)true;
		fal_rule->vlan_id = rule_f->vid;
	}

	if (rule_f->rule_flags & PPE_DRV_DOT1P_RULE_FLAG_PCP) {
		fal_rule->pcp_valid = (a_bool_t)true;
		fal_rule->pcp = rule_f->pcp;
	}

	if (rule_f->rule_flags & PPE_DRV_DOT1P_RULE_FLAG_DEI) {
		fal_rule->dei_valid = (a_bool_t)true;
		fal_rule->dei = rule_f->dei;
	}

	if (rule_f->rule_flags & PPE_DRV_DOT1P_RULE_FLAG_DSCP) {
		fal_rule->dscp_valid = (a_bool_t)true;
		fal_rule->dscp = rule_f->dscp;
		fal_rule->pri_type = 1;
	}

	ppe_drv_trace("%p: src_port: %d, dst_port: %d, vid: %d, pcp: %d, dei: %d, dscp: %d",
			ctx, fal_rule->src_info, fal_rule->dest_info.dest_info_value,
			fal_rule->vlan_id, fal_rule->pcp, fal_rule->dei, fal_rule->dscp);
	return true;
}

/*
 * ppe_drv_dot1p_action_fill()
 *	Fill action related information.
 */
static bool ppe_drv_dot1p_action_fill(struct ppe_drv_dot1p_ctx *ctx, struct ppe_drv_dot1p_rule *info)
{
	fal_gemport_gen_t *fal_gen_rule = &ctx->fal_gen_rule;
	fal_gemport_cfg_t *fal_cfg_rule = &ctx->fal_cfg_rule;
	struct ppe_drv_dot1p_action *action = &info->action;

	if (action->action_flags & PPE_DRV_DOT1P_ACTION_FLAG_GEM_PORT_ID) {
		fal_gen_rule->gemport = action->gem_port;
	}

	if (action->action_flags & PPE_DRV_DOT1P_ACTION_FLAG_SVC_CODE) {
		fal_cfg_rule->service_code_en = (a_bool_t)true;
		fal_cfg_rule->service_code = action->svc_code;
	}

	if (action->action_flags & PPE_DRV_DOT1P_ACTION_FLAG_INT_PRI) {
		fal_cfg_rule->int_pri_en = (a_bool_t)true;
		fal_cfg_rule->int_pri = action->int_pri;
	}

	if (!(action->action_flags & PPE_DRV_DOT1P_ACTION_FLAG_INT_PRI)) {
		fal_cfg_rule->int_pri_en = (a_bool_t)false;
	}

	if (action->action_flags & PPE_DRV_DOT1P_ACTION_FLAG_INT_DP) {
		fal_cfg_rule->int_dp_en = (a_bool_t)true;
		fal_cfg_rule->int_dp = action->int_dp;
	}

	if (action->action_flags & PPE_DRV_DOT1P_ACTION_FLAG_ENQ_VP) {
		fal_cfg_rule->enq_vp_en = (a_bool_t)true;
		fal_cfg_rule->enq_vp = action->enq_vp;
	}

	if (action->action_flags & PPE_DRV_DOT1P_ACTION_FLAG_DST_INFO || dot1p_dest_vp_en) {
		fal_cfg_rule->dest_en = (a_bool_t)true;
		if (dot1p_dest_vp_en) {
			action->dst_port = __builtin_ffs(gem_port_bitmap) - 1;
			fal_cfg_rule->dest_vp = action->dst_port;
		}
		if (PPE_DRV_VIRTUAL_PORT_CHK(action->dst_port)) {
			fal_cfg_rule->dest_vp = action->dst_port;
		} else {
			fal_cfg_rule->dest_pp = action->dst_port;
		}
	}

	if (action->action_flags & PPE_DRV_DOT1P_ACTION_FLAG_FWD_CMD) {
		fal_cfg_rule->fwd_cmd = action->fwd_cmd;
	}

	ppe_drv_trace("%p: gemport: %d, int_pri: %d, enq_vp: %d, service code: %d, int_dp: %d, dst-port pp: %d, dst-port vp:%d\n",
			ctx, fal_gen_rule->gemport, fal_cfg_rule->int_pri,
			fal_cfg_rule->enq_vp, fal_cfg_rule->service_code,
			fal_cfg_rule->int_dp, fal_cfg_rule->dest_pp, fal_cfg_rule->dest_vp);
	return true;
}

/*
 * ppe_drv_dot1p_fill()
 *	Fill DOt1P rule and action information
 */
static bool ppe_drv_dot1p_fill(struct ppe_drv_dot1p_ctx *ctx, struct ppe_drv_dot1p_rule *info)
{
	memset(&ctx->fal_gen_rule, 0, sizeof(ctx->fal_gen_rule));
	memset(&ctx->fal_cfg_rule, 0, sizeof(ctx->fal_cfg_rule));

	if (!ppe_drv_dot1p_rule_fill(ctx, info)) {
		ppe_drv_warn("%p: DOt1P rule fill fail: %p", ctx, info);
		return false;
	}

	if (!ppe_drv_dot1p_action_fill(ctx, info)) {
		ppe_drv_warn("%p: DOT1P action fill fail: %p", ctx, info);
		return false;
	}

	return true;
}

/*
 * ppe_drv_dot1p_policer_fill()
 *	Fill DOt1P policer rule and action information
 */
static bool ppe_drv_dot1p_policer_fill(struct ppe_drv_dot1p_policer_ctx *ctx, struct ppe_drv_dot1p_policer_rule *info)
{
	struct ppe_drv_dot1p_policer_action *action = &info->action;

	memset(&ctx->fal_policer, 0, sizeof(ctx->fal_policer));
	fal_gemport_policer_t *fal_policer = &ctx->fal_policer;

	if (action->action_flags & PPE_DRV_DOT1P_POLICER_ACTION_FLAG_POLICER_ID) {
		fal_policer->us_policer_idx = action->policer_id;
	}

	if (action->action_flags & PPE_DRV_DOT1P_POLICER_ACTION_FLAG_US_POLICER_EN) {
		fal_policer->us_policer_en = (a_bool_t)action->us_policer_en;
	}

	if (action->action_flags & PPE_DRV_DOT1P_POLICER_ACTION_FLAG_DS_POLICER_EN) {
		fal_policer->ds_policer_en = (a_bool_t)action->ds_policer_en;
	}

	return true;
}

/*
 * ppe_drv_dot1p_delete()
 *	Delete a DOT1P rule.
 */
void ppe_drv_dot1p_delete(struct ppe_drv_dot1p_ctx *ctx, uint16_t gemport)
{
	sw_error_t error;
	struct ppe_drv *p = ppe_drv_gbl;
	fal_gemport_gen_t gemport_gen_entry;
	fal_gemport_cfg_t gemport_cfg_entry;
	bool enable = false;

	if (!ctx) {
		ppe_drv_warn("Invalid context pointer\n");
		return;
	}

	memset(&gemport_gen_entry, 0, sizeof(gemport_gen_entry));
	memset(&gemport_cfg_entry, 0, sizeof(gemport_cfg_entry));

	spin_lock_bh(&p->lock);
	if (ctx->rule_valid) {
		error = fal_pon_gemport_gen_entry_set(PPE_DRV_SWITCH_ID, ctx->index, &gemport_gen_entry);
		if (error != SW_OK) {
			ppe_drv_warn("%p: Rule delete failed for index: %d error: %d\n", ctx, ctx->index, error);
		}
		error = fal_pon_gemport_gen_en_set(PPE_DRV_SWITCH_ID, gemport, enable);
		if (error != SW_OK) {
			ppe_drv_warn("%p: Disable gemport gen for gemport: %d failed, error: %d\n", ctx, gemport, error);
		}
		error = fal_pon_gemport_cfg_set(PPE_DRV_SWITCH_ID, gemport, &gemport_cfg_entry);
		if (error != SW_OK) {
			ppe_drv_warn("%p: Rule delete failed for gemport: %d error: %d\n", ctx, gemport, error);
		}
	}
	ppe_drv_dot1p_list_id_free(ctx->index);
	ppe_drv_dot1p_ctx_free(ctx);
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_dot1p_delete);

/*
 * ppe_drv_dot1p_policer_delete()
 *	Delete a DOT1P policer rule.
 */
void ppe_drv_dot1p_policer_delete(struct ppe_drv_dot1p_policer_ctx *ctx, uint16_t gemport)
{
	sw_error_t error;
	struct ppe_drv *p = ppe_drv_gbl;
	fal_gemport_policer_t fal_policer = {0};

	spin_lock_bh(&p->lock);

	if (ctx->rule_valid) {
		error = fal_pon_gemport_policer_set(PPE_DRV_SWITCH_ID, gemport, &fal_policer);
		if (error != SW_OK) {
			ppe_drv_warn("%p: Rule delete failed for index: %d error: %d\n", ctx, ctx->index, error);
		}
	}
	ppe_drv_dot1p_policer_list_id_free(ctx->index);
	ppe_drv_dot1p_policer_ctx_free(ctx);
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_dot1p_policer_delete);

/*
 * ppe_drv_dot1p_def_rule_pause_resume()
 *	Pause/resume default DOT1P rule.
 */
ppe_drv_ret_t ppe_drv_dot1p_def_rule_pause_resume(struct ppe_drv_dot1p_rule *info, uint8_t gen_miss_cmd)
{
	sw_error_t error;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_dot1p_def_rule_match *rule_f = &info->def_rule;

	fal_gemport_global_cfg_t fal_gbl_rule = {0};

	spin_lock_bh(&p->lock);

	if (rule_f->def_rule_flags & PPE_DRV_DOT1P_RULE_FLAG_VID) {
		fal_gbl_rule.vlan_mode = FAL_VLAN_MATCH_VID;
	}

	if (rule_f->def_rule_flags & PPE_DRV_DOT1P_RULE_FLAG_PCP) {
		fal_gbl_rule.pcp_mode = FAL_PCP_MATCH_PCP_DEI;
	}

	if (rule_f->def_rule_flags & PPE_DRV_DOT1P_RULE_FLAG_DEI) {
		fal_gbl_rule.pcp_mode = FAL_PCP_MATCH_PCP_DEI;
	}

	if (rule_f->def_rule_flags & PPE_DRV_DOT1P_RULE_FLAG_GEN_MISS_CMD) {
		fal_gbl_rule.gen_miss_cmd = gen_miss_cmd;
		fal_gbl_rule.gen_miss_pon_port = PON_PORT_ID;
	}

	error = fal_pon_gemport_global_set(PPE_DRV_SWITCH_ID, &fal_gbl_rule);
	if (error != SW_OK) {
		ppe_drv_warn(" Could not set the global cfg = %d\n", error);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_DOT1P_GLOBAL_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_dot1p_def_rule_pause_resume);
/*
 * ppe_drv_dot1p_def_rule_configure()
 *	Configure default DOT1P rule.
 */
ppe_drv_ret_t ppe_drv_dot1p_def_rule_configure(struct ppe_drv_dot1p_rule *info)
{
	sw_error_t error;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_dot1p_def_rule_match *rule_f = &info->def_rule;

	fal_gemport_gen_default_t fal_rule = {0};
	fal_gemport_global_cfg_t fal_gbl_rule = {0};

	spin_lock_bh(&p->lock);
	if (rule_f->def_rule_flags & PPE_DRV_DOT1P_RULE_FLAG_VID) {
		fal_rule.vlan_id = rule_f->vid;
		fal_gbl_rule.vlan_mode = FAL_VLAN_MATCH_VID;
	}

	if (rule_f->def_rule_flags & PPE_DRV_DOT1P_RULE_FLAG_PCP) {
		fal_rule.pcp = rule_f->pcp;
		fal_gbl_rule.pcp_mode = FAL_PCP_MATCH_PCP_DEI;
	}

	if (rule_f->def_rule_flags & PPE_DRV_DOT1P_RULE_FLAG_DEI) {
		fal_rule.dei = rule_f->dei;
		fal_gbl_rule.pcp_mode = FAL_PCP_MATCH_PCP_DEI;
	}

	if (rule_f->def_rule_flags & PPE_DRV_DOT1P_RULE_FLAG_DSCP) {
		fal_rule.dscp = rule_f->dscp;
	}

	if (rule_f->def_rule_flags & PPE_DRV_DOT1P_RULE_FLAG_DSCP_MASK) {
		fal_rule.dscp_mask = rule_f->dscp_mask;
	}

	if (rule_f->def_rule_flags & PPE_DRV_DOT1P_RULE_FLAG_GEN_MISS_CMD) {
		fal_gbl_rule.gen_miss_cmd = rule_f->gen_miss_cmd;
		fal_gbl_rule.gen_miss_pon_port = PON_PORT_ID;
	}

	ppe_drv_trace("default_dot1p_rule_fill: vid: %d, pcp: %d, dei: %d, dscp: %d, dscp mask:%d, vlan_mode: %d, pcp_mode: %d, gen_miss_cmd: %d \n",
			fal_rule.vlan_id, fal_rule.pcp, fal_rule.dei, fal_rule.dscp, fal_rule.dscp_mask, fal_gbl_rule.vlan_mode, fal_gbl_rule.pcp_mode, fal_gbl_rule.gen_miss_cmd);

	error = fal_pon_gemport_global_set(PPE_DRV_SWITCH_ID, &fal_gbl_rule);
	if (error != SW_OK) {
		ppe_drv_warn(" Could not set the global cfg = %d\n", error);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_DOT1P_GLOBAL_CFG_FAIL;
	}

	error = fal_pon_gemport_gen_default_set(PPE_DRV_SWITCH_ID, &fal_rule);
	if (error != SW_OK) {
		ppe_drv_warn("Could not set the def cfg = %d\n", error);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_DOT1P_DEFAULT_CFG_FAIL;
	}
	spin_unlock_bh(&p->lock);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_dot1p_def_rule_configure);

/*
 * ppe_drv_dot1p_pause()
 *	Pause DOT1P rule.
 */
ppe_drv_ret_t ppe_drv_dot1p_rule_pause_resume(struct ppe_drv_dot1p_ctx *ctx, struct ppe_drv_dot1p_rule *info)
{
	sw_error_t error;
	struct ppe_drv *p = ppe_drv_gbl;
	fal_gemport_cfg_t *fal_cfg_rule = &ctx->fal_cfg_rule;
	struct ppe_drv_dot1p_action *action = &info->action;

	spin_lock_bh(&p->lock);
	fal_cfg_rule->fwd_cmd = action->fwd_cmd;

	/*
	 * Update the gemport cfg action to drop
	 */
	error = fal_pon_gemport_cfg_set(PPE_DRV_SWITCH_ID, info->action.gem_port, &ctx->fal_cfg_rule);
	if (error != SW_OK) {
		ppe_drv_warn("%p: Could not bind gemport gen rule to gemport cfg error = %d, index: %d\n", ctx, error, ctx->index);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_DOT1P_RULE_BIND_GEMPORT_FAIL;
	}

	ppe_drv_info("Dot1p_rule_pause: Updated dot1p rule action to drop\n");
	spin_unlock_bh(&p->lock);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_dot1p_rule_pause_resume);

/*
 * ppe_drv_dot1p_configure()
 *     Configure DOT1P rule.
 */
ppe_drv_ret_t ppe_drv_dot1p_rule_configure(struct ppe_drv_dot1p_ctx *ctx,
                                           struct ppe_drv_dot1p_rule *info)
{
	sw_error_t error;
	bool enable;
	ppe_drv_ret_t rc = PPE_DRV_RET_SUCCESS;
	fal_gemport_gen_t gemport_gen_entry;
	struct ppe_drv *p = ppe_drv_gbl;

	memset(&gemport_gen_entry, 0, sizeof(gemport_gen_entry));

	spin_lock_bh(&p->lock);

	if (!ppe_drv_dot1p_fill(ctx, info)) {
		ppe_drv_warn("%p: Invalid DOT1P rule %p\n", ctx, info);
		rc = PPE_DRV_RET_DOT1P_RULE_INVALID;
		goto out_unlock;
	}

	error = fal_pon_gemport_gen_entry_set(PPE_DRV_SWITCH_ID, ctx->index, &ctx->fal_gen_rule);
	if (error != SW_OK) {
		ppe_drv_warn("%p: Could not add dot1p gemport gen rule, error = %d\n", ctx, error);
		rc = PPE_DRV_RET_DOT1P_RULE_ADD_FAIL;
		goto out_unlock;
	}

	/*
	 * Bind the gen entry to the GEM port config
	 */
	error = fal_pon_gemport_cfg_set(PPE_DRV_SWITCH_ID, info->action.gem_port, &ctx->fal_cfg_rule);
	if (error != SW_OK) {
		ppe_drv_warn("%p: Could not bind gemport gen rule to gemport cfg error = %d, index: %d\n",
				ctx, error, ctx->index);
		rc = PPE_DRV_RET_DOT1P_RULE_BIND_GEMPORT_FAIL;
		goto remove_entry;
	}

	enable = true;
	error = fal_pon_gemport_gen_en_set(PPE_DRV_SWITCH_ID, info->action.gem_port, enable);
	if (error != SW_OK) {
		ppe_drv_warn("%p: Could not enable gemport gen, error = %d\n", ctx, error);
		rc = PPE_DRV_RET_DOT1P_RULE_ENABLE_FAIL;
		goto remove_entry;
	}

	ctx->rule_valid = true;
	ppe_drv_info("Created DOT1P rule\n");
	goto out_unlock;

remove_entry:
	error = fal_pon_gemport_gen_entry_set(PPE_DRV_SWITCH_ID, ctx->index, &gemport_gen_entry);
	if (error != SW_OK) {
		ppe_drv_warn("%p: Rule delete failed error: %d, index: %d\n", ctx, error, ctx->index);
	}

out_unlock:
	spin_unlock_bh(&p->lock);
	return rc;
}
EXPORT_SYMBOL(ppe_drv_dot1p_rule_configure);

/*
 * ppe_drv_dot1p_policer_configure()
 *	Configure DOT1P policer rule.
 */
ppe_drv_ret_t ppe_drv_dot1p_policer_rule_configure(struct ppe_drv_dot1p_policer_ctx *ctx, struct ppe_drv_dot1p_policer_rule *info)
{
	sw_error_t error;
	bool enable;
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);

	if (!ppe_drv_dot1p_policer_fill(ctx, info)) {
		ppe_drv_warn("%p: Invalid DOT1P policer rule %p\n", ctx, info);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_DOT1P_RULE_INVALID;
	}

	/*
	 * Fill policer id configuration
	 */
	if (info->action.action_flags & PPE_DRV_DOT1P_POLICER_ACTION_FLAG_POLICER_ID) {
		error = fal_pon_gemport_policer_set(PPE_DRV_SWITCH_ID, info->rule.gemport_id, &ctx->fal_policer);
		if (error != SW_OK) {
			ppe_drv_warn("%p: could not bind policer id with gemport cfg error = %d\n", ctx, error);
			spin_unlock_bh(&p->lock);
			return PPE_DRV_RET_DOT1P_RULE_BIND_GEMPORT_FAIL;
		}
	}

	enable = true;
	error = fal_pon_gemport_gen_en_set(PPE_DRV_SWITCH_ID, info->rule.gemport_id, enable);
	if (error != SW_OK) {
		ppe_drv_warn("%p: Could not enable gemport gen, error = %d\n", ctx, error);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_DOT1P_RULE_ENABLE_FAIL;
	}

	ctx->rule_valid = true;
	ppe_drv_info("Created DOT1P policer rule for gemport_id: %d\n", info->rule.gemport_id);
	spin_unlock_bh(&p->lock);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_dot1p_policer_rule_configure);

/*
 * ppe_drv_dot1p_alloc()
 *	Allocate ctx for DOT1P rules.
 */
struct ppe_drv_dot1p_ctx *ppe_drv_dot1p_alloc()
{
	int16_t list_id = -1;
	struct ppe_drv_dot1p_ctx *ctx = NULL;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_dot1p *dot1p = p->dot1p;

	ctx = ppe_drv_dot1p_ctx_alloc();
	if (!ctx) {
		ppe_drv_warn("Alloc failed, out of memory \n");
		return NULL;
	}

	spin_lock_bh(&p->lock);
	list_id = ppe_drv_dot1p_list_id_get();
	if (list_id < 0) {
		ppe_drv_warn("No free list id available, list id full\n");
		goto fail;
	}

	/*
	 * return rule context pointer.
	 */
	ctx->index = list_id;
	dot1p->list_id[list_id].ctx = ctx;
	spin_unlock_bh(&p->lock);
	ppe_drv_info("%p: dot1p rule allocated ctx: %p, list_id: %d",
			dot1p, ctx, list_id);

	return ctx;
fail:
	if (list_id != -1) {
		ppe_drv_dot1p_list_id_free(list_id);
	}

	if (ctx) {
		ppe_drv_dot1p_ctx_free(ctx);
	}

	spin_unlock_bh(&p->lock);
	return NULL;
}
EXPORT_SYMBOL(ppe_drv_dot1p_alloc);

/*
 * ppe_drv_dot1p_policer_alloc()
 *	Allocate ctx for DOT1P policer rules.
 */
struct ppe_drv_dot1p_policer_ctx *ppe_drv_dot1p_policer_alloc()
{
	int16_t list_id = -1;
	struct ppe_drv_dot1p_policer_ctx *ctx = NULL;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_dot1p_policer *dot1p = p->dot1p_policer;

	ctx = ppe_drv_dot1p_policer_ctx_alloc();
	if (!ctx) {
		ppe_drv_warn("Alloc failed, out of memory \n");
		return NULL;
	}

	spin_lock_bh(&p->lock);
	list_id = ppe_drv_dot1p_policer_list_id_get();
	if (list_id < 0) {
		ppe_drv_warn("No free list id available, list id full\n");
		goto fail;
	}

	/*
	 * return rule context pointer.
	 */
	ctx->index = list_id;
	dot1p->list_id[list_id].ctx = ctx;
	spin_unlock_bh(&p->lock);
	ppe_drv_info("%p: dot1p rule allocated ctx: %p, list_id: %d",
			dot1p, ctx, list_id);

	return ctx;
fail:
	if (list_id != -1) {
		ppe_drv_dot1p_policer_list_id_free(list_id);
	}

	if (ctx) {
		ppe_drv_dot1p_policer_ctx_free(ctx);
	}

	spin_unlock_bh(&p->lock);
	return NULL;
}
EXPORT_SYMBOL(ppe_drv_dot1p_policer_alloc);

/*
 * ppe_drv_dot1p_entries_free()
 *	Free dot1p instance.
 */
void ppe_drv_dot1p_entries_free(struct ppe_drv_dot1p *dot1p)
{
	vfree(dot1p);
}

/*
 * ppe_drv_dot1p_entries_alloc()
 *	Allocates DOT1P entries.
 */
struct ppe_drv_dot1p *ppe_drv_dot1p_entries_alloc(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_dot1p *dot1p;
	int id;

	dot1p = vzalloc(sizeof(struct ppe_drv_dot1p));
	if (!dot1p) {
		ppe_drv_warn("%p: Failed to allocate DOT1P table entries", p);
		return NULL;
	}

	/*
	 * Initialize list_id.
	 */
	for (id = 0; id < PPE_DRV_DOT1P_LIST_ID_MAX; id++) {
		dot1p->list_id[id].list_id_state = PPE_DRV_DOT1P_LIST_ID_FREE;
	}

	return dot1p;
}

/*
 * ppe_drv_dot1p_policer_entries_free()
 *	Free dot1p policer instance.
 */
void ppe_drv_dot1p_policer_entries_free(struct ppe_drv_dot1p_policer *dot1p)
{
	vfree(dot1p);
}

/*
 * ppe_drv_dot1p_policer_entries_alloc()
 *	Allocates DOT1P policer entries.
 */
struct ppe_drv_dot1p_policer *ppe_drv_dot1p_policer_entries_alloc(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_dot1p_policer *dot1p;
	int id;

	dot1p = vzalloc(sizeof(struct ppe_drv_dot1p_policer));
	if (!dot1p) {
		ppe_drv_warn("%p: Failed to allocate DOT1P policer table entries", p);
		return NULL;
	}

	/*
	 * Initialize list_id.
	 */
	for (id = 0; id < PPE_DRV_DOT1P_LIST_ID_MAX; id++) {
		dot1p->list_id[id].list_id_state = PPE_DRV_DOT1P_LIST_ID_FREE;
	}

	return dot1p;
}

/*
 * ppe_drv_dot1p_get_hw_index
 *	Get hw index corresponding to the rule.
 */
void ppe_drv_dot1p_get_hw_index(struct ppe_drv_dot1p_ctx *ctx, uint8_t *hw_index)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	*hw_index = ctx->index;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_dot1p_get_hw_index);

/*
 * ppe_drv_dot1p_policer_get_hw_index
 *	Get hw index corresponding to the rule.
 */
void ppe_drv_dot1p_policer_get_hw_index(struct ppe_drv_dot1p_policer_ctx *ctx, uint8_t *hw_index)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	*hw_index = ctx->index;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_dot1p_policer_get_hw_index);

/*
 * ppe_drv_dot1p_configure_default_rule()
 *	configure default dot1p rule
 *
 */
ppe_drv_ret_t ppe_drv_dot1p_configure_default_rule()
{
	ppe_drv_ret_t ret;
	struct ppe_drv_dot1p_rule info = {0};

	info.def_rule.def_rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_VID;
	info.def_rule.vid = PPE_DRV_DOT1P_DEF_VID_VAL;

	info.def_rule.def_rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_PCP;
	info.def_rule.pcp = PPE_DRV_DOT1P_DEF_PCP_VAL;

	info.def_rule.def_rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_DSCP;
	info.def_rule.dscp = PPE_DRV_DOT1P_DEF_DSCP_VAL;

	info.def_rule.def_rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_GEN_MISS_CMD;
	info.def_rule.gen_miss_cmd = PPE_DRV_DOT1P_CMD_DROP;

	ret = ppe_drv_dot1p_def_rule_configure(&info);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_drv_warn("failed to configure default ppe drv rule\n");
	}

	ppe_drv_trace("Dot1p default rule added succesfully\n");
	return ret;
}

/*
 * ppe_drv_dot1p_reset_default_rule()
 *	Reset/clear the default dot1p rule configured.
 *
 *	Passes a zero-initialized info with no flags set so that
 *	ppe_drv_dot1p_def_rule_configure() writes zeroed FAL structs to HW.
 */
ppe_drv_ret_t ppe_drv_dot1p_reset_default_rule(void)
{
	struct ppe_drv_dot1p_rule info = {0};

	return ppe_drv_dot1p_def_rule_configure(&info);
}
