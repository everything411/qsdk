/*
 **************************************************************************
 * Copyright (c) 2017-2018, 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

#ifndef _NSS_PPE_VLAN_MGR_PRIV_H_
#define _NSS_PPE_VLAN_MGR_PRIV_H_

#ifdef CONFIG_QCA_MINIDUMP
#include <soc/qcom/ctx-save.h>
#endif

#if (NSS_PPE_VLAN_MGR_DEBUG_LEVEL < 1)
#define nss_ppe_vlan_mgr_assert(fmt, args...)
#else
#define nss_ppe_vlan_mgr_assert(c) BUG_ON(!(c))
#endif /* NSS_PPE_VLAN_MGR_DEBUG_LEVEL */

/*
 * Compile messages for dynamic enable/disable
 */
#if defined(CONFIG_DYNAMIC_DEBUG)
#define nss_ppe_vlan_mgr_warn(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_ppe_vlan_mgr_info(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_ppe_vlan_mgr_trace(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#else /* CONFIG_DYNAMIC_DEBUG */
/*
 * Statically compile messages at different levels
 */
#if (NSS_PPE_VLAN_MGR_DEBUG_LEVEL < 2)
#define nss_ppe_vlan_mgr_warn(s, ...)
#else
#define nss_ppe_vlan_mgr_warn(s, ...) \
		pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PPE_VLAN_MGR_DEBUG_LEVEL < 3)
#define nss_ppe_vlan_mgr_info(s, ...)
#else
#define nss_ppe_vlan_mgr_info(s, ...) \
		pr_notice("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PPE_VLAN_MGR_DEBUG_LEVEL < 4)
#define nss_ppe_vlan_mgr_trace(s, ...)
#else
#define nss_ppe_vlan_mgr_trace(s, ...) \
		pr_info("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif
#endif /* CONFIG_DYNAMIC_DEBUG */

#define NSS_PPE_VLAN_MGR_INVALID_PORT -1
#define NSS_PPE_VLAN_MGR_PORT_MAX (PPE_DRV_PORTS_MAX)
#define NSS_PPE_VLAN_MGR_TYPE_SINGLE 0	/**< Single VLAN tag. */
#define NSS_PPE_VLAN_MGR_TYPE_DOUBLE 1	/**< Double VLAN tag (e.g. eth0.10.20 or lan1.10). */
#define NSS_PPE_VLAN_MGR_TYPE_TRIPLE 2	/**< Double user VLAN over DSA (e.g. lan1.10.20) — 3 tags visible to PPE HW. */
#ifdef NSS_VLAN_DSA_QINQ_SUPPORT
#define NSS_PPE_VLAN_MGR_TAG_CNT(v) \
	((v->parent && v->parent->parent) ? NSS_PPE_VLAN_MGR_TYPE_TRIPLE : \
	 (v->parent)                      ? NSS_PPE_VLAN_MGR_TYPE_DOUBLE  : \
	                                    NSS_PPE_VLAN_MGR_TYPE_SINGLE)
#else
#define NSS_PPE_VLAN_MGR_TAG_CNT(v) ((v->parent) ? NSS_PPE_VLAN_MGR_TYPE_DOUBLE : NSS_PPE_VLAN_MGR_TYPE_SINGLE)
#endif
#define NSS_PPE_VLAN_MGR_PORT_ROLE_CHANGED 1

#define NSS_PPE_VLAN_MGR_WHITESPACE		" \t\v\f\n,"
#define NSS_PPE_VLAN_MGR_VLAN_AS_VP_MAX		16

#ifdef NSS_VLAN_MGR_WLANIF_DST_XLATE_SUPPORT
/*
 * nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx
 *	Destination xlate context.
 *
 * Destination xlate context used to update DEST_INFO
 * in the descriptor with the real WLAN dev.
 */
struct nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx {
	struct list_head list;			/* List head */
	struct ppe_drv_tun_encap *ptec;		/* Tunnel encap entry */
	struct kref ref;			/* Reference count */
	int16_t vp_num;				/* Real WLAN dev VP */
};
#endif

/*
 * vlan client context
 */
struct nss_ppe_vlan_mgr_context {
	int ctpid;				/* Customer TPID */
	int stpid;				/* Service TPID */
	int ctpid_ext;				/* Extra Customer TPID */
	int stpid_ext;				/* Extra Service TPID */
	int16_t port_role[NSS_PPE_VLAN_MGR_PORT_MAX];
						/* Role of physical ports */
	struct list_head list;			/* List of vlan private instance */
	spinlock_t lock;			/* Lock to protect vlan private instance */
	struct ctl_table_header *sys_hdr;	/* "/pro/sys/nss/vlan_client" directory */
	nss_ppe_vlan_mgr_br_vlan_cb_t vlan_over_bridge_cb;	/* CB to update bridge manager */
	nss_ppe_bond_dev_get_id_cb_t bond_id_get_cb;	 /* Callback function to get bond_id */
	bool wlan_dst_xlate_en;			/* Flag to check if WLANIF DST XLATE upport is enabled */
#ifdef NSS_VLAN_MGR_WLANIF_DST_XLATE_SUPPORT
	struct list_head xlate_ctx_list;	/* List of wlanif desintaion xlate context */
#endif
};

/*
 * vlan manager private structure
 */
struct nss_vlan_pvt {
	struct list_head list;			/* list head */
	struct nss_vlan_pvt *parent;		/* parent vlan instance */

	/*
	 * Fields for Linux information
	 */
	int ifindex;				/* netdev ifindex */
	int16_t port[NSS_PPE_VLAN_MGR_PORT_MAX];
						/* real physical port of this vlan */
	int32_t bond_id;			/* bond interface number, if vlan is created over bond */
	uint32_t vid;				/* vid info */
	uint32_t tpid;				/* tpid info */
	uint32_t mtu;				/* mtu info */
	uint8_t dev_addr[ETH_ALEN];		/* mac address */

	/*
	 * Fields for PPE information
	 */
	struct ppe_drv_iface *iface;		/* VLAN ppe_iface info */
	struct ppe_drv_iface *bridge_iface;	/* Bridge ppe_iface info if member of a bridge */
	uint32_t ppe_cvid;			/* ppe_cvid info */
	uint32_t ppe_svid;			/* ppe_svid info */
	struct ppe_drv_vlan_xlate_info xlate_info;
						/* XLATE info */
	struct kref ref;			/* Reference count */
	bool is_vlan_as_vp_iface;		/* is VP created for this VLAN */
#ifdef NSS_VLAN_VEIP_FEATURE_SUPPORT
	bool is_vlan_as_veip_iface;		/* is this VEIP interface */
#endif
	struct net_device *br_net_dev;		/* base dev of bridge VLAN netdev*/
	bool is_vlan_over_bridge;		/* VLAN interface is created over bridge */
#ifdef NSS_VLAN_MGR_WLANIF_DST_XLATE_SUPPORT
	struct nss_ppe_vlan_mgr_wlanif_dst_xlate_ctx *xlate_ctx;
						/* wlanif desintaion xlate context */
#endif
};

/*
 * nss_ppe_vlan_minidump_log()
 *	To log data structures into minidump output
 */
static inline void nss_ppe_vlan_mgr_minidump_log(void *start_addr, uint64_t size, const char *name) {
#ifdef CONFIG_QCA_MINIDUMP
	if (minidump_add_segments((uint64_t)(uintptr_t)(start_addr), size, QCA_WDT_LOG_DUMP_TYPE_MOD, name, MINIDUMP_CRASH_TYPE_NSS, "qca_nss_ppe_vlan") != 0)
		pr_warn("minidump_log failed for structure type %s at address %p\n", name, start_addr);
#endif
}

/*
 * nss_ppe_vlan_minidump_free()
 *	To unregister data structures from minidump tlv
 */
static inline void nss_ppe_vlan_mgr_minidump_free(void *start_addr, const char *name) {
#ifdef CONFIG_QCA_MINIDUMP
	if (minidump_remove_segments((uint64_t)(uintptr_t)(start_addr)) != 0)
		pr_warn("minidump_free failed for structure %s at address %p\n", name, start_addr);
#endif
}

#endif
