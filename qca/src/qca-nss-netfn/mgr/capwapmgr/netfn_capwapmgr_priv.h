/*
 * netfn_capwapmgr_priv.h
 *	Network function's CAPWAP manager private header file.
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __NETFN_CAPWAPMGR_PRIV_H
#define __NETFN_CAPWAPMGR_PRIV_H

#include <asm/bitops.h>
#include <linux/spinlock.h>
#include <linux/if_ether.h>
#include <linux/kref.h>
#include <linux/bitmap.h>
#include <linux/mutex.h>

#include <netfn_capwap.h>
#include <netfn_flowmgr.h>
#include <netfn_dtls.h>

/*
 * If dynamic debug is enabled, use pr_warn.
 */
#define netfn_capwapmgr_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define netfn_capwapmgr_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define netfn_capwapmgr_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)

/*
 * Mask to check if dtls is enabled.
 */
#define NETFN_CAPWAPMGR_EXT_VALID_DTLS (NETFN_CAPWAPMGR_EXT_VALID_DTLS_ENC | NETFN_CAPWAPMGR_EXT_VALID_DTLS_DEC)

/*
 * We set the desired Hash table size here.
 * User can choose the size from 1 to 4096.
 * Any size other than this if configured,
 * will be set to default.
 */
#define NETFN_CAPWAPMGR_FLOW_COOKIE_HASHTABLE_SIZE 4096

/*
 * netfn_capwapmgr_stats
 *	Capwap manager stats.
 */
struct netfn_capwapmgr_stats {
	atomic_t tun_dev_alloc;		/* Number of tunnels allocated */
	atomic_t tun_dev_free;		/* Number of tunnels freed */
	atomic_t tunid_dev_alloc;	/* Number of tunid dev allocated */
	atomic_t tunid_dev_free;	/* Number of tunid dev freed */
	atomic64_t error_stats[NETFN_CAPWAPMGR_ERROR_MAX];
					/* Error stats */
};

/*
 * netfn_capwapmgr
 *	Global instance.
 */
struct netfn_capwapmgr {
	struct kref ref;			/* Driver reference */
	struct completion completion;		/* Completion to wait for freeing the driver */
	struct mutex lock;			/* Mutex to serialize config updates */
	struct dentry *dentry;			/* Debugfs for driver stats */
	struct netfn_capwapmgr_stats stats;	/* Stats */
};

extern struct netfn_capwapmgr g_mgr;	/* Global manager object */
extern uint8_t netfn_capwapmgr_snap[NETFN_CAPWAP_SNAP_HDR_LEN];
extern void netfn_capwapmgr_final(struct kref *kref);

#if defined(NETFN_CAPWAPMGR_ONE_NETDEV)
extern struct net_device *netfn_capwapmgr_dev;
#endif

/*
 * netfn_capwapmgr_ref()
 * 	Increment driver object reference
 */
static inline struct netfn_capwapmgr *netfn_capwapmgr_ref(struct netfn_capwapmgr *mgr)
{
	kref_get(&mgr->ref);
	return mgr;
}

/*
 * netfn_capwapmgr_deref()
 * 	Decrement driver object reference
 */
static inline void netfn_capwapmgr_deref(struct netfn_capwapmgr *mgr)
{
	kref_put(&mgr->ref, netfn_capwapmgr_final);
}
#endif /* __NETFN_CAPWAPMGR_PRIV_H */
