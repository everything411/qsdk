/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _NSS_PPE_DSA_MGR_H_
#define _NSS_PPE_DSA_MGR_H_

#include <net/dsa.h>

#if defined(CONFIG_DYNAMIC_DEBUG)
#define nss_ppe_dsa_mgr_warn(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_ppe_dsa_mgr_info(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_ppe_dsa_mgr_trace(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)

#else /* CONFIG_DYNAMIC_DEBUG */
/*
 * Statically compile messages at different levels
 */
#if (NSS_PPE_DSA_MGR_DEBUG_LEVEL < 2)
#define nss_ppe_dsa_mgr_warn(s, ...)
#else
#define nss_ppe_dsa_mgr_warn(s, ...) \
		pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PPE_DSA_MGR_DEBUG_LEVEL < 3)
#define nss_ppe_dsa_mgr_info(s, ...)
#else
#define nss_ppe_dsa_mgr_info(s, ...) \
		pr_notice("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PPE_DSA_MGR_DEBUG_LEVEL < 4)
#define nss_ppe_dsa_mgr_trace(s, ...)
#else
#define nss_ppe_dsa_mgr_trace(s, ...) \
		pr_info("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif
#endif /* CONFIG_DYNAMIC_DEBUG */

/*
 * Per-device protocol state tracking
 */
struct nss_ppe_dsa_proto_state {
	struct list_head item;			/* List iterator */
	struct net_device *dev;			/* Associated net device */
	enum dsa_tag_protocol proto;		/* Current protocol */
};

/*
 * dsa mgr context
 */
struct nss_ppe_dsa_mgr_context {
	struct list_head list;			/* List of dsa private instance */
	spinlock_t lock;				/* Lock to protect dsa private instance */
	struct list_head proto_list;		/* List of per-device protocol states */
	spinlock_t proto_lock;			/* Lock to protect protocol state list */
};

/*
 * dsa mgr private structure
 */
struct nss_ppe_dsa_pvt {
	struct list_head item;			/* List iterator */

	/*
	 * Fields for Linux information
	 */
	struct net_device *dev;		/* corresponding net-device */
	uint32_t mtu;				/* mtu info */
	uint8_t dev_addr[ETH_ALEN];		/* mac address */

	unsigned int swpt_id;		/* switch port id */

	struct metadata_dst	*dsa_meta; /* metadata associated with switch ports, for dsa driver
									to find out dsa interface without atheros header */

	/*
	 * Fields for PPE information
	 */
	struct ppe_drv_iface *iface;		/* ppe_iface info */

	struct kref ref;			/* Reference count */
};

#endif
