/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_DOT1P_H_
#define _PPE_DRV_DOT1P_H_

#include <linux/if_ether.h>
#include <ppe_drv.h>

/*
 * Rule flags.
 */
#define PPE_DRV_DOT1P_RULE_FLAG_SRC_PORT_TYPE		0x00000001	/**< Rule flag to update source info. */
#define PPE_DRV_DOT1P_RULE_FLAG_SRC_INFO		0x00000002	/**< Rule flag to update source info. */
#define PPE_DRV_DOT1P_RULE_FLAG_DST_PORT_TYPE		0x00000004	/**< Rule flag to update source info. */
#define PPE_DRV_DOT1P_RULE_FLAG_DST_INFO		0x00000008	/**< Rule flag to update destination info. */
#define PPE_DRV_DOT1P_RULE_FLAG_VID			0x00000010	/**< Rule flag to update vid. */
#define PPE_DRV_DOT1P_RULE_FLAG_PCP			0x00000020	/**< Rule flag to update PCP info. */
#define PPE_DRV_DOT1P_RULE_FLAG_DEI			0x00000040	/**< Rule flag to update DEI info. */
#define PPE_DRV_DOT1P_RULE_FLAG_DSCP			0x00000080	/**< Rule flag to update DSCP info. */
#define PPE_DRV_DOT1P_RULE_FLAG_DSCP_MASK		0x00000100	/**< Rule flag to update DSCP mask info. */
#define PPE_DRV_DOT1P_DEFAULT_RULE_FLAG			0x00000200	/**< Rule flag to update default info. */
#define PPE_DRV_DOT1P_RULE_FLAG_GEN_MISS_CMD		0x00000400	/**< Rule flag to update DSCP mask info. */
#define PPE_DRV_DOT1P_POLICER_RULE_FLAG_GEMPORT_ID	0x00000800	/**< Policer Rule flag to update gem port id. */

/*
 * DOT1P action flags.
 */
#define PPE_DRV_DOT1P_ACTION_FLAG_GEM_PORT_ID		0x00000001	/**< Rule action to update gem port id. */
#define PPE_DRV_DOT1P_ACTION_FLAG_PQ			0x00000002	/**< Rule action to update priority queue ID. */
#define PPE_DRV_DOT1P_ACTION_FLAG_SVC_CODE		0x00000004	/**< Rule action to update service code. */
#define PPE_DRV_DOT1P_POLICER_ACTION_FLAG_POLICER_ID	0x00000008	/**< Rule action to update policer id. */
#define PPE_DRV_DOT1P_POLICER_ACTION_FLAG_US_POLICER_EN	0x00000010	/**< Rule action to enable upstream policer id. */
#define PPE_DRV_DOT1P_POLICER_ACTION_FLAG_DS_POLICER_EN	0x00000020	/**< Rule action to enable downstream policer id. */
#define PPE_DRV_DOT1P_ACTION_FLAG_INT_DP		0x00000040	/**< Rule action to update drop precedence. */
#define PPE_DRV_DOT1P_ACTION_FLAG_ENQ_VP		0x00000080	/**< Rule action to update Enqueue vp. */
#define PPE_DRV_DOT1P_ACTION_FLAG_INT_PRI		0x00000100	/**< Rule action to update int pri. */
#define PPE_DRV_DOT1P_ACTION_FLAG_FWD_CMD		0x00000200	/**< Rule action for forward command. */
#define PPE_DRV_DOT1P_ACTION_FLAG_DST_INFO		0x00000400	/**< Rule action for destination info command. */
#define PPE_DRV_DOT1P_ACTION_FLAG_BASE_PQ		0x00000800	/**< Rule action to update base priority queue ID. */

#define PPE_DRV_DOT1P_DEF_VID_VAL			0		/**< Default value of VID. */
#define PPE_DRV_DOT1P_DEF_PCP_VAL			0		/**< Default value of PCP. */
#define PPE_DRV_DOT1P_DEF_DSCP_VAL			63		/**< Default value of DSCP. */

struct ppe_drv_dot1p_ctx;
struct ppe_drv_dot1p_policer_ctx;

/*
 * ppe_drv_dot1p_fwd_cmd
 *	DOT1P action command
 */
typedef enum ppe_drv_dot1p_cmd {
	PPE_DRV_DOT1P_CMD_FWD,		/**< DOT1P forward command - forward. */
	PPE_DRV_DOT1P_CMD_DROP,		/**< DOT1P forward command - drop. */
	PPE_DRV_DOT1P_CMD_COPY,		/**< DOT1P forward command - copy to CPU. */
	PPE_DRV_DOT1P_CMD_REDIR		/**< DOT1P forward command - redirect to CPU. */
} ppe_drv_dot1p_cmd_t;

/*
 * ppe_drv_dot1p_port_type
 * 	DOT1P port type.
 */
enum ppe_drv_dot1p_port_type {
	PPE_DRV_DOT1P_PORT_TYPE_BITMAP = 0,
	PPE_DRV_DOT1P_PORT_TYPE_PORT = 1,
};

/*
 * ppe_drv_dot1p_def_rule_match
 *	Single DOT1P defaul rule info
 */
struct ppe_drv_dot1p_def_rule_match {
	uint16_t vid;				/**< VLAN id. */
	uint8_t pcp;				/**< Priority code point value. */
	uint8_t dei;				/**< Drop eligible indicator. */
	uint8_t dscp;				/**< Differentiated srvices code point. */
	uint8_t dscp_mask;			/**< Differentiated srvices code point mask. */
	uint8_t gen_miss_cmd;			/**< action command for no gemport_gen rule match. */
	uint32_t def_rule_flags;		/**< Default Rule flags. */
};

/*
 * ppe_drv_dot1p_rule_match
 *	Single DOT1P rule info
 */
struct ppe_drv_dot1p_rule_match {
	uint8_t src_port_type;			/**< source port info type. */
	uint16_t src_port;			/**< source port. */
	uint8_t dst_port_type;			/**< destination port info type. */
	uint16_t dst_port;			/**< Destination port. */
	uint16_t vid;				/**< VLAN id. */
	uint8_t pcp;				/**< Priority code point value. */
	uint8_t dei;				/**< Drop eligible indicator. */
	uint8_t dscp;				/**< Differentiated srvices code point. */
	uint16_t rule_flags;			/**< Rule flags. */
};

/*
 * ppe_drv_dot1p_action
 *	DOT1P rule action
 */
struct ppe_drv_dot1p_action {
	uint16_t gem_port;			/**< Changed gem_port. */
	uint8_t pq;				/**< Changed priority queue ID. */
	uint8_t base_pq;			/**< Changed base priority queue ID. */
	uint8_t svc_code;			/**< Changed service code. */
	uint16_t policer_id;			/**< Changed policer id. */
	bool us_policer_en;			/**< Enable upstream policer id. */
	bool ds_policer_en;			/**< Enable downstream policer id. */
	uint8_t int_dp;				/**< Changed drop precedence. */
	uint8_t enq_vp;				/**< Changed enqueue VP. */
	uint8_t int_pri;			/**< Changed int priority. */
	uint8_t dst_port;			/**< Destination port. */
	uint8_t fwd_cmd;			/**< Forward action for matched packets. */
	uint16_t action_flags;			/**< Action control flags. */
};

/*
 * ppe_drv_dot1p_policer_rule_match
 *	Single DOT1P policer rule info
 */
struct ppe_drv_dot1p_policer_rule_match {
	uint16_t gemport_id;			/**< Gemport id. */
	uint8_t rule_flags;			/**< Rule flags. */
};

/*
 * ppe_drv_dot1p_policer_action
 *	DOT1P policer rule action
 */
struct ppe_drv_dot1p_policer_action {
	uint16_t policer_id;			/**< Changed policer id. */
	bool us_policer_en;			/**< Enable upstream policer id. */
	bool ds_policer_en;			/**< Enable downstream policer id. */
	uint16_t action_flags;			/**< Action control flags. */
};

/*
 * ppe_drv_dot1p_rule
 *	DOT1P rule and action information.
 */
struct ppe_drv_dot1p_rule {
	uint16_t rule_id;				/**< DOT1P rule id. */
	struct ppe_drv_dot1p_rule_match rule;		/**< DOT1P rule information. */
	struct ppe_drv_dot1p_action action;		/**< DOT1P action information. */
	struct ppe_drv_dot1p_def_rule_match def_rule;	/**< DOT1P default rule information. */
	uint32_t rule_valid_flags;			/**< rule flags. */
};

/*
 * ppe_drv_dot1p_policer_rule
 *	DOT1P policer rule and action information.
 */
struct ppe_drv_dot1p_policer_rule {
	uint16_t gemport_id;					/**< DOT1P policer gemport id. */
	struct ppe_drv_dot1p_policer_rule_match rule;		/**< DOT1P policer rule information. */
	struct ppe_drv_dot1p_policer_action action;		/**< DOT1P polier action information. */
	uint32_t rule_valid_flags;				/**< Rule flags.*/
};

/*
 * ppe_drv_dot1p_alloc()
 *	Allocation for DOT1P rules.
 *
 *
 * @return
 * pointer to ctx object.
 */
struct ppe_drv_dot1p_ctx *ppe_drv_dot1p_alloc(void);

/*
 * ppe_drv_dot1p_get_hw_index
 *	Get hw index corresponding to the rule.
 *
 * @datatypes
 * uint8_t
 * ppe_drv_dot1p_ctx
 *
 * @param[IN] hw_index		hardware index
 * @param[IN] ctx		pointer to ctx object
 *
 * @return
 * none.
 */
void ppe_drv_dot1p_get_hw_index(struct ppe_drv_dot1p_ctx *ctx, uint8_t *hw_index);

/*
 * ppe_drv_dot1p_policer_get_hw_index
 *	Get hw index corresponding to the rule.
 *
 * @datatypes
 * uint8_t
 * ppe_drv_dot1p_policer_ctx
 *
 * @param[IN] hw_index		hardware index
 * @param[IN] ctx		pointer to ctx object
 *
 * @return
 * none.
 */
void ppe_drv_dot1p_policer_get_hw_index(struct ppe_drv_dot1p_policer_ctx *ctx, uint8_t *hw_index);

/**
 * ppe_drv_dot1p_delete()
 *	Delete DOT1P rule in PPE.
 *
 * @datatypes
 * uint16_t
 * ppe_drv_dot1p_ctx
 *
 * @param[IN] gemport		gemport id.
 * @param[IN] ctx		pointer to ctx object.
 *
 * @return
 * None
 */
void ppe_drv_dot1p_delete(struct ppe_drv_dot1p_ctx *ctx, uint16_t gemport);

/*
 * ppe_drv_dot1p_policer_alloc()
 *	Allocation for DOT1P policer rules.
 *
 *
 * @return
 * ctx.
 */
struct ppe_drv_dot1p_policer_ctx *ppe_drv_dot1p_policer_alloc(void);

/**
 * ppe_drv_dot1p_policer_delete()
 *	Delete DOT1P policer rule in PPE.
 *
 * @datatypes
 * uint16_t
 * ppe_drv_dot1p_policer_ctx
 *
 * @param[IN] ctx		pointer to ctx object.
 * @param[IN] gemport		gemport id.
 *
 * @return
 * None
 */
void ppe_drv_dot1p_policer_delete(struct ppe_drv_dot1p_policer_ctx *ctx, uint16_t gemport);

/**
 * ppe_drv_dot1p_def_rule_configure()
 *
 * @datatypes
 * ppe_drv_dot1p_ctx
 * ppe_drv_dot1p_rule
 *
 * @param[IN] info		Pointer to DOT1P info object.
 *
 * @return
 * Status of configuration operation.
 */
ppe_drv_ret_t ppe_drv_dot1p_def_rule_configure(struct ppe_drv_dot1p_rule *info);

/**
 * ppe_drv_dot1p_rule_configure()
 *
 * @datatypes
 * ppe_drv_dot1p_ctx
 * ppe_drv_dot1p_rule
 *
 * @param[IN] ctx		Pointer to ctx object.
 * @param[IN] info		Pointer to DOT1P info object.
 *
 * @return
 * Status of configuration operation.
 */
ppe_drv_ret_t ppe_drv_dot1p_rule_configure(struct ppe_drv_dot1p_ctx *ctx, struct ppe_drv_dot1p_rule *info);

/**
 * ppe_drv_dot1p_policer_rule_configure()
 *
 * @datatypes
 * ppe_drv_dot1p_policer_ctx
 * ppe_drv_dot1p_policer_rule
 *
 * @param[IN] ctx		Pointer to ctx object.
 * @param[IN] info		Pointer to DOT1P info object.
 *
 * @return
 * Status of configuration operation.
 */
ppe_drv_ret_t ppe_drv_dot1p_policer_rule_configure(struct ppe_drv_dot1p_policer_ctx *ctx, struct ppe_drv_dot1p_policer_rule *info);

/**
 * ppe_drv_dot1p_rule_pause_resume()
 *
 * @datatypes
 * ppe_drv_dot1p_ctx
 * ppe_drv_dot1p_rule
 *
 * @param[IN] ctx		Pointer to ctx object.
 * @param[IN] info		Pointer to DOT1P info object.
 *
 * @return
 * Status of configuration operation.
 */
ppe_drv_ret_t ppe_drv_dot1p_rule_pause_resume(struct ppe_drv_dot1p_ctx *ctx, struct ppe_drv_dot1p_rule *info);

/**
 * ppe_drv_dot1p_def_rule_pause_resume()
 *
 * @datatypes
 * uint8_t
 * ppe_drv_dot1p_rule
 *
 * @param[IN] info		Pointer to DOT1P info object.
 * @param[IN] gen_miss_cmd	gen miss command.
 *
 * @return
 * Status of configuration operation.
 */
ppe_drv_ret_t ppe_drv_dot1p_def_rule_pause_resume(struct ppe_drv_dot1p_rule *info, uint8_t gen_miss_cmd);
#endif
