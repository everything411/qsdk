/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <fal/fal_fdb.h>
#include <fal/fal_ip.h>
#include <fal/fal_misc.h>
#include <fal/fal_port_ctrl.h>
#include <fal/fal_qm.h>
#include <fal/fal_qos.h>
#include <fal/fal_vport.h>
#include <fal/fal_vsi.h>
#include <fal/fal_tunnel.h>
#include <fal/fal_policer.h>
#include <ref/ref_vsi.h>
#include "ppe_drv.h"

#if (PPE_DRV_DEBUG_LEVEL == 3)
/*
 * ppe_drv_policer_acl_dump()
 *	Dump ACL policer rule tables.
 */
static void ppe_drv_policer_acl_dump(struct ppe_drv_policer_acl *pol)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_policer_config_t pol_cfg = {0};
	fal_policer_action_t action = {0};
	sw_error_t err = SW_OK;
	uint32_t hw_index = pol->acl_index;

	spin_lock_bh(&p->lock);

	for (uint32_t index = 0; index < pol->num_acl_reserved && hw_index < PPE_DRV_ACL_POLICER_MAX; index++) {
		if (!p->pol_ctx->acl_pol[hw_index].is_configured) {
			hw_index++;
			continue;
		}

		err = fal_acl_policer_entry_get(PPE_DRV_SWITCH_ID, hw_index, &pol_cfg, &action);
		if (err != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("%p: Error in retreiving stats\n", p);
			return;
		}

		ppe_drv_trace("%p: policer config: hw_index: %d\n", p, hw_index);
		ppe_drv_trace("%p: policer config: meter_en: %d\n", p, pol_cfg.meter_en);
		ppe_drv_trace("%p: policer config: couple_en: %d\n", p, pol_cfg.couple_en);
		ppe_drv_trace("%p: policer config: color_mode: %d\n", p, pol_cfg.color_mode);
		ppe_drv_trace("%p: policer config: meter_mode: %d\n", p, pol_cfg.meter_mode);
		ppe_drv_trace("%p: policer config: meter_unit: %d\n", p, pol_cfg.meter_unit);
		ppe_drv_trace("%p: policer config: cir: %d\n", p, pol_cfg.cir);
		ppe_drv_trace("%p: policer config: cbs: %d\n", p, pol_cfg.cbs);
		ppe_drv_trace("%p: policer config: eir: %d\n", p, pol_cfg.eir);
		ppe_drv_trace("%p: policer config: ebs: %d\n", p, pol_cfg.ebs);
		ppe_drv_trace("%p: policer config: meter_type: %d\n", p, pol_cfg.meter_type);


		/*
		 * Action configuration
		 */
		ppe_drv_trace("%p: policer action: yellow_pri_en: %d\n", p, action.yellow_priority_en);
		ppe_drv_trace("%p: policer action: yellow_dp_en: %d\n", p, action.yellow_drop_priority_en);
		ppe_drv_trace("%p: policer action: yellow_pcp_en: %d\n", p, action.yellow_pcp_en);
		ppe_drv_trace("%p: policer action: yellow_dei_en: %d\n", p, action.yellow_dei_en);
		ppe_drv_trace("%p: policer action: yellow_pri: %d\n", p, action.yellow_priority);
		ppe_drv_trace("%p: policer action: yellow_dp: %d\n", p, action.yellow_drop_priority);
		ppe_drv_trace("%p: policer action: yellow_pcp: %d\n", p, action.yellow_pcp);
		ppe_drv_trace("%p: policer action: yellow_dei: %d\n", p, action.yellow_dei);
		ppe_drv_trace("%p: policer action: red_action: %d\n", p, action.red_action);
		ppe_drv_trace("%p: policer action: red_priority_en: %d\n", p, action.red_priority_en);
		ppe_drv_trace("%p: policer action: red_drop_priority_en: %d\n", p, action.red_drop_priority_en);
		ppe_drv_trace("%p: policer action: red_pcp_en: %d\n", p, action.red_pcp_en);
		ppe_drv_trace("%p: policer action: red_dei_en: %d\n", p, action.red_dei_en);
		ppe_drv_trace("%p: policer action: red_priority: %d\n", p, action.red_priority);
		ppe_drv_trace("%p: policer action: red_drop_priority: %d\n", p, action.red_drop_priority);
		ppe_drv_trace("%p: policer action: red_pcp: %d\n", p, action.red_pcp);
		ppe_drv_trace("%p: policer action: red_dei: %d\n", p, action.red_dei);

		ppe_drv_trace("%p: policer action: yellow_dscp_en: %d\n", p, action.yellow_dscp_en);
		ppe_drv_trace("%p: policer action: yellow_dscp: %d\n", p, action.yellow_dscp);
		ppe_drv_trace("%p: policer action: red_dscp_en: %d\n", p, action.red_dscp_en);
		ppe_drv_trace("%p: policer action: red dscp: %d\n", p, action.red_dscp);
		ppe_drv_trace("%p: policer action: yellow_remark_en: %d\n", p, action.yellow_remap_en);
		ppe_drv_trace("%p: policer action: red_remap_en: %d\n", p, action.red_remap_en);

		hw_index++;
	}

	spin_unlock_bh(&p->lock);
}

/*
 * ppe_drv_policer_port_dump()
 *	Dump Port policer rule tables.
 */
static void ppe_drv_policer_port_dump(struct ppe_drv_policer_port *pol)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_policer_config_t pol_cfg = {0};
	fal_policer_action_t action = {0};
	sw_error_t err = SW_OK;

	spin_lock_bh(&p->lock);

	err = fal_port_policer_entry_get(PPE_DRV_SWITCH_ID, pol->index, &pol_cfg, &action);
	if (err != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: Error in retreiving stats\n", p);
		return;
	}

	ppe_drv_trace("%p: policer config: meter_en: %d\n", p, pol_cfg.meter_en);
	ppe_drv_trace("%p: policer config: couple_en: %d\n", p, pol_cfg.couple_en);
	ppe_drv_trace("%p: policer config: color_mode: %d\n", p, pol_cfg.color_mode);
	ppe_drv_trace("%p: policer config: frame_type: %d\n", p, pol_cfg.frame_type);
	ppe_drv_trace("%p: policer config: meter_mode: %d\n", p, pol_cfg.meter_mode);
	ppe_drv_trace("%p: policer config: meter_unit: %d\n", p, pol_cfg.meter_unit);
	ppe_drv_trace("%p: policer config: cir: %d\n", p, pol_cfg.cir);
	ppe_drv_trace("%p: policer config: cbs: %d\n", p, pol_cfg.cbs);
	ppe_drv_trace("%p: policer config: eir: %d\n", p, pol_cfg.eir);
	ppe_drv_trace("%p: policer config: ebs: %d\n", p, pol_cfg.ebs);
	ppe_drv_trace("%p: policer config: meter_type: %d\n", p, pol_cfg.meter_type);


	/*
	 * Action configuration
	 */
	ppe_drv_trace("%p: policer action: yellow_pri_en: %d\n", p, action.yellow_priority_en);
	ppe_drv_trace("%p: policer action: yellow_dp_en: %d\n", p, action.yellow_drop_priority_en);
	ppe_drv_trace("%p: policer action: yellow_pcp_en: %d\n", p, action.yellow_pcp_en);
	ppe_drv_trace("%p: policer action: yellow_dei_en: %d\n", p, action.yellow_dei_en);
	ppe_drv_trace("%p: policer action: yellow_pri: %d\n", p, action.yellow_priority);
	ppe_drv_trace("%p: policer action: yellow_dp: %d\n", p, action.yellow_drop_priority);
	ppe_drv_trace("%p: policer action: yellow_pcp: %d\n", p, action.yellow_pcp);
	ppe_drv_trace("%p: policer action: yellow_dei: %d\n", p, action.yellow_dei);
	ppe_drv_trace("%p: policer action: red_action: %d\n", p, action.red_action);
	ppe_drv_trace("%p: policer action: red_priority_en: %d\n", p, action.red_priority_en);
	ppe_drv_trace("%p: policer action: red_drop_priority_en: %d\n", p, action.red_drop_priority_en);
	ppe_drv_trace("%p: policer action: red_pcp_en: %d\n", p, action.red_pcp_en);
	ppe_drv_trace("%p: policer action: red_dei_en: %d\n", p, action.red_dei_en);
	ppe_drv_trace("%p: policer action: red_priority: %d\n", p, action.red_priority);
	ppe_drv_trace("%p: policer action: red_drop_priority: %d\n", p, action.red_drop_priority);
	ppe_drv_trace("%p: policer action: red_pcp: %d\n", p, action.red_pcp);
	ppe_drv_trace("%p: policer action: red_dei: %d\n", p, action.red_dei);

	ppe_drv_trace("%p: policer action: yellow_dscp_en: %d\n", p, action.yellow_dscp_en);
	ppe_drv_trace("%p: policer action: yellow_dscp: %d\n", p, action.yellow_dscp);
	ppe_drv_trace("%p: policer action: red_dscp_en: %d\n", p, action.red_dscp_en);
	ppe_drv_trace("%p: policer action: red dscp: %d\n", p, action.red_dscp);
	ppe_drv_trace("%p: policer action: yellow_remark_en: %d\n", p, action.yellow_remap_en);
	ppe_drv_trace("%p: policer action: red_remap_en: %d\n", p, action.red_remap_en);
	spin_unlock_bh(&p->lock);
}
#else
static void ppe_drv_policer_acl_dump(struct ppe_drv_policer_acl *pol)
{
}

static void ppe_drv_policer_port_dump(struct ppe_drv_policer_port *pol)
{
}
#endif

/*
 * ppe_drv_policer_port_free()
 *	Policer port free
 */
static void ppe_drv_policer_port_free(struct ppe_drv_policer_port *pol)
{
	fal_policer_config_t pol_cfg = {0};
	fal_policer_action_t action = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	sw_error_t err = SW_OK;

	err = fal_port_policer_entry_set(PPE_DRV_SWITCH_ID, pol->index, &pol_cfg, &action);
	if (err != SW_OK) {
		ppe_drv_warn("%p: cannot configure hw for port policer\n", p);
		ppe_drv_stats_inc(&p->stats.policer_stats.fail_hw_port_policer_destroy_cfg);
		return;
	}

	memset(pol, 0, sizeof(struct ppe_drv_policer_port));
	ppe_drv_stats_inc(&p->stats.policer_stats.success_hw_port_policer_destroy_cfg);
}

/*
 * ppe_drv_policer_acl_free()
 *	Policer acl free
 */
static void ppe_drv_policer_acl_free(struct ppe_drv_policer_acl *pol)
{
	fal_policer_config_t pol_cfg = {0};
	fal_policer_action_t action = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	sw_error_t err = SW_OK;

	err = fal_acl_policer_entry_set(PPE_DRV_SWITCH_ID, pol->acl_index, &pol_cfg, &action);
	if (err != SW_OK) {
		ppe_drv_warn("%p: cannot configure hw for port policer\n", p);
		ppe_drv_stats_inc(&p->stats.policer_stats.fail_hw_acl_policer_destroy_cfg);
		return;
	}

	/*
	 * If Policer is configured for both US and DS, remove both rules
	 */
	if (pol->num_acl_reserved == PPE_DRV_POLICER_DIRECTION_MAX - 1) {
		struct ppe_drv_policer_acl *next_pol = &p->pol_ctx->acl_pol[pol->acl_index + 1];

		err = fal_acl_policer_entry_set(PPE_DRV_SWITCH_ID, pol->acl_index + 1, &pol_cfg, &action);
		if (err != SW_OK) {
			ppe_drv_warn("%p: cannot configure hw for port policer\n", p);
			ppe_drv_stats_inc(&p->stats.policer_stats.fail_hw_acl_policer_destroy_cfg);
			return;
		}
		memset(next_pol, 0, sizeof(struct ppe_drv_policer_acl));
	}

	memset(pol, 0, sizeof(struct ppe_drv_policer_acl));

	ppe_drv_stats_inc(&p->stats.policer_stats.success_hw_acl_policer_destroy_cfg);
}

/*
 * ppe_drv_policer_get_policer_id()
 *	Get acl id from policer context
 */
uint16_t ppe_drv_policer_get_policer_id(struct ppe_drv_policer_acl *ctx)
{
	struct ppe_drv *p = ppe_drv_gbl;
	uint16_t index;

	spin_lock_bh(&p->lock);
	index = ctx->acl_index;
	spin_unlock_bh(&p->lock);

	return index;
}
EXPORT_SYMBOL(ppe_drv_policer_get_policer_id);

/*
 * ppe_drv_policer_get_port_id()
 *	Get port id from policer context
 */
uint16_t ppe_drv_policer_get_port_id(struct ppe_drv_policer_port *ctx)
{
	struct ppe_drv *p = ppe_drv_gbl;
	uint16_t index;

	spin_lock_bh(&p->lock);
	index = ctx->index;
	spin_unlock_bh(&p->lock);

	return index;
}
EXPORT_SYMBOL(ppe_drv_policer_get_port_id);

/*
 * ppe_drv_policer_user2hw_id_map()
 *	User to HW mapping configuration
 */
void ppe_drv_policer_user2hw_id_map(struct ppe_drv_policer_acl *acl_ctx, int index)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_policer_ctx *ctx = p->pol_ctx;

	spin_lock_bh(&p->lock);
	ctx->user2hw_map[index] = acl_ctx->acl_index;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_policer_user2hw_id_map);

/*
 * ppe_drv_policer_user2hw_id()
 *	Get policer HW index from user policer context
 */
int ppe_drv_policer_user2hw_id(int index)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_policer_ctx *ctx = p->pol_ctx;
	int hw_index;

	spin_lock_bh(&p->lock);
	hw_index = ctx->user2hw_map[index];
	spin_unlock_bh(&p->lock);

	return hw_index;
}
EXPORT_SYMBOL(ppe_drv_policer_user2hw_id);

/*
 * ppe_drv_policer_port_destroy()
 *	Port policer destroy
 */
void ppe_drv_policer_port_destroy(struct ppe_drv_policer_port *ctx)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	if (!ctx->in_use) {
		ppe_drv_trace("%p: given port policer ctx not in use\n", ctx);
		spin_unlock_bh(&p->lock);
		return;
	}

	ppe_drv_policer_port_free(ctx);
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_policer_port_destroy);

/*
 * ppe_drv_policer_acl_destroy()
 *	ACL policer destroy
 */
void ppe_drv_policer_acl_destroy(struct ppe_drv_policer_acl *ctx)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	if (!ctx->in_use) {
		ppe_drv_trace("%p: given acl policer ctx not in use\n", ctx);
		spin_unlock_bh(&p->lock);
		return;
	}

	ppe_drv_policer_acl_free(ctx);
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_policer_acl_destroy);

/*
 * ppe_drv_policer_port_create()
 *	Allocates a free policer and takes a reference.
 */
struct ppe_drv_policer_port *ppe_drv_policer_port_create(struct ppe_drv_policer_rule_create *create)
{
	struct ppe_drv_policer_rule_create_port_info *pinfo = &create->msg.port_info;
	struct ppe_drv_policer_rule_create_action *rule_action = &pinfo->action;
	struct ppe_drv_policer_port *pol = NULL;
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_iface *iface;

	sw_error_t err = SW_OK;
	fal_policer_config_t pol_cfg = {0};
	fal_policer_action_t action = {0};
	fal_port_t port_id;

	iface = ppe_drv_iface_get_by_dev(pinfo->dev);
	if (!iface) {
		ppe_drv_warn("no interface: %s\n", pinfo->dev->name);
		return false;
	}

	port_id = ppe_drv_iface_port_idx_get(iface);
	if (port_id < 0) {
		ppe_drv_warn("Invalid port_num: %d\n", port_id);
		return false;
	}

	if (PPE_DRV_VIRTUAL_PORT_CHK(port_id)) {
		ppe_drv_warn("Metering not supported yet for virtual_port: %d\n", port_id);
		return false;
	}

	if (!PPE_DRV_PHY_PORT_CHK(port_id)) {
		ppe_drv_warn("Invalid Physical_port: %d\n", port_id);
		return false;
	}

	spin_lock_bh(&p->lock);

	/*
	 * Get a free policer entry from pool
	 */
	if (!p->pol_ctx->port_pol[port_id].in_use) {
		pol = &p->pol_ctx->port_pol[port_id];
	}

	if (!pol) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: cannot alloc policer, table full for type)", p);
		ppe_drv_stats_inc(&p->stats.policer_stats.fail_port_policer_full);
		return NULL;
	}

	/*
	 * Policer configuration
	 */
	pol_cfg.meter_en = pinfo->meter_en;	/* Default */
	pol_cfg.couple_en = pinfo->coupling_flag;	/* Default */
	pol_cfg.color_mode = pinfo->colour_mode;
	pol_cfg.frame_type = pinfo->meter_flag;
	pol_cfg.meter_mode = pinfo->meter_mode;			/* 0 - rfc2698, 1 - rfc2697, rfc4115, mef */
	pol_cfg.meter_unit = pinfo->meter_unit; 	/* 0 - byte based; 1 - packet based */

	if (pinfo->meter_unit == 0) {
		pol_cfg.cir = (pinfo->cir / 1000) * 8;		/* kbits for byte based */
		pol_cfg.eir = (pinfo->eir / 1000) * 8;		/* kbits for byte based */
	} else if (pinfo->meter_unit == 1) {
		pol_cfg.cir = pinfo->cir;
		pol_cfg.eir = pinfo->eir;
	}

	pol_cfg.cbs = pinfo->cbs;
	pol_cfg.ebs = pinfo->ebs;

	/* We dont support MEF as of now */
	pol_cfg.meter_type = FAL_POLICER_METER_RFC;	/* 0 - legacy feature, 1 - mef */

	/*
	 * Action configuration
	 */
	action.yellow_priority_en = rule_action->exceed_chg_pri;
	action.yellow_drop_priority_en = rule_action->exceed_chg_dp;
	action.yellow_pcp_en = rule_action->exceed_chg_pcp;
	action.yellow_dei_en = rule_action->exceed_chg_dei;
	action.yellow_priority = rule_action->exceed_pri;
	action.yellow_drop_priority = rule_action->exceed_dp;
	action.yellow_pcp = rule_action->exceed_pcp;
	action.yellow_dei = rule_action->exceed_dei;
	action.red_action = rule_action->red_drop;

	action.yellow_dscp_en = rule_action->exceed_chg_dscp;
	action.yellow_dscp = rule_action->exceed_dscp;
	action.red_dscp_en = rule_action->violate_chg_dscp;
	action.red_dscp = rule_action->violate_dscp;
	action.yellow_remap_en = rule_action->exceed_remap;

	err = fal_port_policer_entry_set(PPE_DRV_SWITCH_ID, port_id, &pol_cfg, &action);
	if (err != SW_OK) {
		ppe_drv_warn("%p: cannot configure hw for port policer\n", p);
		ppe_drv_stats_inc(&p->stats.policer_stats.fail_hw_port_policer_create_cfg);
		spin_unlock_bh(&p->lock);
		return NULL;
	}

	/*
	 * Take a reference. This will be let go once the user
	 * derefs this interface.
	 */
	pol->index = port_id;
	p->pol_ctx->port_pol[port_id].in_use = true;

	/*
	 * Fill port index in response
	 */
	create->hw_id = port_id;

	spin_unlock_bh(&p->lock);

	ppe_drv_policer_port_dump(pol);

	ppe_drv_stats_inc(&p->stats.policer_stats.success_hw_port_policer_create_cfg);

	return pol;
}
EXPORT_SYMBOL(ppe_drv_policer_port_create);

/*
 * ppe_drv_policer_acl_create()
 *	Allocates a free policer and takes a reference.
 *
 * TODO: Consider ACL based policer for Virtual Ports also.
 */
struct ppe_drv_policer_acl *ppe_drv_policer_acl_create(struct ppe_drv_policer_rule_create *create)
{
	struct ppe_drv_policer_rule_create_acl_info *ainfo = &create->msg.acl_info;
	struct ppe_drv_policer_rule_create_action *rule_action = &ainfo->action;
	struct ppe_drv_policer_acl *pol = NULL;
	struct ppe_drv *p = ppe_drv_gbl;
	fal_policer_config_t pol_cfg = {0};
	fal_policer_action_t action = {0};
	sw_error_t err = SW_OK;
	uint16_t index, hw_index;

	spin_lock_bh(&p->lock);

	/*
	 * Check if any policer configured at that rule ID for US or DS,
	 * use that index. Else get a free index based on US/DS direction.
	 */
	if (ainfo->dir_info.acl_index_valid) {
		index = ainfo->dir_info.acl_index;

		if (((ainfo->dir_info.dir == PPE_DRV_POLICER_DIRECTION_US)
		&& (!p->pol_ctx->acl_pol[index].is_configured))
		||((ainfo->dir_info.dir == PPE_DRV_POLICER_DIRECTION_DS)
		&& (!p->pol_ctx->acl_pol[index + 1].is_configured))) {
			pol = &p->pol_ctx->acl_pol[index];
			goto process;
		} else {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("%p: Policer index already configured:", p);
			return NULL;
		}
	}

	/*
	 * Get a free policer entry from pool
	 * In case direction is present, search for a subsequent
	 * pair of indexes. Both index and index + 1 would be
	 * reserved to keep the US and DS rules together.
	 */
	for (index = 0; index < PPE_DRV_ACL_POLICER_MAX; index++) {
		if (!p->pol_ctx->acl_pol[index].in_use) {
			if (ainfo->dir_info.dir != PPE_DRV_POLICER_DIRECTION_INVALID) {
				if ((index + 1) >= PPE_DRV_ACL_POLICER_MAX || p->pol_ctx->acl_pol[index + 1].in_use) {
					continue;
				}
			}
			pol = &p->pol_ctx->acl_pol[index];
			break;
		}
	}

process:
	if (!pol) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: cannot alloc policer, table full for type", p);
		ppe_drv_stats_inc(&p->stats.policer_stats.fail_acl_policer_full);
		return NULL;
	}

	/*
	 * Policer configuration
	 */
	pol_cfg.meter_en = ainfo->meter_en;	/* Default */
	pol_cfg.couple_en = ainfo->coupling_flag;	/* Default */
	pol_cfg.color_mode = ainfo->colour_mode;
	pol_cfg.meter_mode = ainfo->meter_mode;		/* 0 - rfc2698, 1 - rfc2697, rfc4115, mef */
	pol_cfg.meter_unit = ainfo->meter_unit;		/* 0 - byte based; 1 - packet based */
	pol_cfg.length_mode = ainfo->length_mode;	/* Frame length mode for rate limit calculation: 0 - original, 1 - decap, 2 - output */

	if (ainfo->meter_unit == 0) {
		pol_cfg.cir = div_u64((uint64_t)ainfo->cir * 8, 1000);		/* kbits for byte based */
		pol_cfg.eir = div_u64((uint64_t)ainfo->eir * 8, 1000);		/* kbits for byte based */
	} else if (ainfo->meter_unit == 1) {
		pol_cfg.cir = ainfo->cir;
		pol_cfg.eir = ainfo->eir;
	}

	pol_cfg.cbs = ainfo->cbs;
	pol_cfg.ebs = ainfo->ebs;

	/* We dont support MEF as of now */
	pol_cfg.meter_type = FAL_POLICER_METER_RFC;	/* 0 - legacy feature, 1 - mef */

	/*
	 * Action configuration
	 */
	action.yellow_priority_en = rule_action->exceed_chg_pri;
	action.yellow_drop_priority_en = rule_action->exceed_chg_dp;
	action.yellow_pcp_en = rule_action->exceed_chg_pcp;
	action.yellow_dei_en = rule_action->exceed_chg_dei;
	action.yellow_priority = rule_action->exceed_pri;
	action.yellow_drop_priority = rule_action->exceed_dp;
	action.yellow_pcp = rule_action->exceed_pcp;
	action.yellow_dei = rule_action->exceed_dei;
	action.red_action = rule_action->red_drop;
	action.red_priority_en = rule_action->violate_chg_pri;
	action.red_drop_priority_en = rule_action->violate_chg_dp;
	action.red_pcp_en = rule_action->violate_chg_pcp;
	action.red_dei_en = rule_action->violate_chg_dei;
	action.red_priority = rule_action->violate_pri;
	action.red_drop_priority = rule_action->violate_dp;
	action.red_pcp = rule_action->violate_pcp;
	action.red_dei = rule_action->violate_dei;

	action.yellow_dscp_en = rule_action->exceed_chg_dscp;
	action.yellow_dscp = rule_action->exceed_dscp;
	action.red_dscp_en = rule_action->violate_chg_dscp;
	action.red_dscp = rule_action->violate_dscp;
	action.yellow_remap_en = rule_action->exceed_remap;
	action.red_remap_en = rule_action->violate_remap;

	hw_index = index;
	if (ainfo->dir_info.dir == PPE_DRV_POLICER_DIRECTION_DS) {
		hw_index = index + 1;
	}

	err = fal_acl_policer_entry_set(PPE_DRV_SWITCH_ID, hw_index, &pol_cfg, &action);
	if (err != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: cannot configure hw for acl policer\n", p);
		ppe_drv_stats_inc(&p->stats.policer_stats.fail_hw_acl_policer_destroy_cfg);
		return NULL;
	}

	/*
	 * Take a reference. This will be let go once the user
	 * derefs this interface.
	 */
	pol->acl_index = index;
	p->pol_ctx->acl_pol[index].in_use = true;

	/*
	 * Fill acl index in response
	 */
	create->hw_id = index;

	/*
	 * Fill US index in case direction is provided.
	 * In case we reserve a pair for both US/DS policer,
	 * mark both indexes in use.
	 */
	if (ainfo->dir_info.dir == PPE_DRV_POLICER_DIRECTION_INVALID) {
		p->pol_ctx->acl_pol[index].is_configured = true;
		p->pol_ctx->acl_pol[index].num_acl_reserved++;
	} else {
		p->pol_ctx->acl_pol[index].num_acl_reserved = PPE_DRV_POLICER_DIRECTION_MAX - 1;

		if (!ainfo->dir_info.acl_index_valid) {
			p->pol_ctx->acl_pol[index + 1].acl_index = index + 1;
			p->pol_ctx->acl_pol[index + 1].in_use = true;
		}

		if (ainfo->dir_info.dir == PPE_DRV_POLICER_DIRECTION_US) {
			p->pol_ctx->acl_pol[index].is_configured = true;
		} else {
			p->pol_ctx->acl_pol[index + 1].is_configured = true;
		}
	}

	spin_unlock_bh(&p->lock);

	ppe_drv_policer_acl_dump(pol);
	ppe_drv_stats_inc(&p->stats.policer_stats.success_hw_acl_policer_create_cfg);

	return pol;
}
EXPORT_SYMBOL(ppe_drv_policer_acl_create);

/*
 * ppe_drv_policer_flow_unregister_cb
 *	Flow unregister callback
 */
void ppe_drv_policer_flow_unregister_cb(void)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_policer_ctx *ctx = p->pol_ctx;

	spin_lock_bh(&p->lock);
	ctx->flow_add_cb = NULL;
	ctx->flow_del_cb = NULL;
	ctx->flow_app_data = NULL;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_policer_flow_unregister_cb);

/*
 * ppe_drv_policer_flow_register_cb
 *	Flow register callback
 */
void ppe_drv_policer_flow_register_cb(ppe_drv_policer_flow_callback_t add_cb, ppe_drv_policer_flow_callback_t del_cb, void *app_data)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_policer_ctx *ctx = p->pol_ctx;

	spin_lock_bh(&p->lock);
	ctx->flow_add_cb = add_cb;
	ctx->flow_del_cb = del_cb;
	ctx->flow_app_data = app_data;
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_policer_flow_register_cb);

/*
 * ppe_drv_policer_entries_free()
 *	Free policer global ctx table entries if it was allocated.
 */
void ppe_drv_policer_entries_free(struct ppe_drv_policer_ctx *ctx)
{
	nss_ppe_drv_minidump_free(ctx, "ppe_drv_policer_ctx");
	kfree(ctx);
}

/*
 * ppe_drv_policer_entries_alloc()
 *	Allocated and initialize policer global context.
 */
struct ppe_drv_policer_ctx *ppe_drv_policer_entries_alloc(void)
{
	struct ppe_drv_policer_ctx *pol;
	struct ppe_drv_policer_acl *acl;
	struct ppe_drv_policer_port *port;
	struct ppe_drv *p = ppe_drv_gbl;
	uint16_t i;

	pol = kzalloc(sizeof(struct ppe_drv_policer_ctx), GFP_KERNEL);
	if (!pol) {
		ppe_drv_warn("%p: failed to allocate policer entries", p);
		return NULL;
	}

	nss_ppe_drv_minidump_log(pol, sizeof(struct ppe_drv_policer_ctx), "ppe_drv_policer_ctx");

	/*
	 * Initialize interface values
	 */
	acl = pol->acl_pol;
	for (i = 0; i < PPE_DRV_ACL_POLICER_MAX; i++) {
		acl[i].acl_index = i;
		acl[i].in_use = false;
	}

	port = pol->port_pol;
	for (i = 0; i < PPE_DRV_PORT_POLICER_MAX; i++) {
		port[i].index = i;
		port[i].in_use = false;
	}

	return pol;
}

/*
 * ppe_drv_port_policer_stats_update()
 *	Update hardware match counters.
 */
void ppe_drv_port_policer_stats_update(struct ppe_drv_policer_port *ctx)
{
	sw_error_t err;
	struct ppe_drv_policer_stat delta = {0};
	fal_policer_counter_t policer_cntrs = {0};

	ppe_drv_trace("%p: updating port poliecr stats", ctx);

	err = fal_port_policer_counter_get(PPE_DRV_SWITCH_ID, ctx->index, &policer_cntrs);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to get stats for port policer index: %u",
				ctx, ctx->index);
		return;
	}

	/*
	 * PPE stats are not clear on read, so we need to calculate the delta
	 * between the latest counters and previously read counters.
	 */
	delta.green_pkts = PPE_DRV_POLICER_PKT_CNTR_ROLLOVER(policer_cntrs.green_packet_counter - ctx->pre_cntrs.green_packet_counter);
	delta.green_bytes = PPE_DRV_POLICER_BYTE_CNTR_ROLLOVER(policer_cntrs.green_byte_counter - ctx->pre_cntrs.green_byte_counter);
	delta.yellow_pkts = PPE_DRV_POLICER_PKT_CNTR_ROLLOVER(policer_cntrs.yellow_packet_counter - ctx->pre_cntrs.yellow_packet_counter);
	delta.yellow_bytes = PPE_DRV_POLICER_BYTE_CNTR_ROLLOVER(policer_cntrs.yellow_byte_counter - ctx->pre_cntrs.yellow_byte_counter);

	delta.red_pkts = PPE_DRV_POLICER_PKT_CNTR_ROLLOVER(policer_cntrs.red_packet_counter - ctx->pre_cntrs.red_packet_counter);
	delta.red_bytes = PPE_DRV_POLICER_BYTE_CNTR_ROLLOVER(policer_cntrs.red_byte_counter - ctx->pre_cntrs.red_byte_counter);

	/*
	 * Update hardware stats packet and byte counters
	 */
	ppe_drv_port_policer_stats_add(ctx, &delta);

	/*
	 * Store current stats for next iteration.
	 */
	ctx->pre_cntrs.green_packet_counter = policer_cntrs.green_packet_counter;
	ctx->pre_cntrs.green_byte_counter = policer_cntrs.green_byte_counter;
	ctx->pre_cntrs.yellow_packet_counter = policer_cntrs.yellow_packet_counter;
	ctx->pre_cntrs.yellow_byte_counter = policer_cntrs.yellow_byte_counter;
	ctx->pre_cntrs.red_packet_counter = policer_cntrs.red_packet_counter;
	ctx->pre_cntrs.red_byte_counter = policer_cntrs.red_byte_counter;
}

/*
 * ppe_drv_acl_policer_stats_update()
 *	Update hardware match counters.
 */
void ppe_drv_acl_policer_stats_update(struct ppe_drv_policer_acl *ctx)
{
	sw_error_t err;
	struct ppe_drv_policer_stat delta = {0};
	fal_policer_counter_t policer_cntrs = {0};

	ppe_drv_trace("%p: updating acl poliecr stats", ctx);

	err = fal_acl_policer_counter_get(PPE_DRV_SWITCH_ID, ctx->acl_index, &policer_cntrs);
	if (err != SW_OK) {
		ppe_drv_warn("%p: failed to get stats for acl policer index: %u",
				ctx, ctx->acl_index);
		return;
	}

	/*
	 * PPE stats are not clear on read, so we need to calculate the delta
	 * between the latest counters and previously read counters.
	 */
	delta.green_pkts = PPE_DRV_POLICER_PKT_CNTR_ROLLOVER(policer_cntrs.green_packet_counter - ctx->pre_cntrs.green_packet_counter);
	delta.green_bytes = PPE_DRV_POLICER_BYTE_CNTR_ROLLOVER(policer_cntrs.green_byte_counter - ctx->pre_cntrs.green_byte_counter);
	delta.yellow_pkts = PPE_DRV_POLICER_PKT_CNTR_ROLLOVER(policer_cntrs.yellow_packet_counter - ctx->pre_cntrs.yellow_packet_counter);
	delta.yellow_bytes = PPE_DRV_POLICER_BYTE_CNTR_ROLLOVER(policer_cntrs.yellow_byte_counter - ctx->pre_cntrs.yellow_byte_counter);

	delta.red_pkts = PPE_DRV_POLICER_PKT_CNTR_ROLLOVER(policer_cntrs.red_packet_counter - ctx->pre_cntrs.red_packet_counter);
	delta.red_bytes = PPE_DRV_POLICER_BYTE_CNTR_ROLLOVER(policer_cntrs.red_byte_counter - ctx->pre_cntrs.red_byte_counter);

	/*
	 * Update hardware stats packet and byte counters
	 */
	ppe_drv_acl_policer_stats_add(ctx, &delta);

	/*
	 * Store current stats for next iteration.
	 */
	ctx->pre_cntrs.green_packet_counter = policer_cntrs.green_packet_counter;
	ctx->pre_cntrs.green_byte_counter = policer_cntrs.green_byte_counter;
	ctx->pre_cntrs.yellow_packet_counter = policer_cntrs.yellow_packet_counter;
	ctx->pre_cntrs.yellow_byte_counter = policer_cntrs.yellow_byte_counter;
	ctx->pre_cntrs.red_packet_counter = policer_cntrs.red_packet_counter;
	ctx->pre_cntrs.red_byte_counter = policer_cntrs.red_byte_counter;
}

/*
 * ppe_drv_policer_port_get_hw_stats
 * 	Get hw stats for port policer.
 */
void ppe_drv_policer_port_get_hw_stats(struct ppe_drv_policer_hw_stats *pol_ctx, struct ppe_drv_policer_port *port_ctx)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);

	pol_ctx->hw_cntrs.gpc = atomic64_read(&port_ctx->green_packet_counter);
	pol_ctx->hw_cntrs.gbc = atomic64_read(&port_ctx->green_byte_counter);
	pol_ctx->hw_cntrs.ypc = atomic64_read(&port_ctx->yellow_packet_counter);
	pol_ctx->hw_cntrs.ybc = atomic64_read(&port_ctx->yellow_byte_counter);
	pol_ctx->hw_cntrs.rpc = atomic64_read(&port_ctx->red_packet_counter);
	pol_ctx->hw_cntrs.rbc = atomic64_read(&port_ctx->red_byte_counter);

	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_policer_port_get_hw_stats);

/*
 * ppe_drv_policer_acl_get_hw_stats
 * 	Get hw stats for acl policer.
 */
void ppe_drv_policer_acl_get_hw_stats(struct ppe_drv_policer_hw_stats *pol_ctx, struct ppe_drv_policer_acl *acl_ctx, ppe_drv_policer_direction_t dir)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	/*
	 * Get the ACL context in case of DS direction
	 */
	if (dir == PPE_DRV_POLICER_DIRECTION_DS) {
		acl_ctx = &p->pol_ctx->acl_pol[acl_ctx->acl_index + 1];
	}

	pol_ctx->hw_cntrs.gpc = atomic64_read(&acl_ctx->green_packet_counter);
	pol_ctx->hw_cntrs.gbc = atomic64_read(&acl_ctx->green_byte_counter);
	pol_ctx->hw_cntrs.ypc = atomic64_read(&acl_ctx->yellow_packet_counter);
	pol_ctx->hw_cntrs.ybc = atomic64_read(&acl_ctx->yellow_byte_counter);
	pol_ctx->hw_cntrs.rpc = atomic64_read(&acl_ctx->red_packet_counter);
	pol_ctx->hw_cntrs.rbc = atomic64_read(&acl_ctx->red_byte_counter);

	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_policer_acl_get_hw_stats);
