/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <ppe_drv_public.h>
#include <ppe_drv.h>
#include <ppe_drv_pm.h>
#include "ppe_pm.h"

/*
 * Global PM context
 */
struct ppe_pm_base ppe_pm_gbl = {0};

/*
 * ppe_pm_alloc()
 *	Allocate memory for an PM gen rule.
 */
static inline void *ppe_pm_alloc(size_t size)
{
	return kzalloc(size, GFP_ATOMIC);
}

/*
 * ppe_pm_free()
 *	Free PM rules.
 */
static inline void ppe_pm_free(void *rule)
{
	kfree(rule);
}

/*
 * ppe_pm_counter_ref_release()
 *	Release callback for a counter context kref.
 */
static void ppe_pm_counter_ref_release(struct kref *kref)
{
	struct ppe_pm_counter_ref *counter_ref = container_of(kref, struct ppe_pm_counter_ref, ref);
	ppe_drv_pm_free(counter_ref->pm_ctx);
	counter_ref->pm_ctx = NULL;
}

/*
 * ppe_pm_counter_alloc_internal()
 *	Allocate new PM counter or take reference on existing one (lock-held version).
 */
static struct ppe_drv_pm_counter_ctx *ppe_pm_counter_alloc_internal(uint8_t counter_id, ppe_pm_rule_dir_t rule_dir)
{
	struct ppe_drv_pm_counter_ctx *pm_ctx = NULL;
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;
	ppe_drv_rule_dir_t dir;

	dir = (rule_dir == PPE_PM_RULE_DIR_INGRESS) ? PPE_DRV_RULE_INGRESS : PPE_DRV_RULE_EGRESS;

	pm_ctx = pm_g->counter_array[counter_id].pm_ctx;
	if (pm_ctx) {
		struct ppe_drv_pm_counter_info *info = ppe_drv_pm_counter_info_get(pm_ctx);
		if (info->rule_dir != dir) {
			ppe_pm_warn("Counter_id %d is already allocated for %s PM Counter table\n",
					counter_id, (info->rule_dir == PPE_DRV_RULE_INGRESS) ? "INGRESS" : "EGRESS");
			ppe_pm_stats_inc(&pm_g->stats.cmn.pm_get_ctx_fail_alloc);
			return NULL;
		}

		kref_get(&pm_g->counter_array[counter_id].ref);
		ppe_pm_info("Ctx for counter_id %d is already allocated, %d rules are associated with this ctx.\n",
				counter_id, kref_read(&pm_g->counter_array[counter_id].ref));
		return pm_ctx;
	}

	/*
	 * Request from ppe_drv
	 */
	pm_ctx = ppe_drv_pm_alloc(dir);
	if (pm_ctx) {
		pm_g->counter_array[counter_id].pm_ctx = pm_ctx;
		kref_init(&pm_g->counter_array[counter_id].ref);
		ppe_pm_info("Counter ctx is allocated succesfully \n");
	}

	/*
	 * Update stats
	 */
	ppe_pm_stats_inc(&pm_g->stats.cmn.pm_ctx_get_req);

	return pm_ctx;
}

/*
 * ppe_pm_counter_alloc()
 *	Allocate new PM counter or take reference on existing one.
 */
int16_t ppe_pm_counter_alloc(uint8_t counter_id, ppe_pm_rule_dir_t rule_dir)
{
	struct ppe_drv_pm_counter_ctx *pm_ctx = NULL;
	struct ppe_drv_pm_counter_info *info;
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;

	if (counter_id >= PPE_DRV_PM_COUNTER_CTX_MAX) {
		ppe_pm_warn("counter_id %d exceeds maximum allowed value\n", counter_id);
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_counter_invalid_id);
		return -1;
	}

	spin_lock_bh(&pm_g->lock);
	pm_ctx = ppe_pm_counter_alloc_internal(counter_id, rule_dir);
	spin_unlock_bh(&pm_g->lock);

	if (!pm_ctx) {
		return -1;
	}

	info = ppe_drv_pm_counter_info_get(pm_ctx);
	return info->hw_index;
}

/*
 * ppe_pm_counter_deref_internal()
 *	Dereference a PM counter by its user ID (lock-held version).
 */
static void ppe_pm_counter_deref_internal(uint8_t counter_id)
{
	struct ppe_drv_pm_counter_ctx *pm_ctx;
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;

	if (counter_id >= PPE_DRV_PM_COUNTER_CTX_MAX) {
		ppe_pm_warn("counter_id %d exceeds maximum allowed value\n", counter_id);
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_counter_invalid_id);
		return;
	}

	pm_ctx = pm_g->counter_array[counter_id].pm_ctx;
	if (pm_ctx) {
		kref_put(&pm_g->counter_array[counter_id].ref, ppe_pm_counter_ref_release);
	}

	/*
	 * Update stats
	 */
	ppe_pm_stats_inc(&pm_g->stats.cmn.pm_ctx_destroy_req);
}

/*
 * ppe_pm_counter_deref()
 *	Deref the PM counter by counter_id.
 */
void ppe_pm_counter_deref(uint8_t counter_id)
{
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;

	spin_lock_bh(&pm_g->lock);
	ppe_pm_counter_deref_internal(counter_id);
	spin_unlock_bh(&pm_g->lock);
}

/*
 * ppe_pm_counter_get()
 *	Read counter stats for a given counter_id.
 */
ppe_pm_ret_t ppe_pm_counter_get(struct ppe_pm_counter *counter_info)
{
	struct ppe_drv_pm_counter_ctx *pm_ctx;
	struct ppe_drv_pm_counter_info *info;
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;
	uint8_t counter_id = 0;
	ppe_pm_ret_t ret;

	counter_id = counter_info->counter_id;
	if (counter_id >= PPE_DRV_PM_COUNTER_CTX_MAX) {
		ppe_pm_warn("counter_id %d exceeds maximum allowed value\n", counter_id);
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_counter_invalid_id);
		counter_info->ret = PPE_PM_RET_INVALID_COUNTER_ID;
		return PPE_PM_RET_INVALID_COUNTER_ID;
	}

	spin_lock_bh(&pm_g->lock);

	pm_ctx = pm_g->counter_array[counter_id].pm_ctx;
	if (!pm_ctx) {
		ppe_pm_warn("counter_id %d is not having valid ctx\n", counter_id);
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_get_fail_invalid_ctx);
		ret = PPE_PM_RET_PM_COUNTER_GET_FAIL;
		goto fail;
	}

	info = ppe_drv_pm_counter_info_get(pm_ctx);

	/*
	 * Get the stats from ppe_drv module
	 */
	counter_info->ucast_packet = atomic64_read(&info->ucast_packet);
	counter_info->bcast_packet = atomic64_read(&info->bcast_packet);
	counter_info->mcast_packet = atomic64_read(&info->mcast_packet);
	counter_info->oversize = atomic64_read(&info->oversize);
	counter_info->octets = atomic64_read(&info->octets);
	counter_info->frame_64 = atomic64_read(&info->frame_64);
	counter_info->frame_65_127 = atomic64_read(&info->frame_65_127);
	counter_info->frame_128_255 = atomic64_read(&info->frame_128_255);
	counter_info->frame_256_511 = atomic64_read(&info->frame_256_511);
	counter_info->frame_512_1023 = atomic64_read(&info->frame_512_1023);
	counter_info->frame_1024_1518 = atomic64_read(&info->frame_1024_1518);

	ret = PPE_PM_RET_SUCCESS;
	counter_info->ret = ret;

	/*
	 * Update stats
	 */
	ppe_pm_stats_inc(&pm_g->stats.cmn.pm_get_req);
	spin_unlock_bh(&pm_g->lock);
	return ret;

fail:
	counter_info->ret = ret;
	spin_unlock_bh(&pm_g->lock);
	return ret;
}
EXPORT_SYMBOL(ppe_pm_counter_get);

/*
 * ppe_pm_rule_fill()
 *	Rule info corresponding to an PM gen rule.
 */
static bool ppe_pm_rule_fill(struct ppe_pm_gen *pm_gen, struct ppe_pm_counter_gen *rule)
{
	struct ppe_drv_pm_counter_gen_rule *info = &pm_gen->info;

	/*
	 * Note: If both FLOW_DIR and PM_DIR flags are set, PM_DIR will take
	 * precedence as it directly maps to INGRESS/EGRESS for PM counters,
	 * while RULE_DIR (UPSTREAM/DOWNSTREAM) defines the flow direction.
	 */
	if (rule->rule_flags & PPE_PM_GEN_RULE_FLAG_RULE_DIR) {
		if (rule->rule_dir == PPE_PM_RULE_DIR_INGRESS) {
			info->rule_dir = PPE_DRV_RULE_INGRESS;
		} else if (rule->rule_dir == PPE_PM_RULE_DIR_EGRESS) {
			info->rule_dir = PPE_DRV_RULE_EGRESS;
		} else {
			ppe_pm_warn("Invalid rule_dir: %d", rule->rule_dir);
			return false;
		}
	}

	if (rule->rule_flags & PPE_PM_GEN_RULE_FLAG_PM_DIR) {
		if (rule->pm_dir == PPE_PM_RULE_DIR_INGRESS) {
			info->rule_dir = PPE_DRV_RULE_INGRESS;
		} else if (rule->pm_dir == PPE_PM_RULE_DIR_EGRESS) {
			info->rule_dir = PPE_DRV_RULE_EGRESS;
		} else {
			ppe_pm_warn("Invalid pm_dir: %d", rule->pm_dir);
			return false;
		}
	}

	if (rule->rule_flags & PPE_PM_GEN_RULE_FLAG_PORT_TYPE) {
		switch (rule->port_type) {
			case PPE_PM_PORT_TYPE_BITMAP:
				info->port_type = PPE_DRV_PM_PORT_TYPE_BITMAP;
				info->port_info.port_bitmap = (uint8_t)rule->port.port_bitmap;
				break;

			case PPE_PM_PORT_TYPE_PORT:
			{
				struct net_device *dev;
				struct ppe_drv_iface *iface;
				int32_t port_num;

				info->port_type = PPE_DRV_PM_PORT_TYPE_PORT;
				if (rule->port.dev_name[0] == '\0') {
					ppe_pm_warn("port name is not provided\n");
					return false;
				}

				dev = dev_get_by_name(&init_net, rule->port.dev_name);
				if (!dev) {
					ppe_pm_warn("Failed to find valid src for dev %s\n", rule->port.dev_name);
					return false;
				}

				iface = ppe_drv_iface_get_by_dev(dev);
				if (!iface) {
					ppe_pm_warn("Failed to find PPE interface for dev: %p(%s)\n", dev, rule->port.dev_name);
					dev_put(dev);
					return false;
				}
#ifdef NSS_PPE_VEIP_FEATURE_SUPPORT
				if (ppe_drv_veip_is_enabled(dev)) {
					port_num = ppe_drv_veip_get_port(iface, PPE_DRV_PORT_VIRTUAL_PON);
				} else
#endif
				{
					port_num = ppe_drv_iface_port_idx_get(iface);
				}

				if (port_num < 0) {
					ppe_pm_warn("failed to find PPE port for iface: %p\n", iface);
					dev_put(dev);
					return false;
				}

				info->port_info.port_num = port_num;
				dev_put(dev);
				break;
			}
			case PPE_PM_PORT_TYPE_GEMPORT:
				info->port_type = PPE_DRV_PM_PORT_TYPE_GEMPORT;
				info->port_info.gemport = (uint8_t)rule->port.gem_port;
				break;

			default:
				ppe_pm_warn("Invalid port_type: %d\n", rule->port_type);
				return false;
		}

		info->flags |= PPE_DRV_PM_RULE_FLAG_PORT_ID;
	}

	if (rule->rule_flags & PPE_PM_GEN_RULE_FLAG_TAG_FORMAT) {
		switch (rule->tag_format) {
			case PPE_PM_TAG_FORMAT_UNTAGGED:
				info->tag_format = PPE_DRV_PM_TAG_FORMAT_UNTAGGED;
				break;

			case PPE_PM_TAG_FORMAT_PRIORITY:
				info->tag_format = PPE_DRV_PM_TAG_FORMAT_PRIORITY;
				break;

			case PPE_PM_TAG_FORMAT_TAGGED:
				info->tag_format = PPE_DRV_PM_TAG_FORMAT_TAGGED;
				break;

			case PPE_PM_TAG_FORMAT_PRI_UNTAG:
				info->tag_format = PPE_DRV_PM_TAG_FORMAT_UNTAGGED | PPE_DRV_PM_TAG_FORMAT_PRIORITY;
				break;

			case PPE_PM_TAG_FORMAT_TAG_UNTAG:
				info->tag_format = PPE_DRV_PM_TAG_FORMAT_UNTAGGED | PPE_DRV_PM_TAG_FORMAT_TAGGED;
				break;

			case PPE_PM_TAG_FORMAT_PRI_TAG:
				info->tag_format = PPE_DRV_PM_TAG_FORMAT_PRIORITY | PPE_DRV_PM_TAG_FORMAT_TAGGED;
				break;

			case PPE_PM_TAG_FORMAT_ALL:
				info->tag_format = PPE_DRV_PM_TAG_FORMAT_UNTAGGED | PPE_DRV_PM_TAG_FORMAT_PRIORITY | PPE_DRV_PM_TAG_FORMAT_TAGGED;
				break;

			default:
				ppe_pm_warn("Invalid tag_format: %d\n", rule->tag_format);
				return false;
		}

		info->flags |= PPE_DRV_PM_RULE_FLAG_TAG_FORMAT;
	}

	if (rule->rule_flags & PPE_PM_GEN_RULE_FLAG_VID) {
		info->vid = rule->vid;
		info->flags |= PPE_DRV_PM_RULE_FLAG_VID;
	}

	if (rule->rule_flags & PPE_PM_GEN_RULE_FLAG_PCP) {
		info->pcp = rule->pcp;
		info->flags |= PPE_DRV_PM_RULE_FLAG_PCP;
	}

	if (rule->rule_flags & PPE_PM_GEN_RULE_FLAG_IPMC) {
		switch (rule->ipmc) {
			case PPE_PM_IPMC_TYPE_NON_IPMC:
				info->ipmc = PPE_DRV_PM_IPMC_TYPE_NON_IPMC;
				break;

			case PPE_PM_IPMC_TYPE_IPV4_MC:
				info->ipmc = PPE_DRV_PM_IPMC_TYPE_IPV4_MC;
				break;

			case PPE_PM_IPMC_TYPE_IPV6_MC:
				info->ipmc = PPE_DRV_PM_IPMC_TYPE_IPV6_MC;
				break;

			case PPE_PM_IPMC_TYPE_NONIP_IPV4_MC:
				info->ipmc = PPE_DRV_PM_IPMC_TYPE_NON_IPMC | PPE_DRV_PM_IPMC_TYPE_IPV4_MC;
				break;

			case PPE_PM_IPMC_TYPE_NONIP_IPV6_MC:
				info->ipmc = PPE_DRV_PM_IPMC_TYPE_NON_IPMC | PPE_DRV_PM_IPMC_TYPE_IPV6_MC;
				break;

			case PPE_PM_IPMC_TYPE_IPV4_IPV6_MC:
				info->ipmc = PPE_DRV_PM_IPMC_TYPE_IPV4_MC | PPE_DRV_PM_IPMC_TYPE_IPV6_MC;
				break;

			case PPE_PM_IPMC_TYPE_ALL:
				info->ipmc = PPE_DRV_PM_IPMC_TYPE_NON_IPMC | PPE_DRV_PM_IPMC_TYPE_IPV4_MC | PPE_DRV_PM_IPMC_TYPE_IPV6_MC;
				break;

			default:
				ppe_pm_warn("Invalid ipmc type: %d\n", rule->ipmc);
				return false;
		}

		info->flags |= PPE_DRV_PM_RULE_FLAG_IPMC;
	}

	return true;
}

/*
 * ppe_pm_counter_gen_rule_create()
 *	Create PM counter generation rule in PPE.
 */
ppe_pm_ret_t ppe_pm_counter_gen_rule_create(struct ppe_pm_counter_gen *rule)
{
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;
	struct ppe_pm_gen *pm_gen = NULL;
	struct ppe_drv_pm_counter_ctx *pm_ctx = NULL;
	struct ppe_drv_pm_counter_gen_ctx *gen_ctx = NULL;
	struct ppe_drv_pm_counter_info *info;
	ppe_pm_rule_dir_t rule_dir;
	uint8_t counter_id = 0;
	ppe_pm_ret_t ret;

	counter_id = rule->counter_id;
	if (counter_id >= PPE_DRV_PM_COUNTER_CTX_MAX) {
		ppe_pm_warn("counter_id %d exceeds maximum allowed value\n", counter_id);
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_counter_invalid_id);
		rule->ret = PPE_PM_RET_INVALID_COUNTER_ID;
		return PPE_PM_RET_INVALID_COUNTER_ID;
	}

	spin_lock_bh(&pm_g->lock);

	if (kref_read(&pm_g->counter_array[counter_id].ref)) {
		ppe_pm_warn("counter_id %d is already in use with %u rules\n",
				counter_id, kref_read(&pm_g->counter_array[counter_id].ref));
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_create_fail_rule_exist);
		rule->ret = PPE_PM_RET_CREATE_FAIL_RULE;
		spin_unlock_bh(&pm_g->lock);
		return PPE_PM_RET_CREATE_FAIL_RULE;
	}

	pm_gen = (struct ppe_pm_gen *)ppe_pm_alloc(sizeof(struct ppe_pm_gen));
	if (!pm_gen) {
		ppe_pm_warn("%p: failed to allocate pm_gen memory: %p", pm_g, rule);
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_create_fail_oom);
		ret = PPE_PM_RET_COUNTER_GEN_FAIL_OOM;
		goto fail;
	}

	/*
	 * Fill PM gen rule info
	 */
	if (!ppe_pm_rule_fill(pm_gen, rule)) {
		ppe_pm_warn("%p: failed to configure PM gen rule", pm_g);
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_create_fail_rule_config);
		ret = PPE_PM_RET_CREATE_FAIL_RULE_FILL;
		goto fail;
	}
	pm_gen->counter_id = counter_id;

	/*
	 * PM counter context allocation.
	 */
	rule_dir = (pm_gen->info.rule_dir == PPE_DRV_RULE_INGRESS) ? PPE_PM_RULE_DIR_INGRESS : PPE_PM_RULE_DIR_EGRESS;
	pm_ctx = ppe_pm_counter_alloc_internal(counter_id, rule_dir);
	if (!pm_ctx) {
		ppe_pm_warn("%p: couldn't allocate a free PM counter context for counter_id: %d\n",
				pm_g, counter_id);
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_create_fail_pm_ctx_alloc);
		ret = PPE_PM_RET_COUNTER_CTX_FAIL_OOM;
		goto fail;
	}

	info = ppe_drv_pm_counter_info_get(pm_ctx);
	pm_gen->counter_info = info;

	/*
	 * Replacing PM gen rule counter_id with hw_index for PM counter table.
	 */
	pm_gen->info.counter_id = info->hw_index;

	/*
	 * Allocate empty rule context in driver.
	 */
	gen_ctx = ppe_drv_pm_gen_alloc(pm_gen->info.rule_dir);
	if (!gen_ctx) {
		ppe_pm_warn("%p: couldn't allocate a free PM gen rule context", pm_g);
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_create_fail_oom);
		ret = PPE_PM_RET_COUNTER_GEN_FAIL_OOM;
		goto fail;
	}

	/*
	 * Create PM gen rule in PPE driver
	 */
	if (ppe_drv_pm_gen_rule_create(gen_ctx, &pm_gen->info) != PPE_DRV_RET_SUCCESS) {
		ppe_pm_warn("%p: failed to configure PM gen rule", pm_g);
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_create_fail);
		ret = PPE_PM_RET_CREATE_FAIL_RULE;
		goto fail;
	}

	/*
	 * Add new rule node to list
	 */
	list_add(&pm_gen->list, &pm_g->active_rules);

	/*
	 * Store the status in response.
	 */
	rule->ret = PPE_PM_RET_SUCCESS;

	/*
	 * Update stats
	 */
	ppe_pm_stats_inc(&pm_g->stats.cmn.pm_create_req);
	ppe_pm_info("%p: PM gen rule created with counter ID: %d", pm_g, rule->counter_id);

	/*
	 * Store information.
	 */
	pm_gen->ctx = gen_ctx;
	kref_init(&pm_gen->ref_cnt);
	memcpy(&pm_gen->rule, rule, sizeof(struct ppe_pm_counter_gen));

	spin_unlock_bh(&pm_g->lock);
	return PPE_PM_RET_SUCCESS;

fail:
	if (pm_gen) {
		ppe_pm_free(pm_gen);
	}

	if (gen_ctx) {
		ppe_drv_pm_gen_destroy(gen_ctx);
	}

	if (pm_ctx) {
		ppe_pm_counter_deref_internal(counter_id);
	}

	rule->ret = ret;
	spin_unlock_bh(&pm_g->lock);
	return ret;
}
EXPORT_SYMBOL(ppe_pm_counter_gen_rule_create);

/*
 * ppe_pm_counter_gen_rule_free()
 *	Free a PM counter generation rule in PPE.
 */
static void ppe_pm_counter_gen_rule_free(struct kref *kref)
{
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;
	struct ppe_pm_gen *pm_gen = container_of(kref, struct ppe_pm_gen, ref_cnt);

	/*
	 * Delete the rule node from active list.
	 */
	list_del(&pm_gen->list);

	/*
	 * TODO: check if last stats need to be read before destroying the context
	 * Destroy the rule in PPE driver.
	 */
	if (pm_gen->ctx) {
		ppe_drv_pm_gen_destroy(pm_gen->ctx);
		pm_gen->ctx = NULL;
	}

	/*
	 * Update stats
	 */
	ppe_pm_stats_inc(&pm_g->stats.cmn.pm_free_req);
	ppe_pm_info("%p: PM gen rule freed for counter_id: %u", pm_gen, pm_gen->counter_id);

	/*
	 * free the PM gen rule memory.
	 */
	ppe_pm_free(pm_gen);

}

/*
 * ppe_pm_counter_gen_rule_destroy()
 *	Destroy a PM counter generation rule in PPE.
 */
ppe_pm_ret_t ppe_pm_counter_gen_rule_destroy(uint8_t counter_id)
{
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;
	struct ppe_pm_gen *pm_gen, *tmp;
	ppe_pm_ret_t ret;
	bool found = false;

	if (counter_id >= PPE_DRV_PM_COUNTER_CTX_MAX) {
		ppe_pm_warn("counter_id %d exceeds maximum allowed value %d\n",
				counter_id, PPE_DRV_PM_COUNTER_CTX_MAX);
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_counter_invalid_id);
		return PPE_PM_RET_INVALID_COUNTER_ID;
	}

	spin_lock_bh(&pm_g->lock);

	if (!kref_read(&pm_g->counter_array[counter_id].ref)) {
		ppe_pm_warn("counter_id %d is not in use\n", counter_id);
		ret = PPE_PM_RET_INVALID_COUNTER_ID;
		goto fail;
	}

	list_for_each_entry_safe(pm_gen, tmp, &pm_g->active_rules, list) {
		if (pm_gen->counter_id == counter_id) {
			kref_put(&pm_gen->ref_cnt, ppe_pm_counter_gen_rule_free);
			found = true;
			break;
		}
	}

	if (!found) {
		ppe_pm_warn("No rule found for counter_id %d\n", counter_id);
		ppe_pm_stats_inc(&pm_g->stats.cmn.pm_destroy_fail_no_rule_found);
		ret = PPE_PM_RET_INVALID_COUNTER_ID;
		goto fail;
	}

	/*
	 * Deref PM counter associated with PM rule.
	 */
	ppe_pm_counter_deref_internal(counter_id);

	/*
	 * Update stats
	 */
	ppe_pm_stats_inc(&pm_g->stats.cmn.pm_destroy_req);
	ppe_pm_info("%p: PM gen rule deleted with counter ID: %d", pm_g, counter_id);

	spin_unlock_bh(&pm_g->lock);
	return PPE_PM_RET_SUCCESS;

fail:
	spin_unlock_bh(&pm_g->lock);
	return ret;
}
EXPORT_SYMBOL(ppe_pm_counter_gen_rule_destroy);

/*
 * ppe_pm_deinit()
 *	PM deinit API.
 */
void ppe_pm_deinit(void)
{
	ppe_pm_dump_exit();
	ppe_pm_stats_debugfs_exit();
}
EXPORT_SYMBOL(ppe_pm_deinit);

/*
 * ppe_pm_init()
 *	PM init API.
 */
void ppe_pm_init(struct dentry *d_counter)
{
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;

	spin_lock_init(&pm_g->lock);

	/*
	 * Initialize active list
	 */
	INIT_LIST_HEAD(&pm_g->active_rules);

	/*
	 * Create debugfs directory/files.
	 */
	ppe_pm_stats_debugfs_init(d_counter);

	/*
	 * Initialization of PM counter dump.
	 */
	ppe_pm_dump_init(d_counter);
}
EXPORT_SYMBOL(ppe_pm_init);
