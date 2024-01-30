/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

union vxlan_addr;
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

struct nss_ppe_vxlanmgr_ctx {
	struct list_head list;			/* VxLAN tunnel list */
	struct dentry *dentry;			/* debugfs entry for qca-nss-vxlanmgr */
	uint32_t tun_count;				/* Number of VxLAN tunnels in the list */
	spinlock_t tun_lock;			/* lock to protect the tunnel list */
};

struct nss_ppe_vxlanmgr_tun_ctx {
	struct list_head head;			/* tunnel context list entry */
	struct net_device *dev;			/* tunnel netdevice pointer */
	struct dentry *dentry;			/* per tunnel debugfs entry */
	uint32_t vni;				/* vnet identifier */
	uint32_t tunnel_flags;			/* vxlan tunnel flags */
	uint16_t src_port_min;			/* minimum source port */
	uint16_t src_port_max;			/* maximum source port*/
	uint16_t dest_port;			/* destination port */
	uint8_t tos;				/* tos value */
	uint8_t ttl;				/* time to live */
	struct nss_ppe_vxlanmgr_ctx *vxlan_ctx;	/* pointer to vxlanmgr context */
	struct ppe_drv_tun_cmn_ctx *tun_hdr;	/* Outer tunnel header */
	bool remote_detected;			/* Indicates whether the remote end point IP address is detected */
};

extern int nss_ppe_vxlanmgr_tunnel_create(struct net_device *dev);
extern int nss_ppe_vxlanmgr_tunnel_destroy(struct net_device *dev);
extern int nss_ppe_vxlanmgr_tunnel_config(struct net_device *dev, struct ppe_drv_tun_cmn_ctx *tun_hdr);
struct nss_ppe_vxlanmgr_tun_ctx *nss_ppe_vxlanmgr_tunnel_ctx_dev_get(struct net_device *dev);
#endif /* __NSS_VXLANMGR_PPE_PRIV_H */
