/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <ppe_drv_public.h>
#include <ppe_drv.h>
#include <ppe_drv_dscp.h>
#include "ppe_dscp.h"
#include "ppe_dscp_dump.h"

/*
 * Global DSCP context
 */
struct ppe_dscp_base ppe_dscp_gbl = {0};

/*
 * ppe_dscp_alloc()
 *	Allocate memory for an DSCP rule.
 */
static inline void *ppe_dscp_alloc(size_t size)
{
	return vzalloc(size);
}

/*
 * ppe_dscp_free()
 *	Free DSCP rules.
 */
static inline void ppe_dscp_free(void *rule)
{
	vfree(rule);
}

/*
 * ppe_dscp_db_entry_find()
 *	Find DSCP DB entry by TOS.
 */
static struct ppe_dscp_db_entry *ppe_dscp_db_entry_find(struct ppe_dscp_base *dscp_g, uint8_t tos)
{
	struct ppe_dscp_db_entry *entry;

	list_for_each_entry(entry, &dscp_g->dscp_map, list) {
		if (entry->tos == tos) {
			return entry;
		}
	}

	return NULL;
}

/*
 * ppe_dscp_rule_fill()
 *	Rule info corresponding to an DSCP_P_bit table rule.
 */
static bool ppe_dscp_rule_fill(struct ppe_dscp *dscp, struct ppe_dscp_rule *rule)
{
	struct ppe_drv_dscp_pcp_rule *dscp_rule = &dscp->info;

	if (rule->p_bit_flags & PPE_DSCP_FLAG_DIR) {
		switch (rule->dir) {
			case PPE_DSCP_RULE_UPSTREAM_DIR:
				dscp_rule->rule_dir = PPE_DRV_RULE_INGRESS;
				break;
			case PPE_DSCP_RULE_DOWNSTREAM_DIR:
				dscp_rule->rule_dir = PPE_DRV_RULE_EGRESS;
				break;
			default:
				ppe_dscp_warn("Invalid direction value: %d", rule->dir);
				return false;
		}
	}

	if (rule->p_bit_flags & PPE_DSCP_FLAG_DSCP) {
		if (rule->dscp_val <= PPE_DSCP_MAX_VALUE) {
			dscp_rule->tos_val = rule->dscp_val << PPE_DSCP_SHIFT; /* Shift DSCP, ECN added later */
		} else {
			ppe_dscp_warn("Invalid dscp value: %d. Must be 0-63", rule->dscp_val);
			return false;
		}
	}

	if (rule->p_bit_flags & PPE_DSCP_FLAG_ECN) {
		switch (rule->ecn) {
			case PPE_DSCP_NON_ECN:
				dscp_rule->tos_val |= PPE_DSCP_ECN_NON;
				break;
			case PPE_DSCP_ECT1:
				dscp_rule->tos_val |= PPE_DSCP_ECN_ECT1;
				break;
			case PPE_DSCP_ECT0:
				dscp_rule->tos_val |= PPE_DSCP_ECN_ECT0;
				break;
			case PPE_DSCP_CE:
				dscp_rule->tos_val |= PPE_DSCP_ECN_CE;
				break;
			default:
				ppe_dscp_warn("Invalid ECN value: %d", rule->ecn);
				return false;
		}
	}
	return true;
}

/*
 * ppe_dscp_p_tbl_configure()
 *	Configure DSCP to P bit table in PPE.
 */
ppe_dscp_ret_t ppe_dscp_p_tbl_configure(struct ppe_dscp_rule *rule)
{
	struct ppe_dscp_base *dscp_g = &ppe_dscp_gbl;
	uint8_t ecn_valid = rule->p_bit_flags & PPE_DSCP_FLAG_ECN;
	uint8_t pcp0_valid = rule->p_bit_flags & PPE_DSCP_FLAG_PCP0;
	uint8_t pcp1_valid = rule->p_bit_flags & PPE_DSCP_FLAG_PCP1;
	struct ppe_dscp *dscp = NULL;
	uint8_t ecn_values[] = {0, 1, 2, 3}; /* All possible ECN values */
	struct ppe_dscp_db_entry *entry;
	int i, num_tos;
	uint8_t tos_val;
	ppe_dscp_ret_t ret;

	dscp = (struct ppe_dscp *)ppe_dscp_alloc(sizeof(struct ppe_dscp));
	if (!dscp) {
		ppe_dscp_warn("%p: failed to allocate dscp_rule memory: %p", dscp_g, rule);
		return PPE_DSCP_RET_CONFIG_FAIL_OOM;
	}

	/*
	 * Fill DSCP_P_bit rule info
	 */
	if (!ppe_dscp_rule_fill(dscp, rule)) {
		ppe_dscp_warn("%p: failed to configure DSCP_P_bit rule", dscp_g);
		ret = PPE_DSCP_RET_CONFIG_FAIL_RULE;
		goto cleanup_dscp;
	}

	/*
	 * Validate PCP0 and PCP1 values once before the loop.
	 */
	if (pcp0_valid && (rule->pcp0 > PPE_DCP_PCP_MAX_VALUE)) {
		ppe_dscp_warn("%p: Invalid pcp0 value: %d (must be 0-7)\n", dscp_g, rule->pcp0);
		ret = PPE_DSCP_RET_CONFIG_FAIL_RULE;
		goto cleanup_dscp;
	}

	if (pcp1_valid && (rule->pcp1 > PPE_DCP_PCP_MAX_VALUE)) {
		ppe_dscp_warn("%p: Invalid pcp1 value: %d (must be 0-7)\n", dscp_g, rule->pcp1);
		ret = PPE_DSCP_RET_CONFIG_FAIL_RULE;
		goto cleanup_dscp;
	}

	/*
	 * Determine number of TOS values.
	 *
	 * When a specific ECN value is provided (ecn_valid = true), we generate
	 * the TOS only for that ECN.
	 *
	 * When ECN is not provided, we must configure the same DSCP→P-bit mapping
	 * for all four possible ECN values (NON_ECN, ECT1, ECT0, CE). Therefore,
	 * num_tos is set to 4 to cover all feasible ECN combinations.
	 */
	num_tos = ecn_valid ? 1 : 4;

	spin_lock_bh(&dscp_g->lock);

	/*
	 * Configure the PPE table
	 */
	for (i = 0; i < num_tos; i++) {
		if (!ecn_valid) {
			/*
			 * Mask ECN bits, add new ECN
			 */
			dscp->info.tos_val = (dscp->info.tos_val & PPE_DSCP_MASK) | ecn_values[i];
		}

		tos_val = dscp->info.tos_val;

		/*
		 * Program PCP0 mapping if requested.
		 */
		if (pcp0_valid) {
			dscp->info.pcp_val = rule->pcp0;
			dscp->info.group_id = PPE_DRV_DSCP_PBIT_GRP_PCP0;

			/*
			 * Configure dscp_p_bit table.
			 */
			if (ppe_drv_dscp_p_tbl_configure(&dscp->info) != PPE_DRV_RET_SUCCESS) {
				ppe_dscp_warn("%p: failed to configure table with tos %u, pcp %u\n",
						dscp_g, dscp->info.tos_val, dscp->info.pcp_val);
				ret = PPE_DSCP_RET_CONFIG_FAIL_RULE;
				goto config_fail;
			}

			/*
			 * Update DB
			 */
			entry = ppe_dscp_db_entry_find(dscp_g, tos_val);
			if (!entry) {
				entry = kzalloc(sizeof(struct ppe_dscp_db_entry), GFP_ATOMIC);
				if (entry) {
					entry->tos = tos_val;
					list_add(&entry->list, &dscp_g->dscp_map);
				} else {
					ppe_dscp_warn("%p: Failed to allocate DB entry for tos %d\n", dscp_g, tos_val);
				}
			}

			if (entry) {
				if (rule->dir == PPE_DSCP_RULE_UPSTREAM_DIR) {
					entry->pcp0_upstream = rule->pcp0;
				} else {
					entry->pcp0_downstream = rule->pcp0;
				}
				entry->valid = true;
			}
		}

		/*
		 * Program PCP1 mapping if requested.
		 */
		if (pcp1_valid) {
			dscp->info.pcp_val = rule->pcp1;
			dscp->info.group_id = PPE_DRV_DSCP_PBIT_GRP_PCP1;

			/*
			 * Configure dscp_p_bit table.
			 */
			if (ppe_drv_dscp_p_tbl_configure(&dscp->info) != PPE_DRV_RET_SUCCESS) {
				ppe_dscp_warn("%p: failed to configure table with tos %u, pcp %u\n",
						dscp_g, dscp->info.tos_val, dscp->info.pcp_val);
				ret = PPE_DSCP_RET_CONFIG_FAIL_RULE;
				goto config_fail;
			}

			/*
			 * Update DB
			 */
			entry = ppe_dscp_db_entry_find(dscp_g, tos_val);
			if (!entry) {
				entry = kzalloc(sizeof(struct ppe_dscp_db_entry), GFP_ATOMIC);
				if (entry) {
					entry->tos = tos_val;
					list_add(&entry->list, &dscp_g->dscp_map);
				} else {
					ppe_dscp_warn("%p: Failed to allocate DB entry for tos %d\n", dscp_g, tos_val);
				}
			}

			if (entry) {
				if (rule->dir == PPE_DSCP_RULE_UPSTREAM_DIR) {
					entry->pcp1_upstream = rule->pcp1;
				} else {
					entry->pcp1_downstream = rule->pcp1;
				}
				entry->valid = true;
			}
		}

		ppe_dscp_info("%p: DSCP to P bit rule configured with tos: %d", dscp_g, dscp->info.tos_val);
	}

	/*
	 * Store the status in response.
	 */
	rule->ret = PPE_DSCP_RET_SUCCESS;

	spin_unlock_bh(&dscp_g->lock);
	ppe_dscp_free(dscp);

	return PPE_DSCP_RET_SUCCESS;

config_fail:
	spin_unlock_bh(&dscp_g->lock);

cleanup_dscp:
	if (dscp) {
		ppe_dscp_free(dscp);
	}

	rule->ret = ret;
	return ret;

}
EXPORT_SYMBOL(ppe_dscp_p_tbl_configure);

/*
 * ppe_dscp_deinit()
 *	DSCP deinit API
 */
void ppe_dscp_deinit(void)
{
	struct ppe_dscp_base *dscp_g = &ppe_dscp_gbl;
	struct ppe_dscp_db_entry *entry, *tmp;

	ppe_dscp_dump_deinit();

	spin_lock_bh(&dscp_g->lock);
	list_for_each_entry_safe(entry, tmp, &dscp_g->dscp_map, list) {
		list_del(&entry->list);
		kfree(entry);
	}
	spin_unlock_bh(&dscp_g->lock);

	ppe_dscp_info("%s\n",__FUNCTION__);

}
EXPORT_SYMBOL(ppe_dscp_deinit);

/*
 * ppe_dscp_init()
 *	DSCP init API
 */
void ppe_dscp_init(struct dentry *d_rule)
{
	struct ppe_dscp_base *dscp_g = &ppe_dscp_gbl;

	spin_lock_init(&dscp_g->lock);
	INIT_LIST_HEAD(&dscp_g->dscp_map);

	ppe_dscp_dump_init(d_rule);
}
EXPORT_SYMBOL(ppe_dscp_init);
