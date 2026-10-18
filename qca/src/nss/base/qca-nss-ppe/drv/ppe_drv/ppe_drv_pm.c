/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppe_drv.h"

/*
 * ppe_drv_pm_entries_free
 *	Free the PM counter generation management structure and all contexts.
 */
void ppe_drv_pm_entries_free(void *pm)
{
	vfree(pm);
}

/*
 * ppe_drv_pm_counter_info_get()
 *	Get counter info from an opaque context.
 */
struct ppe_drv_pm_counter_info *ppe_drv_pm_counter_info_get(struct ppe_drv_pm_counter_ctx *ctx)
{
	if (!ctx) {
		return NULL;
	}

	return &ctx->info;
}
EXPORT_SYMBOL(ppe_drv_pm_counter_info_get);

/*
 * ppe_drv_pm_destroy()
 *	Destroy the PM management structure.
 */
void ppe_drv_pm_destroy(void *pm)
{
	vfree(pm);
}

/*
 * ppe_drv_pm_gen_entries_alloc
 *	Allocate and initialize the PM counter generation management structure.
 */
struct ppe_drv_pm_gen *ppe_drv_pm_gen_entries_alloc(void)
{
	struct ppe_drv_pm_gen *pm_gen;
	int i;

	/*
	 * Allocate ppe_drv_pm_gen
	 */
	pm_gen = vzalloc(sizeof(struct ppe_drv_pm_gen));
	if (!pm_gen) {
		ppe_drv_warn("Failed to allocate ppe_drv_pm_gen");
		return NULL;
	}

	/*
	 * Initialize ingress and egress pm_ctx
	 */
	for (i = 0; i < PPE_DRV_PM_COUNTER_GEN_ID_MAX; i++) {
		pm_gen->pm_ctx[i].idx_state = PPE_DRV_PM_COUNTER_GEN_ID_FREE;
		pm_gen->eg_pm_ctx[i].idx_state = PPE_DRV_PM_COUNTER_GEN_ID_FREE;
	}

	return pm_gen;
}

/*
 * ppe_drv_pm_gen_tbl_idx_get()
 *	Get an available table index from the specified direction's pm gen table.
 */
static int16_t ppe_drv_pm_gen_tbl_idx_get(ppe_drv_rule_dir_t rule_dir)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_pm_gen *pm_gen = p->pm_gen;
	int16_t id;

	/*
	 * Check if PM gen structure is initialized
	 */
	if (!p->pm_gen) {
		ppe_drv_warn("PM gen structure not initialized");
		return -1;
	}

	/*
	 * Select the appropriate table based on direction
	 */
	if (rule_dir == PPE_DRV_RULE_INGRESS) {
		/*
		 * Ingress direction: Use pm_ctx
		 */
		for (id = 0; id < PPE_DRV_PM_COUNTER_GEN_ID_MAX; id++) {
			if (pm_gen->pm_ctx[id].idx_state == PPE_DRV_PM_COUNTER_GEN_ID_FREE) {
				pm_gen->pm_ctx[id].idx_state = PPE_DRV_PM_COUNTER_GEN_ID_USED;
				return id;
			}
		}
	} else {
		/*
		 * Egress direction: Use eg_pm_ctx
		 */
		for (id = 0; id < PPE_DRV_PM_COUNTER_GEN_ID_MAX; id++) {
			if (pm_gen->eg_pm_ctx[id].idx_state == PPE_DRV_PM_COUNTER_GEN_ID_FREE) {
				pm_gen->eg_pm_ctx[id].idx_state = PPE_DRV_PM_COUNTER_GEN_ID_USED;
				return id;
			}
		}
	}

	/*
	 * No free tbl_idx found
	 */
	ppe_drv_warn("No available tbl_idx for direction %d in PM counter gen table", rule_dir);
	return -1;
}

/*
 * ppe_drv_pm_gen_tbl_idx_return()
 *	Return table index to free pool.
 */
static void ppe_drv_pm_gen_tbl_idx_return(int16_t id, ppe_drv_rule_dir_t rule_dir)
{
	struct ppe_drv_pm_gen *pm_gen = ppe_drv_gbl->pm_gen;

	if (!pm_gen || id < 0 || id >= PPE_DRV_PM_COUNTER_GEN_ID_MAX) {
		ppe_drv_warn("PM gen tbl_idx_return: invalid id %d\n", id);
		return;
	}
	if (rule_dir == PPE_DRV_RULE_INGRESS) {
		memset(&pm_gen->pm_ctx[id], 0, sizeof(struct ppe_drv_pm_counter_gen_ctx));
		pm_gen->pm_ctx[id].idx_state = PPE_DRV_PM_COUNTER_GEN_ID_FREE;
	} else {
		memset(&pm_gen->eg_pm_ctx[id], 0, sizeof(struct ppe_drv_pm_counter_gen_ctx));
		pm_gen->eg_pm_ctx[id].idx_state = PPE_DRV_PM_COUNTER_GEN_ID_FREE;
	}
}

/*
 * ppe_drv_pm_gen_alloc()
 *	Allocate ctx for PM counter gen.
 */
struct ppe_drv_pm_counter_gen_ctx *ppe_drv_pm_gen_alloc(ppe_drv_rule_dir_t rule_dir)
{
	struct ppe_drv_pm_counter_gen_ctx *gen_ctx = NULL;
	int16_t id = -1;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_pm_gen *pm_gen = p->pm_gen;

	spin_lock_bh(&p->lock);

	id = ppe_drv_pm_gen_tbl_idx_get(rule_dir);
	if (id < 0) {
		ppe_drv_warn("No free tbl_idx in shadow PM gen table for direction: %d", rule_dir);
		spin_unlock_bh(&p->lock);
		return NULL;
	}

	gen_ctx = (rule_dir == PPE_DRV_RULE_INGRESS) ? &pm_gen->pm_ctx[id] : &pm_gen->eg_pm_ctx[id];
	gen_ctx->tbl_idx = id;
	gen_ctx->rule_dir = rule_dir;

	if (rule_dir == PPE_DRV_RULE_INGRESS) {
		gen_ctx->entry_index = id;
	}

	ppe_drv_trace("PM generation context is allocated with hw_index: %d\n", id);
	spin_unlock_bh(&p->lock);

	return gen_ctx;
}
EXPORT_SYMBOL(ppe_drv_pm_gen_alloc);


/*
 * ppe_drv_pm_gen_fill()
 *	Fill PM gen rule and action information
 */
static bool ppe_drv_pm_gen_fill(struct ppe_drv_pm_counter_gen_ctx *gen_ctx, struct ppe_drv_pm_counter_gen_rule *rule)
{
	fal_pon_pm_counter_entry_t *fal_rule = &gen_ctx->fal_rule;

	if (rule->flags & PPE_DRV_PM_RULE_FLAG_PORT_ID) {
		fal_port_t fal_port;

		switch (rule->port_type) {
			case PPE_DRV_PM_PORT_TYPE_BITMAP:
				fal_port = FAL_PORT_ID(FAL_PORT_TYPE_PPORT, rule->port_info.port_bitmap);
				fal_rule->port_id = fal_port;
				break;
			case PPE_DRV_PM_PORT_TYPE_PORT:
				{
					fal_port = PPE_DRV_VIRTUAL_PORT_CHK(rule->port_info.port_num) ? FAL_PORT_ID(FAL_PORT_TYPE_VPORT, rule->port_info.port_num)
						: FAL_PORT_ID(FAL_PORT_TYPE_PPORT, rule->port_info.port_num);
					if (PPE_DRV_VIRTUAL_PORT_CHK(rule->port_info.port_num)) {
						fal_rule->port_id = fal_port;
					} else {
						fal_rule->port_id = (1ULL << fal_port);
					}
					break;
				}
			case PPE_DRV_PM_PORT_TYPE_GEMPORT:
				fal_port = FAL_PORT_ID(FAL_PORT_TYPE_GEM_PORT, rule->port_info.gemport);
				fal_rule->port_id = fal_port;
				break;
		}

		ppe_drv_trace("%p: rule PORT_ID: 0x%X", gen_ctx, fal_rule->port_id);
	}

	if (rule->flags & PPE_DRV_PM_RULE_FLAG_TAG_FORMAT) {
		fal_rule->vlan_tag_fmt = rule->tag_format;
	} else {
		/*
		 * Default tag format value.
		 */
		fal_rule->vlan_tag_fmt = PPE_DRV_PM_TAG_FORMAT_UNTAGGED | PPE_DRV_PM_TAG_FORMAT_PRIORITY | PPE_DRV_PM_TAG_FORMAT_TAGGED;
	}

	if (rule->flags & PPE_DRV_PM_RULE_FLAG_VID) {
		fal_rule->vlan_id = rule->vid;
		fal_rule->vlan_id_valid = A_TRUE;
		ppe_drv_trace("%p: rule VLAN_ID: %d", gen_ctx, fal_rule->vlan_id);
	}

	if (rule->flags & PPE_DRV_PM_RULE_FLAG_PCP) {
		fal_rule->vlan_pcp = rule->pcp;
		fal_rule->vlan_pcp_valid = A_TRUE;
		ppe_drv_trace("%p: rule VLAN_PCP: %d", gen_ctx, fal_rule->vlan_pcp);
	}

	if (rule->flags & PPE_DRV_PM_RULE_FLAG_IPMC) {
		fal_rule->ipmc_type = rule->ipmc;
	} else {
		/*
		 * Default ipmc value.
		 */
		fal_rule->ipmc_type = PPE_DRV_PM_IPMC_TYPE_NON_IPMC | PPE_DRV_PM_IPMC_TYPE_IPV4_MC | PPE_DRV_PM_IPMC_TYPE_IPV6_MC;
	}
	ppe_drv_trace("%p: rule IPMC_TYPE: %d", gen_ctx, fal_rule->ipmc_type);

	/*
	 * Rule Counter_id
	 */
	fal_rule->counter_id = rule->counter_id;

	return true;
}

/*
 * ppe_drv_pm_gen_rule_create
 *	Create the PM counter gen rule in PPE.
 */
ppe_drv_ret_t ppe_drv_pm_gen_rule_create(struct ppe_drv_pm_counter_gen_ctx *gen_ctx, struct ppe_drv_pm_counter_gen_rule *rule)
{
	struct ppe_drv *p = ppe_drv_gbl;
	sw_error_t error = SW_OK;
	fal_direction_t rule_dir;

	if (!ppe_drv_pm_gen_fill(gen_ctx, rule)) {
		ppe_drv_warn("%p: Invalid PM gen rule %p\n", gen_ctx, rule);
		return PPE_DRV_RET_PM_GEN_RULE_INVALID;
	}

	spin_lock_bh(&p->lock);

	/*
	 * Configure PM_COUNTER_GEN_TBL based on direction.
	 * Upstream direction: Configure PRE_IPO_PM_COUNTER_GEN_TBL.
	 * For ingress, counter ID in fal_pon_pm_counter_entry_t can be flexibly specified
	 * and is mapped to ctx->fal_rule.counter_id by ppe_drv_pm_gen_fill().
	 *
	 * Downstream direction: Configure EG_PM_COUNTER_GEN_TBL.
	 * For egress, the counter ID specified in fal_pon_pm_counter_entry_t will be
	 * ignored and overwritten with the actual table index (rule->counter_id here).
	 * Because, counter will be incremented on the ID programmed in PM Gen rule.
	 */
	if (rule->rule_dir == PPE_DRV_RULE_EGRESS) {
		gen_ctx->entry_index = rule->counter_id;
	}

	rule_dir = (rule->rule_dir == PPE_DRV_RULE_INGRESS) ? FAL_DIR_INGRESS : FAL_DIR_EGRESS;

	/*
	 * Configure PM counter entry
	 */
	error = fal_pon_pm_counter_entry_set(PPE_DRV_SWITCH_ID, gen_ctx->entry_index, rule_dir, &gen_ctx->fal_rule);
	if (error != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to create rule in PM_COUNTER_GEN_TBL for direction: %d with error: %d\n", rule_dir, error);
		return (rule_dir == FAL_DIR_INGRESS) ? PPE_DRV_RET_IN_PM_COUNTER_GEN_CREATE_FAIL : PPE_DRV_RET_EG_PM_COUNTER_GEN_CREATE_FAIL;
	}

	ppe_drv_info("PM_gen rule created for rule_dir: %s at tbl_hw_index: %d\n", (gen_ctx->rule_dir == 1) ? "UPSTREAM" : "DOWNSTREAM", gen_ctx->entry_index);

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_pm_gen_rule_create);

/*
 * ppe_drv_pm_gen_destroy
 *	Destroy the PM counter gen rule and context.
 */
void ppe_drv_pm_gen_destroy(struct ppe_drv_pm_counter_gen_ctx *gen_ctx)
{
	sw_error_t error;
	struct ppe_drv *p = ppe_drv_gbl;
	fal_pon_pm_counter_entry_t fal_rule = {0};
	fal_direction_t rule_dir;

	memset(&fal_rule, 0, sizeof(fal_rule));

	spin_lock_bh(&p->lock);

	rule_dir = (gen_ctx->rule_dir == PPE_DRV_RULE_INGRESS) ? FAL_DIR_INGRESS : FAL_DIR_EGRESS;

	/*
	 * Clear PM counter entry
	 */
	error = fal_pon_pm_counter_entry_set(PPE_DRV_SWITCH_ID, gen_ctx->entry_index, rule_dir, &fal_rule);
	if (error != SW_OK) {
		ppe_drv_warn("%p: Rule delete failed for counter_id: direction: %d error: %d\n", gen_ctx, rule_dir, error);
	}

	ppe_drv_info("PM_gen rule destroyed for rule_dir: %s at tbl_hw_index: %d\n", (gen_ctx->rule_dir == 1) ? "UPSTREAM" : "DOWNSTREAM", gen_ctx->entry_index);
	ppe_drv_pm_gen_tbl_idx_return(gen_ctx->tbl_idx, gen_ctx->rule_dir);
	spin_unlock_bh(&p->lock);

}
EXPORT_SYMBOL(ppe_drv_pm_gen_destroy);

/*
 * ppe_drv_pm_tbl_idx_get
 *	Get an available table index from the specified direction's pm table.
 */
static int16_t ppe_drv_pm_tbl_idx_get(ppe_drv_rule_dir_t rule_dir)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_pm *pm = p->pm;
	int16_t id;

	if (!p->pm) {
		ppe_drv_warn("PM structure not initialized");
		return -1;
	}

	if (rule_dir == PPE_DRV_RULE_INGRESS) {
		for (id = PPE_DRV_PM_COUNTER_ID_START; id <= PPE_DRV_PM_COUNTER_ID_END; id++) {
			if (pm->in_ctx_array[id].state == PPE_DRV_PM_COUNTER_ID_FREE) {
				pm->in_ctx_array[id].state = PPE_DRV_PM_COUNTER_ID_USED;
				return id;
			}
		}
	} else {
		for (id = PPE_DRV_PM_COUNTER_ID_START; id <= PPE_DRV_PM_COUNTER_ID_END; id++) {
			if (pm->eg_ctx_array[id].state == PPE_DRV_PM_COUNTER_ID_FREE) {
				pm->eg_ctx_array[id].state = PPE_DRV_PM_COUNTER_ID_USED;
				return id;
			}
		}
	}

	ppe_drv_warn("No free tbl_idx in shadow PM counter table for direction %d", rule_dir);
	return -1;
}

/*
 * ppe_drv_pm_tbl_idx_return()
 *	Return table index to free pool.
 */
static void ppe_drv_pm_tbl_idx_return(struct ppe_drv_pm_counter_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->state = PPE_DRV_PM_COUNTER_ID_FREE;
}

/*
 * ppe_drv_pm_entries_alloc
 *	Allocate and initialize the PM counter management structure.
 */
struct ppe_drv_pm *ppe_drv_pm_entries_alloc(void)
{
	struct ppe_drv_pm *pm;
	int i;

	/*
	 * Allocate ppe_drv_pm
	 */
	pm = vzalloc(sizeof(struct ppe_drv_pm));
	if (!pm) {
		ppe_drv_warn("Failed to allocate ppe_drv_pm");
		return NULL;
	}

	/*
	 * Initialize context states
	 */
	for (i = PPE_DRV_PM_COUNTER_ID_START; i <= PPE_DRV_PM_COUNTER_ID_END; i++) {
		pm->in_ctx_array[i].state = PPE_DRV_PM_COUNTER_ID_FREE;
		pm->eg_ctx_array[i].state = PPE_DRV_PM_COUNTER_ID_FREE;
	}

	return pm;
}

/*
 * ppe_drv_pm_alloc()
 *	Allocate a new context for a hardware PM counter.
 *
 * This API finds a free hardware index for the given direction, allocates a
 * context for it, and returns an opaque pointer to that context.
 */
struct ppe_drv_pm_counter_ctx *ppe_drv_pm_alloc(ppe_drv_rule_dir_t rule_dir)
{
	struct ppe_drv_pm_counter_ctx *ctx = NULL;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_pm *pm = p->pm;
	int8_t tbl_idx = -1;

	if (!p->pm) {
		ppe_drv_warn("PM structure not initialized");
		return NULL;
	}

	spin_lock_bh(&p->lock);

	tbl_idx = ppe_drv_pm_tbl_idx_get(rule_dir);
	if (tbl_idx < 0) {
		ppe_drv_warn("No free tbl_idx for direction: %s",
				(rule_dir == 1) ? "INGRESS" : "EGRESS");
		spin_unlock_bh(&p->lock);
		return NULL;
	}

	/*
	 * Get context from the appropriate array based on direction.
	 */
	ctx = (rule_dir == PPE_DRV_RULE_INGRESS) ? &pm->in_ctx_array[tbl_idx] : &pm->eg_ctx_array[tbl_idx];

	/*
	 * hw_index is the first free entry in table.
	 */
	ctx->info.hw_index = (uint8_t)tbl_idx;
	ctx->info.rule_dir = rule_dir;

	ppe_drv_trace("PM counter is allocated with hw_index: %d\n", ctx->info.hw_index);
	spin_unlock_bh(&p->lock);
	return ctx;
}
EXPORT_SYMBOL(ppe_drv_pm_alloc);

/*
 * ppe_drv_pm_free()
 *      Free PM counter context.
 *
 * This API will release the hardware index back to the free pool and flush the
 * hardware statistics for that index.
 */
void ppe_drv_pm_free(struct ppe_drv_pm_counter_ctx *ctx)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_pm *pm = p->pm;
	sw_error_t ret = SW_OK;
	fal_direction_t pm_dir;

	if (!pm) {
		ppe_drv_warn("PM structure not initialized during release");
		return;
	}

	if (!ctx || (ctx->state == PPE_DRV_PM_COUNTER_ID_FREE)) {
		ppe_drv_warn("Invalid context or context already free");
		return;
	}

	spin_lock_bh(&p->lock);

	/*
	 * Release hw_id back to the appropriate table and clear pm counter stats
	 */
	pm_dir = (ctx->info.rule_dir == PPE_DRV_RULE_INGRESS) ? FAL_DIR_INGRESS : FAL_DIR_EGRESS;

	/*
	 * Flush PM counter statistics
	 */
	ret = fal_pon_pm_counter_flush(PPE_DRV_SWITCH_ID, ctx->info.hw_index, pm_dir);
	if (ret != SW_OK) {
		ppe_drv_warn("Failed to clear the %s PM counter stats for hw_index: %d, error: %d\n",
				(pm_dir == FAL_DIR_INGRESS) ? "Ingress" : "Egress",
				ctx->info.hw_index, ret);
	}

	ppe_drv_info("PM counter destroyed for rule_dir: %s at tbl_hw_index: %d\n", (ctx->info.rule_dir == 1) ? "UPSTREAM" : "DOWNSTREAM", ctx->info.hw_index);
	ppe_drv_pm_tbl_idx_return(ctx);
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_pm_free);

/*
 * ppe_drv_pm_counter_stats_update()
 *	Update hardware pm counters.
 */
void ppe_drv_pm_counter_stats_update(struct ppe_drv_pm_counter_ctx *ctx)
{
	sw_error_t ret = SW_OK;
	fal_pon_pm_counter_t current_fal_counter = {0};
	fal_direction_t pm_dir;

	if (!ctx || (ctx->state != PPE_DRV_PM_COUNTER_ID_USED)) {
		ppe_drv_warn("Invalid PM counter context or context not in use\n");
		return;
	}

	/*
	 * Read current hardware counters
	 */
	pm_dir = (ctx->info.rule_dir == PPE_DRV_RULE_INGRESS) ? FAL_DIR_INGRESS : FAL_DIR_EGRESS;

	/*
	 * Get PM counter statistics
	 */
	ret = fal_pon_pm_counter_get(PPE_DRV_SWITCH_ID, ctx->info.hw_index, pm_dir, &current_fal_counter);

	if (ret != SW_OK) {
		ppe_drv_warn("Failed to get PM counter stats for hw_index: %d, error: %d\n",
				ctx->info.hw_index, ret);
		return;
	}

	/*
	 * Calculate delta and update atomic counters
	 */

	atomic64_add(PPE_DRV_PM_PKT_CNTR_ROLLOVER(current_fal_counter.packets_ucast - ctx->fal_counter.packets_ucast), &ctx->info.ucast_packet);
	atomic64_add(PPE_DRV_PM_PKT_CNTR_ROLLOVER(current_fal_counter.packets_bcast - ctx->fal_counter.packets_bcast), &ctx->info.bcast_packet);
	atomic64_add(PPE_DRV_PM_PKT_CNTR_ROLLOVER(current_fal_counter.packets_mcast - ctx->fal_counter.packets_mcast), &ctx->info.mcast_packet);
	atomic64_add(PPE_DRV_PM_PKT_CNTR_ROLLOVER(current_fal_counter.packets_oversize - ctx->fal_counter.packets_oversize), &ctx->info.oversize);
	atomic64_add(PPE_DRV_PM_BYTE_CNTR_ROLLOVER(current_fal_counter.bytes - ctx->fal_counter.bytes), &ctx->info.octets);
	atomic64_add(PPE_DRV_PM_PKT_CNTR_ROLLOVER(current_fal_counter.packets_64octets - ctx->fal_counter.packets_64octets), &ctx->info.frame_64);
	atomic64_add(PPE_DRV_PM_PKT_CNTR_ROLLOVER(current_fal_counter.packets_65to127octets - ctx->fal_counter.packets_65to127octets), &ctx->info.frame_65_127);
	atomic64_add(PPE_DRV_PM_PKT_CNTR_ROLLOVER(current_fal_counter.packets_128to255octets - ctx->fal_counter.packets_128to255octets), &ctx->info.frame_128_255);
	atomic64_add(PPE_DRV_PM_PKT_CNTR_ROLLOVER(current_fal_counter.packets_256to511octets - ctx->fal_counter.packets_256to511octets), &ctx->info.frame_256_511);
	atomic64_add(PPE_DRV_PM_PKT_CNTR_ROLLOVER(current_fal_counter.packets_512to1023octets - ctx->fal_counter.packets_512to1023octets), &ctx->info.frame_512_1023);
	atomic64_add(PPE_DRV_PM_PKT_CNTR_ROLLOVER(current_fal_counter.packets_1024to1518octets - ctx->fal_counter.packets_1024to1518octets), &ctx->info.frame_1024_1518);

	/*
	 * Store current stats for next iteration in fal_counter
	 */
	ctx->fal_counter = current_fal_counter;

	ppe_drv_trace("%p: PM counter stats updated for hw_index: %d", ctx, ctx->info.hw_index);
}
