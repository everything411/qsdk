/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <ppe_drv_public.h>
#include <ppe_drv_dot1p.h>
#include "ppe_dot1p.h"

/*
 * Global DOT1P context
 */
struct ppe_dot1p_base ppe_dot1p_gbl = {0};

/*
 * ppe_dot1p_alloc()
 *      Allocate memory for a DOT1P rule.
 */
static inline struct ppe_dot1p *ppe_dot1p_alloc(void)
{
	/*
	 * Allocate memory for a new rule
	 *
	 * kzalloc with GFP_ATOMIC is used assuming DOT1P rules can be created
	 * in softirq context as well while processing an SKB in the system.
	 */
	return kzalloc(sizeof(struct ppe_dot1p), GFP_ATOMIC);
}

/*
 * ppe_dot1p_policer_alloc()
 *      Allocate memory for a DOT1P policer rule.
 */
static inline struct ppe_dot1p_policer *ppe_dot1p_policer_alloc(void)
{
	/*
	 * Allocate memory for a new rule
	 *
	 * kzalloc with GFP_ATOMIC is used assuming DOT1P policer rules can be created
	 * in softirq context as well while processing an SKB in the system.
	 */
	return kzalloc(sizeof(struct ppe_dot1p_policer), GFP_ATOMIC);
}

/*
 * ppe_dot1p_free()
 *      Free v4 connections.
 */
static inline void ppe_dot1p_free(struct ppe_dot1p *dot1p)
{
	kfree(dot1p);
}

/*
 * ppe_dot1p_policer_free()
 *      Free v4 connections.
 */
static inline void ppe_dot1p_policer_free(struct ppe_dot1p_policer *dot1p_policer)
{
	kfree(dot1p_policer);
}

/*
 * ppe_dot1p_rule_find_by_id()
 *	Find a rule corresponding to a rule ID.
 */
static struct ppe_dot1p *ppe_dot1p_rule_find_by_id(ppe_dot1p_rule_id_t id)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p *dot1p;

	if (id > PPE_DOT1P_MAX_RULE_ID) {
		ppe_dot1p_warn("%p: Invalid rule ID: %d", dot1p_g, id);
		return NULL;
	}

	/*
	 * Get the first available dot1p rule ID.
	 */
	if (!list_empty(&dot1p_g->active_rules)) {
		list_for_each_entry(dot1p, &dot1p_g->active_rules, list) {
			if (dot1p->rule_id == id) {
				ppe_dot1p_info("%p: DOT1P rule: %p found for ID: %d",
					dot1p_g, dot1p, id);
				return dot1p;
			}
		}
	}

	/*
	 * Update stats
	 */
	ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_rule_not_found);
	ppe_dot1p_warn("%p: No valid DOT1P rule for ID: %d", dot1p_g, id);
	return NULL;
}

/*
 * ppe_dot1p_policer_rule_find_by_gemport_id()
 *	Find a policer rule corresponding to a gemport ID.
 */
static struct ppe_dot1p_policer *ppe_dot1p_policer_rule_find_by_gemport_id(uint16_t gemport_id)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p_policer *dot1p;

	if (gemport_id >= PPE_DOT1P_MAX_GEMPORT_VAL) {
		ppe_dot1p_warn("%p: Invalid gemport ID: %d", dot1p_g, gemport_id);
		return NULL;
	}

	/*
	 * Get the first available dot1p policer rule gemport ID.
	 */
	if (!list_empty(&dot1p_g->active_policer_rules)) {
		list_for_each_entry(dot1p, &dot1p_g->active_policer_rules, list) {
			if (dot1p->gemport_id == gemport_id) {
				ppe_dot1p_info("%p: DOT1P policer rule: %p found for gemport id: %d",
					dot1p_g, dot1p, gemport_id);
				return dot1p;
			}
		}
	}

	/*
	 * Update stats
	 */
	ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_policer_rule_not_found);
	ppe_dot1p_warn("%p: No valid DOT1P policer rule for gemport ID: %d", dot1p_g, gemport_id);
	return NULL;
}

/*
 * ppe_dot1p_rule_free()
 *	Free the dot1p rule.
 */
static void ppe_dot1p_rule_free(struct kref *kref)
{
	struct ppe_dot1p *dot1p = container_of(kref, struct ppe_dot1p, ref_cnt);
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;

	/*
	 * Delete the rule node from active list.
	 */
	list_del(&dot1p->list);

	/*
	 * Delete the rule in PPE driver.
	 */
	if (dot1p->ctx) {
		ppe_drv_dot1p_delete(dot1p->ctx, dot1p->info.action.gem_port);
		dot1p->ctx = NULL;
	}

	ppe_dot1p_info("%p: DOT1P rule freed: %u", dot1p, dot1p->rule_id);

	/*
	 * free the dot1p rule memory.
	 */
	ppe_dot1p_free(dot1p);

	/*
	 * Update stats
	 */
	ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_free_req);
}

/*
 * ppe_dot1p_policer_rule_free()
 *	Free the dot1p policer rule.
 */
static void ppe_dot1p_policer_rule_free(struct kref *kref)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p_policer *dot1p = container_of(kref, struct ppe_dot1p_policer, ref_cnt);

	/*
	 * Delete the rule node from active list.
	 */
	list_del(&dot1p->list);

	/*
	 * Delete the rule in PPE driver.
	 */
	if (dot1p->ctx) {
		ppe_drv_dot1p_policer_delete(dot1p->ctx, dot1p->gemport_id);
		dot1p->ctx = NULL;
	}

	ppe_dot1p_info("%p: DOT1P policer rule freed: %u", dot1p, dot1p->gemport_id);

	/*
	 * free the dot1p rule memory.
	 */
	ppe_dot1p_policer_free(dot1p);

	/*
	 * Update stats
	 */
	ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_policer_free_req);
}

/*
 * ppe_dot1p_def_rule_info_fill()
 *	Fill the ppe-drv default rule structure based on user information.
 */
static bool ppe_dot1p_def_rule_info_fill(struct ppe_dot1p *dot1p, struct ppe_dot1p_def_rule *r, uint8_t rule_state)
{
	struct ppe_drv_dot1p_rule *info = &dot1p->info;
	struct ppe_drv_dot1p_def_rule_match *rule_f = &info->def_rule;


	info->rule_valid_flags |= PPE_DRV_DOT1P_DEFAULT_RULE_FLAG;

	if (r->def_rule_flags & PPE_DOT1P_RULE_FLAG_VID) {
			rule_f->vid = r->vid;
			rule_f->def_rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_VID;
	}

	if (r->def_rule_flags & PPE_DOT1P_RULE_FLAG_PCP) {
			rule_f->pcp = r->pcp;
			rule_f->def_rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_PCP;
	}

	if (r->def_rule_flags & PPE_DOT1P_RULE_FLAG_DEI) {
			rule_f->dei = r->dei;
			rule_f->def_rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_DEI;
	}

	if (r->def_rule_flags & PPE_DOT1P_RULE_FLAG_DSCP) {
			rule_f->dscp = r->dscp;
			rule_f->def_rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_DSCP;
	}

	if (r->def_rule_flags & PPE_DOT1P_RULE_FLAG_DSCP_MASK) {
			rule_f->dscp_mask = r->dscp_mask;
			rule_f->def_rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_DSCP_MASK;
	}

	if (r->def_rule_flags & PPE_DOT1P_RULE_FLAG_GEN_MISS_CMD) {
			rule_f->gen_miss_cmd = (ppe_drv_dot1p_cmd_t)r->gen_miss_cmd;
			rule_f->def_rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_GEN_MISS_CMD;
	}

	if (rule_state != PPE_DOT1P_RESUME) {
		rule_f->gen_miss_cmd = PPE_DRV_DOT1P_CMD_DROP;
		rule_f->def_rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_GEN_MISS_CMD;
	}

	return true;
}

/*
 * ppe_dot1p_rule_info_fill()
 *	Fill the ppe-drv rule structure based on user information.
 */
static bool ppe_dot1p_rule_info_fill(struct ppe_dot1p *dot1p, struct ppe_dot1p_rule_match *r)
{
	struct ppe_drv_dot1p_rule *info = &dot1p->info;
	struct ppe_drv_dot1p_rule_match *rule_f = &info->rule;
	struct ppe_drv_iface *iface;
	struct net_device *dev;
	int32_t port_num;

	if ((r->rule_flags & PPE_DOT1P_RULE_FLAG_SRC_INFO) &&
		(r->rule_flags & PPE_DOT1P_RULE_FLAG_SRC_PORT_TYPE)) {
		int temp_val, ret = 0;
		if (*r->src_info == '\0') {
			ppe_dot1p_warn("source info is NULL or empty\n");
			return false;
		}
		switch (r->src_port_type) {
		case PPE_DOT1P_PORT_TYPE_BITMAP:
			rule_f->src_port_type = PPE_DRV_DOT1P_PORT_TYPE_BITMAP;
			ret = kstrtoint(r->src_info, 10, &temp_val);
			if (ret != 0) {
				ppe_dot1p_warn("Conversion error: %d\n", ret);
				return false;
			}
			rule_f->src_port = (uint8_t)temp_val;
			break;

		case PPE_DOT1P_PORT_TYPE_PORT:
			rule_f->src_port_type = PPE_DRV_DOT1P_PORT_TYPE_PORT;
			dev = dev_get_by_name(&init_net, r->src_info);
			if (!dev) {
				ppe_dot1p_warn("%p: failed to find valid src for dev %s\n",
						r, r->src_info);
				return false;
			}

			iface = ppe_drv_iface_get_by_dev(dev);
			if (!iface) {
				ppe_dot1p_warn("%p: failed to find PPE interface for dev: %p(%s)\n",
						r, dev, r->src_info);
				dev_put(dev);
				return false;
			}

			port_num = ppe_drv_iface_port_idx_get(iface);
			if (port_num < 0) {
				ppe_dot1p_warn("%p: failed to find PPE port for iface: %p\n",
						r, iface);
				dev_put(dev);
				return false;
			}

			ppe_dot1p_info("%p: Source interface port number: %d\n", r, port_num);

			dev_put(dev);
			rule_f->src_port = port_num;
			break;

		default:
			ppe_dot1p_warn("Incorrect input for src_port_type parameter: %d\n",
					r->src_port_type);
			return false;
		}

		rule_f->rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_SRC_INFO;
	}

	if ((r->rule_flags & PPE_DOT1P_RULE_FLAG_DST_INFO) &&
		(r->rule_flags & PPE_DOT1P_RULE_FLAG_DST_PORT_TYPE)) {
		int temp_val, ret = 0;
		if (*r->dst_info == '\0') {
			ppe_dot1p_warn("destination info is NULL or empty\n");
			return false;
		}

		switch (r->dst_port_type) {
		case PPE_DOT1P_PORT_TYPE_BITMAP:
			rule_f->dst_port_type = PPE_DRV_DOT1P_PORT_TYPE_BITMAP;
			ret = kstrtoint(r->dst_info, 10, &temp_val);
			if (ret != 0) {
				ppe_dot1p_warn("Conversion error: %d\n", ret);
				return false;
			}
			rule_f->dst_port = (uint8_t)temp_val;
			break;

		case PPE_DOT1P_PORT_TYPE_PORT:
			rule_f->dst_port_type = PPE_DRV_DOT1P_PORT_TYPE_PORT;
			dev = dev_get_by_name(&init_net, r->dst_info);
			if (!dev) {
				ppe_dot1p_warn("%p: failed to find valid dst for dev %s\n",
						r, r->dst_info);
				return false;
			}

			iface = ppe_drv_iface_get_by_dev(dev);
			if (!iface) {
				ppe_dot1p_warn("%p: failed to find PPE interface for dev: %p(%s)\n",
						r, dev, r->dst_info);
				dev_put(dev);
				return false;
			}

			port_num = ppe_drv_iface_port_idx_get(iface);
			if (port_num < 0) {
				ppe_dot1p_warn("%p: failed to find PPE port for iface: %p\n",
						r, iface);
				dev_put(dev);
				return false;
			}

			ppe_dot1p_info("%p: Destination interface port number: %d\n", r, port_num);

			dev_put(dev);
			rule_f->dst_port = port_num;
			break;

		default:
			ppe_dot1p_warn("Incorrect input for dst_port_type parameter: %d\n",
					r->dst_port_type);
			return false;

		}

		rule_f->rule_flags |= PPE_DRV_DOT1P_RULE_FLAG_DST_INFO;
	}

	if (r->rule_flags & PPE_DOT1P_RULE_FLAG_VID) {
			rule_f->vid = r->vid;
			rule_f->rule_flags = rule_f->rule_flags | PPE_DRV_DOT1P_RULE_FLAG_VID;
	}

	if (r->rule_flags & PPE_DOT1P_RULE_FLAG_PCP) {
			rule_f->pcp = r->pcp;
			rule_f->rule_flags = rule_f->rule_flags | PPE_DRV_DOT1P_RULE_FLAG_PCP;
	}

	if (r->rule_flags & PPE_DOT1P_RULE_FLAG_DEI) {
			rule_f->dei = r->dei;
			rule_f->rule_flags = rule_f->rule_flags | PPE_DRV_DOT1P_RULE_FLAG_DEI;
	}

	if (r->rule_flags & PPE_DOT1P_RULE_FLAG_DSCP) {
			rule_f->dscp = r->dscp;
			rule_f->rule_flags = rule_f->rule_flags | PPE_DRV_DOT1P_RULE_FLAG_DSCP;
	}
	return true;
}

/*
 * ppe_dot1p_action_fill()
 *	Action corresponding to a DOT1P rule.
 */
static bool ppe_dot1p_action_fill(struct ppe_dot1p *dot1p, struct ppe_dot1p_rule_action *r_action, uint8_t rule_state)
{
	struct ppe_drv_dot1p_rule *info = &dot1p->info;
	struct ppe_drv_dot1p_action *dot1p_action = &info->action;
	struct ppe_dot1p_rule *rule_info = &dot1p->rule;
	struct ppe_dot1p_rule_action *dot1p_rule_action = &rule_info->action;
#ifdef NSS_PPE_PON_SUPPORT
	uint8_t enq_vp, int_pri;
#endif
	struct ppe_drv_iface *iface;
	struct net_device *dev;
	int32_t port_num;

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_GEM_PORT_ID) {
		dot1p->gem_port_id = r_action->gem_port;
		dot1p_action->gem_port = r_action->gem_port;
		dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_ACTION_FLAG_GEM_PORT_ID;
	}

#ifdef NSS_PPE_PON_SUPPORT
	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_PQ) {
		if (r_action->pq >= 0) {
			dot1p_action->pq = r_action->pq;
			dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_ACTION_FLAG_PQ;
			if (!ppe_drv_pon_get_pq_config(r_action->pq, &enq_vp, &int_pri)) {
				return false;
			}
			dot1p_action->enq_vp = enq_vp;
			dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_ACTION_FLAG_ENQ_VP;

			dot1p_rule_action->enq_vp = enq_vp;
			dot1p_rule_action->action_flags = dot1p_rule_action->action_flags | PPE_DOT1P_ACTION_FLAG_ENQ_VP;

			dot1p_action->int_pri = int_pri;
			dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_ACTION_FLAG_INT_PRI;

			dot1p_rule_action->int_pri = int_pri;
			dot1p_rule_action->action_flags = dot1p_rule_action->action_flags | PPE_DOT1P_ACTION_FLAG_INT_PRI;
		}
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_BASE_PQ) {
		if (r_action->base_pq >= 0) {
			dot1p_action->base_pq = r_action->pq;
			dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_ACTION_FLAG_BASE_PQ;
			if (ppe_drv_pon_map_enq_vp_to_base_pq(r_action->base_pq, &enq_vp) != PPE_DRV_RET_SUCCESS) {
				return false;
			}
			dot1p_action->enq_vp = enq_vp;
			dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_ACTION_FLAG_ENQ_VP;

			dot1p_rule_action->enq_vp = enq_vp;
			dot1p_rule_action->action_flags = dot1p_rule_action->action_flags | PPE_DOT1P_ACTION_FLAG_ENQ_VP;
		}
	}
#endif
	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_SVC_CODE) {
		if (r_action->svc_code) {
			dot1p_action->svc_code = r_action->svc_code;
			dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_ACTION_FLAG_SVC_CODE;
		}
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_INT_DP) {
		dot1p_action->int_dp = r_action->int_dp;
		dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_ACTION_FLAG_INT_DP;
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_DST_INFO) {
		if (*r_action->dst_info == '\0') {
			ppe_dot1p_warn("Destination info is NULL or empty\n");
			return false;
		}

		dev = dev_get_by_name(&init_net, r_action->dst_info);
		if (!dev) {
			ppe_dot1p_warn("%p: failed to find valid dev %s\n",
					r_action, r_action->dst_info);
			return false;
		}

		iface = ppe_drv_iface_get_by_dev(dev);
		if (!iface) {
			ppe_dot1p_warn("%p: failed to find PPE interface for dev: %p(%s)\n",
					r_action, dev, r_action->dst_info);
			dev_put(dev);
			return false;
		}

		port_num = ppe_drv_iface_port_idx_get(iface);
		if (port_num < 0) {
			ppe_dot1p_warn("%p: failed to find PPE port for iface: %p\n",
					r_action, iface);
			dev_put(dev);
			return false;
		}

		ppe_dot1p_info("%p: Destination port number: %d\n", r_action, port_num);
		dev_put(dev);
		dot1p_action->dst_port = port_num;
		dot1p_action->action_flags |= PPE_DRV_DOT1P_ACTION_FLAG_DST_INFO;
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_FWD_CMD) {
		if (rule_state != PPE_DOT1P_RESUME) {
			ppe_dot1p_warn("dot1p rule state is paused %d", rule_state);
			dot1p_action->fwd_cmd = PPE_DRV_DOT1P_CMD_DROP;
			dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_ACTION_FLAG_FWD_CMD;
		} else {
			dot1p_action->fwd_cmd = (ppe_drv_dot1p_cmd_t)r_action->fwd_cmd;
			dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_ACTION_FLAG_FWD_CMD;
		}
	}
	return true;
}

/*
 * ppe_dot1p_policer_rule_info_fill()
 *	Fill the ppe-drv rule structure based on user information.
 */
static bool ppe_dot1p_policer_rule_info_fill(struct ppe_dot1p_policer *dot1p, struct ppe_dot1p_policer_rule_match *r)
{
	struct ppe_drv_dot1p_policer_rule *info = &dot1p->info;
	struct ppe_drv_dot1p_policer_rule_match *rule_f = &info->rule;

	if (r->rule_flags & PPE_DOT1P_POLICER_RULE_FLAG_GEM_PORT_ID) {
		rule_f->gemport_id = r->gemport_id;
		rule_f->rule_flags = rule_f->rule_flags | PPE_DRV_DOT1P_POLICER_RULE_FLAG_GEMPORT_ID;
	}

	ppe_dot1p_info("%p: policer rule gemport info: %d\n", rule_f, rule_f->gemport_id);
	return true;
}

/*
 * ppe_dot1p_policer_action_fill()
 *	Action corresponding to a DOT1P rule.
 */
static bool ppe_dot1p_policer_action_fill(struct ppe_dot1p_policer *dot1p, struct ppe_dot1p_policer_rule_action *r_action)
{
	struct ppe_drv_dot1p_policer_rule *info = &dot1p->info;
	struct ppe_drv_dot1p_policer_action *dot1p_action = &info->action;
	int hw_policer_idx = -1;


	if (r_action->action_flags & PPE_DOT1P_POLICER_ACTION_FLAG_POLICER_ID) {
		if (r_action->policer_id >= PPE_DOT1P_MIN_USER_POLICER_ID &&
						r_action->policer_id < PPE_DOT1P_MAX_USER_POLICER_ID) {
			hw_policer_idx = ppe_drv_policer_user2hw_id(r_action->policer_id);
			if (hw_policer_idx != -1) {
				dot1p_action->policer_id = hw_policer_idx;
				dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_POLICER_ACTION_FLAG_POLICER_ID;
			} else {
				ppe_dot1p_warn("Invalid user policer_idx %d, corresponding hw index doesn't exist\n", r_action->policer_id);
				return false;
			}
		}
	}

	if (r_action->action_flags & PPE_DOT1P_POLICER_ACTION_FLAG_US_POLICER_EN) {
		dot1p_action->us_policer_en = r_action->us_policer_en;
		dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_POLICER_ACTION_FLAG_US_POLICER_EN;
	}

	if (r_action->action_flags & PPE_DOT1P_POLICER_ACTION_FLAG_DS_POLICER_EN) {
		dot1p_action->ds_policer_en = r_action->ds_policer_en;
		dot1p_action->action_flags = dot1p_action->action_flags | PPE_DRV_DOT1P_POLICER_ACTION_FLAG_DS_POLICER_EN;
	}

	ppe_dot1p_info("%p: policer id %d: us_policer_enable: %d  ds_policer_enable: %d\n", r_action, dot1p_action->policer_id, dot1p_action->us_policer_en, dot1p_action->ds_policer_en);
	return true;
}

/*
 * ppe_dot1p_rule_exist()
 *	Check if DOT1P rule already exist.
 */
static bool ppe_dot1p_rule_exist(struct ppe_dot1p *dot1p)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p *dot1p_r;

	if (list_empty(&dot1p_g->active_rules)) {
		return false;
	}

	list_for_each_entry(dot1p_r, &dot1p_g->active_rules, list) {

		/*
		 * Don't allow new rule id with an existing rule having same rule id
		 */
		if (dot1p_r->rule_id == dot1p->rule_id) {
			ppe_dot1p_info("%p: specify a different rule ID: %d", dot1p, dot1p->rule_id);
			return true;
		}

		if ((dot1p_r->rule.rule.rule_flags & PPE_DOT1P_RULE_FLAG_SRC_PORT_TYPE) &&
			(dot1p_r->rule.rule.src_port_type != dot1p->rule.rule.src_port_type)) {
			goto rule_not_found;
		}

		if ((dot1p_r->rule.rule.rule_flags & PPE_DOT1P_RULE_FLAG_SRC_INFO) &&
			strcasecmp(dot1p_r->rule.rule.src_info, dot1p->rule.rule.src_info) !=0) {
			goto rule_not_found;
		}

		if ((dot1p_r->rule.rule.rule_flags & PPE_DOT1P_RULE_FLAG_DST_PORT_TYPE) &&
				(dot1p_r->rule.rule.dst_port_type != dot1p->rule.rule.dst_port_type)) {
			goto rule_not_found;
		}

		if ((dot1p_r->rule.rule.rule_flags & PPE_DOT1P_RULE_FLAG_DST_INFO) &&
			strcasecmp(dot1p_r->rule.rule.dst_info, dot1p->rule.rule.dst_info) !=0) {
			goto rule_not_found;
		}

		if ((dot1p_r->rule.rule.rule_flags & PPE_DOT1P_RULE_FLAG_VID) &&
				(dot1p_r->rule.rule.vid != dot1p->rule.rule.vid)) {
			goto rule_not_found;
		}

		if ((dot1p_r->rule.rule.rule_flags & PPE_DOT1P_RULE_FLAG_PCP) &&
				(dot1p_r->rule.rule.pcp != dot1p->rule.rule.pcp)) {
			goto rule_not_found;
		}

		if ((dot1p_r->rule.rule.rule_flags & PPE_DOT1P_RULE_FLAG_DEI) &&
			(dot1p_r->rule.rule.dei != dot1p->rule.rule.dei)) {
			goto rule_not_found;
		}

		if ((dot1p_r->rule.rule.rule_flags & PPE_DOT1P_RULE_FLAG_DSCP) &&
			(dot1p_r->rule.rule.dscp != dot1p->rule.rule.dscp)) {
			goto rule_not_found;
		}
	}

	ppe_dot1p_info("%p: specify a different rule, matching rule found %d",
								dot1p, dot1p->rule_id);
	return true;
rule_not_found:
	ppe_dot1p_info("%p: NO matching rule found", dot1p);
	return false;
}

/*
 * ppe_dot1p_rule_delete()
 *	Delete DOT1P rule in PPE.
 */
ppe_dot1p_ret_t ppe_dot1p_rule_delete(ppe_dot1p_rule_id_t id)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p *dot1p;

	/*
	 * Find the matching rule corresponding to the rule ID.
	 */
	spin_lock_bh(&dot1p_g->lock);
	dot1p = ppe_dot1p_rule_find_by_id(id);
	if (!dot1p) {
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_destroy_fail_invalid_id);
		spin_unlock_bh(&dot1p_g->lock);
		ppe_dot1p_warn("%p: failed to find the rule for ID: %d", dot1p_g, id);
		return PPE_DOT1P_RET_DELETE_FAIL_INVALID_ID;
	}

	if (kref_put(&dot1p->ref_cnt, ppe_dot1p_rule_free)) {
		ppe_dot1p_trace("%p: reference goes down to 0 for dot1p: %p ID: %d\n",
				dot1p_g, dot1p, id);
	}

	ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_destroy_req);
	spin_unlock_bh(&dot1p_g->lock);
	ppe_dot1p_trace("%p: ppe_dot1p_rule_delete\n", dot1p_g);
	return PPE_DOT1P_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_dot1p_rule_delete);

/*
 * ppe_dot1p_policer_rule_delete()
 *	Delete DOT1P policer rule in PPE.
 */
ppe_dot1p_ret_t ppe_dot1p_policer_rule_delete(uint16_t gemport_id)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p_policer *dot1p;

	/*
	 * Find the matching rule corresponding to the rule ID.
	 */
	spin_lock_bh(&dot1p_g->lock);
	dot1p = ppe_dot1p_policer_rule_find_by_gemport_id(gemport_id);
	if (!dot1p) {
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_policer_destroy_fail_invalid_id);
		spin_unlock_bh(&dot1p_g->lock);
		ppe_dot1p_warn("%p: failed to find the policer rule for gemport id: %d", dot1p_g, gemport_id);
		return PPE_DOT1P_RET_DELETE_FAIL_INVALID_ID;
	}

	if (kref_put(&dot1p->ref_cnt, ppe_dot1p_policer_rule_free)) {
		ppe_dot1p_trace("%p: reference goes down to 0 for dot1p: %p ID: %d\n",
				dot1p_g, dot1p, gemport_id);
	}

	ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_policer_destroy_req);
	spin_unlock_bh(&dot1p_g->lock);
	ppe_dot1p_trace("%p: ppe_dot1p_policer_rule_delete\n", dot1p_g);
	return PPE_DOT1P_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_dot1p_policer_rule_delete);

/*
 * ppe_dot1p_rule_flush()
 *	flush DOT1P rules in PPE.
 */
ppe_dot1p_ret_t ppe_dot1p_rule_flush(ppe_dot1p_flush_type_t flush_type)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p *dot1p, *tmp;

	spin_lock_bh(&dot1p_g->lock);

	if (list_empty(&dot1p_g->active_rules)) {
		spin_unlock_bh(&dot1p_g->lock);
		ppe_dot1p_trace("DOT1P rule list already empty!\n");
		return PPE_DOT1P_RET_SUCCESS;
	}
	/*
	 * iterating through rule list to delete all rules
	 */
	list_for_each_entry_safe(dot1p, tmp, &dot1p_g->active_rules, list) {
		switch (flush_type) {
		case PPE_DOT1P_FLUSH_TYPE_USERSPACE:
			if (dot1p->rule.userspace_rule) {
				if (kref_put(&dot1p->ref_cnt, ppe_dot1p_rule_free)) {
					ppe_dot1p_trace("%p: reference goes down to 0 for dot1p: %p\n", dot1p_g, dot1p);
				}
			}
			break;

		case PPE_DOT1P_FLUSH_TYPE_KERNELSPACE:
			if (!dot1p->rule.userspace_rule) {
				if (kref_put(&dot1p->ref_cnt, ppe_dot1p_rule_free)) {
					ppe_dot1p_trace("%p: reference goes down to 0 for dot1p: %p\n", dot1p_g, dot1p);
				}
			}
			break;

		case PPE_DOT1P_FLUSH_TYPE_ALL:
			if (kref_put(&dot1p->ref_cnt, ppe_dot1p_rule_free)) {
				ppe_dot1p_trace("%p: reference goes down to 0 for dot1p: %p\n", dot1p_g, dot1p);
			}
			break;

		case PPE_DOT1P_FLUSH_TYPE_ALL_EXCEPT_FIRST:
			if (dot1p != list_last_entry(&dot1p_g->active_rules, typeof(*dot1p), list)) {
				if (kref_put(&dot1p->ref_cnt, ppe_dot1p_rule_free)) {
					ppe_dot1p_trace("%p: reference goes down to 0 for dot1p: %p\n", dot1p_g, dot1p);
				}
			}
			break;

		default:
			ppe_dot1p_warn("specified incorrect flush type :%d", flush_type);
		}
	}

	/*
	 * Update stats
	 */
	ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_flush_req);
	spin_unlock_bh(&dot1p_g->lock);
	ppe_dot1p_trace("%p: ppe_dot1p_rule_flush \n", dot1p_g);
	return PPE_DOT1P_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_dot1p_rule_flush);

/*
 * ppe_dot1p_policer_rule_flush()
 *	flush DOT1P policer rules in PPE.
 */
ppe_dot1p_ret_t ppe_dot1p_policer_rule_flush(ppe_dot1p_flush_type_t flush_type)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p_policer *dot1p, *tmp;

	spin_lock_bh(&dot1p_g->lock);

	if (list_empty(&dot1p_g->active_policer_rules)) {
		spin_unlock_bh(&dot1p_g->lock);
		ppe_dot1p_trace("DOT1P rule list already empty!\n");
		return PPE_DOT1P_RET_SUCCESS;
	}

	/*
	 * iterating through rule list to delete all rules
	 */
	list_for_each_entry_safe(dot1p, tmp, &dot1p_g->active_policer_rules, list) {
		switch (flush_type) {
		case PPE_DOT1P_FLUSH_TYPE_ALL:
			if (kref_put(&dot1p->ref_cnt, ppe_dot1p_policer_rule_free)) {
				ppe_dot1p_trace("%p: reference goes down to 0 for dot1p: %p\n", dot1p_g, dot1p);
			}
			break;

		default:
			ppe_dot1p_warn("specified incorrect flush type :%d", flush_type);
		}
	}

	/*
	 * Update stats
	 */
	ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_policer_flush_req);
	spin_unlock_bh(&dot1p_g->lock);
	ppe_dot1p_trace("%p: ppe_dot1p_policer_rule_flush \n", dot1p_g);
	return PPE_DOT1P_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_dot1p_policer_rule_flush);

/*
 * ppe_dot1p_rule_create()
 *	Create DOT1P rule in PPE.
 */
ppe_dot1p_ret_t ppe_dot1p_rule_create(struct ppe_dot1p_rule *rule)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p *dot1p = NULL;
	struct ppe_drv_dot1p_ctx *ctx = NULL;
	ppe_dot1p_ret_t ret;
	uint8_t dot1p_rule_state = PPE_DOT1P_RESUME;

	ppe_dot1p_info("%p: rule create request: %p\n", dot1p_g, rule);

	spin_lock_bh(&dot1p_g->lock);
	dot1p_rule_state = dot1p_g->rule_state;
	dot1p = ppe_dot1p_alloc();
	if (!dot1p) {
		ppe_dot1p_warn("%p: failed to allocate dot1p memory: %p\n", dot1p_g, rule);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_create_fail_oom);
		ret = PPE_DOT1P_RET_CREATE_FAIL_OOM;
		goto fail;
	}

	if (rule->valid_flags & PPE_DOT1P_RULE_FLAG_DEFAULT_RULE) {
		dot1p->info.rule_valid_flags |= PPE_DRV_DOT1P_DEFAULT_RULE_FLAG;

		if (!ppe_dot1p_def_rule_info_fill(dot1p, &rule->def_rule, dot1p_rule_state)) {
			ppe_dot1p_warn("%p: failed to configure DOT1P rules\n", dot1p_g);
			ret = PPE_DOT1P_RET_CREATE_FAIL_RULE_CONFIG;
			goto fail;
		}

		memcpy(&dot1p_g->def_rule, &rule->def_rule, sizeof(struct ppe_dot1p_def_rule));

		if (ppe_drv_dot1p_def_rule_configure(&dot1p->info) != PPE_DRV_RET_SUCCESS) {
			ppe_dot1p_warn("failed to configure default DOT1P rule in drv: %p", dot1p_g);
			ret = PPE_DOT1P_RET_CREATE_FAIL_RULE_CONFIG;
			goto fail;
		}

		rule->ret = PPE_DOT1P_RET_SUCCESS;
		if (dot1p) {
			ppe_dot1p_free(dot1p);
		}
		spin_unlock_bh(&dot1p_g->lock);
		return PPE_DOT1P_RET_SUCCESS;

	}

	/*
	 * If rule_id is not specified by user, return error. If specified, validate it.
	 */
	if (!(rule->valid_flags & PPE_DOT1P_RULE_FLAG_RULE_ID)) {
		ret = PPE_DOT1P_RET_CREATE_FAIL_INVALID_ID;
		ppe_dot1p_warn("%p: no rule id specified: %p\n", dot1p_g, rule);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_create_fail_invalid_id);
		goto fail;
	}

	if (rule->rule_id > PPE_DOT1P_MAX_RULE_ID) {
		ret = PPE_DOT1P_RET_CREATE_FAIL_INVALID_ID;
		ppe_dot1p_warn("%p: rule id more than the max supported rules specified: %p\n", dot1p_g, rule);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_create_fail_invalid_id);
		goto fail;
 	}

	memcpy(&dot1p->rule, rule, sizeof(struct ppe_dot1p_rule));

	/*
	 * Fill DOT1P rule
	 */
	if (!ppe_dot1p_rule_info_fill(dot1p, &rule->rule)) {
		ppe_dot1p_warn("%p: failed to configure DOT1P rules\n", dot1p_g);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_create_fail_rule_config);
		ret = PPE_DOT1P_RET_CREATE_FAIL_RULE_CONFIG;
		goto fail;
	}

	/*
	 * Now that the rule information is extraced, confirm same rule
	 * doesn't exist already.
	 */
	dot1p->rule_id = rule->rule_id;
	if (ppe_dot1p_rule_exist(dot1p)) {
		ppe_dot1p_warn("%p: failed to configure DOT1P rule, rule already exists\n", dot1p_g);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_create_fail_rule_exist);
		ret = PPE_DOT1P_RET_CREATE_FAIL_RULE_CONFIG;
		goto fail;
	}

	/*
	 * Fill DOT1P rule action
	 */
	if (!ppe_dot1p_action_fill(dot1p, &rule->action, dot1p_rule_state)) {
		ppe_dot1p_warn("%p: failed to configure DOT1P action\n", dot1p_g);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_create_fail_action_config);
		ret = PPE_DOT1P_RET_CREATE_FAIL_ACTION_CONFIG;
		goto fail;
	}

	ctx = ppe_drv_dot1p_alloc();
	if (!ctx) {
		ppe_dot1p_warn("%p: couldn't allocate a ctx for DOT1P\n", dot1p_g);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_create_fail_alloc);
		ret = PPE_DOT1P_RET_CREATE_FAIL_DRV_ALLOC;
		goto fail;
	}

	if (ppe_drv_dot1p_rule_configure(ctx, &dot1p->info) != PPE_DRV_RET_SUCCESS) {
		ppe_dot1p_warn("%p: failed to configure DOT1P rule in drv: %p", dot1p_g, ctx);
		ret = PPE_DOT1P_RET_CREATE_FAIL_RULE_CONFIG;
		goto fail;
	}

	/*
	 * Store the status in response.
	 */
	rule->ret = PPE_DOT1P_RET_SUCCESS;

	/*
	 * Update stats
	 */
	ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_create_req);
	ppe_dot1p_info("%p: DOT1P rule created with rule ID: %d\n", dot1p_g, rule->rule_id);

	/*
	 * Store book keeping info.
	 */
	dot1p->ctx = ctx;
	kref_init(&dot1p->ref_cnt);

	list_add(&dot1p->list, &dot1p_g->active_rules);

	spin_unlock_bh(&dot1p_g->lock);
	return PPE_DOT1P_RET_SUCCESS;

fail:
	if (ctx) {
		ppe_drv_dot1p_delete(ctx, rule->action.gem_port);
		ctx = NULL;
	}

	if (dot1p) {
		ppe_dot1p_free(dot1p);
		dot1p = NULL;
	}

	rule->ret = ret;
	spin_unlock_bh(&dot1p_g->lock);
	return ret;
}
EXPORT_SYMBOL(ppe_dot1p_rule_create);

/*
 * ppe_dot1p_policer_rule_create()
 *	Create policer DOT1P rule in PPE.
 */
ppe_dot1p_ret_t ppe_dot1p_policer_rule_create(struct ppe_dot1p_policer_rule *rule)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p_policer *dot1p = NULL;
	struct ppe_drv_dot1p_policer_ctx *ctx = NULL;
	ppe_dot1p_ret_t ret;

	ppe_dot1p_info("%p: policer rule create request: %p\n", dot1p_g, rule);

	spin_lock_bh(&dot1p_g->lock);
	dot1p = ppe_dot1p_policer_alloc();
	if (!dot1p) {
		ppe_dot1p_warn("%p: failed to allocate dot1p policer memory: %p\n", dot1p_g, rule);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_policer_create_fail_oom);
		ret = PPE_DOT1P_RET_CREATE_FAIL_OOM;
		goto fail;
	}

	/*
	 * If rule_id is not specified by user, return error. If specified, validate it.
	 */
	if (!(rule->valid_flags & PPE_DOT1P_POLICER_RULE_FLAG_GEM_PORT_ID)) {
		ret = PPE_DOT1P_RET_CREATE_FAIL_INVALID_ID;
		ppe_dot1p_warn("%p: no gemport id specified: %p\n", dot1p_g, rule);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_policer_create_fail_invalid_id);
		goto fail;
	}

	if (rule->gemport_id >= PPE_DOT1P_MAX_GEMPORT_VAL) {
		ret = PPE_DOT1P_RET_CREATE_FAIL_INVALID_ID;
		ppe_dot1p_warn("%p: gemport id more than the max supported rules specified: %p\n", dot1p_g, rule);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_policer_create_fail_invalid_id);
		goto fail;
 	}

	/*
	 * Fill DOT1P policer rule
	 */
	if (!ppe_dot1p_policer_rule_info_fill(dot1p, &rule->rule)) {
		ppe_dot1p_warn("%p: failed to configure DOT1P rules\n", dot1p_g);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_policer_create_fail_rule_config);
		ret = PPE_DOT1P_POLICER_RET_CREATE_FAIL_RULE_CONFIG;
		goto fail;
	}

	/*
	 * Now that the rule information is extraced, confirm same rule
	 * doesn't exist already.
	 */
	dot1p->gemport_id = rule->gemport_id;

	/*
	 * Fill DOT1P policer rule action
	 */
	if (!ppe_dot1p_policer_action_fill(dot1p, &rule->action)) {
		ppe_dot1p_warn("%p: failed to configure DOT1P policer action\n", dot1p_g);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_policer_create_fail_action_config);
		ret = PPE_DOT1P_POLICER_RET_CREATE_FAIL_ACTION_CONFIG;
		goto fail;
	}

	ctx = ppe_drv_dot1p_policer_alloc();
	if (!ctx) {
		ppe_dot1p_warn("%p: couldn't allocate a ctx for DOT1P\n", dot1p_g);
		ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_policer_create_fail_alloc);
		ret = PPE_DOT1P_RET_CREATE_FAIL_DRV_ALLOC;
		goto fail;
	}

	if (ppe_drv_dot1p_policer_rule_configure(ctx, &dot1p->info) != PPE_DRV_RET_SUCCESS) {
		ppe_dot1p_warn("%p: failed to configure DOT1P rule in drv: %p", dot1p_g, ctx);
		ret = PPE_DOT1P_POLICER_RET_CREATE_FAIL_RULE_CONFIG;
		goto fail;
	}

	/*
	 * Store the status in response.
	 */
	rule->ret = PPE_DOT1P_RET_SUCCESS;

	/*
	 * Update stats
	 */
	ppe_dot1p_stats_inc(&dot1p_g->stats.cmn.dot1p_policer_create_req);

	/*
	 * Store book keeping info.
	 */
	dot1p->ctx = ctx;
	kref_init(&dot1p->ref_cnt);
	memcpy(&dot1p->rule, rule, sizeof(struct ppe_dot1p_policer_rule));

	list_add(&dot1p->list, &dot1p_g->active_policer_rules);

	spin_unlock_bh(&dot1p_g->lock);
	ppe_dot1p_info("%p: DOT1P policer rule created for gemport id: %d\n", dot1p_g, rule->gemport_id);
	return PPE_DOT1P_RET_SUCCESS;

fail:
	if (ctx) {
		ppe_drv_dot1p_policer_delete(ctx, rule->gemport_id);
	}

	if (dot1p) {
		ppe_dot1p_policer_free(dot1p);
	}

	rule->ret = ret;
	spin_unlock_bh(&dot1p_g->lock);
	ppe_dot1p_trace("%p: ppe_dot1p_policer_rule_create: %p\n", dot1p_g, dot1p);
	return ret;
}
EXPORT_SYMBOL(ppe_dot1p_policer_rule_create);

/*
 * ppe_dot1p_rule_pause()
 *	Pause DOT1P rule in PPE.
 */
ppe_dot1p_ret_t ppe_dot1p_rule_pause(ppe_dot1p_pause_type_t pause_type)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p *dot1p, *tmp;
	struct ppe_dot1p *dot1p_def = NULL;
	ppe_dot1p_ret_t ret = PPE_DOT1P_RET_SUCCESS;
	uint8_t gen_miss_cmd = PPE_DRV_DOT1P_CMD_DROP;

	spin_lock_bh(&dot1p_g->lock);
	dot1p_def = ppe_dot1p_alloc();

	/*
	 * Update fwd_cmd action field to drop to pause
	 * dot1p mapper rule configs
	 */
	if (!list_empty(&dot1p_g->active_rules)) {
		list_for_each_entry_safe(dot1p, tmp, &dot1p_g->active_rules, list) {
			switch (pause_type) {
			case PPE_DOT1P_PAUSE_TYPE_ALL:
				dot1p->info.action.fwd_cmd = PPE_DRV_DOT1P_CMD_DROP;
				if (ppe_drv_dot1p_rule_pause_resume(dot1p->ctx, &dot1p->info) != PPE_DRV_RET_SUCCESS) {
					spin_unlock_bh(&dot1p_g->lock);
					ppe_dot1p_warn("%p: failed to pause DOT1P rule in drv", dot1p_g);
					ret = PPE_DOT1P_RET_PAUSE_FAIL_RULE_CONFIG;
					return ret;
				}
				break;

			case PPE_DOT1P_PAUSE_TYPE_ALL_EXCEPT_GEM0:
				dot1p->info.action.fwd_cmd = (dot1p->rule.action.gem_port != 0) ? PPE_DRV_DOT1P_CMD_DROP
											: dot1p->rule.action.fwd_cmd;

				if (ppe_drv_dot1p_rule_pause_resume(dot1p->ctx, &dot1p->info) != PPE_DRV_RET_SUCCESS) {
					spin_unlock_bh(&dot1p_g->lock);
					ppe_dot1p_warn("%p: failed to pause DOT1P rule in drv", dot1p_g);
					ret = PPE_DOT1P_RET_PAUSE_FAIL_RULE_CONFIG;
					return ret;
				}
				break;

			default:
				ppe_dot1p_warn("specified incorrect pause type :%d", pause_type);
			}
		}
	}

	if (!ppe_dot1p_def_rule_info_fill(dot1p_def, &dot1p_g->def_rule, dot1p_g->rule_state)) {
		ppe_dot1p_warn("%p: failed to fill default DOT1P rule\n", dot1p_g);
		ret = PPE_DOT1P_RET_CREATE_FAIL_RULE_CONFIG;
	}

	if (ppe_drv_dot1p_def_rule_pause_resume(&dot1p_def->info, gen_miss_cmd) != PPE_DRV_RET_SUCCESS) {
		ppe_dot1p_warn("failed to pause default DOT1P rule in drv: %p", dot1p_g);
		ret = PPE_DOT1P_RET_CREATE_FAIL_RULE_CONFIG;
	}

	ppe_dot1p_info("%p: All active rules action updated to drop", dot1p_g);
	if (dot1p_def) {
		ppe_dot1p_free(dot1p_def);
	}

	if (ret == PPE_DOT1P_RET_SUCCESS && pause_type == PPE_DOT1P_PAUSE_TYPE_ALL) {
		dot1p_g->rule_state = PPE_DOT1P_PAUSE;
	}

	if (ret == PPE_DOT1P_RET_SUCCESS && pause_type == PPE_DOT1P_PAUSE_TYPE_ALL_EXCEPT_GEM0) {
		dot1p_g->rule_state = PPE_DOT1P_PAUSE_EX_GEM_0;
	}

	spin_unlock_bh(&dot1p_g->lock);
	ppe_dot1p_trace("%p: ppe_dot1p_rule_pause\n", dot1p_g);
	return ret;
}
EXPORT_SYMBOL(ppe_dot1p_rule_pause);

/*
 * ppe_dot1p_rule_resume()
 *	Resume DOT1P rule in PPE.
 */
ppe_dot1p_ret_t ppe_dot1p_rule_resume()
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p *dot1p;
	ppe_dot1p_ret_t ret = PPE_DOT1P_RET_SUCCESS;
	uint8_t gen_miss_cmd = PPE_DRV_DOT1P_CMD_DROP;
	struct ppe_dot1p *dot1p_def = NULL;

	spin_lock_bh(&dot1p_g->lock);
	dot1p_def = ppe_dot1p_alloc();

	/*
	 * Restore fwd_cmd action field value
	 * for dot1p rules.
	 */
	if (!list_empty(&dot1p_g->active_rules)) {
		list_for_each_entry(dot1p, &dot1p_g->active_rules, list) {
			dot1p->info.action.fwd_cmd = dot1p->rule.action.fwd_cmd;
			if (ppe_drv_dot1p_rule_pause_resume(dot1p->ctx, &dot1p->info) != PPE_DRV_RET_SUCCESS) {
				spin_unlock_bh(&dot1p_g->lock);
				ppe_dot1p_warn("%p: failed to resume DOT1P rule in drv", dot1p_g);
				ret = PPE_DOT1P_RET_RESUME_FAIL_RULE_CONFIG;
				return ret;
			}
		}
	}

	/*
	 * Get the default rule.
	 */
	if (!ppe_dot1p_def_rule_info_fill(dot1p_def, &dot1p_g->def_rule, dot1p_g->rule_state)) {
		ppe_dot1p_warn("%p: failed to fill default DOT1P rule\n", dot1p_g);
		ret = PPE_DOT1P_RET_CREATE_FAIL_RULE_CONFIG;
	}

	gen_miss_cmd = dot1p_g->def_rule.gen_miss_cmd;

	/*
	 * Restore fwd_cmd action field value
	 * for dot1p default rule.
	 */
	if (ppe_drv_dot1p_def_rule_pause_resume(&dot1p_def->info, gen_miss_cmd) != PPE_DRV_RET_SUCCESS) {
		ppe_dot1p_warn("failed to pause default DOT1P rule in drv: %p", dot1p_g);
		ret = PPE_DOT1P_RET_CREATE_FAIL_RULE_CONFIG;
	}

	ppe_dot1p_info("%p: All active rules action value restored", dot1p_g);
	if (dot1p_def) {
		ppe_dot1p_free(dot1p_def);
	}
	if (ret == PPE_DOT1P_RET_SUCCESS) {
		dot1p_g->rule_state = PPE_DOT1P_RESUME;
	}
	spin_unlock_bh(&dot1p_g->lock);
	ppe_dot1p_trace("%p: ppe_dot1p_rule_resume \n", dot1p_g);
	return ret;
}
EXPORT_SYMBOL(ppe_dot1p_rule_resume);

/*
 * ppe_dot1p_rule_get_state()
 *	Get state DOT1P rule in PPE.
 */
ppe_dot1p_ret_t ppe_dot1p_rule_get_state(uint8_t *get_state)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;

	spin_lock_bh(&dot1p_g->lock);
	*get_state = dot1p_g->rule_state;
	spin_unlock_bh(&dot1p_g->lock);

	ppe_dot1p_trace("%p: ppe_dot1p_rule_get_state \n", dot1p_g);
	return PPE_DOT1P_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_dot1p_rule_get_state);

/*
 * ppe_dot1p_deinit()
 *	DOT1P deinit API
 */
void ppe_dot1p_deinit(void)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;

	dot1p_g->rule_state = PPE_DOT1P_RESUME;
	ppe_dot1p_stats_debugfs_exit();
	ppe_dot1p_dump_exit();
	ppe_dot1p_info("%p: DOT1P deinit done", dot1p_g);
}

/*
 * ppe_dot1p_init()
 *	DOT1P init API
 */
void ppe_dot1p_init(struct dentry *d_rule)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;

	spin_lock_init(&dot1p_g->lock);

	/*
	 * Initialize active list
	 */
	INIT_LIST_HEAD(&dot1p_g->active_rules);
	INIT_LIST_HEAD(&dot1p_g->active_policer_rules);

	/*
	 * Initialization of DOT1P dump.
	 */
	ppe_dot1p_dump_init(d_rule);

	/*
	 * Create debugfs directory/files.
	 */
	ppe_dot1p_stats_debugfs_init(d_rule);

	dot1p_g->rule_state = PPE_DOT1P_RESUME;
	ppe_dot1p_info("%p: DOT1P init done", dot1p_g);
}
