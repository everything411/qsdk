/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <fal/fal_api.h>
#include <fal/fal_flow.h>
#include <ppe_drv_gemport.h>
#include <fal/fal_pon.h>

/*
 *GEM PORT list ID macros.
 */
#define PPE_DRV_GEM_PORT_LIST_ID_MAX 		128
#define PPE_DRV_GEM_PORT_LIST_ID_USED		1
#define PPE_DRV_GEM_PORT_LIST_ID_FREE		2
#define PPE_DRV_GEM_PORT_LIST_ID_START 		0

/*
 * RULE macros.
 */
#define PPE_DRV_GEM_PORT_RULE_ID 1

/*
 * ppe_drv_gem_port_ctx
 *	GEM port rule context
 */
struct ppe_drv_gem_port_ctx {
	int16_t index;				/* Associated index. */
	bool rule_valid;			/* Rule valid flag to handle failure with partial configuration. */
	bool rule_type_valid;			/* Indicate if rule type is already set. */

	/*
	 * Rule shadow.
	 */
	fal_gemport_map_t fal_rule;		/* FAL rule structure */
};

/*
 * ppe_drv_gem_port_list
 *	GEM PORT list ID structure.
 */
struct ppe_drv_gem_port_list {
	uint8_t list_id_state;			/* state of list_id */
	struct ppe_drv_gem_port_ctx *ctx;	/* context of gemport rule */
};

/*
 * ppe_drv_gem_port
 *	Complete list of rows and actions
 */
struct ppe_drv_gem_port {
	struct ppe_drv_gem_port_list gem_port_rules[PPE_DRV_GEM_PORT_LIST_ID_MAX]; /* list of gemport rules. */
};

/*
 * Internal APIs.
 */
void ppe_drv_gem_port_entries_free(struct ppe_drv_gem_port *gem_port);
struct ppe_drv_gem_port *ppe_drv_gem_port_entries_alloc(void);
