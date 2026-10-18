/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_dot1p.h
 *	NSS PPE DOT1P definitions.
 */

#ifndef _PPE_DOT1P_H_
#define _PPE_DOT1P_H_

#include <linux/if.h>
#include <linux/if_ether.h>

/**
 * DOT1P rule ID
 */
typedef int16_t ppe_dot1p_rule_id_t;

#define PPE_DOT1P_MAX_RULE_ID					127	/**< Maximum value for Rule ID. */
#define PPE_DOT1P_MAX_INT_DP_VAL				3	/**< Maximum value for int_dp can be 3. */
#define PPE_DOT1P_MAX_DSCP_VAL					63	/**< Maximum value for DSCP. */
#define PPE_DOT1P_MAX_USER_POLICER_ID				128	/**< Maximum user policer ID. */
#define PPE_DOT1P_MIN_USER_POLICER_ID				0	/**< Minimum value for user policer ID. */
#define PPE_DOT1P_MAX_PQ_VAL					128	/**< Maximum value for PQ. */
#define PPE_DOT1P_MAX_GEMPORT_VAL				128	/**< Maximum value for gemport. */
#define PPE_DOT1P_MAX_DEI_VAL					1	/**< Maximum value for DEI. */
#define PPE_DOT1P_MAX_PCP_VAL					7	/**< Maximum value for PCP. */


/*
 * DOT1P rule flags.
 */
#define PPE_DOT1P_RULE_FLAG_RULE_ID				0x00000001	/**< Rule id for dot1p table. */
#define PPE_DOT1P_RULE_FLAG_SRC_PORT_TYPE			0x00000002	/**< Rule flag to update source info. */
#define PPE_DOT1P_RULE_FLAG_SRC_INFO				0x00000004	/**< Rule flag to update source info. */
#define PPE_DOT1P_RULE_FLAG_DST_PORT_TYPE			0x00000008	/**< Rule flag to update destination info. */
#define PPE_DOT1P_RULE_FLAG_DST_INFO				0x00000010	/**< Rule flag to update destination info. */
#define PPE_DOT1P_RULE_FLAG_VID					0x00000020	/**< Rule flag to update vid. */
#define PPE_DOT1P_RULE_FLAG_PCP					0x00000040	/**< Rule flag to update PCP info. */
#define PPE_DOT1P_RULE_FLAG_DEI					0x00000080	/**< Rule flag to update DEI info. */
#define PPE_DOT1P_RULE_FLAG_DSCP				0x00000100	/**< Rule flag to update DSCP info. */
#define PPE_DOT1P_RULE_FLAG_DSCP_MASK				0x00000200	/**< Rule flag to update DSCP mask info. */
#define PPE_DOT1P_RULE_FLAG_DEFAULT_RULE			0x00000400	/**< Default rule for DOT1P. */
#define PPE_DOT1P_RULE_FLAG_GEN_MISS_CMD			0x00000800	/**< gen miss command. */
#define PPE_DOT1P_POLICER_RULE_FLAG_GEM_PORT_ID			0x00001000	/**< Policer Rule flag to update gemport id. */

/*
 * DOT1P action flags.
 */
#define PPE_DOT1P_ACTION_FLAG_GEM_PORT_ID			0x00000001	/**< Rule action to update gem port id. */
#define PPE_DOT1P_ACTION_FLAG_PQ				0x00000002	/**< Rule action to update priority queue ID. */
#define PPE_DOT1P_ACTION_FLAG_SVC_CODE				0x00000004	/**< Rule action to update service code. */
#define PPE_DOT1P_POLICER_ACTION_FLAG_POLICER_ID		0x00000008	/**< Rule action to update policer id. */
#define PPE_DOT1P_POLICER_ACTION_FLAG_US_POLICER_EN		0x00000010	/**< Rule action to enable upstream policer id. */
#define PPE_DOT1P_POLICER_ACTION_FLAG_DS_POLICER_EN		0x00000020	/**< Rule action to enable downstream policer id. */
#define PPE_DOT1P_ACTION_FLAG_INT_DP				0x00000040	/**< Rule action to update drop precedence. */
#define PPE_DOT1P_ACTION_FLAG_FWD_CMD				0x00000080	/**< Rule action for forward command. */
#define PPE_DOT1P_ACTION_FLAG_DST_INFO				0x00000100	/**< Rule action for destination info command. */
#define PPE_DOT1P_ACTION_FLAG_BASE_PQ				0x00000200	/**< Rule action to update base priority queue ID. */
#define PPE_DOT1P_ACTION_FLAG_ENQ_VP				0x00000400	/**< Rule action to update enqueue vp. */
#define PPE_DOT1P_ACTION_FLAG_INT_PRI				0x00000800	/**< Rule action to update int_pri. */

/**
 * ppe_dot1p_ret
 *	DOT1P return code
 */
typedef enum ppe_dot1p_ret {
	PPE_DOT1P_RET_SUCCESS = 0,				/**< Success */
	PPE_DOT1P_RET_CREATE_FAIL_OOM,				/**< Rule create failed due to out of memory. */
	PPE_DOT1P_RET_CREATE_FAIL_DRV_ALLOC,			/**< Rule create failed due to ppe-drv context alloc failure. */
	PPE_DOT1P_RET_CREATE_FAIL_RULE_CONFIG,			/**< Rule create failed due to invalid configuration. */
	PPE_DOT1P_RET_CREATE_FAIL_ACTION_CONFIG,		/**< Rule create failed due to invalid configuration. */
	PPE_DOT1P_RET_CREATE_FAIL_INVALID_PQ,			/**< Rule create failed due to invalid pq. */
	PPE_DOT1P_RET_CREATE_FAIL_INVALID_ID,			/**< Rule create failed due to invalid rule ID. */
	PPE_DOT1P_RET_DELETE_FAIL_INVALID_ID,			/**< Rule delete failed due to invalid rule ID. */
	PPE_DOT1P_RET_FLUSH_FAIL,				/**< Rule flush failed. */
	PPE_DOT1P_RET_GET_TBL_FAIL,				/**< failed to get the dot1p table. */
	PPE_DOT1P_RET_PAUSE_FAIL_RULE_CONFIG,			/**< Failed to pause rules. */
	PPE_DOT1P_RET_RESUME_FAIL_RULE_CONFIG,			/**< failed to resume rules. */
	PPE_DOT1P_POLICER_RET_CREATE_FAIL_RULE_CONFIG,		/**< Rule create failed due to invalid configuration. */
	PPE_DOT1P_POLICER_RET_CREATE_FAIL_ACTION_CONFIG,	/**< Rule create failed due to invalid configuration. */
	PPE_DOT1P_POLICER_RET_DELETE_FAIL_INVALID_ID,		/**< Rule delete failed due to invalid rule ID. */
	PPE_DOT1P_POLICER_RET_FLUSH_FAIL,			/**< Rule flush failed. */
} ppe_dot1p_ret_t;

/**
 * ppe_dot1p_flush_type
 *	Flush type for DOT1P rule
 */
typedef enum ppe_dot1p_flush_type {
	PPE_DOT1P_FLUSH_TYPE_USERSPACE = 0,			/**< Flush userspace DOT1P rules. */
	PPE_DOT1P_FLUSH_TYPE_KERNELSPACE,			/**< Flush kernel DOT1P rules. */
	PPE_DOT1P_FLUSH_TYPE_ALL_EXCEPT_FIRST,			/**< Flush all the rules except the first one. */
	PPE_DOT1P_FLUSH_TYPE_ALL,				/**< Flush all the rules. */
} ppe_dot1p_flush_type_t;

/**
 * ppe_dot1p_pause_type
 *	Pause type for DOT1P rule
 */
typedef enum ppe_dot1p_pause_type {
	PPE_DOT1P_PAUSE_TYPE_ALL_EXCEPT_GEM0,			/**< Pause all the rules except the gemport 0. */
	PPE_DOT1P_PAUSE_TYPE_ALL,				/**< Pause all the rules. */
} ppe_dot1p_pause_type_t;

/**
 * ppe_dot1p_fwd_cmd
 *	PPE DOT1P forward action
 */
typedef enum ppe_dot1p_cmd {
	PPE_DOT1P_CMD_FWD,					/**< Forward command to allow regular forwarding. */
	PPE_DOT1P_CMD_DROP,					/**< Forward command to drop matching packets. */
	PPE_DOT1P_CMD_COPY,					/**< Forward command to copy the matching packets. */
	PPE_DOT1P_CMD_REDIR,					/**< Forward command to redirecting the matching packets. */
} ppe_dot1p_cmd_t;

/*
 * ppe_dot1p_port_type
 * 	DOT1P port type.
 */
typedef enum ppe_dot1p_port_type {
	PPE_DOT1P_PORT_TYPE_BITMAP = 0,				/**< source port info type is a bitmap. */
	PPE_DOT1P_PORT_TYPE_PORT = 1,				/**< source port info is a port number. */
} ppe_dot1p_port_type_t;


/**
 * ppe_dot1p_rule_state
 *	PPE DOT1P Rule state
 */
typedef enum ppe_dot1p_rule_state {
	PPE_DOT1P_RESUME,					/**< Resumed state of dot1p. */
	PPE_DOT1P_PAUSE,					/**< Paused state of dot1p. */
	PPE_DOT1P_PAUSE_EX_GEM_0,				/**< Paused state of dot1p for gemport 0. */
} ppe_dot1p_rule_state_t;

/**
 * ppe_dot1p_rule_action
 *	DOT1P rule action
 */
struct ppe_dot1p_rule_action {
	/*
	 * Action values.
	 */
	uint16_t gem_port;					/**< Changed gem_port. */
	uint8_t pq;						/**< Changed priority queue ID. */
	uint8_t base_pq;					/**< Changed base priority queue ID. */
	uint8_t svc_code;					/**< Changed service code. */
	uint16_t policer_id;					/**< Changed policer id. */
	bool us_policer_en;					/**< Enable policer id for upstream traffic. */
	bool ds_policer_en;					/**< Enable policer id for dowstream traffic. */
	uint8_t int_dp;						/**< Changed drop precedence. */
	uint8_t enq_vp;						/**< Enqueue VP derived from pq configuration. */
	uint8_t int_pri;					/**< Internal priority derived from pq configuration. */
	char dst_info[IFNAMSIZ];				/**< Destination info. */
	ppe_dot1p_cmd_t fwd_cmd;				/**< Forward action for matched packets. */

	/*
	 * Action control flags.
	 */
	uint16_t action_flags;					/**< Action control flags. */
};

/**
 * ppe_dot1p_rule_match
 *	DOT1P rule info
 */
struct ppe_dot1p_rule_match {
	/*
	 * Rule values.
	 */
	ppe_dot1p_port_type_t src_port_type;	/**< source port type as in bitmap or value. */
	char src_info[IFNAMSIZ];		/**< source info. */
	ppe_dot1p_port_type_t dst_port_type;	/**< destination port type as in bitmap or value. */
	char dst_info[IFNAMSIZ];		/**< Destination info. */
	uint16_t vid;				/**< VLAN id. */
	uint8_t pcp;				/**< Priority code point value. */
	uint8_t dei;				/**< Drop eligible indicator. */
	uint8_t dscp;				/**< Differentiated srvices code point. */

	/*
	 * Rule control flags.
	 */
	uint16_t rule_flags;			/**< Rule flags. */
};

/**
 * ppe_dot1p_rule_def
 *	DOT1P rule info for deafult config
 */
struct ppe_dot1p_def_rule {
	/*
	 *  default rule values.
	 */
	uint16_t vid;				/**< VLAN id. */
	uint8_t pcp;				/**< Priority code point value. */
	uint8_t dei;				/**< Drop eligible indicator. */
	uint8_t dscp;				/**< Differentiated srvices code point. */
	uint8_t dscp_mask;			/**< DSCP mask value. */
	ppe_dot1p_cmd_t gen_miss_cmd;		/**< forward command action for packets which don't match any gemport gen rule. */

	/*
	 * Rule control flags.
	 */
	uint16_t def_rule_flags;		/**< Default Rule flags. */
};

/**
 * ppe_dot1p_rule
 *	DOT1P rule.
 */
struct ppe_dot1p_rule {
	/*
	 * Request
	 */
	ppe_dot1p_rule_id_t rule_id;		/**< Rule ID. */
	uint32_t valid_flags;			/**< Bits indicating valid rule types in the rule. */
	bool userspace_rule;			/**< Flag indicating userspace rule */
	struct ppe_dot1p_rule_match rule;	/**< DOT1P rule object. */
	struct ppe_dot1p_rule_action action;	/**< DOT1P action object. */
	bool except_flag;			/**< Flush flag. */
	bool pause_except_flag;			/**< rule pause flag. */
	struct ppe_dot1p_def_rule def_rule;	/**< Default rule object. */
	uint8_t rule_state;			/**< state of dot1p rule. */

	/*
	 * Response
	 */
	ppe_dot1p_ret_t ret;			/**< DOT1P return type. */

};

/**
 * ppe_dot1p_policer_rule_action
 *	DOT1P policer rule action
 */
struct ppe_dot1p_policer_rule_action {
	/*
	 * Action values.
	 */
	uint16_t policer_id;			/**< Changed policer id. */
	bool us_policer_en;			/**< Enable policer id for upstream traffic. */
	bool ds_policer_en;			/**< Enable policer id for dowstream traffic. */

	/*
	 * Action control flags.
	 */
	uint16_t action_flags;			/**< Action control flags. */
};

/**
 * ppe_dot1p_policer_rule_match
 *	DOT1P policer rule info
 */
struct ppe_dot1p_policer_rule_match {

	uint16_t gemport_id;			/**< gem_port id. */
	uint16_t rule_flags;			/**< Rule flags. */
};

/**
 * ppe_dot1p_policer_rule
 *	DOT1P policer rule.
 */
struct ppe_dot1p_policer_rule {
	/*
	 * Request
	 */
	uint16_t gemport_id;			/**< gem_port id. */
	uint32_t valid_flags;			/**< Bits indicating valid rule types in the rule. */
	struct ppe_dot1p_policer_rule_match rule;	/**< DOT1P rule object. */
	struct ppe_dot1p_policer_rule_action action;	/**< DOT1P action object. */

	/*
	 * Response
	 */
	ppe_dot1p_ret_t ret;			/**< DOT1P return type. */

};

/**
 * ppe_dot1p_rule_delete()
 *	Delete DOT1P rule in PPE.
 *
 * @datatypes
 * ppe_dot1p_rule_id_t
 *
 * @param[IN] rule_id		DOT1P rule id.
 *
 * @return
 * Status of rule delete operation.
 */
ppe_dot1p_ret_t ppe_dot1p_rule_delete(ppe_dot1p_rule_id_t id);

/**
 * ppe_dot1p_rule_create()
 *	Create DOT1P rule in PPE.
 *
 * @datatypes
 * ppe_dot1p_rule
 *
 * @param[IN] rule		DOT1P rule information.
 *
 * @return
 * Status of rule create operation.
 */
ppe_dot1p_ret_t ppe_dot1p_rule_create(struct ppe_dot1p_rule *rule);

/**
 * ppe_dot1p_rule_pause()
 *	Pause DOT1P rule in PPE.
 *
 *
 * @return
 * Status of rule pause operation.
 */
ppe_dot1p_ret_t ppe_dot1p_rule_pause(ppe_dot1p_pause_type_t pause_type);

/**
 * ppe_dot1p_rule_resume()
 *	Resume DOT1P rule in PPE.
 *
 *
 * @return
 * Status of rule resume operation.
 */
ppe_dot1p_ret_t ppe_dot1p_rule_resume(void);

/**
 * ppe_dot1p_rule_get_state()
 *	Get state of DOT1P rule in PPE.
 *
 *
 * @return
 * Status of rule get state operation.
 */
ppe_dot1p_ret_t ppe_dot1p_rule_get_state(uint8_t *get_state);

/**
 * ppe_dot1p_rule_flush()
 *	Flush all DOT1P rules in PPE.
 *
 * @datatypes
 * ppe_dot1p_flush_type_t
 *
 * @return
 * Status of rule flush operation.
 */
ppe_dot1p_ret_t ppe_dot1p_rule_flush(ppe_dot1p_flush_type_t flush_type);

/**
 * ppe_dot1p_policer_rule_create()
 *	Create DOT1P policer rule in PPE.
 *
 * @datatypes
 * ppe_dot1p_policer_rule
 *
 * @param[IN] rule		DOT1P policer rule information.
 *
 * @return
 * Status of rule create operation.
 */
ppe_dot1p_ret_t ppe_dot1p_policer_rule_create(struct ppe_dot1p_policer_rule *rule);

/**
 * ppe_dot1p_policer_rule_delete()
 *	Delete DOT1P policer rule in PPE.
 *
 * @datatypes
 * uint16_t
 *
 * @param[IN] gemport_id		DOT1P gemport id.
 *
 * @return
 * Status of policer rule delete operation.
 */
ppe_dot1p_ret_t ppe_dot1p_policer_rule_delete(uint16_t gemport_id);

/**
 * ppe_dot1p_policer_rule_flush()
 *	Flush all DOT1P policer rules in PPE.
 *
 * @datatypes
 * ppe_dot1p_flush_type_t
 *
 * @return
 * Status of rule flush operation.
 */
ppe_dot1p_ret_t ppe_dot1p_policer_rule_flush(ppe_dot1p_flush_type_t flush_type);

#endif /* _PPE_DOT1P_H_ */
