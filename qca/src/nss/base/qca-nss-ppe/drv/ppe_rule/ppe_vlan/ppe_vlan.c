/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <ppe_drv_public.h>
#include <ppe_drv_vlan.h>
#include <ppe_drv.h>
#include <ppe_pm.h>
#include "ppe_vlan.h"

#define PPE_VLAN_MANDATORY_RULE_FIELDS (PPE_VLAN_RULE_FLAG_RULE_ID | \
		PPE_VLAN_RULE_FLAG_PORT_TYPE | \
		PPE_VLAN_RULE_FLAG_PORT_VAL | \
		PPE_VLAN_RULE_FLAG_FLOW_DIR)

/*
 * Global VLAN context
 */
struct ppe_vlan_base ppe_vlan_gbl = {0};

/*
 * ppe_vlan_alloc()
 *	Allocate memory for an VLAN rule.
 */
static inline void *ppe_vlan_alloc(size_t size)
{
	return kzalloc(size, GFP_ATOMIC);
}

/*
 * ppe_vlan_free()
 *	Free VLAN rules.
 */
static inline void ppe_vlan_free(void *rule)
{
	kfree(rule);
}

/*
 * ppe_vlan_rule_find_by_id()
 *	Find a rule corresponding to a rule ID.
 */
static struct ppe_vlan *ppe_vlan_rule_find_by_id(int id)
{
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;
	struct ppe_vlan *vlan;

	list_for_each_entry(vlan, &vlan_g->active_rules, list) {
		if (vlan->rule_id == id) {
			ppe_vlan_info("Rule found for ID: %d", id);
			return vlan;
		}
	}

	ppe_vlan_warn("No valid rule for ID: %d", id);
	return NULL;
}

/*
 * ppe_vlan_rule_exist()
 *      Check if VLAN rule already exist.
 */
static bool ppe_vlan_rule_exist(struct ppe_vlan *vlan)
{
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;
	struct ppe_vlan *vlan_active;
	struct ppe_drv_vlan_rule_match *rule_active, *rule_new;
	struct ppe_drv_vlan_action *action_active, *action_new;

	list_for_each_entry(vlan_active, &vlan_g->active_rules, list) {
		if (vlan_active->rule_id == vlan->rule_id) {
			ppe_vlan_info("Rule with same ID: %d exists", vlan->rule_id);
			return true;
		}

		/*
		 * Rules in different directions (upstream vs downstream) are never duplicates.
		 */
		if (vlan_active->info.rule_dir != vlan->info.rule_dir) {
			continue;
		}

		rule_active = &vlan_active->info.rule_f;
		rule_new = &vlan->info.rule_f;
		action_active = &vlan_active->info.action_f;
		action_new = &vlan->info.action_f;

		if ((rule_active->flags != rule_new->flags) || (action_active->flags != action_new->flags)) {
			continue;
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_PORT_TYPE) {
			if (rule_active->port_type != rule_new->port_type) {
				continue;
			}

			/*
			 * For port based VLAN, match dev pointer. For others, match port value.
			 */
			if (rule_new->port_type == PPE_DRV_VLAN_PORT_TYPE_PORT) {
				if (vlan_active->info.src_dev != vlan->info.src_dev) {
					continue;
				}
			} else if (rule_active->port_val != rule_new->port_val) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_STAG_FORMAT) {
			if (rule_active->stag_format != rule_new->stag_format) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_SVID_VAL) {
			if (rule_active->svid != rule_new->svid) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_SPCP_VAL) {
			if (rule_active->spcp != rule_new->spcp) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_SDEI_VAL) {
			if (rule_active->sdei != rule_new->sdei) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_CTAG_FORMAT) {
			if (rule_active->ctag_format != rule_new->ctag_format) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_CVID_VAL) {
			if (rule_active->cvid != rule_new->cvid) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_CPCP_VAL) {
			if (rule_active->cpcp != rule_new->cpcp) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_CDEI_VAL) {
			if (rule_active->cdei != rule_new->cdei) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_FTYPE_VAL) {
			if (rule_active->frame_type != rule_new->frame_type) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_PROTO_VAL) {
			if (rule_active->proto != rule_new->proto) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_VSI_VAL) {
			if (rule_active->vsi != rule_new->vsi) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_VNI_RESV_TYP) {
			if (rule_active->vni_resv_type != rule_new->vni_resv_type) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_VNI_RESV_VAL) {
			if (rule_active->vni_resv != rule_new->vni_resv) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_STPID) {
			if (rule_active->stpid != rule_new->stpid) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_CTPID) {
			if (rule_active->ctpid != rule_new->ctpid) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_DHCP_TYPE) {
			if (rule_active->dhcp_type != rule_new->dhcp_type) {
				continue;
			}
		}

		if (rule_active->flags & PPE_DRV_VLAN_RULE_FLAG_MC_TYPE) {
			if (rule_active->mc_type != rule_new->mc_type) {
				continue;
			}
		}

		ppe_vlan_warn("Identical VLAN rule exists with ID: %d", vlan_active->rule_id);
		return true;
	}

	return false;
}

/*
 * ppe_vlan_action_fill()
 *	Action corresponding to an VLAN rule.
 */
static bool ppe_vlan_action_fill(struct ppe_vlan *vlan, struct ppe_vlan_rule *rule)
{
	struct ppe_vlan_rule_action *r_action = &rule->action_f;
	struct ppe_drv_vlan_cfg  *info = &vlan->info;
	struct ppe_drv_vlan_action *vlan_action = &info->action_f;

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_VID_SWP) {
		if (r_action->swap_svid_cvid) {
			vlan_action->swap_svid_cvid = A_TRUE;
		} else {
			vlan_action->swap_svid_cvid = A_FALSE;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_VID_SWP;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SVID_XLT_CMD) {
		switch (r_action->svid_xlate_cmd) {
			case PPE_VLAN_XLT_CMD_UNCHANGE:
				vlan_action->svid_xlate_cmd = PPE_DRV_VLAN_VID_XLT_CMD_UNCHANGED;
				break;
			case PPE_VLAN_XLT_CMD_ADD:
				vlan_action->svid_xlate_cmd = PPE_DRV_VLAN_VID_XLT_CMD_ADDORREPLACE;
				break;
			case PPE_VLAN_XLT_CMD_DEL:
				vlan_action->svid_xlate_cmd = PPE_DRV_VLAN_VID_XLT_CMD_DELETE;
				break;
			case PPE_VLAN_XLT_CMD_CP_SVID:
				vlan_action->svid_xlate_cmd = PPE_DRV_VLAN_VID_XLT_CMD_CP_SVID;
				break;
			case PPE_VLAN_XLT_CMD_CP_CVID:
				vlan_action->svid_xlate_cmd = PPE_DRV_VLAN_VID_XLT_CMD_CP_CVID;
				break;
			default:
				ppe_vlan_warn("Incorrect input for svid_xlate_cmd parameter: %d\n",
						r_action->svid_xlate_cmd);
				return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_SVID_XLT_CMD;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SVID_XLT_VAL) {
		vlan_action->svidxlate = r_action->svidxlate;
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_SVID_XLT_VAL;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CVID_XLT_CMD) {
		switch (r_action->cvid_xlate_cmd) {
			case PPE_VLAN_XLT_CMD_UNCHANGE:
				vlan_action->cvid_xlate_cmd = PPE_DRV_VLAN_VID_XLT_CMD_UNCHANGED;
				break;
			case PPE_VLAN_XLT_CMD_ADD:
				vlan_action->cvid_xlate_cmd = PPE_DRV_VLAN_VID_XLT_CMD_ADDORREPLACE;
				break;
			case PPE_VLAN_XLT_CMD_DEL:
				vlan_action->cvid_xlate_cmd = PPE_DRV_VLAN_VID_XLT_CMD_DELETE;
				break;
			case PPE_VLAN_XLT_CMD_CP_SVID:
				vlan_action->cvid_xlate_cmd = PPE_DRV_VLAN_VID_XLT_CMD_CP_SVID;
				break;
			case PPE_VLAN_XLT_CMD_CP_CVID:
				vlan_action->cvid_xlate_cmd = PPE_DRV_VLAN_VID_XLT_CMD_CP_CVID;
				break;
			default:
				ppe_vlan_warn("Incorrect input for cvid_xlate_cmd parameter: %d\n",
						r_action->cvid_xlate_cmd);
				return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_CVID_XLT_CMD;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CVID_XLT_VAL) {
		vlan_action->cvidxlate = r_action->cvidxlate;
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_CVID_XLT_VAL;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_PCP_SWP) {
		if (r_action->swap_spcp_cpcp) {
			vlan_action->swap_spcp_cpcp = A_TRUE;
		} else {
			vlan_action->swap_spcp_cpcp = A_FALSE;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_PCP_SWP;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SPCP_XLT_CMD) {
		switch (r_action->spcp_xlate_cmd) {
			case PPE_VLAN_PCP_XLT_CMD_UNCHANGE:
				vlan_action->spcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_UNCHANGED;
				break;
			case PPE_VLAN_PCP_XLT_CMD_REPLACE:
				vlan_action->spcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_REPLACE;
				break;
			case PPE_VLAN_PCP_XLT_CMD_CP_SPCP:
				vlan_action->spcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_CP_SPCP;
				break;
			case PPE_VLAN_PCP_XLT_CMD_CP_CPCP:
				vlan_action->spcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_CP_CPCP;
				break;
			case PPE_VLAN_PCP_XLT_CMD_DSCP_PCP0:
				vlan_action->spcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_DSCP;
				vlan_action->dscp_p_bit_map_ind = PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP0;
				break;
			case PPE_VLAN_PCP_XLT_CMD_DSCP_PCP1:
				vlan_action->spcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_DSCP;
				vlan_action->dscp_p_bit_map_ind = PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP1;
				break;
			case PPE_VLAN_PCP_XLT_CMD_ADD_REP_PCP:
				vlan_action->spcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_ADD_REP_PCP;
				break;
			case PPE_VLAN_PCP_XLT_CMD_ADD_CP_SPCP:
				vlan_action->spcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_ADD_CP_SPCP;
				break;
			case PPE_VLAN_PCP_XLT_CMD_ADD_CP_CPCP:
				vlan_action->spcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_ADD_CP_CPCP;
				break;
			case PPE_VLAN_PCP_XLT_CMD_ADD_DSCP_PCP0:
				vlan_action->spcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_ADD_DSCP;
				vlan_action->dscp_p_bit_map_ind = PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP0;
				break;
			case PPE_VLAN_PCP_XLT_CMD_ADD_DSCP_PCP1:
				vlan_action->spcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_ADD_DSCP;
				vlan_action->dscp_p_bit_map_ind = PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP1;
				break;
			default:
				ppe_vlan_warn("Incorrect input for spcp_xlate_cmd parameter: %d\n",
						r_action->spcp_xlate_cmd);
				return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_SPCP_XLT_CMD;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SPCP_XLT_VAL) {
		/*
		 * The spcptranslation field is a 3-bit value, allowing a maximum value of 7.
		 */
		if (r_action->spcptranslation <= 7) {
			vlan_action->spcptranslation = r_action->spcptranslation;
		} else {
			ppe_vlan_warn("Invalid value for spcp_translation parameter: %d.\n"
					"Valid Range: 0-7\n", r_action->spcptranslation);
			return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_SPCP_XLT_VAL;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CPCP_XLT_CMD) {
		switch (r_action->cpcp_xlate_cmd) {
			case PPE_VLAN_PCP_XLT_CMD_UNCHANGE:
				vlan_action->cpcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_UNCHANGED;
				break;
			case PPE_VLAN_PCP_XLT_CMD_REPLACE:
				vlan_action->cpcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_REPLACE;
				break;
			case PPE_VLAN_PCP_XLT_CMD_CP_SPCP:
				vlan_action->cpcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_CP_SPCP;
				break;
			case PPE_VLAN_PCP_XLT_CMD_CP_CPCP:
				vlan_action->cpcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_CP_CPCP;
				break;
			case PPE_VLAN_PCP_XLT_CMD_DSCP_PCP0:
				vlan_action->cpcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_DSCP;
				vlan_action->dscp_p_bit_map_ind = PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP0;
				break;
			case PPE_VLAN_PCP_XLT_CMD_DSCP_PCP1:
				vlan_action->cpcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_DSCP;
				vlan_action->dscp_p_bit_map_ind = PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP1;
				break;
			case PPE_VLAN_PCP_XLT_CMD_ADD_REP_PCP:
				vlan_action->cpcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_ADD_REP_PCP;
				break;
			case PPE_VLAN_PCP_XLT_CMD_ADD_CP_SPCP:
				vlan_action->cpcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_ADD_CP_SPCP;
				break;
			case PPE_VLAN_PCP_XLT_CMD_ADD_CP_CPCP:
				vlan_action->cpcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_ADD_CP_CPCP;
				break;
			case PPE_VLAN_PCP_XLT_CMD_ADD_DSCP_PCP0:
				vlan_action->cpcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_ADD_DSCP;
				vlan_action->dscp_p_bit_map_ind = PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP0;
				break;
			case PPE_VLAN_PCP_XLT_CMD_ADD_DSCP_PCP1:
				vlan_action->cpcp_xlate_cmd = PPE_DRV_VLAN_PCP_XLT_CMD_ADD_DSCP;
				vlan_action->dscp_p_bit_map_ind = PPE_DRV_VLAN_DSCP_PBIT_INDEX_PCP1;
				break;
			default:
				ppe_vlan_warn("Incorrect input for cpcp_xlate_cmd parameter: %d\n",
						r_action->cpcp_xlate_cmd);
				return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_CPCP_XLT_CMD;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CPCP_XLT_VAL) {
		/*
		 * The cpcptranslation field is a 3-bit value, allowing a maximum value of 7.
		 */
		if (r_action->cpcptranslation <= 7) {
			vlan_action->cpcptranslation = r_action->cpcptranslation;
		} else {
			ppe_vlan_warn("Invalid value for cpcp_translation parameter: %d.\n"
					"Valid Range: 0-7\n", r_action->cpcptranslation);
			return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_CPCP_XLT_VAL;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_DEI_SWP) {
		if (r_action->swap_sdei_cdei) {
			vlan_action->swap_sdei_cdei = A_TRUE;
		} else {
			vlan_action->swap_sdei_cdei = A_FALSE;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_DEI_SWP;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SDEI_XLT_CMD) {
		switch (r_action->sdei_xlate_cmd) {
			case PPE_VLAN_DEI_XLT_CMD_UNCHANGE:
				vlan_action->sdei_xlate_cmd = PPE_DRV_VLAN_DEI_XLT_CMD_UNCHANGED;
				break;
			case PPE_VLAN_DEI_XLT_CMD_REPLACE:
				vlan_action->sdei_xlate_cmd = PPE_DRV_VLAN_DEI_XLT_CMD_REPLACE;
				break;
			case PPE_VLAN_DEI_XLT_CMD_CP_SDEI:
				vlan_action->sdei_xlate_cmd = PPE_DRV_VLAN_DEI_XLT_CMD_CP_SDEI;
				break;
			case PPE_VLAN_DEI_XLT_CMD_CP_CDEI:
				vlan_action->sdei_xlate_cmd = PPE_DRV_VLAN_DEI_XLT_CMD_CP_CDEI;
				break;
			default:
				ppe_vlan_warn("Incorrect input for sdei_xlate_cmd parameter: %d\n",
						r_action->sdei_xlate_cmd);
				return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_SDEI_XLT_CMD;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SDEI_XLT_VAL) {
		/*
		 * The sdeitranslation field is a 1-bit value, allowing a maximum value of 1.
		 */
		if (r_action->sdeitranslation <= 1) {
			vlan_action->sdeitranslation = r_action->sdeitranslation;
		} else {
			ppe_vlan_warn("Invalid value for sdei_translation parameter: %d.\n"
					"Valid Range 0-1\n", r_action->sdeitranslation);
			return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_SDEI_XLT_VAL;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CDEI_XLT_CMD) {
		switch (r_action->cdei_xlate_cmd) {
			case PPE_VLAN_DEI_XLT_CMD_UNCHANGE:
				vlan_action->cdei_xlate_cmd = PPE_DRV_VLAN_DEI_XLT_CMD_UNCHANGED;
				break;
			case PPE_VLAN_DEI_XLT_CMD_REPLACE:
				vlan_action->cdei_xlate_cmd = PPE_DRV_VLAN_DEI_XLT_CMD_REPLACE;
				break;
			case PPE_VLAN_DEI_XLT_CMD_CP_SDEI:
				vlan_action->cdei_xlate_cmd = PPE_DRV_VLAN_DEI_XLT_CMD_CP_SDEI;
				break;
			case PPE_VLAN_DEI_XLT_CMD_CP_CDEI:
				vlan_action->cdei_xlate_cmd = PPE_DRV_VLAN_DEI_XLT_CMD_CP_CDEI;
				break;
			default:
				ppe_vlan_warn("Incorrect input for cdei_xlate_cmd parameter: %d\n",
						r_action->cdei_xlate_cmd);
				return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_CDEI_XLT_CMD;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CDEI_XLT_VAL) {
		/*
		 * The cdeitranslation field is a 1-bit value, allowing a maximum value of 1.
		 */
		if (r_action->cdeitranslation <= 1) {
			vlan_action->cdeitranslation = r_action->cdeitranslation;
		} else {
			ppe_vlan_warn("Invalid value for cdei_translation parameter: %d.\n"
					"Valid Range 0-1\n", r_action->cdeitranslation);
			return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_CDEI_XLT_VAL;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_TAGS_TO_REMOVE) {
		/*
		 * The tags_to_remove field is a 2-bit value,
		 * 	allowing a maximum value of 3 but 3 is reserved.
		 */
		if (r_action->tags_to_remove < 3) {
			vlan_action->tags_to_remove = r_action->tags_to_remove;
		} else {
			ppe_vlan_warn("Invalid value for tags_to_remove  parameter: %d.\n"
					"Valid Range 0-2\n", r_action->tags_to_remove);
			return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_TAGS_TO_REMOVE;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_STPID_CMD) {
		switch (r_action->stpid_cmd) {
			case PPE_VLAN_TPID_CMD_UNCHANGE:
				vlan_action->stpid_cmd = PPE_DRV_VLAN_TPID_CMD_UNCHANGED;
				break;
			case PPE_VLAN_TPID_CMD_REPLACE:
				vlan_action->stpid_cmd = PPE_DRV_VLAN_TPID_CMD_REPLACE;
				break;
			case PPE_VLAN_TPID_CMD_CP_STPID:
				vlan_action->stpid_cmd = PPE_DRV_VLAN_TPID_CMD_CP_STPID;
				break;
			case PPE_VLAN_TPID_CMD_CP_CTPID:
				vlan_action->stpid_cmd = PPE_DRV_VLAN_TPID_CMD_CP_CTPID;
				break;
			default:
				ppe_vlan_warn("Incorrect input for stpid_cmd parameter: %d\n",
						r_action->stpid_cmd);
				return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_STPID_CMD;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_STPID) {
		vlan_action->stpid_action = ppe_drv_get_tpid_index(r_action->stpid_action, "stpid");
		if (vlan_action->stpid_action < 0) {
			ppe_vlan_warn("Invalid STPID: 0x%x not found in global TPID list", r_action->stpid_action);
			return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_STPID;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CTPID_CMD) {
		switch (r_action->ctpid_cmd) {
			case PPE_VLAN_TPID_CMD_UNCHANGE:
				vlan_action->ctpid_cmd = PPE_DRV_VLAN_TPID_CMD_UNCHANGED;
				break;
			case PPE_VLAN_TPID_CMD_REPLACE:
				vlan_action->ctpid_cmd = PPE_DRV_VLAN_TPID_CMD_REPLACE;
				break;
			case PPE_VLAN_TPID_CMD_CP_STPID:
				vlan_action->ctpid_cmd = PPE_DRV_VLAN_TPID_CMD_CP_STPID;
				break;
			case PPE_VLAN_TPID_CMD_CP_CTPID:
				vlan_action->ctpid_cmd = PPE_DRV_VLAN_TPID_CMD_CP_CTPID;
				break;
			default:
				ppe_vlan_warn("Incorrect input for ctpid_cmd parameter: %d\n",
						r_action->ctpid_cmd);
				return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_CTPID_CMD;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CTPID) {
		vlan_action->ctpid_action = ppe_drv_get_tpid_index(r_action->ctpid_action, "ctpid");
		if (vlan_action->ctpid_action < 0) {
			ppe_vlan_warn("Invalid CTPID: 0x%x not found in global TPID list", r_action->ctpid_action);
			return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_CTPID;
	}

	/*
	 * counter_id is only valid when counter_mode is also specified.
	 */
	if ((r_action->action_flags & PPE_VLAN_ACTION_FLAG_CNTR_ID) &&
			!(r_action->action_flags & PPE_VLAN_ACTION_FLAG_CNTR_MODE)) {
		ppe_vlan_warn("counter_id is only valid when counter_mode is specified\n");
		return false;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CNTR_MODE) {
		if (rule->rule_dir == PPE_VLAN_RULE_DIR_EGRESS) {
			ppe_vlan_warn("counter_mode/counter_id is not valid with DOWNSTREAM rule_dir\n");
			return false;
		}

		switch (r_action->counter_mode) {
			case PPE_VLAN_COUNTER_MODE_VLAN:
				if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CNTR_ID) {
					ppe_vlan_warn("counter_id is not valid with VLAN counter_mode\n");
					return false;
				}

				vlan_action->counter_mode = PPE_DRV_VLAN_COUNTER_MODE_VLAN;
				break;

			case PPE_VLAN_COUNTER_MODE_PON_PM:
				if (!(r_action->action_flags & PPE_VLAN_ACTION_FLAG_CNTR_ID)) {
					ppe_vlan_warn("counter_id must be specified with PON_PM counter_mode\n");
					return false;
				}

				if (r_action->counter_id < 0 || r_action->counter_id >= 64) {
					ppe_vlan_warn("Invalid value for counter parameter: %d.\n"
							"Valid Range 0-63\n", r_action->counter_id);
					return false;
				}

				vlan_action->counter_id = r_action->counter_id;
				vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_CNTR_ID;
				vlan_action->counter_mode = PPE_DRV_VLAN_COUNTER_MODE_PONPM;
				break;

			default:
				ppe_vlan_warn("Incorrect input for counter_mode parameter: %d\n",
						r_action->counter_mode);
				return false;
		}

		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_CNTR_MODE;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_VSI_XLT_VAL) {
		if (rule->rule_dir == PPE_VLAN_RULE_DIR_EGRESS) {
			ppe_vlan_warn("vsitranslation is not supported with downstream rule_dir\n");
			return false;
		}

		/*
		 * The vsitranslation field is a 6-bit value, allowing a maximum value of 63.
		 */
		if(r_action->vsitranslation <= 63) {
			vlan_action->vsitranslation = r_action->vsitranslation;
		} else {
			ppe_vlan_warn("Invalid value for vsi_translation parameter: %d\n",
					r_action->vsitranslation);
			return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_VSI_XLT_VAL;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SRC_INFO_TYP) {
		if (rule->rule_dir == PPE_VLAN_RULE_DIR_EGRESS) {
			ppe_vlan_warn("src_info_type is not supported with downstream rule_dir\n");
			return false;
		}

		switch (r_action->src_info_type) {
			case PPE_VLAN_SRC_INFO_TYPE_VP:
				vlan_action->src_info_type = PPE_DRV_VLAN_SRC_INFO_TYPE_VP;
				break;
			case PPE_VLAN_SRC_INFO_TYPE_L3_IF:
				vlan_action->src_info_type = PPE_DRV_VLAN_SRC_INFO_TYPE_L3IF;
				break;
			default:
				ppe_vlan_warn("Incorrect input for src_info_type parameter: %d\n",
						r_action->src_info_type);
				return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_SRC_INFO_TYP;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SRC_INFO_VAL) {
		if (rule->rule_dir == PPE_VLAN_RULE_DIR_EGRESS) {
			ppe_vlan_warn("src_info is not supported with downstream rule_dir\n");
			return false;
		}

		struct net_device *src_dev = dev_get_by_name(&init_net, r_action->src_info);
		if (!src_dev) {
			ppe_vlan_warn("Failed to find valid src for dev %s\n", r_action->src_info);
			return false;
		}

		struct ppe_drv_iface *iface = ppe_drv_iface_get_by_dev(src_dev);
		if (!iface) {
			ppe_vlan_warn("Failed to find PPE iface for dev %s\n", r_action->src_info);
			dev_put(src_dev);
			return false;
		}

		if (vlan_action->src_info_type == PPE_DRV_VLAN_SRC_INFO_TYPE_VP) {
			vlan_action->src_info = ppe_drv_iface_port_idx_get(iface);
		} else if (vlan_action->src_info_type == PPE_DRV_VLAN_SRC_INFO_TYPE_L3IF) {
			vlan_action->src_info = ppe_drv_iface_l3_if_idx_get(iface);
		}

		dev_put(src_dev);
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_SRC_INFO_VAL;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_VNI_RESV_VAL) {
		vlan_action->vni_resv_action = r_action->vni_resv_action;
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_VNI_RESV_VAL;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_FWD_CMD) {
		switch (r_action->fwd_cmd) {
			case PPE_VLAN_FWD_CMD_FORWARD:
				vlan_action->fwd_cmd = PPE_DRV_VLAN_FWD_CMD_FORWARD;
				break;
			case PPE_VLAN_FWD_CMD_DROP:
				vlan_action->fwd_cmd = PPE_DRV_VLAN_FWD_CMD_DROP;
				break;
			case PPE_VLAN_FWD_CMD_COPY:
				if (rule->rule_dir == PPE_VLAN_RULE_DIR_INGRESS) {
					vlan_action->fwd_cmd = PPE_DRV_VLAN_FWD_CMD_COPY;
				} else {
					ppe_vlan_warn("Incorrect input for fwd_cmd parameter: %d for DOWNSTREAM\n",
							r_action->fwd_cmd);
					return false;
				}
				break;
			case PPE_VLAN_FWD_CMD_REDIRECT:
				if (rule->rule_dir == PPE_VLAN_RULE_DIR_INGRESS) {
					vlan_action->fwd_cmd = PPE_DRV_VLAN_FWD_CMD_REDIRECT;
				} else {
					ppe_vlan_warn("Incorrect input for fwd_cmd parameter: %d for DOWNSTREAM\n",
							r_action->fwd_cmd);
					return false;
				}
				break;
			default:
				ppe_vlan_warn("Incorrect input for fwd_cmd parameter: %d\n",
						r_action->fwd_cmd);
				return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_FWD_CMD;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SVC_CODE) {
		if (rule->rule_dir == PPE_VLAN_RULE_DIR_EGRESS) {
			ppe_vlan_warn("sc is not supported with downstream rule_dir\n");
			return false;
		}

		/*
		 * The service code field is a 8-bit value, allowing a maximum value of 255.
		 */
		if (r_action->sc <= 255) {
			vlan_action->sc = r_action->sc;
		} else {
			ppe_vlan_warn("Invalid value for service code parameter: %d\n",
					r_action->sc);
			return false;
		}
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_SVC_CODE;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_DEST_INFO) {
		if (rule->rule_dir == PPE_VLAN_RULE_DIR_EGRESS) {
			ppe_vlan_warn("dest_info is not supported with downstream rule_dir\n");
			return false;
		}

		vlan->info.dst_dev = dev_get_by_name(&init_net, r_action->dest_info);
		if (!vlan->info.dst_dev) {
			ppe_vlan_warn("Failed to find valid dest for dev %s\n",
					r_action->dest_info);
			return false;
		}

		dev_put(vlan->info.dst_dev);
		vlan_action->flags |= PPE_DRV_VLAN_ACTION_FLAG_DEST_INFO;
	}

	return true;
}

/*
 * ppe_vlan_rule_fill()
 *	Rule info corresponding to an VLAN rule.
 */
static bool ppe_vlan_rule_fill(struct ppe_vlan *vlan, struct ppe_vlan_rule *rule)
{
	struct ppe_vlan_rule_match *r_rule = &rule->rule_f;
	struct ppe_drv_vlan_cfg *info = &vlan->info;
	struct ppe_drv_vlan_rule_match *vlan_rule = &info->rule_f;

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_PORT_TYPE) {
		switch (r_rule->port_type) {
			case PPE_VLAN_PORT_TYPE_BITMAP:
				vlan_rule->port_type = PPE_DRV_VLAN_PORT_TYPE_BITMAP;
				vlan_rule->port_val = (uint8_t)r_rule->port_val.port_bitmap;
				break;
			case PPE_VLAN_PORT_TYPE_PORT:
				vlan_rule->port_type = PPE_DRV_VLAN_PORT_TYPE_PORT;
				vlan->info.src_dev = dev_get_by_name(&init_net, r_rule->port_val.dev_name);
				if (!vlan->info.src_dev) {
					ppe_vlan_warn("Failed to find valid src for dev %s\n",
							r_rule->port_val.dev_name);
					return false;
				}
				dev_put(vlan->info.src_dev);
				break;
			case PPE_VLAN_PORT_TYPE_GEM_PORT:
				if (rule->rule_dir == PPE_VLAN_RULE_DIR_EGRESS) {
					ppe_vlan_warn("GEM_PORT is not supported with downstream rule_dir\n");
					return false;
				}
				vlan_rule->port_type = PPE_DRV_VLAN_PORT_TYPE_GEMPORT;
				vlan_rule->port_val = (uint8_t)r_rule->port_val.gem_port;
				break;
			default:
				ppe_vlan_warn("Incorrect input for port_type parameter: %d\n",
						r_rule->port_type);
				return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_PORT_TYPE;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_STAG_FORMAT) {
		switch (r_rule->stag_format) {
			case PPE_VLAN_TAG_FORMAT_UNTAGGED:
				vlan_rule->stag_format = PPE_DRV_VLAN_XLT_MATCH_UNTAGGED;
				break;
			case PPE_VLAN_TAG_FORMAT_PRIORITY:
				vlan_rule->stag_format = PPE_DRV_VLAN_XLT_MATCH_PRIORITY;
				break;
			case PPE_VLAN_TAG_FORMAT_PRI_UNTAG:
				vlan_rule->stag_format = PPE_DRV_VLAN_XLT_MATCH_PRIORITY | PPE_DRV_VLAN_XLT_MATCH_UNTAGGED;
				break;
			case PPE_VLAN_TAG_FORMAT_TAGGED:
				vlan_rule->stag_format = PPE_DRV_VLAN_XLT_MATCH_TAGGED;
				break;
			case PPE_VLAN_TAG_FORMAT_TAG_UNTAG:
				vlan_rule->stag_format = PPE_DRV_VLAN_XLT_MATCH_TAGGED | PPE_DRV_VLAN_XLT_MATCH_UNTAGGED;
				break;
			case PPE_VLAN_TAG_FORMAT_PRI_TAG:
				vlan_rule->stag_format = PPE_DRV_VLAN_XLT_MATCH_PRIORITY | PPE_DRV_VLAN_XLT_MATCH_TAGGED;
				break;
			case PPE_VLAN_TAG_FORMAT_ALL:
				vlan_rule->stag_format = (PPE_DRV_VLAN_XLT_MATCH_UNTAGGED | PPE_DRV_VLAN_XLT_MATCH_PRIORITY | PPE_DRV_VLAN_XLT_MATCH_TAGGED);
				break;
			default:
				ppe_vlan_warn("Incorrect input for stag_format parameter: %d\n",
						r_rule->stag_format);
				return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_STAG_FORMAT;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_SVID_VAL) {
		if (r_rule->svid > 4095) {
			ppe_vlan_warn("Invalid svid value: %u. Valid range is 0-4095\n", r_rule->svid);
			return false;
		}
		vlan_rule->svid = r_rule->svid;
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_SVID_VAL;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_SPCP_VAL) {
		/*
		 * The spcp field is a 3-bit value, allowing a maximum value of 7.
		 */
		if (r_rule->spcp <= 7) {
			vlan_rule->spcp = r_rule->spcp;
		} else {
			ppe_vlan_warn("Invalid spcp value: %d. Value should be in range of 1-7\n",
					r_rule->spcp);
			return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_SPCP_VAL;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_SDEI_VAL) {
		/*
		 * The sdei field is a 1-bit value, allowing a maximum value of 1.
		 */
		if (r_rule->sdei <=1) {
			vlan_rule->sdei = r_rule->sdei;
		} else {
			ppe_vlan_warn("Invalid sdei value: %d\n", r_rule->sdei);
			return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_SDEI_VAL;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_CTAG_FORMAT) {
		switch (r_rule->ctag_format) {
			case PPE_VLAN_TAG_FORMAT_UNTAGGED:
				vlan_rule->ctag_format = PPE_DRV_VLAN_XLT_MATCH_UNTAGGED;
				break;
			case PPE_VLAN_TAG_FORMAT_PRIORITY:
				vlan_rule->ctag_format = PPE_DRV_VLAN_XLT_MATCH_PRIORITY;
				break;
			case PPE_VLAN_TAG_FORMAT_PRI_UNTAG:
				vlan_rule->ctag_format = PPE_DRV_VLAN_XLT_MATCH_PRIORITY | PPE_DRV_VLAN_XLT_MATCH_UNTAGGED;
				break;
			case PPE_VLAN_TAG_FORMAT_TAGGED:
				vlan_rule->ctag_format = PPE_DRV_VLAN_XLT_MATCH_TAGGED;
				break;
			case PPE_VLAN_TAG_FORMAT_TAG_UNTAG:
				vlan_rule->ctag_format = PPE_DRV_VLAN_XLT_MATCH_TAGGED | PPE_DRV_VLAN_XLT_MATCH_UNTAGGED;
				break;
			case PPE_VLAN_TAG_FORMAT_PRI_TAG:
				vlan_rule->ctag_format = PPE_DRV_VLAN_XLT_MATCH_PRIORITY | PPE_DRV_VLAN_XLT_MATCH_TAGGED;
				break;
			case PPE_VLAN_TAG_FORMAT_ALL:
				vlan_rule->ctag_format = (PPE_DRV_VLAN_XLT_MATCH_UNTAGGED | PPE_DRV_VLAN_XLT_MATCH_PRIORITY | PPE_DRV_VLAN_XLT_MATCH_TAGGED);
				break;
			default:
				ppe_vlan_warn("Incorrect input for ctag_format parameter: %d\n",
						r_rule->ctag_format);
				return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_CTAG_FORMAT;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_CVID_VAL) {
		if (r_rule->cvid > 4095) {
			ppe_vlan_warn("Invalid cvid value: %u. Valid range is 0-4095\n", r_rule->cvid);
			return false;
		}
		vlan_rule->cvid = r_rule->cvid;
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_CVID_VAL;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_CPCP_VAL) {
		/*
		 * The cpcp field is a 3-bit value, allowing a maximum value of 7.
		 */
		if (r_rule->cpcp <= 7) {
			vlan_rule->cpcp = r_rule->cpcp;
		} else {
			ppe_vlan_warn("Invalid cpcp value: %d. Value should be in range of 1-7",
					r_rule->cpcp);
			return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_CPCP_VAL;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_CDEI_VAL) {
		/*
		 * The cdei field is a 1-bit value, allowing a maximum value of 1.
		 */
		if (r_rule->cdei <=1) {
			vlan_rule->cdei = r_rule->cdei;
		} else {
			ppe_vlan_warn("Invalid cdei value: %d", r_rule->cdei);
			return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_CDEI_VAL;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_FTYPE_VAL) {
		if (rule->rule_dir == PPE_VLAN_RULE_DIR_EGRESS) {
			ppe_vlan_warn("frame_type is not supported for downstream direction\n");
			return false;
		}

		switch (r_rule->frame_type) {
			case PPE_VLAN_FRAME_TYPE_ETHERNET:
				vlan_rule->frame_type = PPE_DRV_VLAN_FRAME_TYPE_ETHERNET;
				break;
			case PPE_VLAN_FRAME_TYPE_RFC_1024:
				vlan_rule->frame_type = PPE_DRV_VLAN_FRAME_TYPE_RFC_1024;
				break;
			case PPE_VLAN_FRAME_TYPE_LLC_OTHER:
				vlan_rule->frame_type = PPE_DRV_VLAN_FRAME_TYPE_LLC_OTHER;
				break;
			case PPE_VLAN_FRAME_TYPE_ETHORRFC1024:
				vlan_rule->frame_type = PPE_DRV_VLAN_FRAME_TYPE_ETHORRFC1024;
				break;
			default:
				ppe_vlan_warn("Incorrect input for frame_type parameter: %d\n",
						r_rule->frame_type);
				return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_FTYPE_VAL;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_PROTO_VAL) {
		vlan_rule->proto = r_rule->proto;
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_PROTO_VAL;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_VSI_VAL) {
		int max_vsi;

		if (rule->rule_dir == PPE_VLAN_RULE_DIR_INGRESS) {
			ppe_vlan_warn("vsi is not supported for upstream direction\n");
			return false;
		}

		/*
		 * Get maximum VSI count from PPE driver
		 * ppe_drv_get_vsi_num() returns number of VSIs supported (N).
		 * Valid VSI indices are typically 0 .. N-1.
		 */
		max_vsi = ppe_drv_get_vsi_num();
		if (max_vsi <= 0) {
			ppe_vlan_warn("Invalid max vsi returned by driver: %d\n", max_vsi);
			return false;
		}

		if (r_rule->vsi < max_vsi) {
			vlan_rule->vsi = r_rule->vsi;
		} else {
			ppe_vlan_warn("Invalid vsi value: %d (max supported: %d)\n",
					r_rule->vsi, max_vsi - 1);
			return false;
		}

		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_VSI_VAL;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_VNI_RESV_TYP) {
		switch (r_rule->vni_resv_type) {
			case PPE_VLAN_VNI_RESV_TYPE_VNI_ONLY:
				vlan_rule->vni_resv_type = PPE_DRV_VLAN_VNI_RESV_TYPE_VNI_ONLY;
				break;
			case PPE_VLAN_VNI_RESV_TYPE_VNI_RESV:
				vlan_rule->vni_resv_type = PPE_DRV_VLAN_VNI_RESV_TYPE_VNI_RESV;
				break;
			default:
				ppe_vlan_warn("Incorrect input for vni_resv_type parameter: %d\n",
						r_rule->vni_resv_type);
				return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_VNI_RESV_TYP;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_VNI_RESV_VAL) {
		vlan_rule->vni_resv = r_rule->vni_resv;
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_VNI_RESV_VAL;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_STPID) {
		vlan_rule->stpid = ppe_drv_get_tpid_index(r_rule->stpid, "stpid");
		if (vlan_rule->stpid < 0) {
			ppe_vlan_warn("Invalid STPID: 0x%x not found in global TPID list", r_rule->stpid);
			return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_STPID;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_CTPID) {
		vlan_rule->ctpid = ppe_drv_get_tpid_index(r_rule->ctpid, "ctpid");
		if (vlan_rule->ctpid < 0) {
			ppe_vlan_warn("Invalid CTPID: 0x%x not found in global TPID list", r_rule->ctpid);
			return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_CTPID;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_DHCP_TYPE) {
		switch (r_rule->dhcp_type) {
			case PPE_VLAN_DHCP_TYPE_NON_DHCP:
				vlan_rule->dhcp_type = PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP;
				break;
			case PPE_VLAN_DHCP_TYPE_DHCP_V4:
				vlan_rule->dhcp_type = PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4;
				break;
			case PPE_VLAN_DHCP_TYPE_NON_DHCP_V4:
				vlan_rule->dhcp_type = PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4;
				break;
			case PPE_VLAN_DHCP_TYPE_DHCP_V6:
				vlan_rule->dhcp_type = PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6;
				break;
			case PPE_VLAN_DHCP_TYPE_NON_DHCP_V6:
				vlan_rule->dhcp_type = PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6;
				break;
			case PPE_VLAN_DHCP_TYPE_DHCP_V4_V6:
				vlan_rule->dhcp_type = PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4 | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6;
				break;
			case PPE_VLAN_DHCP_TYPE_ALL:
				vlan_rule->dhcp_type = (PPE_DRV_VLAN_DHCP_TYPE_NON_DHCP | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V4 | PPE_DRV_VLAN_DHCP_TYPE_DHCP_V6);
				break;
			default:
				ppe_vlan_warn("Incorrect input for dhcp_type parameter: %d\n",
						r_rule->dhcp_type);
				return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_DHCP_TYPE;
	}

	if (r_rule->rule_flags & PPE_VLAN_RULE_FLAG_MC_TYPE) {
		switch (r_rule->mc_type) {
			case PPE_VLAN_MC_TYPE_NON_MC:
				vlan_rule->mc_type = PPE_DRV_VLAN_MC_TYPE_NON_MC;
				break;
			case PPE_VLAN_MC_TYPE_IP_MC:
				vlan_rule->mc_type = PPE_DRV_VLAN_MC_TYPE_IP_MC;
				break;
			case PPE_VLAN_MC_TYPE_NON_MC_IP_MC:
				vlan_rule->mc_type = PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_IP_MC;
				break;
			case PPE_VLAN_MC_TYPE_NON_IP_MC:
				vlan_rule->mc_type = PPE_DRV_VLAN_MC_TYPE_NON_IP_MC;
				break;
			case PPE_VLAN_MC_TYPE_NON_MC_NON_IP_MC:
				vlan_rule->mc_type = PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC;
				break;
			case PPE_VLAN_MC_TYPE_IP_MC_NON_IP_MC:
				vlan_rule->mc_type = PPE_DRV_VLAN_MC_TYPE_IP_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC;
				break;
			case PPE_VLAN_MC_TYPE_ALL:
				vlan_rule->mc_type = (PPE_DRV_VLAN_MC_TYPE_NON_MC | PPE_DRV_VLAN_MC_TYPE_IP_MC | PPE_DRV_VLAN_MC_TYPE_NON_IP_MC);
				break;
			default:
				ppe_vlan_warn("Incorrect input for mc_type parameter: %d\n",
						r_rule->mc_type);
				return false;
		}
		vlan_rule->flags |= PPE_DRV_VLAN_RULE_FLAG_MC_TYPE;
	}

	return true;
}

/*
 * ppe_vlan_rule_create()
 *	Create VLAN rule in PPE.
 */
ppe_vlan_ret_t ppe_vlan_rule_create(struct ppe_vlan_rule *rule)
{
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;
	struct ppe_vlan *vlan = NULL;
	struct ppe_drv_vlan_ctx *vlan_ctx = NULL;
#ifdef NSS_PPE_VEIP_FEATURE_SUPPORT
	struct ppe_drv_iface *iface = NULL;
#endif
	int16_t pm_hw_id = -1;
	ppe_vlan_ret_t ret;

	if ((rule->rule_f.rule_flags & PPE_VLAN_MANDATORY_RULE_FIELDS) != PPE_VLAN_MANDATORY_RULE_FIELDS) {
		ppe_vlan_warn("Missing mandatory fields.\n");
		ret = PPE_VLAN_RET_CREATE_FAIL_INVALID_CMN;
		rule->ret = ret;
		return ret;
	}

	spin_lock_bh(&vlan_g->lock);
	vlan = (struct ppe_vlan *)ppe_vlan_alloc(sizeof(struct ppe_vlan));
	if (!vlan) {
		ppe_vlan_warn("%p: failed to allocate vlan memory: %p", vlan_g, rule);
		ppe_vlan_stats_inc(&vlan_g->stats.cmn.vlan_create_fail_oom);
		ret = PPE_VLAN_RET_CREATE_FAIL_OOM;
		goto fail;
	}

	if (rule->rule_id >= PPE_VLAN_RULE_ID_MAX) {
		ppe_vlan_warn("%p: Invalid rule ID: %p", vlan_g, rule);
		ppe_vlan_stats_inc(&vlan_g->stats.cmn.vlan_create_fail_invalid_id);
		ret = PPE_VLAN_RET_CREATE_FAIL_INVALID_ID;
		goto fail;
	}

	/*
	 * Decide on flow direction.
	 */
	vlan->info.rule_dir = (rule->rule_dir == PPE_VLAN_RULE_DIR_INGRESS) ? PPE_DRV_RULE_INGRESS : PPE_DRV_RULE_EGRESS;

	/*
	 * Fill VLAN rule info
	 */
	if (!ppe_vlan_rule_fill(vlan, rule)) {
		ppe_vlan_warn("%p: failed to configure VLAN rule", vlan_g);
		ppe_vlan_stats_inc(&vlan_g->stats.cmn.vlan_create_fail_rule_config);
		ret = PPE_VLAN_RET_CREATE_FAIL_RULE_FILL;
		goto fail;
	}

	/*
	 * Now that the rule information is extraced, confirm same rule
	 * doesn't exist already.
	 */
	vlan->rule_id = rule->rule_id;
	if (ppe_vlan_rule_exist(vlan)) {
		ppe_vlan_warn("%p: failed to configure VLAN rule", vlan_g);
		ppe_vlan_stats_inc(&vlan_g->stats.cmn.vlan_create_fail_rule_exist);
		ret = PPE_VLAN_RET_CREATE_FAIL_RULE_FILL;
		goto fail;
	}

	/*
	 * Fill VLAN rule action
	 */
	if (!ppe_vlan_action_fill(vlan, rule)) {
		ppe_vlan_warn("%p: failed to configure VLAN action", vlan_g);
		ppe_vlan_stats_inc(&vlan_g->stats.cmn.vlan_create_fail_action_config);
		ret = PPE_VLAN_RET_CREATE_FAIL_ACTION_FILL;
		goto fail;
	}

	/*
	 * Allocate empty rule context in driver.
	 */
	vlan_ctx = ppe_drv_vlan_alloc(vlan->info.rule_dir);
	if (!vlan_ctx) {
		ppe_vlan_warn("%p: couldn't allocate a free VLAN rule context.", vlan_g);
		ppe_vlan_stats_inc(&vlan_g->stats.cmn.vlan_create_fail_alloc);
		ret = PPE_VLAN_RET_CREATE_FAIL_DRV_ALLOC;
		goto fail;
	}

	if (vlan->info.action_f.counter_mode == PPE_DRV_VLAN_COUNTER_MODE_PONPM) {
		pm_hw_id = ppe_pm_counter_alloc(rule->action_f.counter_id, PPE_PM_RULE_DIR_INGRESS);
		if (pm_hw_id == -1) {
			ppe_vlan_warn("%p: couldn't allocate a PM counter for counter_id: %d\n",
					vlan_g, rule->action_f.counter_id);
			ppe_vlan_stats_inc(&vlan_g->stats.cmn.vlan_create_fail_pm_ctx_alloc);
			ret = PPE_VLAN_RET_COUNTER_CTX_FAIL;
			goto fail;
		}

		vlan->counter_id = rule->action_f.counter_id;
		vlan->counter_valid_flag = A_TRUE;
		vlan->info.action_f.counter_id = pm_hw_id;
	}

	if (ppe_drv_vlan_rule_create(vlan_ctx, &vlan->info) != PPE_DRV_RET_SUCCESS) {
		ppe_vlan_warn("%p: failed to configure VLAN rule", vlan_g);
		ret = PPE_VLAN_RET_CREATE_FAIL_RULE;
		goto fail;
	}

#ifdef NSS_PPE_VEIP_FEATURE_SUPPORT
	/*
	 * Check if HGU rule is valid and create HGU rule if needed
	 */
	iface = ppe_drv_vlan_ctx_iface_get(vlan_ctx);
	if (iface && ppe_drv_veip_is_hgu_rule_valid(iface)) {
		if (ppe_drv_vlan_hgu_rule_create(&vlan->info, vlan_ctx) != PPE_DRV_RET_SUCCESS) {
			ppe_vlan_warn("%p: Failed to add ingress translation rule for hgu case\n", vlan_g);
			ret = PPE_VLAN_RET_CREATE_FAIL_RULE;
			goto fail;
		}
		vlan->hgu_rule_valid = A_TRUE;
	}
#endif

	/*
	 * Add new rule node to list
	 */
	list_add(&vlan->list, &vlan_g->active_rules);

	/*
	 * Store the status in response.
	 */
	rule->ret = PPE_VLAN_RET_SUCCESS;

	/*
	 * Update stats
	 */
	ppe_vlan_stats_inc(&vlan_g->stats.cmn.vlan_create_req);
	ppe_vlan_info("%p: VLAN rule created with rule ID: %d", vlan_g, rule->rule_id);

	/*
	 * Store book keeping info.
	 */
	vlan->ctx = vlan_ctx;
	kref_init(&vlan->ref_cnt);
	memcpy(&vlan->rule, rule, sizeof(struct ppe_vlan_rule));

	spin_unlock_bh(&vlan_g->lock);
	return PPE_VLAN_RET_SUCCESS;

fail:
	if (vlan_ctx) {
		ppe_drv_vlan_destroy(vlan_ctx);
	}

	if (vlan) {
		ppe_vlan_free(vlan);
	}

	rule->ret = ret;
	spin_unlock_bh(&vlan_g->lock);
	return ret;
}
EXPORT_SYMBOL(ppe_vlan_rule_create);

/*
 * ppe_vlan_rule_free()
 *	Free the vlan rule.
 */
static void ppe_vlan_rule_free(struct kref *kref)
{
	struct ppe_vlan *vlan = container_of(kref, struct ppe_vlan, ref_cnt);
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;

	/*
	 * Delete the rule node from active list.
	 */
	list_del(&vlan->list);

#ifdef NSS_PPE_VEIP_FEATURE_SUPPORT
	/*
	 * Check if HGU rule needs to be destroyed first.
	 * Use  vlan->hgu_rule_valid which is set after ppe_drv_vlan_hgu_rule_create
	 * to reliably determine if an HGU ingress rule was programmed for this ctx.
	 */
	if (vlan->ctx && vlan->hgu_rule_valid) {
		if (ppe_drv_vlan_hgu_rule_destroy(vlan->ctx) != PPE_DRV_RET_SUCCESS) {
			ppe_vlan_warn("%p: Failed to delete ingress translation rule for hgu case\n", vlan_g);
		}
	}
#endif
	/*
	 * Destroy the rule in PPE driver.
	 */
	if (vlan->ctx) {
		ppe_drv_vlan_destroy(vlan->ctx);
		vlan->ctx = NULL;
	}

	if (vlan->counter_valid_flag) {
		ppe_pm_counter_deref(vlan->counter_id);
	}

	ppe_vlan_info("%p: VLAN rule freed: %u", vlan, vlan->rule_id);

	/*
	 * free the vlan rule memory.
	 */
	ppe_vlan_free(vlan);

	/*
	 * Update stats
	 */
	ppe_vlan_stats_inc(&vlan_g->stats.cmn.vlan_free_req);
}

/*
 * ppe_vlan_rule_destroy()
 *	Destroy VLAN rule in PPE.
 */
ppe_vlan_ret_t ppe_vlan_rule_destroy(ppe_vlan_rule_id_t id)
{
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;
	struct ppe_vlan *vlan;

	spin_lock_bh(&vlan_g->lock);

	if (id >= PPE_VLAN_RULE_ID_MAX) {
		ppe_vlan_warn("%p: Invalid rule ID: %d", vlan_g, id);
		ppe_vlan_stats_inc(&vlan_g->stats.cmn.vlan_destroy_fail_invalid_id);
		spin_unlock_bh(&vlan_g->lock);
		return PPE_VLAN_RET_DESTROY_FAIL_INVALID_ID;
	}

	/*
	 * Find the matching rule corresponding to the rule ID.
	 */
	vlan = ppe_vlan_rule_find_by_id(id);
	if (!vlan) {
		ppe_vlan_warn("%p: failed to find the rule for ID: %d", vlan_g, id);
		ppe_vlan_stats_inc(&vlan_g->stats.cmn.rule_not_found);
		spin_unlock_bh(&vlan_g->lock);
		return PPE_VLAN_RET_DESTROY_FAIL_INVALID_ID;
	}

	if (kref_put(&vlan->ref_cnt, ppe_vlan_rule_free)) {
		ppe_vlan_trace("%p: reference goes down to 0 for ID: %d\n", vlan_g, id);
	}

	/*
	 * Update stats
	 */
	ppe_vlan_stats_inc(&vlan_g->stats.cmn.vlan_destroy_req);

	spin_unlock_bh(&vlan_g->lock);
	return PPE_VLAN_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_vlan_rule_destroy);

/*
 * ppe_vlan_rule_flush()
 *	flush VLAN rules in PPE.
 */
ppe_vlan_ret_t ppe_vlan_rule_flush(ppe_vlan_flush_type_t flush_type)
{
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;
	struct ppe_vlan *vlan, *tmp;

	spin_lock_bh(&vlan_g->lock);

	if (list_empty(&vlan_g->active_rules)) {
		ppe_vlan_trace("VLAN rule list already empty!\n");
		spin_unlock_bh(&vlan_g->lock);
		return PPE_VLAN_RET_SUCCESS;
	}

	/*
	 * iterating through rule list to delete all rules
	 */
	list_for_each_entry_safe(vlan, tmp, &vlan_g->active_rules, list) {
		switch (flush_type) {
			case PPE_VLAN_FLUSH_TYPE_EXCEPT_FIRST:
				/* NOTE: Rules are inserted using list_add() (head insertion),
				 * so the oldest/first-inserted rule is at the tail (list_last_entry()).
				 */
				if (vlan != list_last_entry(&vlan_g->active_rules, typeof(*vlan), list)) {
					if (kref_put(&vlan->ref_cnt, ppe_vlan_rule_free)) {
						ppe_vlan_trace("%p: reference goes down to 0 for vlan: %p\n",
								vlan_g, vlan);
					}
				}
				break;

			case PPE_VLAN_FLUSH_TYPE_ALL:
				if (kref_put(&vlan->ref_cnt, ppe_vlan_rule_free)) {
					ppe_vlan_trace("%p: reference goes down to 0 for vlan: %p\n",
							vlan_g, vlan);
				}
		}
	}

	/*
	 * Update stats
	 */
	ppe_vlan_stats_inc(&vlan_g->stats.cmn.vlan_flush_req);

	spin_unlock_bh(&vlan_g->lock);
	return PPE_VLAN_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_vlan_rule_flush);

/*
 * ppe_vlan_deinit()
 *	VLAN deinit API
 */
void ppe_vlan_deinit(void)
{
	ppe_vlan_stats_debugfs_exit();
	ppe_vlan_dump_exit();

}
EXPORT_SYMBOL(ppe_vlan_deinit);

/*
 * ppe_vlan_init()
 *	VLAN init API
 */
void ppe_vlan_init(struct dentry *d_rule)
{
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;

	spin_lock_init(&vlan_g->lock);

	/*
	 * Initialize active list
	 */
	INIT_LIST_HEAD(&vlan_g->active_rules);

	/*
	 * Initialization of VLAN dump.
	 */
	ppe_vlan_dump_init(d_rule);

	/*
	 * Create debugfs directory/files.
	 */
	ppe_vlan_stats_debugfs_init(d_rule);
}
EXPORT_SYMBOL(ppe_vlan_init);
