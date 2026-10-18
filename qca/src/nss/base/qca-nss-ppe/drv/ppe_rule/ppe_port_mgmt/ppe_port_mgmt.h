/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include <linux/version.h>
#include <ppe_drv_public.h>
#include <ppe_port_mgmt.h>
#include "ppe_port_mgmt_stats.h"
#include "ppe_port_mgmt_gen_dump.h"
/*
 * PPE port_mgmt debug macros
 */
#if (PPE_PORT_MGMT_DEBUG_LEVEL == 3)
#define ppe_port_mgmt_assert(c, s, ...)
#else
#define ppe_port_mgmt_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)

/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define ppe_port_mgmt_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_port_mgmt_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_port_mgmt_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_PORT_MGMT_DEBUG_LEVEL < 2)
#define ppe_port_mgmt_warn(s, ...)
#else
#define ppe_port_mgmt_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_PORT_MGMT_DEBUG_LEVEL < 3)
#define ppe_port_mgmt_info(s, ...)
#else
#define ppe_port_mgmt_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_PORT_MGMT_DEBUG_LEVEL < 4)
#define ppe_port_mgmt_trace(s, ...)
#else
#define ppe_port_mgmt_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/**
 * ppe_port_mgmt_base
 *	Global port‑management context.
 */
struct ppe_port_mgmt_base {
	spinlock_t lock;			 /**< Spinlock for port‑management operations */
	struct list_head active_rules;		 /**< List of active port‑management rules */
	struct ppe_port_mgmt_stats stats;	 /**< OMCI port management statistics */
	struct dentry *dentry;			 /**< Debugfs dentry for stats */

#ifdef NSS_PPE_PORT_ISOL_SUPPORT
	uint64_t port_phy_isol_bitmap[PPE_PORT_MGMT_OMCI_ID_MAX];				/**< PHY‑port isolation bitmap per OMCI ID */
	uint64_t port_vp_isol_bitmap[PPE_PORT_MGMT_OMCI_ID_MAX];				/**< VP‑port isolation bitmap per OMCI ID */
	uint32_t port_phy_isol_action[PPE_PORT_MGMT_OMCI_ID_MAX][PPE_PORT_MGMT_ACT_ARR_SIZE];	/**< PHY‑port isolation action table */
	uint32_t port_vp_isol_action[PPE_PORT_MGMT_OMCI_ID_MAX][PPE_PORT_MGMT_ACT_ARR_SIZE];	/**< VP‑port isolation action table */
	uint8_t port_omci_id_map[PPE_PORT_MGMT_PORT_MAX];					/**< Mapping of port ID to OMCI ID */
#endif
};

/**
 * ppe_port_mgmt_init()
 *	Port management init API
 *
 * @param[in] d_rule	Pointer to the debugfs dentry
 *
 * @return
 * None
 */
void ppe_port_mgmt_init(struct dentry *d_rule);

/**
 * ppe_port_mgmt_deinit()
 *	Port management deinit API
 *
 * @return
 * None
 */
void ppe_port_mgmt_deinit(void);
