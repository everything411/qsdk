/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_DSCP_H_
#define _PPE_DRV_DSCP_H_

#include <ppe_drv.h>

/*
 * ppe_drv_dscp_pbit_grp_t
 *	DSCP to P bit group ID
 */
typedef enum ppe_drv_dscp_pbit_grp {
	PPE_DRV_DSCP_PBIT_GRP_PCP0,	/**< Group ID PCP0. */
	PPE_DRV_DSCP_PBIT_GRP_PCP1,	/**< Group ID PCP1. */
} ppe_drv_dscp_pbit_grp_t;

/*
 * ppe_drv_dscp_pcp_rule
 *	Fields for DSCP to P bit map table
 */
struct ppe_drv_dscp_pcp_rule {
	ppe_drv_rule_dir_t rule_dir;		/**< Rule direction for which this mapping applies. */
	uint8_t tos_val;			/**< TOS value to be mapped. */
	ppe_drv_dscp_pbit_grp_t group_id;	/**< Group identifier (whether PCP0 or PCP1) selecting the mapping profile. */
	uint8_t pcp_val;			/**< PCP (P-bit) value to program. */
};

/**
 * ppe_drv_dscp_p_tbl_configure
 *	Configure DSCP→PCP (P-bit) mapping in PPE hardware.
 *
 * @param[in] rule   Pointer to the DSCP_Pbit rule structure.
 *
 * @return
 *	PPE_DRV_RET_SUCCESS on success, or one of:
 *	- PPE_DRV_RET_IN_VLAN_DSCP_PBIT_TBL_CONFIG_FAIL
 *	- PPE_DRV_RET_DSCP_PBIT_TBL_CONFIG_FAIL
 *	- PPE_DRV_RET_L2_DSCP_PBIT_TBL_CONFIG_FAIL
 *	on failure depending on which table configuration failed.
 */
ppe_drv_ret_t ppe_drv_dscp_p_tbl_configure(struct ppe_drv_dscp_pcp_rule *rule);

#endif /* _PPE_DRV_DSCP_H_ */
