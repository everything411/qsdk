/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @file ppe_cos_map.h
 *	NSS PPE CoS Map definitions.
 */

#ifndef _PPE_COS_MAP_H_
#define _PPE_COS_MAP_H_

/* Action */
#define PPE_COS_MAP_ACTION_SET_DSCP  0x1	/**< Set DSCP */
#define PPE_COS_MAP_ACTION_SET_PCP  0x2	/**< Set PCP */
#define PPE_COS_MAP_ACTION_SET_DEI  0x4	/**< Set DEI */
#define PPE_COS_MAP_ACTION_SET_INT_PRI  0x8	/**< set int_pri */
#define PPE_COS_MAP_ACTION_SET_DP   0x10	/**< Set  drop precedence */

/* Rule type flags */
#define PPE_COS_MAP_RULE_TOS   0x1	/**< TOS based rule */
#define PPE_COS_MAP_RULE_TCI   0x2	/**< TCI based rule */

/* Port group */
#define PPE_COS_MAP_PORT_GROUP_0  0	/**< Port group 0 */
#define PPE_COS_MAP_PORT_GROUP_1  1	/**< Port group 1 */

/*
 * ppe_cos_map_status
 *	ppe rule status
 */
typedef enum ppe_cos_map_ret {
	PPE_COS_MAP_SUCCESS = 0,			/**< Success */
	PPE_COS_MAP_CREATE_RULE_FAIL,		/**< Rule creation fail */
	PPE_COS_MAP_DESTROY_RULE_FAIL,		/**< Destroy rule failure */
	PPE_COS_MAP_FLUSH_RULE_FAIL,			/**< Flush rule failure */
	PPE_COS_MAP_PORT_GROUP_SET_FAIL,		/**< Port group set failure */
	PPE_COS_MAP_FAIL,		/**< CoS map failure. */
} ppe_cos_map_ret_t;

/*
 * ppe_cos_map_action
 *	PPE CoS map action info
 */
struct ppe_cos_map_action {
	uint8_t dscp;		/**< Packet dscp */
	uint8_t pcp;		/**< Packet pcp */
	uint8_t dei;		/**< Packet dei */
	uint8_t pri;		/**< Packet priority */
	uint8_t dp;		/**< Packet drop priority */
	uint32_t flags;
};

/*
 * ppe_cos_map_destroy_info
 *	User CoS map destroy information
 */
struct ppe_cos_map_destroy_info {
	uint16_t rule_id;				/**< Rule id for CoS map */
	enum ppe_cos_map_ret ret;			/**< Return status */
};

/*
 * ppe_cos_map_create_info
 *	User CoS map create information
 */
struct ppe_cos_map_create_info {
	uint16_t rule_id;				/**< Rule id for CoS Map */
	uint16_t group_id;		/**< Group ID */
	uint32_t dscp_val;		/**< DSCP value */
	uint32_t pcp_val;		/**< PCP value */
	uint32_t dei_val;		/**< DEI value */
	uint32_t type_flag;		/**< rule type flag */
	struct ppe_cos_map_action action_info;	/* cos map action info */
	enum ppe_cos_map_ret ret;			/**< Return status */
};

/*
 * ppe_cos_map_port_group_info
 *	User CoS port group information
 */
struct ppe_cos_map_port_group_info {
	char dev[IFNAMSIZ];			/**< Device name */
	uint16_t tci_grp_id;		/**< TCI Group ID */
	uint16_t tos_grp_id;		/**< TOS Group ID */
	enum ppe_cos_map_ret ret;	/**< Return status */
};

/**
 * ppe_cos_map_port_group_set
 *	Create PPE CoS map port group.
 *
 * @datatypes
 * struct ppe_cos_map_port_group_inf
 *
 * @param[in]info  Pointer to user configuration for Port group.
 *
 * @return
 * status of ppe_cos_map_port_group_set
 */
ppe_cos_map_ret_t ppe_cos_map_port_group_set(struct ppe_cos_map_port_group_info *info);

/**
 * ppe_cos_map_destroy
 *	Destroy PPE CoS map rule.
 *
 * @datatypes
 * struct ppe_cos_map_destroy_info
 *
 * @param[in] destroy Pointer to user configuration for CoS map.
 *
 * @return
 * status of ppe_cos_map_destroy
 */
ppe_cos_map_ret_t ppe_cos_map_destroy(struct ppe_cos_map_destroy_info *destroy);

/**
 * ppe_cos_map_create
 *	Create PPE CoS map rule.
 *
 * @datatypes
 * struct ppe_cos_map_create_info
 *
 * @param[in] create  Pointer to user configuration for CoS map.
 *
 * @return
 * status of ppe_cos_map_create
 */
ppe_cos_map_ret_t ppe_cos_map_create(struct ppe_cos_map_create_info *create);

/**
 * ppe_cos_map_flush
 *	Flush PPE CoS map rules.
 *
 * @return
 * Status of rule flush operation.
 */
ppe_cos_map_ret_t ppe_cos_map_rule_flush(void);

#endif /* _PPE_COS_MAP_H_ */
