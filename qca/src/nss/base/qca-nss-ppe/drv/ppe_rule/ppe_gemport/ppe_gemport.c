/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <ppe_drv_public.h>
#include <ppe_drv_gemport.h>
#include "ppe_gemport.h"

/*
 * Global GEM PORT context
 */
struct ppe_gem_port_base ppe_gem_port_gbl = {0};

/*
 * ppe_gem_port_alloc()
 *      Allocate memory for a GEM PORT rule.
 */
static inline struct ppe_gem_port *ppe_gem_port_alloc(void)
{
	/*
	 * Allocate memory for a new rule
	 *
	 * kzalloc with GFP_ATOMIC is used assuming GEMPORT rules can be created
	 * in softirq context as well while processing an SKB in the system.
	 */
	return kzalloc(sizeof(struct ppe_gem_port), GFP_ATOMIC);
}

/*
 * ppe_gem_port_free()
 *      Free v4 connections.
 */
static inline void ppe_gem_port_free(struct ppe_gem_port *gem_port)
{
	kfree(gem_port);
}

/*
 * ppe_gem_port_rule_find_by_id()
 *	Find a rule corresponding to a gemport ID.
 */
static struct ppe_gem_port *ppe_gem_port_rule_find_by_id(uint8_t gem_port_id)
{
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;
	struct ppe_gem_port *gem_port;

	/*
	 * Get the first available gem port rule ID.
	 */
	if (!list_empty(&gem_port_g->active_rules)) {
		list_for_each_entry(gem_port, &gem_port_g->active_rules, list) {
			if (gem_port->rule.rule.gem_port_id == gem_port_id) {
				ppe_gem_port_info("%p: GEM PORT rule: %p found for ID: %d",
					gem_port_g, gem_port, gem_port_id);
				return gem_port;
			}
		}
	}

	ppe_gem_port_warn("%p: No valid GEM PORT rule for gemport id: %d", gem_port_g, gem_port_id);
	return NULL;
}

/*
 * ppe_gem_port_rule_free()
 *	Free the gem port rule.
 */
static void ppe_gem_port_rule_free(struct kref *kref)
{
	struct ppe_gem_port *gem_port = container_of(kref, struct ppe_gem_port, ref_cnt);
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;

	/*
	 * Delete the rule node from active list.
	 */
	list_del(&gem_port->list);

	/*
	 * Delete the rule in PPE driver.
	 */
	if (gem_port->ctx) {
		ppe_drv_gem_port_delete(gem_port->ctx, gem_port->info.rule.gem_port_id);
		gem_port->ctx = NULL;
	}

	ppe_gem_port_info("%p: GEM PORT rule freed for gemport id: %u", gem_port, gem_port->rule.rule.gem_port_id);

	/*
	 * free the gem_port rule memory.
	 */
	ppe_gem_port_free(gem_port);

	/*
	 * Update stats
	 */
	ppe_gem_port_stats_inc(&gem_port_g->stats.cmn.gem_port_free_req);
}

/*
 * ppe_gem_port_rule_delete()
 *	Delete GEM PORT rule in PPE.
 */
ppe_gem_port_ret_t ppe_gem_port_rule_delete(uint8_t gem_port_id)
{
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;
	struct ppe_gem_port *gem_port;

	spin_lock_bh(&gem_port_g->lock);

	/*
	 * Find the matching rule corresponding to the rule ID.
	 */
	gem_port = ppe_gem_port_rule_find_by_id(gem_port_id);
	if (!gem_port) {
		ppe_gem_port_warn("%p: failed to find the rule for gem_port ID: %d", gem_port_g, gem_port_id);
		ppe_gem_port_stats_inc(&gem_port_g->stats.cmn.gem_port_destroy_fail_invalid_id);
		spin_unlock_bh(&gem_port_g->lock);
		return PPE_GEM_PORT_RET_DELETE_FAIL_INVALID_ID;
	}

	if (kref_put(&gem_port->ref_cnt, ppe_gem_port_rule_free)) {
		ppe_gem_port_trace("%p: reference goes down to 0 for gem port: %p ID: %d\n",
				gem_port_g, gem_port, gem_port_id);
	}

	ppe_gem_port_stats_inc(&gem_port_g->stats.cmn.gem_port_destroy_req);
	ppe_gem_port_info("%p: gem_port_id: %u ref dec: %u", gem_port_g, gem_port_id, kref_read(&gem_port->ref_cnt));
	spin_unlock_bh(&gem_port_g->lock);
	return PPE_GEM_PORT_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_gem_port_rule_delete);

/*
 * ppe_gem_port_rule_flush()
 *	flush GEM PORT rules in PPE.
 */
ppe_gem_port_ret_t ppe_gem_port_rule_flush(ppe_gem_port_flush_type_t flush_type)
{
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;
	struct ppe_gem_port *gem_port, *tmp;

	spin_lock_bh(&gem_port_g->lock);

	if (list_empty(&gem_port_g->active_rules)) {
		ppe_gem_port_trace("GEM port  rule list already empty!\n");
		spin_unlock_bh(&gem_port_g->lock);
		return PPE_GEM_PORT_RET_SUCCESS;
	}

	/*
	 * iterating through rule list to delete all rules
	 */
	list_for_each_entry_safe(gem_port, tmp, &gem_port_g->active_rules, list) {
		switch (flush_type) {
		case PPE_GEM_PORT_FLUSH_TYPE_USERSPACE:
			if (gem_port->rule.userspace_rule) {
				if (kref_put(&gem_port->ref_cnt, ppe_gem_port_rule_free)) {
					ppe_gem_port_trace("%p: reference goes down to 0 for gem_port: %p\n", gem_port_g, gem_port);
				}
			}
			break;

		case PPE_GEM_PORT_FLUSH_TYPE_KERNELSPACE:
			if (!gem_port->rule.userspace_rule) {
				if (kref_put(&gem_port->ref_cnt, ppe_gem_port_rule_free)) {
					ppe_gem_port_trace("%p: reference goes down to 0 for gem port: %p\n", gem_port_g, gem_port);
				}
			}
			break;

		case PPE_GEM_PORT_FLUSH_TYPE_ALL:
			if (kref_put(&gem_port->ref_cnt, ppe_gem_port_rule_free)) {
				ppe_gem_port_trace("%p: reference goes down to 0 for gem port: %p\n", gem_port_g, gem_port);
			}
		}
	}

	/*
	 * Update stats
	 */
	ppe_gem_port_stats_inc(&gem_port_g->stats.cmn.gem_port_flush_req);
	spin_unlock_bh(&gem_port_g->lock);
	return PPE_GEM_PORT_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_gem_port_rule_flush);

/*
 * ppe_gem_port_rule_info_fill()
 *	Fill the ppe-drv rule structure based on user information.
 */
static bool ppe_gem_port_rule_info_fill(struct ppe_gem_port *gem_port, struct ppe_gem_port_rule_match *r)
{
	struct ppe_drv_gem_port_rule *info = &gem_port->info;
	struct ppe_drv_gem_port_rule_match *rule_f = &info->rule;

	if (r->rule_flags & PPE_GEM_PORT_RULE_FLAG_GEM_PORT_ID) {
		if (r->gem_port_id < PPE_GEM_PORT_ID_MAX) {
			rule_f->gem_port_id = r->gem_port_id;
			rule_f->rule_flags = rule_f->rule_flags | PPE_DRV_GEM_PORT_RULE_FLAG_GEM_PORT_ID;
		} else {
			return false;
		}
	} else {
		return false;
	}

	return true;
}

/*
 * ppe_gem_port_action_fill()
 *	Action corresponding to a GEM PORT rule.
 */
static bool ppe_gem_port_action_fill(struct ppe_gem_port *gem_port, struct ppe_gem_port_rule_action *r_action)
{
	struct ppe_drv_gem_port_rule *info = &gem_port->info;
	struct ppe_drv_gem_port_rule_action *gem_port_action = &info->action;
	struct ppe_drv_iface *iface;
	struct net_device *dev;
	int32_t port_num;

	if (r_action->action_flags & PPE_GEM_PORT_ACTION_FLAG_SRC_INFO) {
		if (*r_action->src_info == '\0') {
			ppe_gem_port_warn("source info is NULL or empty\n");
			return false;
		}

		dev = dev_get_by_name(&init_net, r_action->src_info);
		if (!dev) {
			ppe_gem_port_warn("%p: failed to find valid src for dev %s\n",
					r_action, r_action->src_info);
			return false;
		}

		iface = ppe_drv_iface_get_by_dev(dev);
		if (!iface) {
			ppe_gem_port_warn("%p: failed to find PPE interface for dev: %p(%s)\n",
					r_action, dev, r_action->src_info);
			dev_put(dev);
			return false;
		}

		port_num = ppe_drv_iface_port_idx_get(iface);
		if (port_num < 0) {
			ppe_gem_port_warn("%p: failed to find PPE port for iface: %p\n",
					r_action, iface);
			dev_put(dev);
			return false;
		}

		ppe_gem_port_info("%p: Source port number: %d\n", r_action, port_num);

		dev_put(dev);
		gem_port_action->src_port = port_num;
		gem_port_action->action_flags |= PPE_DRV_GEM_PORT_ACTION_FLAG_SRC_INFO;
	}

	if ((r_action->action_flags & PPE_GEM_PORT_ACTION_FLAG_DST_INFO) &&
		(r_action->action_flags & PPE_GEM_PORT_ACTION_FLAG_DST_PORT_TYPE)) {
		int temp_val, ret = 0;

		if (*r_action->dst_info == '\0') {
			ppe_gem_port_warn("destination info is NULL or empty\n");
			return false;
		}

		switch (r_action->dst_port_type) {
		case PPE_GEM_PORT_TYPE_BITMAP:
			gem_port_action->dst_port_type = PPE_DRV_GEM_PORT_TYPE_BITMAP;
			ret = kstrtoint(r_action->dst_info, 10, &temp_val);
			if (ret != 0) {
				ppe_gem_port_warn("Conversion error: %d\n", ret);
				return false;
			}
			gem_port_action->dst_port = (uint8_t)temp_val;
			break;

		case PPE_GEM_PORT_TYPE_PORT:
			gem_port_action->dst_port_type = PPE_DRV_GEM_PORT_TYPE_PORT;
			dev = dev_get_by_name(&init_net, r_action->dst_info);
			if (!dev) {
				ppe_gem_port_warn("%p: failed to find valid dst for dev %s\n",
						r_action, r_action->dst_info);
				return false;
			}

			iface = ppe_drv_iface_get_by_dev(dev);
			if (!iface) {
				ppe_gem_port_warn("%p: failed to find PPE interface for dev: %p(%s)\n",
						r_action, dev, r_action->dst_info);
				dev_put(dev);
				return false;
			}

			port_num = ppe_drv_iface_port_idx_get(iface);
			if (port_num < 0) {
				ppe_gem_port_warn("%p: failed to find PPE port for iface: %p\n",
						r_action, iface);
				dev_put(dev);
				return false;
			}

			ppe_gem_port_info("%p: Source interface port number: %d\n", r_action, port_num);

			dev_put(dev);
			gem_port_action->dst_port = port_num;
		}
		gem_port_action->action_flags |= PPE_DRV_GEM_PORT_ACTION_FLAG_DST_INFO;
		gem_port_action->action_flags |= PPE_DRV_GEM_PORT_ACTION_FLAG_DST_PORT_TYPE;
	}

	if (r_action->action_flags & PPE_GEM_PORT_ACTION_FLAG_INT_PRI) {
		gem_port_action->int_pri = r_action->int_pri;
		gem_port_action->action_flags = gem_port_action->action_flags | PPE_DRV_GEM_PORT_ACTION_FLAG_INT_PRI;
	}

	if (r_action->action_flags & PPE_GEM_PORT_ACTION_FLAG_DST_SELECTION) {
		if (r_action->dst_selection == PPE_GEM_PORT_DST_GEM_PORT_TBL) {
			gem_port_action->svc_code = PPE_DRV_SC_FDB_BYPASS;
			gem_port_action->action_flags |= PPE_DRV_GEM_PORT_ACTION_FLAG_DST_SELECTION;
		}
	}

	if (r_action->action_flags & PPE_GEM_PORT_ACTION_FLAG_INT_DP) {
		gem_port_action->int_dp = r_action->int_dp;
		gem_port_action->action_flags = gem_port_action->action_flags | PPE_DRV_GEM_PORT_ACTION_FLAG_INT_DP;
	}

	return true;
}

/*
 * ppe_gem_port_rule_exist()
 *	Check if GEM PORT rule already exist.
 */
static bool ppe_gem_port_rule_exist(struct ppe_gem_port *gem_port)
{
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;
	struct ppe_gem_port *gem_port_r;

	if (list_empty(&gem_port_g->active_rules)) {
		return false;
	}

	list_for_each_entry(gem_port_r, &gem_port_g->active_rules, list) {

		/*
		 * Don't allow new rule id with an existing rule having same rule id
		 */
		if (gem_port_r->rule.rule.gem_port_id == gem_port->info.rule.gem_port_id) {
			ppe_gem_port_info("%p: found a matching rule with gemport ID: %d", gem_port, gem_port->rule.rule.gem_port_id);
			return true;
		//TODO: add functionality to update the rule if the rule needs to be updated
		}
	}

	ppe_gem_port_info("%p: NO matching rule found for gemport id: %d", gem_port, gem_port->info.rule.gem_port_id);

	return false;
}

/*
 * ppe_gem_port_rule_create()
 *	Create GEM PORT rule in PPE.
 */
ppe_gem_port_ret_t ppe_gem_port_rule_create(struct ppe_gem_port_rule *rule)
{
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;
	struct ppe_gem_port *gem_port = NULL;
	struct ppe_drv_gem_port_ctx *ctx = NULL;
	ppe_gem_port_ret_t ret;

	ppe_gem_port_info("%p: rule create request: %p", gem_port_g, rule);

	spin_lock_bh(&gem_port_g->lock);
	gem_port = ppe_gem_port_alloc();
	if (!gem_port) {
		ppe_gem_port_warn("%p: failed to allocate gem port memory: %p", gem_port_g, rule);
		ppe_gem_port_stats_inc(&gem_port_g->stats.cmn.gem_port_create_fail_oom);
		ret = PPE_GEM_PORT_RET_CREATE_FAIL_OOM;
		goto fail;
	}

	/*
	 * Fill GEM PORT rule
	 */
	if (!ppe_gem_port_rule_info_fill(gem_port, &rule->rule)) {
		ppe_gem_port_warn("%p: failed to configure GEM PORT rules", gem_port_g);
		ppe_gem_port_stats_inc(&gem_port_g->stats.cmn.gem_port_create_fail_rule_config);
		ret = PPE_GEM_PORT_RET_CREATE_FAIL_RULE_CONFIG;
		goto fail;
	}

	/*
	 * Now that the rule information is extraced, confirm same rule
	 * doesn't exist already.
	 */
	if (ppe_gem_port_rule_exist(gem_port)) {
		ppe_gem_port_warn("%p: failed to configure GEM PORT rule, rule already exists", gem_port_g);
		ret = PPE_GEM_PORT_RET_CREATE_FAIL_RULE_CONFIG;
		goto fail;
	}

	/*
	 * Fill GEM PORT rule action
	 */
	if (!ppe_gem_port_action_fill(gem_port, &rule->action)) {
		ppe_gem_port_warn("%p: failed to configure GEM PORT action", gem_port_g);
		ppe_gem_port_stats_inc(&gem_port_g->stats.cmn.gem_port_create_fail_action_config);
		ret = PPE_GEM_PORT_RET_CREATE_FAIL_ACTION_CONFIG;
		goto fail;
	}

	ctx = ppe_drv_gem_port_alloc();
	if (!ctx) {
		ppe_gem_port_warn("%p: couldn't allocate a ctx for GEMPORT\n", gem_port_g);
		ppe_gem_port_stats_inc(&gem_port_g->stats.cmn.gem_port_create_fail_alloc);
		ret = PPE_GEM_PORT_RET_CREATE_FAIL_DRV_ALLOC;
		goto fail;
	}

	if (ppe_drv_gem_port_rule_configure(ctx, &gem_port->info) != PPE_DRV_RET_SUCCESS) {
		ppe_gem_port_warn("%p: failed to configure gemport mapping rule in drv: %p", gem_port_g, ctx);
		ret = PPE_GEM_PORT_RET_CREATE_FAIL_RULE_CONFIG;
		goto fail;
	}

	/*
	 * Store the status in response.
	 */
	rule->ret = PPE_GEM_PORT_RET_SUCCESS;

	/*
	 * Update stats
	 */
	ppe_gem_port_stats_inc(&gem_port_g->stats.cmn.gem_port_create_req);
	ppe_gem_port_info("%p: GEM PORT rule created with gemport ID: %d\n", gem_port_g, rule->rule.gem_port_id);

	/*
	 * Store book keeping info.
	 */
	gem_port->ctx = ctx;
	kref_init(&gem_port->ref_cnt);
	memcpy(&gem_port->rule, rule, sizeof(struct ppe_gem_port_rule));

	/*
	 * Add new rule node to list
	 */
	list_add(&gem_port->list, &gem_port_g->active_rules);


	spin_unlock_bh(&gem_port_g->lock);
	return PPE_GEM_PORT_RET_SUCCESS;

fail:
	if (ctx) {
		ppe_drv_gem_port_delete(ctx, rule->rule.gem_port_id);
		ctx = NULL;
	}

	if (gem_port) {
		ppe_gem_port_free(gem_port);
		gem_port = NULL;
	}

	rule->ret = ret;
	spin_unlock_bh(&gem_port_g->lock);
	return ret;
}
EXPORT_SYMBOL(ppe_gem_port_rule_create);

/*
 * ppe_gem_port_deinit()
 *	GEM PORT deinit API
 */
void ppe_gem_port_deinit(void)
{
	struct ppe_gem_port_base *gem_port_g __maybe_unused = &ppe_gem_port_gbl;

	ppe_gem_port_dump_exit();
	ppe_gem_port_stats_debugfs_exit();
	ppe_gem_port_info("%p: GEM PORT deinit done", gem_port_g);
}

/*
 * ppe_gem_port_init()
 *	GEM PORT init API
 */
void ppe_gem_port_init(struct dentry *d_rule)
{
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;

	spin_lock_init(&gem_port_g->lock);

	/*
	 * Initialize active list
	 */
	INIT_LIST_HEAD(&gem_port_g->active_rules);

	/*
	 * Initialization of GEM PORT dump.
	 */
	ppe_gem_port_dump_init(d_rule);

	/*
	 * Create debugfs directory/files.
	 */
	ppe_gem_port_stats_debugfs_init(d_rule);
	ppe_gem_port_info("%p: GEM PORT init done", gem_port_g);

}
