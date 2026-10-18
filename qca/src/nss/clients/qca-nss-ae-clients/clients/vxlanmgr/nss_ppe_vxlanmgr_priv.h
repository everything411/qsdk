/*
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * nss_ppe_vxlanmgr_priv.h
 *	VxLAN manager header
 */
#ifndef __NSS_PPE_VXLANMGR_PRIV_H
#define __NSS_PPE_VXLANMGR_PRIV_H

#include <net/vxlan.h>
#include <nss_ppe_vxlanmgr.h>

#ifdef CONFIG_QCA_MINIDUMP
#include <soc/qcom/ctx-save.h>
#endif

#define NSS_PPE_VXLANMGR_DST_PORT_MIN 1
#define NSS_PPE_VXLANMGR_DST_PORT_MAX 65535

struct ppe_drv_tun_cmn_ctx;

/*
 * Compile messages for dynamic enable/disable
 */
#if defined(CONFIG_DYNAMIC_DEBUG)
#define nss_ppe_vxlanmgr_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define nss_ppe_vxlanmgr_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define nss_ppe_vxlanmgr_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else /* CONFIG_DYNAMIC_DEBUG */
/*
 * Statically compile messages at different levels
 */
#if (NSS_PPE_VXLAN_MGR_DEBUG_LEVEL < 2)
#define nss_ppe_vxlanmgr_warn(s, ...)
#else
#define nss_ppe_vxlanmgr_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PPE_VXLAN_MGR_DEBUG_LEVEL < 3)
#define nss_ppe_vxlanmgr_info(s, ...)
#else
#define nss_ppe_vxlanmgr_info(s, ...)   pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PPE_VXLAN_MGR_DEBUG_LEVEL < 4)
#define nss_ppe_vxlanmgr_trace(s, ...)
#else
#define nss_ppe_vxlanmgr_trace(s, ...)  pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif /* CONFIG_DYNAMIC_DEBUG */

#define nss_ppe_vxlanmgr_assert(c) BUG_ON(!(c))

#define NSS_PPE_VXLAN_MGR_O_DSCP_GET(x, y) (x >> y)

/*
 * VxLAN/VxLAN-GPE global variable
 * 2^8 = 256 is the size of the hash table.
 */
#define NSS_PPE_VXLANMGR_HASH_TABLE_SIZE 8
#define NSS_PPE_VXLANMGR_MAX_REMOTES 8

extern int dstport;					/* Global VxLAN dst port */
extern int dstport_gpe;					/* Global VxLAN-gpe dst port */
extern bool inherit_ttl;				/* Global ttl inherit config for encap */
extern bool inherit_dscp;				/* Global dscp inherit config for encap */
extern uint8_t encap_ecn_mode;				/* Global ECN config for encap */
extern uint8_t decap_ecn_mode;				/* Global ECN config for decap */
extern struct nss_ppe_vxlanmgr_ctx vxlan_ctx;		/* Global VxLAN context */
extern spinlock_t nss_ppe_vxlanmgr_tunnel_tbl_lock;	/* Global spin lock for vxlan hash table */
extern DECLARE_HASHTABLE(nss_ppe_vxlanmgr_tunnel_tbl, NSS_PPE_VXLANMGR_HASH_TABLE_SIZE);	/* VxLAN hash table */

/*
 * VXLAN global context.
 */
struct nss_ppe_vxlanmgr_ctx {
	struct dentry *dentry;			/* per tunnel debugfs entry */
	uint16_t nack_limit;	/* nack limit */
	spinlock_t tun_lock;			/* lock to protect the tunnel list */
};

/*
 * VXLAN remote information.
 */
struct nss_ppe_vxlanmgr_remote_info {
	struct kref mac_address_ref;			/* reference count to keep track of number of clients connected to a remote */
	union vxlan_addr remote_ip;	/* Remote IP address */
	struct net_device *nss_netdev;	/* The child/dummy netdevice */
	struct net_device *pdev;	/* The parent netdevice */
	uint32_t vp_status_nack;		/* The nack limit for the remote */
	bool bridge_joined;	/* NSS netdevice is in the bridge */
};

/*
 * VXLAN RTM_NEIGH event data.
 * RTM_NEIGH event is an irq context hence the below data-structure is used to store the information
 * so that it will be used later in the work-queue.
 */
struct nss_ppe_vxlanmgr_rtm_neigh_event_data {
	struct  net_device *parent_netdev;	/* Parent/linux netdevice of the tunnel */
	union vxlan_addr rip;			/* Remote IP address received in SWITCHDEV_VXLAN_FDB_ADD_TO_DEVICE event */
	uint8_t event;	/* SWITCHDEV_VXLAN_FDB_ADD_TO_DEVICE or SWITCHDEV_VXLAN_FDB_DEL_TO_DEVICE event */
	struct list_head rtm_event_list;	/* list to maintain the RTM events */
};

/*
 * FIB event data for VXLAN GPE
 * The below data-structure is used to store the information which will be used later in the work-queue.
 */
struct nss_ppe_vxlanmgr_fib_event_data {
	__be32 vni;				/* VxLAN Network Identifier */
	uint8_t tos;				/* tos value */
	uint8_t ttl;				/* time to live */
	uint8_t event;				/* FIB add, delete, update event */
	__be16 tun_flags;			/* Tunnel flags */
	union vxlan_addr rip;			/* Remote IP address received in FIB update event */
	union vxlan_addr sip;			/* Local IP address received in FIB update event */
	struct net_device *parent_netdev;	/* Parent/linux netdevice of the tunnel */
	struct list_head fib_event_list;	/* list to maintain the FIB events */
};

/*
 * VXLAN tunnel context for each tunnel (VXLAN remote).
 */
struct nss_ppe_vxlanmgr_tun_ctx {
	uint32_t vni;				/* vnet identifier */
	struct net_device *parent_dev;			/* tunnel netdevice pointer */
	struct dentry *dentry;			/* per tunnel debugfs entry */
	uint32_t tunnel_flags;			/* vxlan tunnel flags */
	uint16_t src_port_min;			/* minimum source port */
	uint16_t src_port_max;			/* maximum source port*/
	uint16_t dest_port;			/* destination port */
	uint8_t tos;				/* tos value */
	uint8_t ttl;				/* time to live */
	struct ppe_drv_tun_cmn_ctx *tun_hdr;	/* Outer tunnel header */
	struct nss_ppe_vxlanmgr_remote_info remote_info;	/* The remote database */
	enum nss_ppe_vxlanmgr_vp_creation vp_status;	/* VP creation status for each VXLAN tunnel */
	struct nss_ppe_vxlanmgr_ctx *vxlan_ctx;	/* VXLAN global context */
	struct hlist_node node;	/* Hash node for each VXLAN PPE tunnel */
};

/*
 * struct nss_ppe_vxlanmgr_nss_dev_priv
 *	Private structure for the child/dummy nss-netdevice.
 */
struct nss_ppe_vxlanmgr_nss_dev_priv {
	int pdev_ifindex;	/* Linux-kernel/Parent netdevice index */
};

/*
 * nss_ppe_vxlanmgr_minidump_log()
 *	To log data structures into minidump output
 */
static inline void nss_ppe_vxlanmgr_minidump_log(void *start_addr, uint64_t size, const char *name) {
#ifdef CONFIG_QCA_MINIDUMP
	if (minidump_add_segments((uint64_t)(uintptr_t)(start_addr), size, QCA_WDT_LOG_DUMP_TYPE_MOD, name, MINIDUMP_CRASH_TYPE_NSS, "qca_nss_ppe_vxlanmgr") != 0)
		pr_warn("minidump_log failed for structure type %s at address %p\n", name, start_addr);
#endif
}

/*
 * nss_ppe_vxlanmgr_minidump_free()
 *	To unregister data structures from minidump tlv
 */
static inline void nss_ppe_vxlanmgr_minidump_free(void *start_addr, const char *name) {
#ifdef CONFIG_QCA_MINIDUMP
	if (minidump_remove_segments((uint64_t)(uintptr_t)(start_addr)) != 0)
		pr_warn("minidump_free failed for structure %s at address %p\n", name, start_addr);
#endif
}

/*
 * VxLAN database helper function
 */
void nss_ppe_vxlanmgr_tunnel_ctx_attach(struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx);
uint8_t nss_ppe_vxlanmgr_get_remote_count(struct net_device *dev, __be32 vni_key);
bool nss_ppe_vxlanmgr_new_remote(__be32 vni_key, union vxlan_addr *rip);
void nss_ppe_vxlanmgr_read_tunnel_config(struct net_device *pdev, struct seq_file *m);
struct nss_ppe_vxlanmgr_tun_ctx *nss_ppe_vxlanmgr_tunnel_ctx_get_and_dettach(struct net_device* nss_dev);
struct nss_ppe_vxlanmgr_tun_ctx *nss_ppe_vxlanmgr_get_tun_ctx_by_vni_and_rip(__be32 vni_key, union vxlan_addr *rip);
enum nss_ppe_vxlanmgr_vp_creation nss_ppe_vxlanmgr_get_ifindex_and_vp_status(struct net_device *dev, union vxlan_addr *remote_ip, uint32_t vni, int *ifindex);

/*
 * VxLAN tunnel configuration API
 */
void nss_ppe_vxlanmgr_gpe_all_remotes_set_mtu(struct net_device *pdev, unsigned int mtu);
void nss_ppe_vxlanmgr_all_remotes_set_mtu(struct net_device *pdev, unsigned int mtu);
void nss_ppe_vxlanmgr_all_remotes_decap_enable(struct net_device *pdev);
void nss_ppe_vxlanmgr_all_remotes_decap_disable(struct net_device *pdev);
void nss_ppe_vxlanmgr_all_remotes_leave_bridge(struct net_device *pdev);
void nss_ppe_vxlanmgr_all_remotes_join_bridge(struct net_device *pdev);
void nss_ppe_vxlanmgr_delete_all_remotes(void);
int nss_ppe_vxlanmgr_wq_init(void);
int nss_ppe_vxlanmgr_wq_exit(void);
int nss_ppe_vxlanmgr_gpe_wq_init(void);
int nss_ppe_vxlanmgr_gpe_wq_exit(void);
#endif /* __NSS_VXLANMGR_PPE_PRIV_H */
