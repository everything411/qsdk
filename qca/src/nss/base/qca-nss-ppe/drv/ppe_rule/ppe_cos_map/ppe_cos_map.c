/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <ppe_drv_public.h>
#include <ppe_cos_map.h>
#include "ppe_cos_map.h"

#define PPE_COS_MAP_MAX_GROUP 2
#define PPE_COS_MAP_MAX_DEI_VAL 2
#define PPE_COS_MAP_MAX_PCP_VAL 8
#define PPE_COS_MAP_MAX_DSCP_VAL 256

struct ppe_cos_map_base gbl_ppe_cos_map = {0};

/*
 * ppe_cos_map_get_port_id()
 *	Get the Port ID
 */
static int ppe_cos_map_get_port_id(char *dev_name)
{
	struct net_device *dev = NULL;
	int port_id;

	dev = dev_get_by_name(&init_net, dev_name);
	if (!dev) {
		ppe_cos_map_warn("failed to find valid dev");
		return -1;
	}

	port_id = ppe_drv_port_num_from_dev(dev);
	if ((port_id < 0) || (port_id >= PPE_DRV_PORTS_MAX)) {
		ppe_cos_map_warn("invalid port_num");
		dev_put(dev);
		return -1;
	}

	dev_put(dev);

	ppe_cos_map_trace("dev:%s port_id:%d", dev_name, port_id);
	return port_id;
}

/*
 * ppe_cos_map_port_group_delete()
 * 	Delete CoS map port groups
 */
static void ppe_cos_map_port_group_delete(void)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	struct ppe_cos_map_port_group *port_grp, *tmp;

	spin_lock_bh(&g_cos_map->lock);
	if (list_empty(&g_cos_map->port_list)) {
		spin_unlock_bh(&g_cos_map->lock);
		ppe_cos_map_trace("CoS map port list already empty!\n");
		return;
	}

	/*
	 * Iterating through port list to delete all CoS map port groups
	 */
	list_for_each_entry_safe(port_grp, tmp, &g_cos_map->port_list, list) {
		list_del(&port_grp->list);
		kfree(port_grp);
	}

	spin_unlock_bh(&g_cos_map->lock);
}

/*
 * ppe_cos_map_alloc
 *	Alloc ppe CoS map alloc
 */
struct ppe_cos_map_rule *ppe_cos_map_alloc(void)
{
	/*
	 * Allocate memory for a new rule
	 *
	 * kzalloc with GFP_ATOMIC is used assuming rules can be created
	 * in softirq context as well while processing an SKB in the system.
	 */
	return kzalloc(sizeof(struct ppe_cos_map_rule), GFP_ATOMIC);
}

/*
 * ppe_cos_map_free
 *	Alloc ppe CoS map free
 */
void ppe_cos_map_free(struct ppe_cos_map_rule *rule)
{
	kfree(rule);
}

/*
 * ppe_cos_map_cosmap_tos_set()
 *	Set cosmap based on TOS value
 */
static ppe_cos_map_ret_t ppe_cos_map_cosmap_tos_set(struct ppe_cos_map_rule *rule)
{
	rule->cfg.type =  PPE_DRV_COS_MAP_TYPE_TOS;
	rule->cfg.val = rule->dscp_val;

	if (ppe_drv_cos_map_cosmap_set(&rule->cfg) != PPE_DRV_RET_SUCCESS) {
		ppe_cos_map_warn("cos map configuration failed for dscp:%d", rule->dscp_val);
		return PPE_COS_MAP_FAIL;
	}

	return PPE_COS_MAP_SUCCESS;
}

/*
 * ppe_cos_map_cosmap_tci_set()
 *	Set cosmap based on PCP and DEI value
 */
static ppe_cos_map_ret_t ppe_cos_map_cosmap_tci_set(struct ppe_cos_map_rule *rule)
{
	/*
	 * Set one entry for PCP and DEI combination
	 */
	rule->cfg.type =  PPE_DRV_COS_MAP_TYPE_TCI;
	rule->cfg.val = (rule->pcp_val << 1) + rule->dei_val;

	if (ppe_drv_cos_map_cosmap_set(&rule->cfg) != PPE_DRV_RET_SUCCESS) {
		ppe_cos_map_warn("cos map configuration failed for pcp:%d dei:%d", rule->pcp_val, rule->dei_val);
		return PPE_COS_MAP_FAIL;
	}

	return PPE_COS_MAP_SUCCESS;
}

/*
 * ppe_cos_map_rule_find_by_type()
 *	Find a rule corresponding to a CoS type.
 */
static struct ppe_cos_map_rule *ppe_cos_map_rule_find_by_type(uint32_t group_id, uint32_t type, uint32_t dscp, uint32_t pcp, uint32_t dei)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	struct ppe_cos_map_rule *rule;

	if (!list_empty(&g_cos_map->active_rules)) {
		list_for_each_entry(rule, &g_cos_map->active_rules, list) {
			if (rule->cfg.group_id != group_id) {
				continue;
			}

			if (!(rule->type_flag & type)) {
				continue;
			}

			if ((rule->type_flag & PPE_COS_MAP_RULE_TOS) && (rule->dscp_val == dscp)) {
				goto rule_found;
			}

			if ((rule->type_flag & PPE_COS_MAP_RULE_TCI) && (rule->pcp_val == pcp) && (rule->dei_val == dei)) {
				goto rule_found;
			}
		}
	}

	ppe_cos_map_trace("%p: No valid CoS map rule for cos type:%d", g_cos_map, type);
	return NULL;

rule_found:
	ppe_cos_map_trace("%p: CoS map rule:%p ID:%d found for group_id:%d type:%d",
			g_cos_map, rule, rule->rule_id, rule->cfg.group_id, rule->type_flag);
	return rule;
}

/*
 * ppe_cos_map_rule_find_by_id()
 *	Find a rule corresponding to a rule ID.
 */
static struct ppe_cos_map_rule *ppe_cos_map_rule_find_by_id(int16_t id)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	struct ppe_cos_map_rule *rule;

	if (!list_empty(&g_cos_map->active_rules)) {
		list_for_each_entry(rule, &g_cos_map->active_rules, list) {
			if (rule->rule_id == id) {
				ppe_cos_map_trace("%p: qoS map rule: %p found for ID: %d", g_cos_map, rule, id);
				return rule;
			}
		}
	}

	ppe_cos_map_trace("%p: No valid CoS map rule for ID: %d", g_cos_map, id);
	return NULL;
}

/*
 * ppe_cos_map_create_rule()
 *	Create acl CoS map
 */
static ppe_cos_map_ret_t ppe_cos_map_create_rule(struct ppe_cos_map_create_info *info)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	struct ppe_cos_map_rule *rule;
	int ret = 0;

	rule = ppe_cos_map_rule_find_by_id(info->rule_id);
	if (rule) {
		ppe_cos_map_stats_inc(&g_cos_map->stats.cos_map_rule_already_exists);
		ppe_cos_map_warn("%p: CoS map index already configured: %d", g_cos_map, info->rule_id);
		return PPE_COS_MAP_FAIL;
	}

	rule = ppe_cos_map_rule_find_by_type(info->group_id, info->type_flag, info->dscp_val, info->pcp_val, info->dei_val);
	if (rule) {
		ppe_cos_map_stats_inc(&g_cos_map->stats.cos_map_rule_already_exists);
		ppe_cos_map_warn("%p: CoS map rule:%d already configured for group_id:%d CoS type:%d", g_cos_map, info->rule_id, info->group_id, info->type_flag);
		return PPE_COS_MAP_FAIL;
	}

	if ((info->type_flag & PPE_COS_MAP_RULE_TOS) && (info->dscp_val >= PPE_COS_MAP_MAX_DSCP_VAL)) {
		ppe_cos_map_stats_inc(&g_cos_map->stats.cos_map_rule_invalid_dscp_val);
		ppe_cos_map_warn("%p: CoS map rule invalid DSCP val:%d", g_cos_map, info->dscp_val);
		return PPE_COS_MAP_FAIL;
	} else if ((info->type_flag & PPE_COS_MAP_RULE_TCI) && ((info->pcp_val >= PPE_COS_MAP_MAX_PCP_VAL) || (info->dei_val >= PPE_COS_MAP_MAX_DEI_VAL))){
		ppe_cos_map_stats_inc(&g_cos_map->stats.cos_map_rule_invalid_pcp_dei_val);
		ppe_cos_map_warn("%p: CoS map rule invalid pcp val:%d or dei val:%d", g_cos_map, info->pcp_val, info->dei_val);
		return PPE_COS_MAP_FAIL;
	}

	rule = ppe_cos_map_alloc();
	if (!rule) {
		ppe_cos_map_stats_inc(&g_cos_map->stats.cos_map_rule_mem_alloc_failed);
		ppe_cos_map_warn("%p: failed to allocate memory for CoS map rule: %p", g_cos_map, info);
		return PPE_COS_MAP_FAIL;
	}

	/* CoS map */
	rule->rule_id = info->rule_id;
	rule->type_flag = info->type_flag;
	rule->dscp_val = info->dscp_val;
	rule->pcp_val = info->pcp_val;
	rule->dei_val = info->dei_val;
	rule->cfg.group_id = info->group_id;

	rule->cfg.map.dscp = info->action_info.dscp;
	rule->cfg.map.pcp = info->action_info.pcp;
	rule->cfg.map.pri = info->action_info.pri;
	rule->cfg.map.dei = info->action_info.dei;
	rule->cfg.map.dp = info->action_info.dp;
	rule->cfg.map.flags = info->action_info.flags;

	if (info->type_flag & PPE_COS_MAP_RULE_TOS) {
		ret = ppe_cos_map_cosmap_tos_set(rule);
	} else if (info->type_flag & PPE_COS_MAP_RULE_TCI) {
		ret = ppe_cos_map_cosmap_tci_set(rule);
	} else {
		goto fail;
	}

	if (ret) {
		goto fail;
	}

	list_add(&rule->list, &g_cos_map->active_rules);

	ppe_cos_map_trace("%p: CoS map rule created with rule ID: %d", g_cos_map, rule->rule_id);
	return PPE_COS_MAP_SUCCESS;

fail:
	ppe_cos_map_free(rule);
	ppe_cos_map_warn("%p: CoS map rule creation failed: %p", g_cos_map, info);
	return PPE_COS_MAP_FAIL;
}

/*
 * ppe_cos_map_destroy()
 *	CoS map rule delete
 */
static ppe_cos_map_ret_t ppe_cos_map_delete_rule(struct ppe_cos_map_rule *rule)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	int ret = 0;

	memset(&rule->cfg.map, 0, sizeof(struct ppe_drv_cos_map_cosmap));
	if (rule->type_flag & PPE_COS_MAP_RULE_TOS) {
		ret = ppe_cos_map_cosmap_tos_set(rule);
	} else if (rule->type_flag & PPE_COS_MAP_RULE_TCI) {
		ret = ppe_cos_map_cosmap_tci_set(rule);
	} else {
		goto fail;
	}

	if (ret) {
		goto fail;
	}

	ppe_cos_map_trace("%p: CoS map rule deleted with rule ID: %d", g_cos_map, rule->rule_id);
	return PPE_COS_MAP_SUCCESS;

fail:
	ppe_cos_map_warn("%p: CoS map rule deletion failed: %p", g_cos_map, rule);
	return PPE_COS_MAP_FAIL;
}

/*
 * ppe_cos_map_destroy()
 * 	Destroy CoS map rule
 */
ppe_cos_map_ret_t ppe_cos_map_destroy(struct ppe_cos_map_destroy_info *destroy)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	struct ppe_cos_map_rule *rule = NULL;

	if (!destroy) {
		ppe_cos_map_warn("%p: CoS map rule destroy info not valid", g_cos_map);
		return PPE_COS_MAP_DESTROY_RULE_FAIL;
	}

	spin_lock_bh(&g_cos_map->lock);
	rule = ppe_cos_map_rule_find_by_id(destroy->rule_id);
	if (!rule) {
		ppe_cos_map_stats_inc(&g_cos_map->stats.cos_map_rule_not_found_destroy_fail);
		spin_unlock_bh(&g_cos_map->lock);
		ppe_cos_map_warn("%p: failed to find the rule for ID", g_cos_map);
		return PPE_COS_MAP_DESTROY_RULE_FAIL;
	}

	/*
	 * Reset cosmap for the given rule
	 */
	if (ppe_cos_map_delete_rule(rule)) {
		ppe_cos_map_stats_inc(&g_cos_map->stats.destroy_cos_map_rule_failed);
		spin_unlock_bh(&g_cos_map->lock);
		return PPE_COS_MAP_DESTROY_RULE_FAIL;
	}

	list_del(&rule->list);
	ppe_cos_map_free(rule);

	ppe_cos_map_stats_inc(&g_cos_map->stats.destroy_cos_map_rule_success);
	spin_unlock_bh(&g_cos_map->lock);
	return PPE_COS_MAP_SUCCESS;
}
EXPORT_SYMBOL(ppe_cos_map_destroy);

/*
 * ppe_cos_map_rule_flush()
 * 	Flush  CoS map rule
 */
ppe_cos_map_ret_t ppe_cos_map_rule_flush(void)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	struct ppe_cos_map_rule *rule, *tmp;

	spin_lock_bh(&g_cos_map->lock);
	ppe_cos_map_stats_inc(&g_cos_map->stats.cos_map_rules_flush_req);
	if (list_empty(&g_cos_map->active_rules)) {
		ppe_cos_map_trace("CoS map rule list already empty!\n");
		spin_unlock_bh(&g_cos_map->lock);
		return PPE_COS_MAP_SUCCESS;
	}

	/*
	 * Iterating through rule list to flush all CoS map rules
	 */
	if (!list_empty(&g_cos_map->active_rules)) {
		list_for_each_entry_safe(rule, tmp, &g_cos_map->active_rules, list) {

			/*
			 * Reset cosmap for the given rule and free rule
			 */
			ppe_cos_map_delete_rule(rule);

			list_del(&rule->list);
			ppe_cos_map_free(rule);
		}
	}

	spin_unlock_bh(&g_cos_map->lock);
	return PPE_COS_MAP_SUCCESS;
}
EXPORT_SYMBOL(ppe_cos_map_rule_flush);

/*
 * ppe_cos_map_create()
 * 	Create PPE CoS map rule
 */
ppe_cos_map_ret_t ppe_cos_map_create(struct ppe_cos_map_create_info *create)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;

	if (!create) {
		ppe_cos_map_warn("%p: CoS map rule create info not valid", g_cos_map);
		return PPE_COS_MAP_CREATE_RULE_FAIL;
	}

	spin_lock_bh(&g_cos_map->lock);
	if (ppe_cos_map_create_rule(create)) {
		ppe_cos_map_stats_inc(&g_cos_map->stats.create_cos_map_rule_failed);
		spin_unlock_bh(&g_cos_map->lock);
		ppe_cos_map_warn("Unable to create CoS map rule\n");
		return PPE_COS_MAP_CREATE_RULE_FAIL;
	}

	ppe_cos_map_stats_inc(&g_cos_map->stats.create_cos_map_rule_success);
	spin_unlock_bh(&g_cos_map->lock);

	ppe_cos_map_trace("cos map rule creation successful for rule:%d", create->rule_id);
	return PPE_COS_MAP_SUCCESS;
}
EXPORT_SYMBOL(ppe_cos_map_create);

/*
 * ppe_cos_map_port_group_set()
 * 	Sets port group
 */
ppe_cos_map_ret_t ppe_cos_map_port_group_set(struct ppe_cos_map_port_group_info *info)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	struct ppe_cos_map_port_group *port_grp;
	int port_id;
	bool exists = false;

	if (!info) {
		ppe_cos_map_warn("%p cos port group configuration iinfo not valid", g_cos_map);
		return PPE_COS_MAP_PORT_GROUP_SET_FAIL;
	}

	port_id = ppe_cos_map_get_port_id(info->dev);
	if (port_id < 0) {
		ppe_cos_map_stats_inc(&g_cos_map->stats.set_cos_map_port_group_failed);
		ppe_cos_map_warn("cos port group configuration failed, invalid dev:%s", info->dev);
		return PPE_COS_MAP_PORT_GROUP_SET_FAIL;
	}

	if ((info->tci_grp_id >= PPE_COS_MAP_MAX_GROUP) || (info->tos_grp_id >= PPE_COS_MAP_MAX_GROUP)) {
		ppe_cos_map_stats_inc(&g_cos_map->stats.set_cos_map_port_group_failed);
		ppe_cos_map_warn("cos port group configuration failed for dev:%s", info->dev);
		return PPE_COS_MAP_PORT_GROUP_SET_FAIL;
	}

	/*
	 * Check if the configuration for the given port already exists, overwrite it
	 */
	spin_lock_bh(&g_cos_map->lock);
	if (!list_empty(&g_cos_map->port_list)) {
		list_for_each_entry(port_grp,  &g_cos_map->port_list, list) {
			if (port_grp->port_id == port_id) {
				exists = true;
				ppe_cos_map_trace("cos port group configuration will be updated for dev:%s", info->dev);
				break;
			}
		}
	}

	if (!exists) {
		port_grp = kzalloc(sizeof(struct ppe_cos_map_port_group), GFP_ATOMIC);
		if (!port_grp) {
			ppe_cos_map_stats_inc(&g_cos_map->stats.cos_map_rule_mem_alloc_failed);
			spin_unlock_bh(&g_cos_map->lock);
			ppe_cos_map_warn("cos port group configuration failed for dev:%s", info->dev);
			return PPE_COS_MAP_PORT_GROUP_SET_FAIL;
		}
	}

	strlcpy(port_grp->dev, info->dev, IFNAMSIZ);
	port_grp->port_id = port_id;
	port_grp->cfg.tci_grp_id = info->tci_grp_id;
	port_grp->cfg.tos_grp_id = info->tos_grp_id;

	if (ppe_drv_cos_map_port_group_set(port_id, &port_grp->cfg) != PPE_DRV_RET_SUCCESS) {
		ppe_cos_map_stats_inc(&g_cos_map->stats.set_cos_map_port_group_failed);
		if (!exists) {
			kfree(port_grp);
		}

		spin_unlock_bh(&g_cos_map->lock);
		ppe_cos_map_warn("cos port group configuration failed for dev:%s", info->dev);
		return PPE_COS_MAP_PORT_GROUP_SET_FAIL;
	}

	if (!exists) {
		list_add(&port_grp->list, &g_cos_map->port_list);
	}

	ppe_cos_map_stats_inc(&g_cos_map->stats.set_cos_map_port_group_success);
	spin_unlock_bh(&g_cos_map->lock);

	ppe_cos_map_trace("cos port group configuration successful for dev:%s", info->dev);
	return PPE_COS_MAP_SUCCESS;
}
EXPORT_SYMBOL(ppe_cos_map_port_group_set);

/*
 * ppe_cos_map_deinit()
 *	CoS map deinit API
 */
void ppe_cos_map_deinit(void)
{
	ppe_cos_map_rule_flush();
	ppe_cos_map_port_group_delete();
	ppe_cos_map_dump_exit();
	ppe_cos_map_stats_debugfs_exit();
}

/*
 * ppe_cos_map_init()
 *	CoS map init API
 */
void ppe_cos_map_init(struct dentry *d_rule)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;

	spin_lock_init(&g_cos_map->lock);
	INIT_LIST_HEAD(&g_cos_map->port_list);
	INIT_LIST_HEAD(&g_cos_map->active_rules);
	ppe_cos_map_stats_debugfs_init(d_rule);
	ppe_cos_map_dump_init(d_rule);
}
