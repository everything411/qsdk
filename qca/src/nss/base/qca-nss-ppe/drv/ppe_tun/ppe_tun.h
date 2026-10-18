/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_TUN_PRIV_H_
#define _PPE_TUN_PRIV_H_

#include <ppe_acl.h>
#include <ppe_drv_sc.h>
#include <ppe_tun.h>
#include <linux/udp.h>

#ifdef CONFIG_QCA_MINIDUMP
#include <soc/qcom/ctx-save.h>
#endif

/*
 * PPE Tunnel debug macros
 */
#if (PPE_TUN_DEBUG_LEVEL == 3)
#define ppe_tun_assert(c, s, ...)
#else
#define ppe_tun_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define ppe_tun_warn(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define ppe_tun_info(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define ppe_tun_trace(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_TUN_DEBUG_LEVEL < 2)
#define ppe_tun_warn(s, ...)
#else
#define ppe_tun_warn(s, ...) pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_TUN_DEBUG_LEVEL < 3)
#define ppe_tun_info(s, ...)
#else
#define ppe_tun_info(s, ...) pr_notice("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_TUN_DEBUG_LEVEL < 4)
#define ppe_tun_trace(s, ...)
#else
#define ppe_tun_trace(s, ...) pr_info("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * Maximum number of virtual port tunnels
 */
#define PPE_TUN_MAX	PPE_DRV_VIRTUAL_MAX

/*
 * Convert virtual port number to an idx
 */
#define PPE_TUN_VP_NUM_TO_IDX(vp_num)	((vp_num) - PPE_DRV_VIRTUAL_START)

#define PPE_TUN_DISABLE	0
#define PPE_TUN_ENABLE	1

enum ppe_tun_state {
	PPE_TUN_STATE_CONFIGURED	= (1 << 0),
	PPE_TUN_STATE_ACTIVATED		= (1 << 1),
};

enum xcpn_mode {PPE_TUN_XCPN_MODE_0, PPE_TUN_XCPN_MODE_1};

/*
 * ppe_tun_accel
 *	Enable / Disable acceleration for tunnel type
 */
struct ppe_tun_accel {
	bool ppe_tun_gretap_accel;	/* Controls gretap acceleration */
	bool ppe_tun_vxlan_accel;	/* Controls vxlan acceleration */
	bool ppe_tun_ipip6_accel;	/* Controls ipip6 acceleration */
	bool ppe_tun_mapt_accel;	/* Controls mapt acceleration */
	bool ppe_tun_l2tp_accel;	/* Controls l2tp acceleration */
	bool ppe_tun_cust_accel;	/* Controls custom tunnel acceleration */
	bool ppe_tun_vxlan_gpe_accel;	/* Controls vxlan gpe acceleration */
	bool ppe_tun_gretun_accel;	/* Controls gretap acceleration */
};

/*
 * ppe_tun_xcpn_mode
 *	Enable / Disable xcpn_mode for tunnel type
 */
struct ppe_tun_xcpn_mode {
	uint8_t gretap;	/* Controls gretap exception mode */
	uint8_t ipip6;	/* Controls ipip6 exception mode */
	uint8_t l2tp;	/* Controls l2tp exception mode */
	uint8_t gretun; /* Controls gretun exception mode */
	uint8_t vxlan;	/* Controls vxlan exception mode */
};

/*
 * ppe_tun_hybrid_offload
 *	Enable / Disable hybrid offload support for tunnel in PPE
 */
struct ppe_tun_hybrid_offload {
	bool en_gretap_hybrid_ol;	/* Controls gretap hyrid offload */
};

/*
 * ppe_tun
 *	Main ppe_tun structure to hold information about tunnel node.
 */
struct ppe_tun {
	struct kref ref;			/* Reference count */
	int32_t idx;				/* PPE tunne Index */
	ppe_vp_num_t vp_num;			/* Port number attached for VP */
	enum ppe_tun_state state;		/* PPE tunnel status flags */
	enum ppe_drv_tun_cmn_ctx_type type;	/* Tunnel type */
	struct net_device *dev;			/* Tunnel netdev */
	struct net_device *phys_dev;		/* Physical dev attached to VP */
	ppe_tun_exception_method_t src_excp;	/* Callback for exception packets with src VP */
	ppe_tun_exception_method_t dest_excp;	/* Callback for exception packets with dest VP */
	atomic64_t exception_packet;		/* Number of exception packets seen by tunnel */
	atomic64_t exception_bytes;		/* Total exception bytes total */
	atomic64_t tun_hybrid_offload_tx_fail_cnt;	/* tunnel hybrid offload tx failure count*/
	atomic64_t tun_hybrid_offload_tx_pkt_cnt;	/* tunnel hybrid offload tx offload count*/
	ppe_tun_stats_method_t stats_excp;	/* Callback for updating tunnel statistics */
	uint8_t tun_header_len;		/* Tunnel header length */
	uint32_t tun_xmit_port_mtu;	/* Tunnel xmit port MTU */
	ppe_tun_data *tun_data;		/* Tunnel specific data from client */
	uint8_t exp_mode;		/* Tunnel exception mode */
};

/*
 * ppe_tun_priv
 *	Private structure maintaining state of PPE tunnel driver.
 */
struct ppe_tun_priv {
	struct ppe_tun *tun[PPE_TUN_MAX];	/* Active tunnels index by vp_num PPE */
	spinlock_t lock;			/* Base lock */
	struct dentry *dentry;			/* Debugfs entry */
	struct ppe_tun_accel tun_accel;		/* Enable or disable acceleration per tunnel type */
	struct ppe_tun_xcpn_mode xcpn_mode;	/* Toggle exception mode */
	struct ppe_tun_hybrid_offload tun_hb_info;	/* Tunnel hybrid offload control */
	ppe_acl_rule_id_t ppe_tun_l2_tunnel_rule_id;	/* PPE ACL rule-id for L2 Tunnels */
	atomic_t total_free;			/* Number of available tunnel instance*/
	atomic_t free_pending;			/* Number of tunnel delete pending */
	atomic_t alloc_fail;			/* Number of tunnel alloc fails */
};

/*
 * nss_ppe_tun_minidump_log()
 *	To log data structures into minidump output
 */
static inline void nss_ppe_tun_minidump_log(void *start_addr, uint64_t size, const char *name) {
#ifdef CONFIG_QCA_MINIDUMP
	if (minidump_add_segments((uint64_t)(uintptr_t)(start_addr), size, QCA_WDT_LOG_DUMP_TYPE_MOD, name, MINIDUMP_CRASH_TYPE_NSS, "qca_nss_ppe_tun") != 0)
		pr_warn("minidump_log failed for structure type %s at address %p\n", name, start_addr);
#endif
}

/*
 * nss_ppe_tun_minidump_free()
 *	To unregister data structures from minidump tlv
 */
static inline void nss_ppe_tun_minidump_free(void *start_addr, const char *name) {
#ifdef CONFIG_QCA_MINIDUMP
	if (minidump_remove_segments((uint64_t)(uintptr_t)(start_addr)) != 0)
		pr_warn("minidump_free failed for structure %s at address %p\n", name, start_addr);
#endif
}

#endif /* _PPE_TUN_PRIV_H_ */
