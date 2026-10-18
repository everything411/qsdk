/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <fal/fal_api.h>
#include <fal/fal_flow.h>
#include <ppe_drv_dot1p.h>
#include <fal/fal_pon.h>

/*
 * DOT1P list ID macros.
 */
#define PPE_DRV_DOT1P_LIST_ID_MAX 		128
#define PPE_DRV_DOT1P_LIST_ID_USED		1
#define PPE_DRV_DOT1P_LIST_ID_FREE		2
#define PPE_DRV_DOT1P_LIST_ID_START 		0

/*
 * RULE macros.
 */
#define PPE_DRV_DOT1P_RULE_ID 1

/*
 * ppe_drv_dot1p_ctx
 *	DOT1P rule context
 */
struct ppe_drv_dot1p_ctx {
	int16_t index;			/* Associated index. */
	bool rule_valid;			/* Rule valid flag to handle failure with partial configuration. */
	bool rule_type_valid;			/* Indicate if rule type is already set. */

	/*
	 * Rule shadow.
	 */
	fal_gemport_gen_t fal_gen_rule;		/* FAL rule structure */
	fal_gemport_cfg_t fal_cfg_rule;		/* FAL rule structure */
	fal_gemport_policer_t fal_policer;	/* FAL policer info structure */
};

/*
 * ppe_drv_dot1p_policer_ctx
 *	DOT1P policer rule context
 */
struct ppe_drv_dot1p_policer_ctx {
	int16_t index;				/* Associated index. */
	bool rule_valid;			/* Rule valid flag to handle failure with partial configuration. */
	bool rule_type_valid;			/* Indicate if rule type is already set. */

	/*
	 * Rule shadow.
	 */
	fal_gemport_policer_t fal_policer;	/* FAL policer info structure */
};

/*
 * ppe_drv_dot1p_list
 *	DOT1P list ID structure.
 */
struct ppe_drv_dot1p_list {
	uint8_t list_id_state;
	struct ppe_drv_dot1p_ctx *ctx;
};

/*
 * ppe_drv_dot1p_policer_list
 *	DOT1P policer list ID structure.
 */
struct ppe_drv_dot1p_policer_list {
	uint8_t list_id_state;
	struct ppe_drv_dot1p_policer_ctx *ctx;
};

/*
 * ppe_drv_dot1p
 *	Complete list of rows and actions
 */
struct ppe_drv_dot1p {
	struct ppe_drv_dot1p_list list_id[PPE_DRV_DOT1P_LIST_ID_MAX];
};

/*
 * ppe_drv_dot1p_policer
 *	Complete list of rows and actions
 */
struct ppe_drv_dot1p_policer {
	struct ppe_drv_dot1p_policer_list list_id[PPE_DRV_DOT1P_LIST_ID_MAX];
};

/*
 * Internal APIs.
 */
void ppe_drv_dot1p_entries_free(struct ppe_drv_dot1p *dot1p);
struct ppe_drv_dot1p *ppe_drv_dot1p_entries_alloc(void);
void ppe_drv_dot1p_policer_entries_free(struct ppe_drv_dot1p_policer *dot1p);
struct ppe_drv_dot1p_policer *ppe_drv_dot1p_policer_entries_alloc(void);

/**
 * ppe_drv_dot1p_configure_default_dot1p_rule
 *	Configures default dot1p rules.
 *
 *
 * @return
 * Success or error code.
 */
ppe_drv_ret_t ppe_drv_dot1p_configure_default_rule(void);
ppe_drv_ret_t ppe_drv_dot1p_reset_default_rule(void);
