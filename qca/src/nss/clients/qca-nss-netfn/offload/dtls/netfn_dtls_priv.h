/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __NETFN_DTLS_PRIV_H
#define __NETFN_DTLS_PRIV_H

#include <linux/kref.h>
#include <linux/debugfs.h>
#include <eip.h>

#include "netfn_dtls.h"
#include "netfn_dtls_session.h"
#include "netfn_dtls_tun.h"
#include "netfn_dtls_enc.h"
#include "netfn_dtls_dec.h"

#define NETFN_DTLS_DEFAULT_SVC EIP_SVC_DTLS

#define NETFN_DTLS_MAX_STR_LEN 64	/* Maximum print lenght */
#define NETFN_DTLS_HASH_SHIFT 6
#define NETFN_DTLS_HASH_SIZE (1 << NETFN_DTLS_HASH_SHIFT)
#define NETFN_DTLS_HASH_MASK (NETFN_DTLS_HASH_SIZE - 1)

/*
 * IPsec driver object.
 */
struct netfn_dtls_drv {
	struct kref ref;		/* Driver reference */
	struct completion completion;	/* Completion to wait for all deref */

	struct kmem_cache *session_cache;	/* Kmem cache for session memory */
	struct eip_ctx *ctx;		/* DMA context to use for transformation request */
	struct dentry *dentry;		/* Driver debugfs dentry */
};

extern struct netfn_dtls_drv g_dtls_drv;	/* Global Driver object */
extern void netfn_dtls_drv_final(struct kref *kref);

/*
 * netfn_dtls_drv_ref()
 * 	Increment driver object reference
 */
static inline struct netfn_dtls_drv *netfn_dtls_drv_ref(struct netfn_dtls_drv *drv)
{
	kref_get(&drv->ref);
	return drv;
}

/*
 * netfn_dtls_drv_deref()
 * 	Decrement driver object reference
 */
static inline void netfn_dtls_drv_deref(struct netfn_dtls_drv *drv)
{
	kref_put(&drv->ref, netfn_dtls_drv_final);
}

#endif /* !__NETFN_DTLS_PRIV_H */
