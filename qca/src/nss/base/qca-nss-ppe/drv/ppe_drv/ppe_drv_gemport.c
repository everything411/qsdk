/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppe_drv.h"

/*
 * ppe_drv_gem_port_ctx_alloc()
 *	Allocate memory for a GEM PORT rule.
 */
static inline struct ppe_drv_gem_port_ctx *ppe_drv_gem_port_ctx_alloc(void)
{
	/*
	 * Allocate memory for a new rule
	 *
	 * kzalloc with GFP_ATOMIC is used assuming GEM PORT rules can be created
	 * in softirq context as well while processing an SKB in the system.
	 */
	return kzalloc(sizeof(struct ppe_drv_gem_port_ctx), GFP_ATOMIC);
}

/*
 * ppe_drv_gem_port_ctx_free()
 *	Free GEM PORT context.
 */
static inline void ppe_drv_gem_port_ctx_free(struct ppe_drv_gem_port_ctx *ctx)
{
	kfree(ctx);
}

/*
 * ppe_drv_gem_port_list_id_get()
 *	Get a free list id.
 */
static int16_t ppe_drv_gem_port_list_id_get(void)
{
	int16_t id;
	uint16_t list_id_start = PPE_DRV_GEM_PORT_LIST_ID_START;
	uint16_t list_id_end = PPE_DRV_GEM_PORT_LIST_ID_MAX;
	struct ppe_drv_gem_port *gemport = ppe_drv_gbl->gemport;

	for (id = list_id_start; id < list_id_end; id++) {
		if (gemport->gem_port_rules[id].list_id_state == PPE_DRV_GEM_PORT_LIST_ID_FREE) {
			gemport->gem_port_rules[id].list_id_state = PPE_DRV_GEM_PORT_LIST_ID_USED;
			return id;
		}
	}

	return -1;
}

/*
 * ppe_drv_gem_port_list_id_return()
 *	Return a list id to free pool.
 */
static void ppe_drv_gem_port_list_id_return(int16_t id)
{
	struct ppe_drv_gem_port *gemport = ppe_drv_gbl->gemport;

	gemport->gem_port_rules[id].ctx = NULL;
	gemport->gem_port_rules[id].list_id_state = PPE_DRV_GEM_PORT_LIST_ID_FREE;
}

/*
 * ppe_drv_gem_port_delete()
 *	Delete a GEMPORT rule.
 */
void ppe_drv_gem_port_delete(struct ppe_drv_gem_port_ctx *ctx, uint16_t gemport)
{
	sw_error_t error;
	struct ppe_drv *p = ppe_drv_gbl;
	spin_lock_bh(&p->lock);
	fal_gemport_map_t entry = {0};

	if (ctx->rule_valid) {
		error = fal_pon_gemport_map_set(PPE_DRV_SWITCH_ID, gemport, &entry);
		if (error != SW_OK) {
			ppe_drv_warn("%p: Rule delete failed for gem_port: %d error: %d\n", ctx, gemport, error);
		}
	}
	ppe_drv_gem_port_list_id_return(ctx->index);
	ppe_drv_gem_port_ctx_free(ctx);
	spin_unlock_bh(&p->lock);
	ppe_drv_trace("%p: gemport rule delete successful \n", p);
}
EXPORT_SYMBOL(ppe_drv_gem_port_delete);

/*
 * ppe_drv_gem_port_action_fill()
 *	Fill action related information.
 */
static bool ppe_drv_gem_port_action_fill(struct ppe_drv_gem_port_ctx *ctx, struct ppe_drv_gem_port_rule *info)
{
	fal_gemport_map_t *fal_rule = &ctx->fal_rule;
	struct ppe_drv_gem_port_rule_action *action = &info->action;

	if (action->action_flags & PPE_DRV_GEM_PORT_ACTION_FLAG_SRC_INFO) {
		fal_rule->src_en = (a_bool_t)true;
		fal_rule->src_port = action->src_port;
	}

	if ((action->action_flags & PPE_DRV_GEM_PORT_ACTION_FLAG_DST_INFO) &&
		(action->action_flags & PPE_DRV_GEM_PORT_ACTION_FLAG_DST_PORT_TYPE)) {
		fal_dest_info_t fal_dest_info;
		fal_rule->dest_en = (a_bool_t)true;

		switch(action->dst_port_type) {
			case PPE_DRV_GEM_PORT_TYPE_BITMAP:
				fal_dest_info.dest_info_type = FAL_DEST_INFO_PORT_BMP;
				break;
			case PPE_DRV_GEM_PORT_TYPE_PORT:
				fal_dest_info.dest_info_type = FAL_DEST_INFO_PORT_ID;
				break;
		}
		fal_dest_info.dest_info_value = action->dst_port;
		fal_rule->dest_info = fal_dest_info;
	}

	if (action->action_flags & PPE_DRV_GEM_PORT_ACTION_FLAG_DST_SELECTION) {
			fal_rule->service_code = action->svc_code;
	}

	if (action->action_flags & PPE_DRV_GEM_PORT_ACTION_FLAG_INT_PRI) {
		fal_rule->int_pri_dp_en = (a_bool_t)true;
		fal_rule->int_pri = action->int_pri;
	}

	if (action->action_flags & PPE_DRV_GEM_PORT_ACTION_FLAG_INT_DP) {
		fal_rule->int_pri_dp_en = (a_bool_t)true;
		fal_rule->int_dp = action->int_dp;
	}

	ppe_drv_trace("%p: int_pri: %d, service code: %d, int_dp: %d, dst-port: %d, src-port: %d \n",
			ctx, fal_rule->int_pri, fal_rule->service_code, fal_rule->int_dp,
			fal_rule->dest_info.dest_info_value, fal_rule->src_port);
	return true;
}

/*
 * ppe_drv_gem_port_rule_configure()
 *	Configure GEMPORT rule.
 */
ppe_drv_ret_t ppe_drv_gem_port_rule_configure(struct ppe_drv_gem_port_ctx *ctx, struct ppe_drv_gem_port_rule *info)
{
	sw_error_t error;
	memset(&ctx->fal_rule, 0, sizeof(ctx->fal_rule));
	uint32_t gemport_id = 0;
	struct ppe_drv_gem_port_rule_match *rule_f = &info->rule;
	struct ppe_drv *p = ppe_drv_gbl;
	a_bool_t port_mapping_en = (a_bool_t)false;
	fal_port_t pon_port = FAL_PORT_ID(FAL_PORT_TYPE_PPORT, PON_PORT_ID);

	if (rule_f->rule_flags & PPE_DRV_GEM_PORT_RULE_FLAG_GEM_PORT_ID) {
		gemport_id = rule_f->gem_port_id;
	}

	spin_lock_bh(&p->lock);
	if (!ppe_drv_gem_port_action_fill(ctx, info)) {
		ppe_drv_warn("%p: Invalid GEMPORT rule %p\n", ctx, info);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_GEMPORT_RULE_INVALID;
	}

	error = fal_pon_gemport_map_en_get(PPE_DRV_SWITCH_ID, pon_port, &port_mapping_en);
	if (!port_mapping_en) {
		error = fal_pon_gemport_map_en_set(PPE_DRV_SWITCH_ID, pon_port, (a_bool_t)true);
	}

	error = fal_pon_gemport_map_set(PPE_DRV_SWITCH_ID, gemport_id, &ctx->fal_rule);
	if (error != SW_OK) {
		ppe_drv_warn("%p: Could not add gemport mapping rule, error = %d\n", ctx, error);
		spin_unlock_bh(&p->lock);
		return PPE_DRV_RET_GEMPORT_RULE_ADD_FAIL;
	}

	ctx->rule_valid = true;
	spin_unlock_bh(&p->lock);
	ppe_drv_trace("Created GEM PORT rule\n");
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_gem_port_rule_configure);

/*
 * ppe_drv_gem_port_alloc()
 *	Allocate ctx for GEMPORT rules.
 */
struct ppe_drv_gem_port_ctx *ppe_drv_gem_port_alloc()
{
	int16_t index = -1;
	struct ppe_drv_gem_port_ctx *ctx = NULL;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_gem_port *gemport = p->gemport;

	ctx = ppe_drv_gem_port_ctx_alloc();
	if (!ctx) {
		ppe_drv_warn("Alloc failed, out of memory \n");
		return NULL;
	}

	spin_lock_bh(&p->lock);
	index = ppe_drv_gem_port_list_id_get();
	if (index < 0) {
		ppe_drv_warn("No free list id available, list id full\n");
		goto fail;
	}

	/*
	 * return rule context pointer.
	 */
	ctx->index = index;
	gemport->gem_port_rules[index].ctx = ctx;
	spin_unlock_bh(&p->lock);
	ppe_drv_trace("%p: gemport rule allocated ctx: %p, index: %d\n",
			gemport, ctx, index);

	return ctx;
fail:
	if (index != -1) {
		ppe_drv_gem_port_list_id_return(index);
	}

	if (ctx) {
		ppe_drv_gem_port_ctx_free(ctx);
	}

	spin_unlock_bh(&p->lock);
	return NULL;
}
EXPORT_SYMBOL(ppe_drv_gem_port_alloc);

/*
 * ppe_drv_gem_port_entries_free()
 *	Free gem port instance.
 */
void ppe_drv_gem_port_entries_free(struct ppe_drv_gem_port *gem_port)
{
	vfree(gem_port);
}

/*
 * ppe_drv_gem_port_entries_alloc()
 *	Allocates GEM PORT entries.
 */
struct ppe_drv_gem_port *ppe_drv_gem_port_entries_alloc(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_gem_port *gem_port;
	int id;

	gem_port = vzalloc(sizeof(struct ppe_drv_gem_port));
	if (!gem_port) {
		ppe_drv_warn("%p: Failed to allocate GEM PORT table entries", p);
		return NULL;
	}

	/*
	 * Initialize gemport rules state.
	 */
	for (id = 0; id < PPE_DRV_GEM_PORT_LIST_ID_MAX; id++) {
		gem_port->gem_port_rules[id].list_id_state = PPE_DRV_GEM_PORT_LIST_ID_FREE;
	}

	return gem_port;
}
