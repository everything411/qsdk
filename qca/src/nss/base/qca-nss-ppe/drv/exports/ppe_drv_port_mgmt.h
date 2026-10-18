/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_drv_port_mgmt.h
 *	NSS PPE DRV Port Management definitions.
 */

#include <linux/if_ether.h>
#include <ppe_drv.h>
#include <fal/fal_portvlan.h>
#include <fal/fal_fdb.h>

#ifndef _PPE_DRV_PORT_MGMT_H_
#define _PPE_DRV_PORT_MGMT_H_

/*
 * PORT_MGMT ISOL table rule flags.
 */
#define PPE_DRV_OMCI_INDEX_MAX						255
#define PPE_DRV_PORT_MGMT_ACT_ARR_SIZE					4
#define PPE_DRV_VP_PROFILE_ID_MAX					64
#define PPE_DRV_PORT_MGMT_ISOL_RULE_FLAG_PORT_ID			0x1

/*
 * ppe_drv_port_mgmt_fwd_cmd
 *	port mgmt action command
 */
typedef enum ppe_drv_port_mgmt_fwd_cmd {
	PPE_DRV_PORT_MGMT_FWD_CMD_FWD,		      /**< PORT_MGMT forward command - forward. */
	PPE_DRV_PORT_MGMT_FWD_CMD_DROP,		      /**< PORT_MGMT forward command - drop. */
	PPE_DRV_PORT_MGMT_FWD_CMD_COPY,		      /**< PORT_MGMT forward command - copy to CPU. */
	PPE_DRV_PORT_MGMT_FWD_CMD_REDIR		      /**< PORT_MGMT forward command - redirect to CPU. */
} ppe_drv_port_mgmt_fwd_cmd_t;

/**
 * ppe_drv_vp_profile_id_info
 *	VP (Virtual Port) profile‑ID mapping information.
 */
struct ppe_drv_vp_profile_id_info {
	uint8_t profile_id;	     /**< VP profile ID */
	uint32_t omci_id;	     /**< OMCI entity ID associated with the VP */
	uint32_t port_id;	     /**< Physical port ID mapped to this VP profile */
	bool in_use;		     /**< Indicates whether this profile entry is active */
};

/**
 * ppe_drv_omci_index_info
 *	OMCI index mapping information for port‑management operations.
 */
struct ppe_drv_omci_index_info {
	uint16_t omci_index;	     /**< Internal OMCI index */
	uint32_t omci_id;	     /**< OMCI entity ID */
	uint32_t port_cnt;	     /**< Number of ports associated with this OMCI entity */
	bool in_use;		     /**< Indicates whether this OMCI index entry is active */
};

/*
 * ppe_drv_port_mgmt_ret
 *	Return type for port‑management APIs.
 */
typedef enum ppe_drv_port_mgmt_ret {
	PPE_DRV_PORT_MGMT_RET_SUCCESS = 0,   /**< API call successful */
	PPE_DRV_PORT_MGMT_RET_FAILURE,	     /**< API call failed */
	PPE_DRV_PORT_MGMT_ISOL_RET_FAILURE,  /**< Port isolation get/set failed */
} ppe_drv_port_mgmt_ret_t;

/**
 * ppe_drv_port_mgmt_isol
 *	Port isolation configuration.
 */
struct ppe_drv_port_mgmt_isol {
	uint32_t port_id;		  /**< Port ID */
	uint32_t omci_id;		  /**< OMCI ID */
	bool mc_isol_en;		  /**< Multicast isolation enable */
	bool bc_isol_en;		  /**< Broadcast isolation enable */
	ppe_drv_port_mgmt_ret_t ret;	  /**< Port‑management return type */
};

/**
 * ppe_drv_omci_port_node
 *	Node representing a single OMCI-managed port tracked in ppe_drv_port_mgmt.
 */
struct ppe_drv_omci_port_node {
	struct list_head list;		/**< List linkage */
	char dev_name[IFNAMSIZ];	/**< Device name */
	uint32_t port_id;		/**< Resolved PPE port ID */
	atomic_t refcount;		/**< Reference count; freed by del/flush when it reaches 0 */
};

/*
 * ppe_drv_port_mac_lrn_limit
 *	Port mac learn limit configuration
 */
struct ppe_drv_port_mac_lrn_limit {
	uint32_t port_id;				/**< Port id */
	bool port_learn_limit_en;			/**< Mac learn limit enable */
	uint32_t port_learn_limit;			/**< Mac learn limit */
	bool lrn_exceed_action_en;			/**< Mac learn action valid */
	ppe_drv_port_mgmt_fwd_cmd_t lrn_exceed_action;	/**< Mac learn exceed action command */
};

/*
 * ppe_drv_port_mgmt
 *	Structure for global port‑management context.
 */
struct ppe_drv_port_mgmt {
	spinlock_t lock;							   	/**< Spinlock for port‑management operations */
	struct list_head active_rules;						  	/**< List of active port‑management rules */
	struct list_head omci_port_list;					  	/**< List of OMCI-managed ports */

	uint64_t port_phy_isol_bitmap[PPE_DRV_OMCI_INDEX_MAX];			  	/**< PHY‑port isolation bitmap for each OMCI index */
	uint64_t port_vp_isol_bitmap[PPE_DRV_OMCI_INDEX_MAX];			  	/**< VP‑port isolation bitmap for each OMCI index */

	uint32_t port_phy_isol_action[PPE_DRV_OMCI_INDEX_MAX][PPE_DRV_PORT_MGMT_ACT_ARR_SIZE];
											/**< PHY isolation action table */
	uint32_t port_vp_isol_action[PPE_DRV_OMCI_INDEX_MAX][PPE_DRV_PORT_MGMT_ACT_ARR_SIZE];
											/**< VP isolation action table */
	uint8_t port_omci_index_map[PPE_DRV_PORTS_MAX];				  	/**< Mapping from port ID to OMCI index */
	struct ppe_drv_vp_profile_id_info vp_profile_id_info[PPE_DRV_VP_PROFILE_ID_MAX];
											/**< VP profile‑ID information table */
	struct ppe_drv_omci_index_info omci_index_info[PPE_DRV_OMCI_INDEX_MAX];   	/**< OMCI index mapping information table */
	bool mc_isol_en;							   	/**< Global multicast isolation enable */
	bool bc_isol_en;							   	/**< Global broadcast isolation enable */
	uint8_t us_port_id;							  	/**< Upstream port ID */
	struct ppe_drv_port_mac_lrn_limit mac_lrn_limit_cache[PPE_DRV_PORTS_MAX];
											/**< Cached MAC learn-limit config per port */
	bool mac_lrn_limit_valid[PPE_DRV_PORTS_MAX];
											/**< Whether a learn-limit has been set for this port */
	struct list_head mac_filter_cache;						/**< Cached MAC filter entries (set by mac_filter_set, removed by mac_filter_clear) */
};

/*
 * ppe_drv_port_mac_filter
 *	mac filter or block configuration
 */
struct ppe_drv_port_mac_filter {
	uint8_t mac[ETH_ALEN];				/**< MAC addressto filter. */
	bool fid_valid;					/**< Filter ID info valid. */
	uint8_t fid_index;				/**< Filter Index. */
};

/*
 * ppe_drv_mac_filter_node
 *	Cache node for a MAC filter entry installed by ppe_drv_port_mgmt_mac_filter_set().
 *	Stores only the MAC address and FID needed for the debugfs dump.
 *	Stored in ppe_drv_port_mgmt.mac_filter_cache list.
 */
struct ppe_drv_mac_filter_node {
	struct list_head list;			/**< List linkage */
	uint8_t mac[ETH_ALEN];			/**< Filtered MAC address */
	uint8_t fid_index;			/**< Forwarding/filter domain ID */
};

/**
 * ppe_drv_port_mgmt_act_ctrl_set()
 *	Configures the action control parameters for port‑management‑based
 *	isolation or forwarding behavior.
 *
 * @datatypes
 * ppe_drv_port_isolation_info_t
 *
 * @param[in] isol_info       Pointer to port‑management isolation/action
 *			      control configuration structure.
 *
 * @return
 * ppe_drv_ret_t	      Status of the action control configuration
 *			      (success or failure).
 */
ppe_drv_ret_t ppe_drv_port_mgmt_act_ctrl_set(struct ppe_drv_port_mgmt_isol *isol_info);

/**
 * ppe_drv_port_mgmt_isol_configure()
 *	Applies isolation configuration for a given port or port group.
 *	This function sets up ingress/egress isolation rules based on
 *	the supplied isolation information.
 *
 * @datatypes
 * ppe_drv_port_isolation_info_t
 *
 * @param[in] isol_info       Pointer to isolation configuration parameters
 *			      for port‑management operations.
 *
 * @return
 * ppe_drv_ret_t	      Status of isolation configuration update.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_isol_configure(struct ppe_drv_port_mgmt_isol *isol_info);

/**
 * ppe_drv_port_mgmt_default_isol_set()
 *	Sets the default port isolation behavior across the system.
 *	Ensures that baseline isolation rules are in place before
 *	applying specific port‑level isolation policies.
 *
 * @return
 * ppe_drv_ret_t	      Status of default isolation configuration.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_default_isol_set(void);

/**
 * ppe_drv_port_mgmt_mac_lrn_limit_set()
 *	Sets the MAC learning limit for the specified port.
 *
 * @param[in] mac_lrn_limit    MAC learning limit configuration.
 *
 * @return
 * ppe_drv_ret_t	       Status of MAC limit update.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_mac_lrn_limit_set(struct ppe_drv_port_mac_lrn_limit *mac_lrn_limit);

/**
 * ppe_drv_port_mgmt_mac_filter_set()
 *	Sets the MAC filter configuration for the specified port.
 *
 * @param[in] mac_filter       MAC filter configuration parameters.
 *
 * @return
 * ppe_drv_ret_t	       Status of MAC filter update.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_mac_filter_set(struct ppe_drv_port_mac_filter *mac_filter);

/**
 * ppe_drv_port_mgmt_mac_filter_clear()
 *	Clears the MAC filter configuration for the specified port.
 *
 * @param[in] mac_filter       MAC filter entry to clear.
 *
 * @return
 * ppe_drv_ret_t	       Status of MAC filter clear operation.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_mac_filter_clear(struct ppe_drv_port_mac_filter *mac_filter);

/**
 * ppe_drv_port_mgmt_omci_port_add()
 *	Add a port to the OMCI-managed port list in ppe_drv_port_mgmt.
 *
 * @param[in] dev_name		Device name to add.
 *
 * @return
 * PPE_DRV_RET_SUCCESS or failure code.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_omci_port_add(const char *dev_name);

/**
 * ppe_drv_port_mgmt_omci_port_del()
 *	Remove a port from the OMCI-managed port list.
 *
 * @param[in] dev_name		Device name to remove.
 *
 * @return
 * PPE_DRV_RET_SUCCESS or failure code.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_omci_port_del(const char *dev_name);

/**
 * ppe_drv_port_mgmt_omci_port_flush()
 *	Remove all ports from the OMCI-managed port list.
 *
 * @return
 * PPE_DRV_RET_SUCCESS or failure code.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_omci_port_flush(void);

/**
 * ppe_drv_port_mgmt_omci_port_list_show()
 *	Print the OMCI-managed port list to a seq_file (for debugfs).
 *
 * @param[in] m		seq_file to write output to.
 */
void ppe_drv_port_mgmt_omci_port_list_show(struct seq_file *m);

/**
 * ppe_drv_port_mgmt_isol_dump()
 *	Dump port isolation state grouped by OMCI ID to a seq_file.
 *	For each active OMCI index, prints the omci_id, port_cnt, and
 *	the dev_name + port_id of every port mapped to that OMCI index.
 *
 * @param[in] m		seq_file to write output to.
 */
void ppe_drv_port_mgmt_isol_dump(struct seq_file *m);

/**
 * ppe_drv_port_mgmt_omci_port_find_and_ref()
 *	Find a device in the OMCI-managed port list and increment its refcount.
 *	Caller must call ppe_drv_port_mgmt_omci_port_deref() when done with the node.
 *
 * @param[in] dev_name		Device name to look up.
 *
 * @return
 * Pointer to the node with refcount incremented, or NULL if not found.
 */
struct ppe_drv_omci_port_node *ppe_drv_port_mgmt_omci_port_find_and_ref(const char *dev_name);

/**
 * ppe_drv_port_mgmt_omci_port_deref()
 *	Decrement the refcount of an OMCI port node.
 *	The node is NOT freed by this function; memory is managed by del/flush.
 *
 * @param[in] node		Node previously returned by find_and_ref().
 */
void ppe_drv_port_mgmt_omci_port_deref(struct ppe_drv_omci_port_node *node);

/**
 * ppe_drv_port_mgmt_mac_lrn_limit_dump()
 *	Dump the cached MAC learn-limit configuration for every port that has
 *	had a limit applied, to a seq_file (for debugfs).
 *
 * @param[in] m		seq_file to write output to.
 */
void ppe_drv_port_mgmt_mac_lrn_limit_dump(struct seq_file *m);

/**
 * ppe_drv_port_mgmt_mac_filter_dump()
 *	Dump all static DROP entries currently present in the FDB to a
 *	seq_file (for debugfs).
 *
 * @param[in] m		seq_file to write output to.
 */
void ppe_drv_port_mgmt_mac_filter_dump(struct seq_file *m);

/**
 * ppe_drv_port_mgmt_fid_get()
 *      Retrieves the FID associated with the specified device.
 *
 * @param[in]  dev            Network device for which the FID is requested.
 * @param[out] fid_index      Pointer to store the retrieved FID index.
 *
 * @return
 * ppe_drv_ret_t              Status of the FID retrieval operation.
 */
ppe_drv_ret_t ppe_drv_port_mgmt_fid_get(struct net_device *dev, uint8_t* fid_index);
#endif // _PPE_DRV_PORT_MGMT_H_
