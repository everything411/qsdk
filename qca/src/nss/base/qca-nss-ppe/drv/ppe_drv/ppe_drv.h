/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include <linux/jhash.h>
#include <ppe_drv_public.h>
#include <ppe_drv_tun_cmn_ctx.h>
#include <ppe_drv_tun_public.h>
#include "ppe_drv_acl.h"
#include "ppe_drv_exception.h"
#include "ppe_drv_cc.h"
#include "ppe_drv_flow.h"
#include "ppe_drv_host.h"
#include "ppe_drv_l3_if.h"
#include "ppe_drv_iface.h"
#include <nss_debug.h>
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
#include "ppe_drv_nptv6.h"
#include "ppe_drv_nptv6_hairpin.h"
#endif
#include "ppe_drv_nexthop.h"
#include "ppe_drv_policer.h"
#include "ppe_drv_port.h"
#include "ppe_drv_pppoe.h"
#include "ppe_drv_pub_ip.h"
#include "ppe_drv_sc.h"
#include "ppe_drv_stats.h"
#include "ppe_drv_vsi.h"
#include "ppe_drv_v4.h"
#include "ppe_drv_v6.h"
#include "ppe_drv_flow_dump.h"
#include "ppe_drv_if_map.h"
#ifdef NSS_PPE_DSCP_PBIT_FEATURE_SUPPORT
#include "ppe_drv_dscp.h"
#endif
#include <fal/fal_portvlan.h>
#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
#include <fal/fal_fdb.h>
#include "ppe_drv_pm.h"
#endif
#include "ppe_drv_vlan.h"
#ifdef NSS_PPE_DRV_PORT_MGMT_SUPPORT
#include "ppe_drv_port_mgmt.h"
#endif
#ifdef CONFIG_QCA_MINIDUMP
#include <soc/qcom/ctx-save.h>
#endif
#ifdef NSS_PPE_FEATURE_DOT1P
#include "ppe_drv_dot1p.h"
#endif
#ifdef NSS_PPE_FEATURE_GEMPORT
#include "ppe_drv_gemport.h"
#endif
#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
#include "ppe_drv_veip.h"
#endif
#ifdef NSS_PPE_PON_SUPPORT
#include <fal/fal_port_ctrl.h>
#include <linux/netdevice.h>
#endif

extern uint32_t static_dbg_level;
extern bool flow_deacclr_dis;
extern int mac_lrn_exception_en;
extern uint32_t ppe_drv_jhash_rnd __read_mostly;
extern int wlan_host_assist_qdisc_en;
extern int unknown_dst_rfs_enable;

#ifdef PPE_DRV_DDRQ_ENABLE
extern bool ddrq_lp_sc_set_en;
#endif

/* Ensure module tag and default category are available before debug macros */
#ifndef KMODNAME
#define KMODNAME KBUILD_MODNAME
#endif

#ifndef PPE_DRV_DEFAULT_CAT
#define PPE_DRV_DEFAULT_CAT (NSS_LOG_CAT_GENERIC)
#endif

#ifdef PPE_DRV_ESP_SPI_PASSTH_ENABLE
extern int ppe_drv_ipsec_passth_en;
#endif

/*
 * ppe_drv_static_dbg_level
 *	PPE static debug level
 */
enum ppe_drv_static_dbg_level {
	PPE_DRV_STATIC_DBG_LEVEL_NONE,
	PPE_DRV_STATIC_DBG_LEVEL_WARN,
	PPE_DRV_STATIC_DBG_LEVEL_INFO,
	PPE_DRV_STATIC_DBG_LEVEL_TRACE,
};

/*
 * PPE debug macros
 */
#if (PPE_DRV_DEBUG_LEVEL == 3)
#define ppe_drv_assert(c, s, ...)
#else
#define ppe_drv_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * With dynamic debug, guard evaluation and choose path:
 * - If NSS print is enabled, route to nss_warn/info/trace.
 * - Otherwise, use pr_debug to leverage dynamic debug controls.
 */
#define ppe_drv_warn(s, ...) \
    if (nss_debug_enable_get()) \
        nss_warn(KMODNAME, PPE_DRV_DEFAULT_CAT, "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    else \
        pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define ppe_drv_info(s, ...) \
    if (nss_debug_enable_get()) \
        nss_info(KMODNAME, PPE_DRV_DEFAULT_CAT, "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    else \
        pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define ppe_drv_trace(s, ...) \
    if (nss_debug_enable_get()) \
        nss_trace(KMODNAME, PPE_DRV_DEFAULT_CAT, "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    else \
        pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_DRV_DEBUG_LEVEL < 2)
#define ppe_drv_warn(s, ...)
#else
#define ppe_drv_warn(s, ...) \
	if (static_dbg_level >= PPE_DRV_STATIC_DBG_LEVEL_WARN) \
		pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_DRV_DEBUG_LEVEL < 3)
#define ppe_drv_info(s, ...)
#else
#define ppe_drv_info(s, ...) \
	if (static_dbg_level >= PPE_DRV_STATIC_DBG_LEVEL_INFO) \
		pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_DRV_DEBUG_LEVEL < 4)
#define ppe_drv_trace(s, ...)
#else
#define ppe_drv_trace(s, ...) \
	if (static_dbg_level >= PPE_DRV_STATIC_DBG_LEVEL_TRACE) \
		pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * Default switch ID
 */
#define PPE_DRV_SWITCH_ID		0

/*
 * Profile ID
 */
#define PPE_DRV_COMMON_PROFILE_ID	FAL_QM_PROFILE_COMMON_ID
#define PPE_DRV_PO_PROFILE_ID		FAL_QM_PROFILE_PO_ID
#define PPE_DRV_REDIR_PROFILE_ID	9
#define PPE_DRV_GRO_PROFILE_ID		10


/*
 * MAX queue priority
 */
#define PPE_DRV_MAX_PRIORITY		16

/*
 * Maximum queue priority supported per core
 */
#define PPE_DRV_MAX_PRIORITY_PER_CORE	8

/*
 * PPE Hash seed and mask
 *
 * Note: we don't initialize the seed value with a random value
 * to keep the hash calculation persistent across reboots.
 */
#define PPE_DRV_HASH_SEED_DEFAULT	0xabbcdefa
#define PPE_DRV_HASH_MASK		0xfff
#define PPE_DRV_HASH_MIX_V4_SIP		0x13
#define PPE_DRV_HASH_MIX_V4_DIP		0xb
#define PPE_DRV_HASH_MIX_V4_PROTO	0x13
#define PPE_DRV_HASH_MIX_V4_DPORT	0xb
#define PPE_DRV_HASH_MIX_V4_SPORT	0x13
#define PPE_DRV_HASH_FIN_MASK		0x1f
#define PPE_DRV_HASH_FIN_INNER_OUTER_0		0x205
#define PPE_DRV_HASH_FIN_INNER_OUTER_1		0x264
#define PPE_DRV_HASH_FIN_INNER_OUTER_2		0x227
#define PPE_DRV_HASH_FIN_INNER_OUTER_3		0x245
#define PPE_DRV_HASH_FIN_INNER_OUTER_4		0x201

#define PPE_DRV_HASH_SIPV6_MIX_0		0x13
#define PPE_DRV_HASH_SIPV6_MIX_1		0xb
#define PPE_DRV_HASH_SIPV6_MIX_2		0x13
#define PPE_DRV_HASH_SIPV6_MIX_3		0xb
#define PPE_DRV_HASH_DIPV6_MIX_0		0x13
#define PPE_DRV_HASH_DIPV6_MIX_1		0xb
#define PPE_DRV_HASH_DIPV6_MIX_2		0x13
#define PPE_DRV_HASH_DIPV6_MIX_3		0xb

/*
 * MACID macros
 */
#define PPE_DRV_INVALID_MACID		0

/*
 * DSCP macros
 */
#define PPE_DRV_DSCP_SHIFT 2
#define PPE_DRV_DSCP_MASK 0xFC

/*
 * VLAN macros
 */
#define PPE_DRV_VLAN_NOT_CONFIGURED	0xFFF
#define PPE_DRV_VLAN_ID_MASK		0xFFF
#define PPE_DRV_VLAN_TPID_MASK		0xFFFF0000
#define PPE_DRV_VLAN_TCI_MASK		0xFFFF
#define PPE_DRV_VLAN_PRIORITY_MASK	0xE000
#define PPE_DRV_VLAN_PRIORITY_SHIFT	13

#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
#define PPE_DRV_VLAN_CTPID_IDX		0
#define PPE_DRV_VLAN_STPID_IDX		1
#define PPE_DRV_VLAN_CTPID_EXT_IDX	2
#define PPE_DRV_VLAN_STPID_EXT_IDX	3
#endif

/*
 * SAWF macros
 */
#define PPE_DRV_SAWF_VALID_TAG				0xAA
#define PPE_DRV_SAWF_TAG_SHIFT				24
#define PPE_DRV_SAWF_TAG_GET(x)				(x >> PPE_DRV_SAWF_TAG_SHIFT)

#define PPE_DRV_SAWF_MSDUQ_MASK				0x3F
#define PPE_DRV_SAWF_MSDUQ_GET(x)			(x & PPE_DRV_SAWF_MSDUQ_MASK)

#define PPE_DRV_SAWF_MARK_SHIFT				6
#define PPE_DRV_SAWF_MARK_MASK				0x3FFFF
#define PPE_DRV_SAWF_MARK_GET(x)			((x >> PPE_DRV_SAWF_MARK_SHIFT) & PPE_DRV_TREE_ID_SAWF_MARK_MASK)

/*
 * MLO macros
 */
#define PPE_DRV_MLO_MARK_SHIFT				6
#define PPE_DRV_MLO_MARK_MASK				0x3FFFF
#define PPE_DRV_MLO_MARK_GET(x)				((x >> PPE_DRV_MLO_MARK_SHIFT) & PPE_DRV_TREE_ID_MLO_MARK_MASK)

#define PPE_DRV_MLO_MSDUQ_MASK				0x3F
#define PPE_DRV_MLO_MSDUQ_GET(x)			(x & PPE_DRV_MLO_MSDUQ_MASK)

/*
 * Tree ID macros
 */
#define PPE_DRV_TREE_ID_HOST_QDISC_VALID_MASK		0x00800000
#define PPE_DRV_TREE_ID_HOST_QDISC_VALID_SET(w)		((*w) |= PPE_DRV_TREE_ID_HOST_QDISC_VALID_MASK)
#define PPE_DRV_TREE_ID_TYPE_SHIFT			20
#define PPE_DRV_TREE_ID_TYPE_MASK			0x00700000
#define PPE_DRV_TREE_ID_TYPE_SET(w, x)			((*w) |= (((x) << PPE_DRV_TREE_ID_TYPE_SHIFT) & PPE_DRV_TREE_ID_TYPE_MASK))

/*
 * Setting SAWF mark into Tree ID
 */
#define PPE_DRV_TREE_ID_SAWF_MARK_MASK			0x0003FFFF
#define PPE_DRV_TREE_ID_SAWF_MARK_SET(w, x)		((*w) |= ((x) & PPE_DRV_TREE_ID_SAWF_MARK_MASK))

/*
 * Setting MLO mark into Tree ID
 */
#define PPE_DRV_TREE_ID_MLO_MARK_MASK			0x0003FFFF
#define PPE_DRV_TREE_ID_MLO_MARK_SET(w, x)		((*w) |= ((x) & PPE_DRV_TREE_ID_MLO_MARK_MASK))

/*
 * HW flow stats sync timer frequency in milliseconds
 */
#define PPE_DRV_HW_FLOW_STATS_MS	1000

/*
 * Core to service code mapping
 */
#define PPE_DRV_CORE2SC_NOEDIT(core_id) (PPE_DRV_SC_NOEDIT_REDIR_CORE0 + core_id)
#define PPE_DRV_CORE2SC_EDIT(core_id) (PPE_DRV_SC_EDIT_REDIR_CORE0 + core_id)

/*
 * Default port number return from ssdk is 0xF
 * Which indicates no port is set for mirror analysis
 */
#define PPE_DRV_MIRR_INVAL_PORT 0xF

#if defined(NSS_PPE_IPQ53XX)
#define PPE_DRV_PORT_OFFLOAD_MAX_VAL		0x3
#elif defined(NSS_PPE_IPQ54XX)
#define PPE_DRV_PORT_OFFLOAD_MAX_VAL            0x7
#elif defined(NSS_PPE_IPQ95XX)
#define PPE_DRV_PORT_OFFLOAD_MAX_VAL		0x3f
#else
#define PPE_DRV_PORT_OFFLOAD_MAX_VAL		0xff
#endif

/*
 * L2TP Tunnel default UDP Port
 */
#define PPE_DRV_L2TP_DEFAULT_UDP_PORT	1701

#ifdef NSS_PPE_SIXTH_TUPLE
/*
 * sixth tuple mode: 0/1/2:disable/vsi/vlanid
 */
#define PPE_DRV_SIXTH_TUPLE_MODE_DISABLE		0
#define PPE_DRV_SIXTH_TUPLE_MODE_VSI			1
#define PPE_DRV_SIXTH_TUPLE_MODE_VLAN			2
#endif

/*
 * Maximum number of MLO link IDs
 */
#define PPE_DRV_DS_MLO_LINK_NODE_ID_MAX	3

/*
 * Note: Ports 32,33 could be used for trunk. Hence MAX enqueue vp is 30[From 34 to 63].
 */
#define PPE_DRV_ENQ_VP_QID_NONE			0

/*
 * Default coremask value. This value means that flows created,
 * can be mapped to 3 cores i.e. 0, 1 and 2.
 */
#define PPE_DRV_RFS_COREMASK_DEFAULT		0x7
#define PPE_DRV_GRO_COREMASK_DEFAULT		0x7

#ifdef NSS_PPE_DRV_HW_GRO
#define NSS_PPE_DRV_MAX_GRO_FLOWS 8
#endif

#define PPE_DRV_INVALID_PORT			-1

#define PPE_DRV_DDRQ_MAX_CNT			160
#define PPE_DRV_BITS_IN_WORD			32
#define PPE_DRV_DDRQ_MAX_CNT_WORD		(PPE_DRV_DDRQ_MAX_CNT/PPE_DRV_BITS_IN_WORD)

#define PPE_DRV_CONN_HASH_SHIFT 10
#define PPE_DRV_CONN_HASH_SIZE (1 << PPE_DRV_CONN_HASH_SHIFT)

/*
 * ppe_drv_rfs_interface_type
 *	PPE interfaces for which RFS coremask is stored
 */
typedef enum ppe_drv_rfs_interface_type {
	PPE_DRV_RFS_INTERFACE_TYPE_NONE = 0,		/* Interface type is invalid type. */
	PPE_DRV_RFS_INTERFACE_TYPE_PHYSICAL,		/* Interface type is for physical ports i.e. eth0, eth1, etc. */
	PPE_DRV_RFS_INTERFACE_TYPE_WLAN,		/* Interface type is for wlan devs. */
	PPE_DRV_RFS_INTERFACE_TYPE_TUNNEL,		/* Interface type used for Tunnel Devices. */
	PPE_DRV_RFS_INTERFACE_TYPE_DEFAULT,		/* Used for any generic interface other than the above. */
	PPE_DRV_RFS_INTERFACE_TYPE_UDP_ST,		/* Interface type is for dummy dev created for RFS support at nss_udp_st Rx*/
	PPE_DRV_RFS_INTERFACE_TYPE_MCAST,		/* Interface type is for multicast RFS flows. */
	PPE_DRV_RFS_INTERFACE_TYPE_UNKNOWN_DST,	/* Interface type when dest is not known */
	PPE_DRV_RFS_INTERFACE_TYPE_MAX,			/* Maximun number of interface types. */
} ppe_drv_rfs_interface_t;

#ifdef NSS_PPE_DRV_HW_GRO
/*
 * ppe_drv_gro_info
 *	PPE GRO specific global context in ppe_drv
 */
struct ppe_drv_gro_info {
	uint8_t core2enq_vp[NR_CPUS];					/* Storing the enqueue VP number corresponding to each core. */
	uint8_t coremask;						/* Global access value of the coremasks of different interfaces. */
	uint8_t shadow_coremask;					/* Global access value of the coremasks of different interfaces. */
};

/*
 * ppe_drv_gro_ctx
 *	GRO information
 */
struct ppe_drv_gro_ctx {
	atomic_t num_hw_gro_flows;			/* Number of HW GRO offloaded */
	struct ppe_drv_gro_info gro_info;		/* PPE GRO global context */
};
#endif

/*
 * ppe_drv_rfs_ctx
 *	PPE RFS specific global context in ppe_drv
 */
struct ppe_drv_rfs_ctx {
	bool passive_vp_enable;						/* Enable/Disable Passive VP creation for SFE Flows, can be changed through module params. */
	bool wlan_rfs_enable;						/* Enable/Disable RFS for wlan flows based on this. */
	bool unknown_dst_rfs_enable;				/* Enable/Disable RFS for flows whose destination dev is not a recognized PPE port. */
	uint8_t core2enq_vp[NR_CPUS];					/* Storing the enqueue VP number corresponding to each core. */
	uint8_t coremask[PPE_DRV_RFS_INTERFACE_TYPE_MAX];		/* Global access value of the coremasks of different interfaces. */
	uint8_t shadow_coremask[PPE_DRV_RFS_INTERFACE_TYPE_MAX];	/* Global access value of the coremasks of different interfaces. */
	struct ppe_drv_iface *cpu_iface;				/* Storing iface allocated to CPU over here. */
};

/*
 * ppe_drv_entry_valid
 *	PPE entry validity
 */
enum ppe_drv_entry_valid {
	PPE_DRV_ENTRY_INVALID,	/* Entry invalid. */
	PPE_DRV_ENTRY_VALID,	/* Entry valid. */
};

/*
 * ppe_drv_tun_l2tp
 *	l2tp tunnel specific global data
 */
struct ppe_drv_tun_l2tp {
	uint16_t l2tp_sport;				/* L2TP Source port */
	uint16_t l2tp_dport;				/* L2TP Destination port */
};

/*
 * GRE checksum ACL object.
 */
struct ppe_drv_tun_gre_acl;

/*
 * ppe_drv_tun_gbl
 *	PPE tunnel specific global context in ppe drv
 */
struct ppe_drv_tun_gbl {
	struct ppe_drv_tun_l2tp tun_l2tp;				/* L2TP global object */
	struct ppe_drv_tun_gre_acl *gre;				/* GREtap ACL rules for checksum handling */
};

#if defined(PPE_LOOPBACK_PORT_SUPPORT)
/*
 * ppe_drv_loopback_port_feature_type
 *	loopback port feature type
 */
enum ppe_drv_loopback_port_feature_type  {
	PPE_DRV_LOOPBACK_PORT_FT_TYPE_NONE = 0x00,			/* Loopback port feature is disabled */
	PPE_DRV_LOOPBACK_PORT_FT_TYPE_ACL_PON = 0x01,			/* Loopback port feature is enabled for PON */
#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
	PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_DS = 0x02,		/* Loopback port feature is enabled for PON HGU downstream */
	PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_US = 0x03,		/* Loopback port feature is enabled for PON HGU upstream */
#endif
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	PPE_DRV_LOOPBACK_PORT_FT_TYPE_HAIRPIN_NAT = 0x04,		/* Loopback port feature is enabled for Hairpin NPTV6 */
#endif
};

/*
 * ppe_drv_loopback_port_ctx_dir
 *	Types of loopback port context direction.
 */
enum ppe_drv_loopback_port_ctx_dir {
	PPE_DRV_LOOPBACK_PORT_CTX_FLOW_SC,			/* Context for flow direction first service code */
	PPE_DRV_LOOPBACK_PORT_CTX_FLOW_SC_NEXT,			/* Context for flow direction next service code */
	PPE_DRV_LOOPBACK_PORT_CTX_RETURN_SC,			/* Context for return direction first service code */
	PPE_DRV_LOOPBACK_PORT_CTX_RETURN_SC_NEXT,		/* Context for return direction next service code */
	PPE_DRV_LOOPBACK_PORT_CTX_PON_ACL_SC,			/* Context for pon direction first service code */
	PPE_DRV_LOOPBACK_PORT_CTX_PON_ACL_SC_NEXT,		/* Context for PON direction next service code */
#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
	PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC,		/* Context for PON HGU US direction first service code */
	PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC_NEXT,		/* Context for PON HGU US direction next service code */
	PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC_NEXT_DDRQ,	/* Context for PON HGU US direction DDRQ third pass service code */
	PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_DS_SC,		/* Context for PON HGU DS direction first service code */
	PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_DS_SC_NEXT,		/* Context for PON HGU DS direction next service code */
#endif
	PPE_DRV_LOOPBACK_PORT_CTX_MAX,
};


/* ppe_drv_loopback_ring_info
 *	loopback base structure
 */
struct ppe_drv_loopback_port_info {
	bool enabled;						/* Indicates if loopback port is enabled */
	uint16_t port_id;					/* Loopback port number */
	enum ppe_drv_loopback_port_feature_type ft_type;	/* Feature type */
	uint16_t ucastq_start;					/* Unicast base queue */
	uint16_t ucastq_num;					/* Number of unicast queue */
	uint16_t mcastq_start;					/* multicast base queue */
	uint16_t mcastq_num;					/* Number of multicast queue */
	struct {
		enum ppe_drv_loopback_port_feature_type dir_ft_type;	/* Feature type for loopback port of this direction context */
		ppe_drv_sc_t sc;					/* first service code */
	} ctx[PPE_DRV_LOOPBACK_PORT_CTX_MAX];
};
#endif

#if defined(PPE_LOOPBACK_RING_SUPPORT)
/* ppe_drv_loopback_ring_info
 *	loopback base structure
 */
struct ppe_drv_loopback_ring_info {
	int base_queue;		/* Loopback base queue_id */
	bool enabled;		/* Indicates if loopback ring is enabled */
	uint32_t ft_type;	/* Feature type for loopback ring. only one loopback feature can be enabled at a time */
};
#endif

#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
/*
 * Maximum number of forced ingress→egress pairs in lpbk_buffer_map.
 */
#define PPE_DRV_LPBK_BUFFER_MAP_MAX		64

/*
 * Maximum string length for lpbk_buffer_map sysctl read output (all pairs).
 * Each entry serialises as "IFNAMSIZ IFNAMSIZ\n" = IFNAMSIZ*2+2 bytes.
 */
#define PPE_DRV_LPBK_BUFFER_MAP_STR_LEN		((IFNAMSIZ * 2 + 2) * PPE_DRV_LPBK_BUFFER_MAP_MAX)

/*
 * ppe_drv_lpbk_buffer_map_entry
 * 	ingress→egress pair for DDR buffer speed-mismatch feature.
 *
 */
struct ppe_drv_lpbk_buffer_map_entry {
	struct list_head	list;				/* Linked list node */
	struct net_device	*ingress_dev;			/* Physical ingress net_device (holds refcount) */
	struct net_device	*egress_dev;			/* Physical egress net_device (holds refcount) */
	char			ingress_name[IFNAMSIZ];		/* User configured ingress interface name */
	char			egress_name[IFNAMSIZ];		/* User configured egress interface name */
	uint16_t		ingress_vlan_id[PPE_DRV_MAX_VLAN];	/* Outer [0], inner [1] VLAN IDs */
	uint16_t		egress_vlan_id[PPE_DRV_MAX_VLAN];	/* Outer [0], inner [1] VLAN IDs */
	uint8_t			ingress_vlan_cnt;		/* Number of ingress VLAN levels (0, 1, or 2) */
	uint8_t			egress_vlan_cnt;		/* Number of egress VLAN levels (0, 1, or 2) */
	bool			ingress_pppoe;			/* Ingress interface is PPPoE interface */
	bool			egress_pppoe;			/* Egress interface is PPPoE interface */
	uint16_t		pppoe_session_id;		/* PPPoE session ID; valid when a PPPoE side is true */
	uint8_t			pppoe_server_mac[ETH_ALEN];	/* PPPoE server MAC; valid when a PPPoE side is true */
};

/*
 * ppe_drv_lpbk_map_cmd
 * 	Types of supported commands for loopback buffer map
 */
enum ppe_drv_lpbk_map_cmd {
	PPE_DRV_LPBK_MAP_CMD_RULE_ADD = 0,		/* Command for rule add. */
	PPE_DRV_LPBK_MAP_CMD_FLUSH_PAIR,		/* Command for a rule delete */
	PPE_DRV_LPBK_MAP_CMD_FLUSH_ALL,		/* Command for entire lpbk_buffer_map delete. */
	PPE_DRV_LPBK_MAP_CMD_INVALID
};
#endif

/*
 * ppe_drv
 *	PPE DRV base structure
 */
struct ppe_drv {
	spinlock_t lock;				/* PPE lock */
	spinlock_t stats_lock;				/* PPE statistics lock */
	spinlock_t notifier_lock;			/* PPE notifiers lock */

	uint32_t iface_num;				/* Number of PPE interface */
	uint32_t l3_if_num;				/* Number of entries in PPE L3_IF table */
	uint32_t port_num;				/* Number of entries in PPE Port table */
	uint32_t vsi_num;				/* Number of entries in PPE VSI table */
	uint32_t pub_ip_num;				/* Number of entries in PPE Public IP table */
	uint32_t host_num;				/* Number of entries in PPE Host table */
	uint32_t flow_num;				/* Number of entries in PPE Flow table */
	uint32_t pppoe_session_max;			/* Number of entries in PPE PPPoe Session table */
	uint32_t nexthop_num;				/* Number of entries in PPE Nexthop table */
	uint32_t sc_num;				/* Number of entries in PPE Service Code table */
	uint32_t queue_num;				/* Number of entries in PPE Service Code table */
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	uint32_t prefix_num;				/* Number of entries in PPE prefix table */
	uint32_t iid_num;				/* Number of entries in PPE IID table */
#endif
	/*
	 * Timer
	 */
	unsigned long hw_flow_stats_ticks;		/* Ticks to re-arm the hardware stats timer */
	struct timer_list hw_flow_stats_timer;		/* Timer used to poll for stats from PPE_HW */
	struct ppe_drv_stats stats;			/* PPE statistics */

	/*
	 * Pointer to memory pool for different PPE tables
	 */
	uint8_t core2queue[NR_CPUS];			/* Core to queue mapping */
	uint32_t prof2portmap[PPE_DRV_PORT_SRC_PROFILE_MAX][8];  /* Source profile to port bitmap (256 ports) */
	struct ppe_drv_iface *iface;			/* Memory for PPE interface shadow table */
	struct ppe_drv_flow *flow;			/* Memory for PPE Flow table */
	struct ppe_drv_host *host;			/* Memory for PPE Host table */
	struct ppe_drv_nexthop *nexthop;		/* Memory for PPE nexthop table */
	struct ppe_drv_pub_ip *pub_ip;			/* Memory for PPE Public IP table */
	struct ppe_drv_vsi *vsi;			/* Memory for PPE VSI shadow table */
	struct ppe_drv_port *port;			/* Memory for PPE Port table */
	struct ppe_drv_l3_if *l3_if;			/* Memory for PPE L3_IF shadow table */
	struct ppe_drv_pppoe *pppoe;			/* Memory for PPE PPPoe table */
	struct ppe_drv_queue *queue;			/* Memory for PPE queue table */
	struct ppe_drv_policer_ctx *pol_ctx;		/* Policer global context */
	struct ppe_drv_tun_encap *ptun_ec;	/* PPE EG tunnel/translate control entries */
	struct ppe_drv_tun_decap *ptun_dc;	/* PPE tunnel decap control entries */
	struct ppe_drv_tun_l3_if *ptun_l3_if;	/* PPE tunnel L3 interface info */
	struct ppe_drv_tun_decap *decap_map_entries;	/* PPE EG tunnel/translate control entries */
	struct ppe_drv_tun_encap_xlate_rule *encap_xlate_rules; 	/* PPE EG translate rule entries */
	struct ppe_drv_tun_decap_xlate_rule *decap_xlate_rules; 	/* PPE Tunnel decap xlate rules */
	struct ppe_drv_sc *sc;				/* Memory for PPE Service Code table */
	struct ppe_drv_cc *cc;				/* Memory for PPE CPU Code table */
	struct ppe_drv_acl *acl;			/* Memory for PPE ACL entries */
#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
	struct ppe_drv_pm *pm;				/* Memory for PM counter management */
	struct ppe_drv_pm_gen *pm_gen;			/* Memory for PM counter gen management */
#endif
	struct ppe_drv_vlan_tbl *vlan;			/* Memory for PPE VLAN entries */
#ifdef NSS_PPE_DRV_PORT_MGMT_SUPPORT
	struct ppe_drv_port_mgmt *port_mgmt;		/* Memory for PORT_MGMT counter management */
	struct ppe_drv_port_mgmt_gen *port_mgmt_gen;	/* Memory for PORT_MGMT counter gen management */
#endif
#ifdef NSS_PPE_FEATURE_DOT1P
	struct ppe_drv_dot1p *dot1p;			/* Memory for PPE DOT1P entries */
	struct ppe_drv_dot1p_policer *dot1p_policer;	/* Memory for PPE DOT1P policer entries */
#endif
#ifdef NSS_PPE_FEATURE_GEMPORT
	struct ppe_drv_gem_port *gemport;		/* Memory for PPE GEMPORT entries */
#endif
#ifdef PPE_DRV_NPTV6_HW_SUPPORT
	struct ppe_drv_nptv6_prefix *pfx;		/* Memory for PPE prefix table */
	struct ppe_drv_nptv6_iid *iid;			/* Memory for PPE IID table */
#endif
#ifdef NSS_PPE_DSA_QINQ_SUPPORT
	uint32_t dsa_if_count;                          /* Number of active DSA QinQ interfaces */
#endif
	struct dentry *dentry;				/* Debugfs entry */
	struct dentry *stats_dentry;				/* Debugfs entry */
	struct ctl_table_header *ppe_drv_header;	/* PPE DRV sysctl */
	ppe_drv_v4_sync_callback_t ipv4_stats_sync_cb;		/* Callback to call to sync ipv4 statistics */
	void *ipv4_stats_sync_data;				/* Argument for above callback: ipv4_stats_sync_cb */
	ppe_drv_v6_sync_callback_t ipv6_stats_sync_cb;		/* Callback to call to sync ipv6 statistics */
	void *ipv6_stats_sync_data;				/* Argument for above callback: ipv6_stats_sync_cb */
	ppe_drv_qos_int_pri_callback_t int_pri_get_cb;		/* Callback to call to get INT-PRI value */
	ppe_drv_qos_queue_info_callback_t queue_info_get_cb;		/* Callback to call to get PPE PORT-ID */
	struct list_head nh_active;			/* List of active nexthops */
	struct list_head nh_free;			/* List of free nexthops */
	struct kref ref;				/* Reference count */

	struct net_device *upstream_dev;		/* Upstream port for fontana usecase */

#if defined(PPE_DRV_DDR_BUFFER_SPEED_MISMATCH)
	struct list_head lpbk_buffer_map;		/* List of forced ingress→egress pairs (Mode C, highest priority) */
	uint32_t lpbk_buffer_map_count;			/* Number of entries in lpbk_buffer_map */
	bool ddr_buffer_auto_en;			/* Enable automatic DDR buffering based on phydev speed detection (Mode A) */
#endif

	/*
	 * v4 and v6 connection list
	 */
	struct list_head conn_v4; /* All list head of v4 connection in PPE */
	struct list_head conn_v6; /* All list head of v6 connection in PPE */
	struct hlist_head hash_conn_v4[PPE_DRV_CONN_HASH_SIZE]; /* Hash list head of v4 connection in PPE */
	struct hlist_head hash_conn_v6[PPE_DRV_CONN_HASH_SIZE]; /* Hash list head of v6 connection in PPE */

	struct list_head conn_tun_v4;		/* List of v4 tunnel connection in PPE */
	struct list_head conn_tun_v6;		/* List of v6 tunnel connection in PPE */
	struct ppe_drv_fse_ops *fse_ops;        /* Wi-Fi FSE block operations */
	struct kref fse_ops_ref;		/* FSE Reference count */
	int vxlan_dport;			/* VXLAN destination port */
	int vxlan_gpe_dport;			/* VXLAN-GPE destination port */
	bool fse_enable;			/* FSE enabled */
	bool is_wifi_fse_up;			/* Wi-FI FSE ops registered with PPE */

#if defined(PPE_LOOPBACK_RING_SUPPORT)
	struct ppe_drv_loopback_ring_info loopback_ring_info;	/* Loopback information */
#endif

#if defined(PPE_LOOPBACK_PORT_SUPPORT)
	struct ppe_drv_loopback_port_info loopback_port_info;		/* Loopback port information */
#endif

	/*
	 * Switch TPIDs
	 */
	uint16_t gbl_ctpid;			/**< Switch C-TPID. */
	uint16_t gbl_stpid;			/**< Switch S-TPID. */

#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	uint16_t gbl_ctpid_ext;			/**< Switch C-TPID Extra. */
	uint16_t gbl_stpid_ext;			/**< Switch S-TPID Extra. */
#endif

	bool toggled_v4;			/* Toggled bit for v4 sync during a particular iteration */
	bool toggled_v6;			/* Toggled bit for v6 sync during a particular iteration */
	bool tun_toggled_v4;		        /* Tunnel specific Toggled bit for v4 sync during a particular iteration*/
	bool tun_toggled_v6;		        /* Tunnel specific Toggled bit for v6 sync during a particular iteration*/
	struct list_head notifier_list_head;	/* List of event notifier operations in PPE */
	struct ppe_drv_tun_prgm_prsr *pgm;	/* Program Parser entries list */
	struct ppe_drv_tun_udf *pgm_udf;	/* Program Parser udf entries list */
	struct ppe_drv_tun_encap_hdr_ctrl *ecap_hdr_ctrl;	/* header control protomap data */
	bool disable_port_mtu_check;			/* Flag to disable MTU check for all the ports */
	bool eth2eth_offload_if_bitmap;		/* Flag to enable if bitmap check for eth to eth flows */
	struct ppe_drv_tun_gbl tun_gbl;		/* ppe tunnel global context */
	struct ppe_drv_rfs_ctx rfs;		/* PPE RFS global context */
#ifdef NSS_PPE_TUNNEL_TPR_ENABLE
	struct ppe_drv_tun_tpr *tun_tpr;	/* Tunnel TPR entries list */
#endif
#ifdef NSS_PPE_DRV_HW_GRO
	struct ppe_drv_gro_ctx gro_ctx;		/* HW GRO specific context */
#endif
#ifdef NSS_PPE_PON_SUPPORT
	int ppe_drv_pon_port_start_pq;		/* Start queue number mapped to pon port. */
	int ppe_drv_pon_port_max_pq;		/* max pq to be used for pon port. */
	struct ppe_drv_port_hw_stats pon_stats_acc;	/* 64-bit SW accumulator for PON port counters */
	fal_port_cnt_t pon_stats_prev;		/* Previous HW snapshot; zero until first sync */
	ppe_drv_qos_pon_port_alloc_callback_t pon_port_alloc_cb;	/* Callback to allocate QoS resources for PON port */
#endif
	struct ppe_drv_iface *pon_iface;	/* Memory for PPE PON interface */
};

/*
 * ppe_drv_tree_id_type_get()
 *	Returns the tree_id type.
 */
static inline ppe_drv_tree_id_type_t ppe_drv_tree_id_type_get(struct ppe_drv_flow_metadata *fl_mdata)
{
	return fl_mdata->tree_id_data.type;
}

/*
 * ppe_drv_assist_feature_type_check()
 *      Checks assist feature type.
 */
static inline bool ppe_drv_assist_feature_type_check(uint32_t feature, uint32_t flag)
{
	return !!(feature & flag);
}

/*
 * ppe_drv_assist_feature_type_is_rfs()
 *      Checks assist feature is a rfs feature.
 */
static inline bool ppe_drv_assist_feature_type_is_rfs(uint32_t feature)
{
	bool ret = ((feature & PPE_DRV_ASSIST_FEATURE_RFS_ETH) ||
		    (feature & PPE_DRV_ASSIST_FEATURE_RFS_WLAN) ||
		    (feature & PPE_DRV_ASSIST_FEATURE_RFS_TUNNEL) ||
		    (feature & PPE_DRV_ASSIST_FEATURE_RFS_UDP_ST) ||
		    (feature & PPE_DRV_ASSIST_FEATURE_RFS_MCAST) ||
		    (feature & PPE_DRV_ASSIST_FEATURE_RFS_UNKNOWN_DST));

	return ret;
}

#if defined(PPE_LOOPBACK_RING_SUPPORT)
/*
 * ppe_drv_tun_gretap_to_mapt_loopback_enabled()
 *	Check gretap to mapt loopback ring enabled.
 */
static inline bool ppe_drv_tun_gretap_to_mapt_loopback_enabled(struct ppe_drv *p)
{
	return !!(p->loopback_ring_info.enabled &&
		(p->loopback_ring_info.ft_type & PPE_DRV_LOOPBACK_FEATURE_TYPE_GRETAP_MAPT));
}
#endif

#if defined(PPE_LOOPBACK_PORT_SUPPORT)
/*
 * ppe_drv_loopback_port_ft_pon_enabled()
 *      Check pon loopback port enabled.
 */
static inline bool ppe_drv_loopback_port_ft_pon_enabled(struct ppe_drv *p) {
	return !!(p->loopback_port_info.enabled &&
			(p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_ACL_SC].dir_ft_type = PPE_DRV_LOOPBACK_PORT_FT_TYPE_ACL_PON));
}
#endif

#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
/*
 * ppe_drv_loopback_port_ft_pon_hgu_us_enabled()
 *	Check pon loopback port enabled for HGU US used case.
 */
static inline bool ppe_drv_loopback_port_ft_pon_hgu_us_enabled(struct ppe_drv *p) {
	return !!(p->loopback_port_info.enabled &&
			(p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_US_SC].dir_ft_type == PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_US));
}

/*
 * ppe_drv_loopback_port_ft_pon_hgu_ds_enabled()
 *	Check pon loopback port enabled for HGU DS used case.
 */
static inline bool ppe_drv_loopback_port_ft_pon_hgu_ds_enabled(struct ppe_drv *p) {
	return !!(p->loopback_port_info.enabled &&
			(p->loopback_port_info.ctx[PPE_DRV_LOOPBACK_PORT_CTX_PON_HGU_DS_SC].dir_ft_type == PPE_DRV_LOOPBACK_PORT_FT_TYPE_PON_HGU_DS));
}

#endif

#ifdef PPE_DRV_NPTV6_HW_SUPPORT
/*
 * ppe_drv_nptv6_hairpin_loopback_port_enabled()
 *	Check NPTv6 Hairpin NAT loopback port enabled.
 */
static inline bool ppe_drv_nptv6_hairpin_loopback_port_enabled(struct ppe_drv *p)
{
#if defined(PPE_LOOPBACK_PORT_SUPPORT)
    return !!(p->loopback_port_info.enabled &&
        (p->loopback_port_info.ft_type == PPE_DRV_LOOPBACK_PORT_FT_TYPE_HAIRPIN_NAT));
#else
    return false;
#endif
}

/*
 * ppe_drv_nptv6_hairpin_loopback_enabled()
 *	Check NPTv6 Hairpin NAT loopback support enabled.
 */
static inline bool ppe_drv_nptv6_hairpin_loopback_enabled(struct ppe_drv *p)
{
#if defined(PPE_LOOPBACK_PORT_SUPPORT)
    if (ppe_drv_nptv6_hairpin_loopback_port_enabled(p)) {
        return true;
    }
#endif
#if defined(PPE_LOOPBACK_RING_SUPPORT)
    return !!(p->loopback_ring_info.enabled &&
        (p->loopback_ring_info.ft_type & PPE_DRV_LOOPBACK_FEATURE_TYPE_V6_HAIRPIN_NAT));
#else
    return false;
#endif
}
#endif

/*
 * ppe_drv_assist_feature_is_valid()
 *	Checks if the feature type is one of the assist features.
 */
static inline bool ppe_drv_assist_feature_is_valid(uint32_t feature)
{
	return !!(feature & (PPE_DRV_ASSIST_FEATURE_RFS_ETH |
			     PPE_DRV_ASSIST_FEATURE_RFS_WLAN |
			     PPE_DRV_ASSIST_FEATURE_PRIORITY |
			     PPE_DRV_ASSIST_FEATURE_RFS_TUNNEL |
			     PPE_DRV_ASSIST_FEATURE_RFS_UDP_ST |
			     PPE_DRV_ASSIST_FEATURE_RFS_MCAST |
			     PPE_DRV_ASSIST_FEATURE_RFS_UNKNOWN_DST));
}

/*
 * ppe_drv_sawf_metadata
 *	SAWF information from create rule
 */
struct ppe_drv_sawf_metadata {
	uint32_t sawf_mark;	/* SAWF mark from create rule. */
	uint8_t service_class;	/* SAWF service class from create rule. */
};

/*
 * ppe_drv_scs_metadata
 *      SCS information from create rule
 */
struct ppe_drv_scs_metadata {
	uint32_t scs_mark;	/* SCS mark from create rule. */
};

/*
 * ppe_drv_flow_cookie_metadata
 *	Flow Cookie information from the create rule
 */
struct ppe_drv_flow_cookie_metadata {
	union {
		uint32_t mark;				/* Mark value for tree id type none from create rule message */
		struct ppe_drv_sawf_metadata sawf;	/* SAWF metadata from create rule message */
		struct ppe_drv_scs_metadata scs;	/* SCS metadata from create rule message */
	}type;
};

/*
 * nss_ppe_drv_minidump_log()
 *	To log data structures into minidump output
 */
static inline void nss_ppe_drv_minidump_log(void *start_addr, uint64_t size, const char *name) {
#ifdef CONFIG_QCA_MINIDUMP
	if (minidump_add_segments((uint64_t)(uintptr_t)(start_addr), size, QCA_WDT_LOG_DUMP_TYPE_MOD, name, MINIDUMP_CRASH_TYPE_NSS, "qca_nss_ppe") != 0)
		pr_warn("minidump_log failed for structure type %s at address %p\n", name, start_addr);
#endif
}

/*
 * nss_ppe_drv_minidump_free()
 *	To unregister data structures from minidump tlv
 */
static inline void nss_ppe_drv_minidump_free(void *start_addr, const char *name) {
#ifdef CONFIG_QCA_MINIDUMP
	if (minidump_remove_segments((uint64_t)(uintptr_t)(start_addr)) != 0)
		pr_warn("minidump_free failed for structure %s at address %p\n", name, start_addr);
#endif
}

#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
int ppe_drv_get_tpid_index(uint16_t tpid, const char *type);
#endif
#ifdef NSS_PPE_SIXTH_TUPLE
int ppe_drv_get_sixth_tuple_mode(void);
#endif
extern int ppe_drv_get_vxlan_dport(void);
extern int ppe_drv_get_vxlan_gpe_dport(void);
void ppe_drv_fse_ops_free(struct kref *kref);
extern struct ppe_drv *ppe_drv_gbl;
extern uint32_t if_bm_to_offload;
#ifdef PPE_LOOPBACK_PORT_SUPPORT
extern bool ppe_drv_lpbk_port_info_ctx_fill(enum ppe_drv_loopback_port_feature_type ft_type);
#endif
#ifdef NSS_PPE_PON_SUPPORT
extern uint32_t gem_port_bitmap;
extern int dot1p_dest_vp_en;
/**
 * ppe_drv_pon_map_enqueue_vp_to_pq
 *	Provides mapping of enqueue_vp to queue
 *	for pon flows.
 *
 * @param[in] port	PON port structure.
 *
 * @return
 * Success or error code.
 */
ppe_drv_ret_t ppe_drv_pon_map_enqueue_vp_to_pq(struct ppe_drv_port *port);

/**
 * ppe_drv_dp_pon_stats_update
 *	Read HW counters and accumulate rollover-safe deltas into p->pon_stats_acc.
 *	Called from ppe_drv_hw_stats_sync() while p->lock is held.
 *
 * @param[in] p  Global PPE driver context.
 */
void ppe_drv_dp_pon_stats_update(struct ppe_drv *p);

/**
 * ppe_drv_is_pon_enable_internal
 *	Checks whether PON is enabled on this RDP without taking the driver lock.
 *
 * @return
 * true if PON interface is set, false otherwise.
 */
bool ppe_drv_is_pon_enable_internal(void);
#endif

#ifdef PPE_DRV_DDRQ_ENABLE
extern uint32_t ppe_drv_ddrq_en_bitmask[PPE_DRV_DDRQ_MAX_CNT_WORD];
#endif
