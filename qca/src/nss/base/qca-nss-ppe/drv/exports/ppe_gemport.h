/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
/**
 * @file ppe_gemport.h
 *	NSS PPE GEM PORT definitions.
 */

#ifndef _PPE_GEMPORT_H_
#define _PPE_GEMPORT_H_

#include <linux/if.h>
#include <linux/if_ether.h>

/**
 * GEM PORT rule ID
 */
typedef int16_t ppe_gem_port_rule_id_t;

#define PPE_GEM_PORT_MAX_ID						128		/**< Maximum value for gemport ID. */

#define PPE_GEM_PORT_RULE_FLAG_GEM_PORT_ID				0x00000001	/**< Rule flag to update gem port id. */

#define PPE_GEM_PORT_ACTION_FLAG_SRC_INFO				0x00000001	/**< Rule action to update src_info. */
#define PPE_GEM_PORT_ACTION_FLAG_INT_PRI				0x00000002	/**< Rule action to update int_pri. */
#define PPE_GEM_PORT_ACTION_FLAG_INT_DP					0x00000004	/**< Rule action to update int_dp. */
#define PPE_GEM_PORT_ACTION_FLAG_DST_PORT_TYPE				0x00000008	/**< Rule action to update dst info. */
#define PPE_GEM_PORT_ACTION_FLAG_DST_INFO				0x00000010	/**< Rule action to update dst info. */
#define PPE_GEM_PORT_ACTION_FLAG_DST_SELECTION				0x00000020	/**< Rule action to update source from where to use dst info. */
#define PPE_GEM_PORT_ACTION_FLAG_POLICER_ID				0x00000040	/**< Rule action to update Policer ID. */

/**
 * ppe_gem_port_ret
 *	GEM PORT return code
 */
typedef enum ppe_gem_port_ret {
	PPE_GEM_PORT_RET_SUCCESS = 0,				/**< Success */
	PPE_GEM_PORT_RET_CREATE_FAIL_OOM,			/**< Rule create failed due to out of memory. */
	PPE_GEM_PORT_RET_CREATE_FAIL_DRV_ALLOC,			/**< Rule create failed due to ppe-drv context alloc failure. */
	PPE_GEM_PORT_RET_CREATE_FAIL_RULE_CONFIG,		/**< Rule create failed due to invalid configuration. */
	PPE_GEM_PORT_RET_CREATE_FAIL_ACTION_CONFIG,		/**< Rule create failed due to invalid configuration. */
	PPE_GEM_PORT_RET_CREATE_FAIL_INVALID_PQ,		/**< Rule create failed due to invalid pq. */
	PPE_GEM_PORT_RET_CREATE_FAIL_INVALID_ID,		/**< Rule create failed due to invalid rule ID. */
	PPE_GEM_PORT_RET_DELETE_FAIL_INVALID_ID,		/**< Rule delete failed due to invalid rule ID. */
	PPE_GEM_PORT_RET_FLUSH_FAIL,				/**< Rule flush failed. */
} ppe_gem_port_ret_t;

/**
 * ppe_gem_port_flush_type
 *	Flush type for Gem port rule
 */
typedef enum ppe_gem_port_flush_type {
	PPE_GEM_PORT_FLUSH_TYPE_USERSPACE = 0,			/**< Flush userspace GEM PORT rules. */
	PPE_GEM_PORT_FLUSH_TYPE_KERNELSPACE,			/**< Flush kernel GEM PORT rules. */
	PPE_GEM_PORT_FLUSH_TYPE_ALL,				/**< Flush all the rules. */
} ppe_gem_port_flush_type_t;

/**
 * ppe_gem_port_dst_info_src
 *	PPE GEM PORT decide
 *	the source of destination info
 */
typedef enum ppe_gem_port_dst_selection {
	PPE_GEM_PORT_DST_FDB_TBL,				/**< Specify to take destination info from fdb table. */
	PPE_GEM_PORT_DST_GEM_PORT_TBL,				/**< Specify to take destination info from gem port table. */
} ppe_gem_port_dst_selection_t;

/*
 * ppe_gemport_port_type
 * 	gemport port type.
 */
typedef enum ppe_gemport_port_type {
	PPE_GEM_PORT_TYPE_BITMAP = 0,				/**< source port info type is a bitmap. */
	PPE_GEM_PORT_TYPE_PORT = 1,				/**< source port info is a port number. */
} ppe_gem_port_type_t;

/**
 * ppe_gem_port_rule_action
 *	GEM PORT rule action
 */
struct ppe_gem_port_rule_action {
	/*
	 * Action values.
	 */
	char src_info[IFNAMSIZ];				/**< source info. */
	uint8_t int_pri;
	ppe_gem_port_type_t dst_port_type;			/**< destination port type as bitmap or value. */
	char dst_info[IFNAMSIZ];				/**< Destination info. */
	uint8_t int_dp;						/**< Changed drop precedence. */
	uint16_t policer_id;					/**< Policer ID. */
	ppe_gem_port_dst_selection_t dst_selection;			/**< source for taking the destination info. */

	/*
	 * Action control flags.
	 */
	uint16_t action_flags;					/**< Action control flags. */
};

/**
 * ppe_gem_port_rule_match
 *	Gem port rule info
 */
struct ppe_gem_port_rule_match {
	uint8_t gem_port_id;					/**< Gem port id value. */
	uint16_t rule_flags;					/**< Rule flags. */
};

/**
 * ppe_gem_port_rule
 *	GEM PORT rule.
 */
struct ppe_gem_port_rule {
	/*
	 * Request
	 */
	uint32_t valid_flags;					/**< Bits indicating valid rule types in the rule. */
	bool userspace_rule;                            	/**< Flag indicating userspace rule */
	struct ppe_gem_port_rule_match rule;			/**< rule object. */
	struct ppe_gem_port_rule_action action;			/**< action object. */

	/*
	 * Response
	 */
	ppe_gem_port_ret_t ret;					/**< return type. */

};

/**
 * ppe_gem_port_rule_delete()
 *	delete GEM PORT rule in PPE.
 *
 * @datatypes
 * uint8_t
 *
 * @param[IN] gem_port_id		GEM PORT id information.
 *
 * @return
 * Status of rule create operation.
 */
ppe_gem_port_ret_t ppe_gem_port_rule_delete(uint8_t gem_porT_id);

/**
 * ppe_gem_port_rule_create()
 *	Create GEM PORT rule in PPE.
 *
 * @datatypes
 * ppe_gem_port_rule
 *
 * @param[IN] rule		GEM PORT rule information.
 *
 * @return
 * Status of rule create operation.
 */
ppe_gem_port_ret_t ppe_gem_port_rule_create(struct ppe_gem_port_rule *rule);

/**
 * ppe_gem_port_rule_flush()
 *	Flush all GEM PORT rules in PPE.
 *
 * @datatypes
 * ppe_gem_port_flush_type_t
 *
 * @return
 * Status of rule flush operation.
 */
ppe_gem_port_ret_t ppe_gem_port_rule_flush(ppe_gem_port_flush_type_t flush_type);

#endif /* _PPE_GEMPORT_H_ */
