/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_port_mgmt.h
 *      NSS PPE VLAN definitions.
 */

#include <linux/if.h>
#include <linux/if_ether.h>

#ifndef _PPE_PORT_MGMT_H_
#define _PPE_PORT_MGMT_H_

#define PPE_PORT_MGMT_OMCI_ID_MAX	0x40
#define PPE_PORT_MGMT_ACT_ARR_SIZE	16
#define PPE_PORT_MGMT_PORT_MAX		256

/**
 * ppe_port_mgmt_fwd_cmd
 *	PPE PORT MGMT forward action
 */
typedef enum ppe_port_mgmt_fwd_cmd {
	PPE_PORT_MGMT_FWD_CMD_FWD,		      /**< Forward command to allow regular forwarding. */
	PPE_PORT_MGMT_FWD_CMD_DROP,		      /**< Forward command to drop matching packets. */
	PPE_PORT_MGMT_FWD_CMD_COPY,		      /**< Forward command to copy the matching packets. */
	PPE_PORT_MGMT_FWD_CMD_REDIR,		      /**< Forward command to redirecting the matching packets. */
} ppe_port_mgmt_fwd_cmd_t;

/*
 * ppe_port_mgmt_ret
 *	Return type
 */
typedef enum ppe_port_mgmt_ret {
	PPE_PORT_MGMT_RET_SUCCESS = 0,				/**< API call successful */
	PPE_PORT_MGMT_RET_FAILURE,				/**< API call failed */
	PPE_PORT_MGMT_ISOL_RET_FAILURE,				/**< Port isolation get/set failed */
} ppe_port_mgmt_ret_t;

/**
 * ppe_port_mgmt_isol
 *	Port isolation configuration
 */
struct ppe_port_mgmt_isol {
	uint32_t port_id;					/**< Port id */
	uint32_t omci_id;					/**< OMCI id */
	char port_name[IFNAMSIZ];				/**< Port Name */
	bool mc_isol_en;					/**< Multicast Isolation Enable */
	bool bc_isol_en;					/**< Broadcast Isolation Enable */
	ppe_port_mgmt_ret_t ret;				/**< PORT_MGMT return type. */
};

/*
 * ppe_port_mac_lrn_limit
 *	Port mac learn limit configuration
 */
struct ppe_port_mac_lrn_limit {
	uint32_t port_id;				/**< Port id */
	char port_name[IFNAMSIZ];			/**< Port Name */
	bool port_learn_limit_en;			/**< Mac learn limit enable */
	uint32_t port_learn_limit;			/**< Mac learn limit */
	bool lrn_exceed_action_en;			/**< Mac learn action valid */
	ppe_port_mgmt_fwd_cmd_t lrn_exceed_action;	/**< Mac learn exceed action command */
	ppe_port_mgmt_ret_t ret;			/**< PORT_MGMT return type. */
};

/*
 * ppe_port_mac_filter
 *      Port mac filter configuration
 */
struct ppe_port_mac_filter {
	uint8_t mac[ETH_ALEN];				/**< MAC addressto filter. */
	bool fid_valid;					/**< Filter ID info valid. */
	char fid_name[IFNAMSIZ];			/**< Filter ID name. */
	uint8_t fid_index;				/**< Filter Index. */
        ppe_port_mgmt_ret_t ret;			/**< PORT_MGMT return type. */
};

/**
 * ppe_port_omci_port
 *	OMCI-managed port configuration for add/del operations.
 */
struct ppe_port_omci_port {
	char port_name[IFNAMSIZ];		/**< Device name (from CLI dev=) */
	ppe_port_mgmt_ret_t ret;		/**< PORT_MGMT return type */
};

/**
 * ppe_port_mgmt_omci_port_add()
 *	Add a port to the OMCI-managed port list.
 *
 * @param[in] omci_port		OMCI port configuration.
 *
 * @return
 * Status of the operation, indicating success or failure.
 */
ppe_port_mgmt_ret_t ppe_port_mgmt_omci_port_add(struct ppe_port_omci_port *omci_port);

/**
 * ppe_port_mgmt_omci_port_del()
 *	Remove a port from the OMCI-managed port list.
 *
 * @param[in] omci_port		OMCI port configuration.
 *
 * @return
 * Status of the operation, indicating success or failure.
 */
ppe_port_mgmt_ret_t ppe_port_mgmt_omci_port_del(struct ppe_port_omci_port *omci_port);

/**
 * ppe_port_mgmt_omci_port_flush()
 *	Remove all ports from the OMCI-managed port list.
 *
 * @return
 * Status of the operation, indicating success or failure.
 */
ppe_port_mgmt_ret_t ppe_port_mgmt_omci_port_flush(void);

/**
 * ppe_port_mgmt_isol_config()
 *	Set port isolation configuration
 *
 * @param[in] isol_info		Port isolation configuration
 *
 * @return
 * Status of the operation, indicating success or failure.
 */
ppe_port_mgmt_ret_t ppe_port_mgmt_isol_config(struct ppe_port_mgmt_isol *isol_info);

/**
 * ppe_port_mgmt_act_ctrl_set()
 *	Set Action Control configuration
 *
 * @param[in] isol_info		Port isolation configuration
 *
 * @return
 * Status of the operation, indicating success or failure.
 */
ppe_port_mgmt_ret_t ppe_port_mgmt_act_ctrl_set(struct ppe_port_mgmt_isol *isol_info);

/**
 * ppe_port_mgmt_default_isol_set()
 *	Set Default isolation configuration
 *
 * @return
 * Status of the operation, indicating success or failure.
 */
ppe_port_mgmt_ret_t ppe_port_mgmt_default_isol_set(void);

/**
 * ppe_drv_port_mgmt_mac_lrn_limit_set()
 *	Set mac learn limit for port
 *
 * @return
 * Status of the operation, indicating success or failure.
 */
ppe_port_mgmt_ret_t ppe_port_mgmt_mac_lrn_limit_set(struct ppe_port_mac_lrn_limit *mac_lrn_limit);

/**
 * ppe_port_mgmt_mac_filter_set()
 *	Block mac address in FDB
 *
 * @return
 * Status of the operation, indicating success or failure.
 */
ppe_port_mgmt_ret_t ppe_port_mgmt_mac_filter_set(struct ppe_port_mac_filter *mac_filter);

/**
 * ppe_port_mgmt_mac_filter_clear()
 *	Allow the blocked mac address in FDB
 *
 * @return
 * Status of the operation, indicating success or failure.
 */
ppe_port_mgmt_ret_t ppe_port_mgmt_mac_filter_clear(struct ppe_port_mac_filter *mac_filter);
#endif // _PPE_PORT_MGMT_H_
