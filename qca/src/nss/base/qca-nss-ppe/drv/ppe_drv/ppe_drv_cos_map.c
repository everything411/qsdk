/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/types.h>

#include <fal/fal_qos.h>

#include "ppe_drv.h"

/*
 * ppe_drv_cos_map_cosmap_flags_check()
 *	check action flags.
 */
static inline bool ppe_drv_cos_map_cosmap_flags_check(struct ppe_drv_cos_map_cosmap *map, uint32_t flags)
{
	return (map->flags & flags);
}

/*
 * ppe_drv_cos_map_cosmap_set()
 *	Sets internal PCP, internal priority, internal DP,
 * and internal DSCP-based on packet DSCP/PCP.
 */
ppe_drv_ret_t ppe_drv_cos_map_cosmap_set(struct ppe_drv_cos_map_cfg *cfg)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_qos_cosmap_t cos_cfg = {0};
	sw_error_t err;

	if (!cfg) {
		ppe_drv_warn("%p cos map configuration failed", cfg);
		return PPE_DRV_RET_COS_MAP_CFG_FAIL;
	}

	/*
	 * Set DSCP fields in CoS config
	 */
	if (ppe_drv_cos_map_cosmap_flags_check(&cfg->map, PPE_DRV_COS_MAP_ACTION_SET_DSCP)) {
		cos_cfg.internal_dscp = cfg->map.dscp << PPE_DRV_DSCP_SHIFT;
		cos_cfg.dscp_mask = PPE_DRV_DSCP_MASK;
		cos_cfg.dscp_en = A_TRUE;
	}

	/*
	 * Set PCP fields in CoS config
	 */
	if (ppe_drv_cos_map_cosmap_flags_check(&cfg->map, PPE_DRV_COS_MAP_ACTION_SET_PCP)) {
		cos_cfg.internal_pcp = cfg->map.pcp;
		cos_cfg.pcp_en = A_TRUE;
	}

	/*
	 * Set DEI fields in CoS config
	 */
	if (ppe_drv_cos_map_cosmap_flags_check(&cfg->map, PPE_DRV_COS_MAP_ACTION_SET_DEI)) {
		cos_cfg.internal_dei = cfg->map.dei;
		cos_cfg.dei_en = A_TRUE;
	}

	/*
	 * Set priority profile in CoS config
	 */
	if (ppe_drv_cos_map_cosmap_flags_check(&cfg->map, PPE_DRV_COS_MAP_ACTION_SET_INT_PRI)) {
		cos_cfg.internal_pri = cfg->map.pri;
		cos_cfg.pri_en = A_TRUE;
	}

	/*
	 * Set drop precedence profile in CoS config
	 */
	if (ppe_drv_cos_map_cosmap_flags_check(&cfg->map, PPE_DRV_COS_MAP_ACTION_SET_DP)) {
		cos_cfg.internal_dp = cfg->map.dp;
		cos_cfg.dp_en = A_TRUE;
	}

	ppe_drv_trace("%p cosmap set configuration type:%d val:%d actions:"
		"dscp_en:%d internal_dscp:%d pcp_en:%d internal_pcp:%d dei_en:%d internal_dei:%d"
		"pri_en:%d internal_pri:%d dp_en:%d internal_dp:%d", cfg, cfg->type, cfg->val, cos_cfg.dscp_en,
		cos_cfg.internal_dscp, cos_cfg.pcp_en, cos_cfg.internal_pcp, cos_cfg.dei_en, cos_cfg.internal_dei,
		cos_cfg.pri_en, cos_cfg.internal_pri, cos_cfg.dp_en, cos_cfg.internal_dp);

	spin_lock_bh(&p->lock);
	switch (cfg->type) {
		case PPE_DRV_COS_MAP_TYPE_TOS:
			err = fal_qos_cosmap_dscp_set(PPE_DRV_SWITCH_ID, cfg->group_id, cfg->val, &cos_cfg);
			if (err != SW_OK) {
				spin_unlock_bh(&p->lock);
				ppe_drv_warn("%p cos map configuration failed for cos_type:%d val:%d flags:%d", cfg, cfg->type, cfg->val, cfg->map.flags);
				return PPE_DRV_RET_COS_MAP_CFG_FAIL;
			}

			break;

		case PPE_DRV_COS_MAP_TYPE_TCI:
			err = fal_qos_cosmap_pcp_set(PPE_DRV_SWITCH_ID, cfg->group_id, cfg->val, &cos_cfg);
			if (err != SW_OK) {
				spin_unlock_bh(&p->lock);
				ppe_drv_warn("%p cos map configuration failed for cos_type:%d val:%d flags:%d", cfg, cfg->type, cfg->val, cfg->map.flags);
				return PPE_DRV_RET_COS_MAP_CFG_FAIL;
			}

			break;

		default:
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("%p cos map configuration failed for cos_type:%d val:%d flags:%d", cfg, cfg->type, cfg->val, cfg->map.flags);
			return PPE_DRV_RET_COS_MAP_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_trace("%p cos map configuration done for cos_type:%d val:%d flags:%d", cfg, cfg->type, cfg->val, cfg->map.flags);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_cos_map_cosmap_set);

/*
 * ppe_drv_cos_map_port_group_set()
 *	 Sets port group ID for PCP, DSCP, and flow-based QOS.
 */
ppe_drv_ret_t ppe_drv_cos_map_port_group_set(uint32_t port_id, struct ppe_drv_cos_map_port_group_cfg *cfg)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_qos_group_t grp_cfg = {0};
	sw_error_t err;

	if (!cfg) {
		ppe_drv_warn("%p cos port group configuration failed", cfg);
		return PPE_DRV_RET_COS_MAP_CFG_FAIL;
	}

	/*
	 * Set DSCP, PCP, flow group
	 */
	grp_cfg.pcp_group = (uint8_t)cfg->tci_grp_id;
	grp_cfg.dscp_group = (uint8_t)cfg->tos_grp_id;
	grp_cfg.flow_group = (uint8_t)cfg->flow_grp_id;

	spin_lock_bh(&p->lock);
	err = fal_qos_port_group_set(PPE_DRV_SWITCH_ID, port_id, &grp_cfg);
	if (err != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p cos port group configuration failed for port:%d", cfg, port_id);
		return PPE_DRV_RET_COS_MAP_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_trace("%p cos port group configuration done for port:%d", cfg, port_id);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_cos_map_port_group_set);
