/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_drv_cos_map.h
 *	PPE CoS map specific definitions.
 */

#ifndef _PPE_DRV_COS_MAP_H_
#define _PPE_DRV_COS_MAP_H_

/**
 * @addtogroup ppe_drv_cos_map_subsystem
 * @{
 */

/* Action */
#define PPE_DRV_COS_MAP_ACTION_SET_DSCP		0x1	/**< Set DSCP */
#define PPE_DRV_COS_MAP_ACTION_SET_PCP		0x2	/**< Set PCP */
#define PPE_DRV_COS_MAP_ACTION_SET_DEI		0x4	/**< Set DEI */
#define PPE_DRV_COS_MAP_ACTION_SET_INT_PRI	0x8	/**< Set int_pri */
#define PPE_DRV_COS_MAP_ACTION_SET_DP		0x10	/**< Set  drop precedence */

/**
 * ppe_drv_cos_map_type
 *	CoS type.
 */
enum ppe_drv_cos_map_type {
	PPE_DRV_COS_MAP_TYPE_TOS,	/**< TOS cos type. */
	PPE_DRV_COS_MAP_TYPE_TCI,	/**< TCI cos type. */
	PPE_DRV_COS_MAP_TYPE_MAX	/**< Maximum cos type. */
};
typedef enum ppe_drv_cos_map_type ppe_drv_cos_map_type_t;

/**
 * ppe_drv_cos_map_port_group
 *	CoS map port group.
 */
enum ppe_drv_cos_map_port_group {
	PPE_DRV_COS_MAP_PORT_GROUP_0,	/**< Port group 0. */
	PPE_DRV_COS_MAP_PORT_GROUP_1,	/**< Port group 1. */
	PPE_DRV_COS_MAP_PORT_GROUP_MAX,	/**< Port group max. */
};
typedef enum ppe_drv_cos_map_port_group ppe_drv_cos_map_port_group_t;

/**
 * ppe_drv_cos_group_cfg
 *	Information for port group.
 */
struct ppe_drv_cos_map_port_group_cfg {
	ppe_drv_cos_map_port_group_t tci_grp_id;	/**< TCI group ID. */
	ppe_drv_cos_map_port_group_t tos_grp_id;	/**< TOS group ID. */
	ppe_drv_cos_map_port_group_t flow_grp_id;	/**< Flow group ID. */
};

/**
 * ppe_drv_cos_map_action
 *	PPE CoS map action
 */
struct ppe_drv_cos_map_cosmap {
	uint8_t dscp;		/**< Packet dscp */
	uint8_t pcp;		/**< Packet pcp */
	uint8_t dei;		/**< Packet dei */
	uint8_t pri;		/**< Packet priority */
	uint8_t dp;		/**< Packet drop priority */
	uint32_t flags;
};

/**
 * ppe_drv_cos_map_res
 *	Information for CoS resources.
 */
struct ppe_drv_cos_map_cfg {
	uint8_t group_id;	/**< cos group ID. */
	ppe_drv_cos_map_type_t type;	/** < cos type. */
	uint8_t val;					/** < cos value. */
	struct ppe_drv_cos_map_cosmap map;	/** < cos cosmap. */
};

/**
 * ppe_drv_cos_map_cosmap_set
 *	Sets internal PCP, internal priority, internal DP,
 * and internal DSCP-based on packet DSCP/PCP.
 *
 * @datatypes
 * ppe_drv_cos_map_cfg
 *
 * @param[in] cfg       Pointer to the configuration.
 *
 * @return
 * Status of the configuration set operation.
 */
ppe_drv_ret_t ppe_drv_cos_map_cosmap_set(struct ppe_drv_cos_map_cfg *cfg);

/**
 * ppe_drv_cos_map_port_group_set
 *	Sets port group ID for PCP, DSCP, and flow-based QOS.
 *
 * @datatypes
 * ppe_drv_cos_map_port_group_cfg
 *
 * @param[in] port_id   Port ID.
 * @param[in] cfg       Pointer to the configuration.
 *
 * @return
 * Status of the configuration set operation.
 */
ppe_drv_ret_t ppe_drv_cos_map_port_group_set(uint32_t port_id, struct ppe_drv_cos_map_port_group_cfg *cfg);

/** @} */ /* end_addtogroup ppe_drv_cos_map_subsystem */

#endif /* _PPE_DRV_COS_MAP_H_ */
