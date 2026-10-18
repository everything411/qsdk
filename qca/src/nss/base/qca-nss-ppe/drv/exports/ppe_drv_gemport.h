/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_GEMPORT_H_
#define _PPE_DRV_GEMPORT_H_

#include <linux/if_ether.h>
#include <ppe_drv.h>

/*
 * GEM PORT rule flags.
 */
#define PPE_DRV_GEM_PORT_RULE_FLAG_GEM_PORT_ID					0x00000001	/**< Rule flag to update gem port id. */

/*
 * GEM PORT action flags.
 */
#define PPE_DRV_GEM_PORT_ACTION_FLAG_SRC_INFO					0x00000001	/**< Rule action to update src_info. */
#define PPE_DRV_GEM_PORT_ACTION_FLAG_INT_PRI					0x00000002	/**< Rule action to update int_pri. */
#define PPE_DRV_GEM_PORT_ACTION_FLAG_INT_DP					0x00000004	/**< Rule action to update int_dp. */
#define PPE_DRV_GEM_PORT_ACTION_FLAG_DST_PORT_TYPE				0x00000008	/**< Rule action to update dst info. */
#define PPE_DRV_GEM_PORT_ACTION_FLAG_DST_INFO					0x00000010	/**< Rule action to update dst info. */
#define PPE_DRV_GEM_PORT_ACTION_FLAG_DST_SELECTION				0x00000020	/**< Rule action to update source from where to use dst info. */

struct ppe_drv_gem_port_ctx;							/**<maintain context for gemport rule. */

/**
 * ppe_drv_gem_port_dst_selection
 *	PPE GEM PORT decide
 *	the source of destination info
 */
typedef enum ppe_drv_gem_port_dst_selection {
	PPE_DRV_GEM_PORT_DST_FDB_TBL,			/**< Source of destination info is fdb table. */
	PPE_DRV_GEM_PORT_DST_GEM_PORT_TBL,		/**< Source of destination info is gem port table. */
} ppe_drv_gem_port_dst_selection_t;

/*
 * ppe_drv_gem_port_type
 *     gemport port type.
 */
enum ppe_drv_gem_port_type
{
       PPE_DRV_GEM_PORT_TYPE_BITMAP = 0,		/**< take bitmap as input from user.*/
       PPE_DRV_GEM_PORT_TYPE_PORT = 1,			/**< take specific port as input. */
};

/**
 * ppe_drv_gem_port_rule_action
 *	GEM PORT rule action
 */
struct ppe_drv_gem_port_rule_action {
	/*
	 * Action values.
	 */
	uint16_t src_port;				/**< source port. */
	uint8_t int_pri;				/**< Changed gem_port. */
	uint8_t dst_port_type;				/**< Destination port iis a bitmap or port number.*/
	uint16_t dst_port;				/**< Destination port. */
	uint8_t int_dp;					/**< Changed drop precedence. */
	uint8_t svc_code;				/**< service code */

	uint16_t action_flags;				/**< Action control flags. */
};

/**
 * ppe_drv_gem_port_rule_match
 *	Gem port rule info
 */
struct ppe_drv_gem_port_rule_match {
	uint8_t gem_port_id;				/**< Gem port id value. */
	uint16_t rule_flags;				/**< Rule flags. */
};

/**
 * ppe_drv_gem_port_rule
 *	DRV GEM PORT rule.
 */
struct ppe_drv_gem_port_rule {
	struct ppe_drv_gem_port_rule_match rule;	/**< rule object. */
	struct ppe_drv_gem_port_rule_action action;	/**< action object. */
};

/*
 * ppe_drv_gem_port_alloc()
 *	Allocation for gem port rules.
 *
 *
 * @return
 * ctx.
 */
struct ppe_drv_gem_port_ctx *ppe_drv_gem_port_alloc(void);

/**
 * ppe_drv_gem_port_delete()
 *	Delete Gemport rule in PPE.
 *
 * @datatypes
 * uint16_t
 * ppe_drv_gem_port_ctx
 *
 * @param[IN] ctx		pointer to ctx object.
 * @param[IN] gemport		gemport id.
 *
 * @return
 * void
 */
void ppe_drv_gem_port_delete(struct ppe_drv_gem_port_ctx *ctx, uint16_t gemport);

/**
 * ppe_drv_gem_port_rule_configure()
 *
 * @datatypes
 * ppe_drv_gem_port_ctx
 * ppe_drv_gem_port_rule
 *
 * @param[IN] ctx		Pointer to ctx object.
 * @param[IN] info		Pointer to GEM PORT info object.
 *
 * @return
 * Status of configuration operation.
 */
ppe_drv_ret_t ppe_drv_gem_port_rule_configure(struct ppe_drv_gem_port_ctx *ctx, struct ppe_drv_gem_port_rule *info);

#endif
