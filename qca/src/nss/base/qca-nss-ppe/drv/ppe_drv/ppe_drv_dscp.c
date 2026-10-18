/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppe_drv.h"

/*
 * ppe_drv_dscp_pbit_rule_dump()
 *	 Dumps of DSCP to P bit rule
 */
static void ppe_drv_dscp_pbit_rule_dump(struct ppe_drv_dscp_pcp_rule *rule) {
	ppe_drv_info("%px dscp to p bit rule dump from ppe_drv\n"
			"rule_dir: %s\n"
			"tos_value: %d\n"
			"group_id: %d\n"
			"pcp_val: %d\n",
			rule, (rule->rule_dir == PPE_DRV_RULE_INGRESS) ? "UPSTREAM" : "DOWNSTREAM",
			rule->tos_val, rule->group_id, rule->pcp_val);
}

/*
 * ppe_drv_dscp_p_tbl_configure()
 *	Configure the dscp_p table in PPE.
 */
ppe_drv_ret_t ppe_drv_dscp_p_tbl_configure(struct ppe_drv_dscp_pcp_rule *rule)
{
	struct ppe_drv *p = ppe_drv_gbl;
	uint8_t pcp_val = 0;
	sw_error_t err = SW_OK;

	if (unlikely(!rule)) {
		ppe_drv_warn("NULL dscp_pcp rule\n");
		return PPE_DRV_RET_DSCP_PBIT_TBL_CONFIG_FAIL;
	}

	spin_lock_bh(&p->lock);

	/*
	 * Dump DSCP_Pbit rule
	 */
	ppe_drv_dscp_pbit_rule_dump(rule);

	if (rule->rule_dir == PPE_DRV_RULE_INGRESS) {
		/*
		 * Upstream direction: Configure IN_VLAN_DSCP_PBIT_MAP_TBL.
		 */
		err = fal_vlan_trans_dscp_pcp_mapping_set(PPE_DRV_SWITCH_ID,  FAL_PORT_VLAN_INGRESS, rule->group_id,  rule->tos_val, rule->pcp_val);
		if (err != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("Failed to configure IN_VLAN_DSCP_PBIT table for tos_val %d, error: %d\n", rule->tos_val, err);
			return PPE_DRV_RET_IN_VLAN_DSCP_PBIT_TBL_CONFIG_FAIL;
		}
	} else {
		/*
		 * Downstream direction: Configure DSCP_PBIT_MAP_TBL and L2_DSCP_PBIT_MAP_TBL.
		 */
		err = fal_vlan_trans_dscp_pcp_mapping_set(PPE_DRV_SWITCH_ID, FAL_PORT_VLAN_EGRESS, rule->group_id,  rule->tos_val, rule->pcp_val);
		if (err != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("Failed to configure DSCP_PBIT table for tos_val %d, error: %d\n", rule->tos_val, err);
			return PPE_DRV_RET_DSCP_PBIT_TBL_CONFIG_FAIL;
		}

		err = fal_acl_dscp_pcp_mapping_set(PPE_DRV_SWITCH_ID, rule->group_id, rule->tos_val, rule->pcp_val);
		if (err != SW_OK) {
			ppe_drv_warn("Failed to configure L2_DSCP_PBIT table for tos_val %d, error: %d\n", rule->tos_val, err);

			/*
			 * Resetting the configured PCP for vlan_tras_dscp_p_bit_tbl.
			 */
			err = fal_vlan_trans_dscp_pcp_mapping_set(PPE_DRV_SWITCH_ID, FAL_PORT_VLAN_EGRESS, rule->group_id,  rule->tos_val, pcp_val);
			if (err != SW_OK) {
				spin_unlock_bh(&p->lock);
				ppe_drv_warn("Failed to reset the DSCP_PBIT table for tos_val %d, error: %d\n", rule->tos_val, err);
				return PPE_DRV_RET_DSCP_PBIT_TBL_CONFIG_FAIL;
			}

			spin_unlock_bh(&p->lock);
			return PPE_DRV_RET_L2_DSCP_PBIT_TBL_CONFIG_FAIL;
		}
	}

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_dscp_p_tbl_configure);
