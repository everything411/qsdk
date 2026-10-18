/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __GRE_MGR_PRIV_H_
#define __GRE_MGR_PRIV_H_

#include <linux/netdevice.h>
#include <nss_client_mgr.h>

/*
 * GRE MGR Backend debug macros
 */
#if (GRE_MGR_DEBUG_LEVEL < 1)
#define gre_mgr_assert(fmt, args...)
#else
#define ppe_gre_mgr_assert(c) if (!(c)) { BUG_ON(!(c)); }
#endif

/*
 * Compile messages for dynamic enable/disable
 */
#if defined(CONFIG_DYNAMIC_DEBUG)
#define gre_mgr_warning(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define gre_mgr_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define gre_mgr_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else /* CONFIG_DYNAMIC_DEBUG */

/*
 * Statically compile messages at different levels
 */
#if (GRE_MGR_DEBUG_LEVEL < 2)
#define gre_mgr_warning(s, ...)
#else
#define gre_mgr_warning(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (GRE_MGR_DEBUG_LEVEL < 3)
#define gre_mgr_info(s, ...)
#else
#define gre_mgr_info(s, ...)   pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (GRE_MGR_DEBUG_LEVEL < 4)
#define gre_mgr_trace(s, ...)
#else
#define gre_mgr_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif /* CONFIG_DYNAMIC_DEBUG */

/*
 * GRE Tunnel type GRETAP/GRETUN
 */
enum gre_mgr_cmn_ctx_type {
	GRE_MGR_TUN_CMN_CTX_TYPE_GRETAP = 1,
	GRE_MGR_TUN_CMN_CTX_TYPE_GRETUN,
	GRE_MGR_TUN_CMN_CTX_TYPE_MAX,
};

/*
 * GRE Client Frontends Method
 */
typedef bool (*gre_mgr_netdev_up_event_t)(struct net_device *netdev, enum gre_mgr_cmn_ctx_type gre_tun_type);
typedef void (*gre_mgr_netdev_down_event_t)(struct net_device *netdev);
typedef bool (*gre_mgr_netdev_register_event_t)(struct net_device *netdev, enum gre_mgr_cmn_ctx_type gre_tun_type);
typedef void (*gre_mgr_netdev_unregister_event_t)(struct net_device *netdev);
typedef bool (*gre_mgr_netdev_changemtu_event_t)(struct net_device *netdev);
typedef bool (*gre_mgr_bridgeleave_event_t)(struct net_device *netdev);
typedef bool (*gre_mgr_bridgejoin_event_t)(struct net_device *netdev);

/*
 * gre_mgr_cmn_ctx
 * 	Global GRE MGR client context
 */
struct gre_mgr_cmn_ctx {
	gre_mgr_netdev_up_event_t netdev_up;
	gre_mgr_netdev_down_event_t netdev_down;
	gre_mgr_netdev_register_event_t netdev_register;
	gre_mgr_netdev_unregister_event_t netdev_unregister;
	gre_mgr_netdev_changemtu_event_t netdev_changemtu;
	gre_mgr_bridgeleave_event_t netdev_bridgeleave;
	gre_mgr_bridgejoin_event_t netdev_bridgejoin;
};

#endif /* __GRE_MGR_PRIV_H_ */
