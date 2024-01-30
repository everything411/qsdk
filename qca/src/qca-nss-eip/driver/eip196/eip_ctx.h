/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __EIP_CTX_H
#define __EIP_CTX_H

/*
 * eip_ctx_tr
 *	Tranform record list.
 */
struct eip_ctx_tr {
	struct list_head head;
	rwlock_t lock;
	uint16_t count;
	struct {
		uint64_t alloc;
		uint64_t dealloc;
		uint64_t free;
	} stats;
};

/*
 * eip_ctx
 *	Per service HW context
 */
struct eip_ctx {
	struct eip_dma *dma;			/* DMA object to use */
	struct kmem_cache *tk_cache;		/* Token cache for crypto operation & auth key */
	struct kmem_cache *sw_cache;		/* Software desc cache */

	struct eip_ctx_tr tr;			/* TR list under this context */

	struct kref ref;			/* Reference incremented for each tr */
	enum eip_svc svc;			/* Service for which context is allocated */
	const struct eip_svc_entry *svc_db;	/* Service algorithm database */
	size_t db_size;			/* Size of database */

	struct eip_pdev *ep;			/* EIP platform data */
	struct dentry *dentry;			/* debugfs file node */
	uint16_t ip_id;				/* IP indentifier for IPsec service */
};

void eip_ctx_final(struct kref *kref);
void eip_ctx_add_tr(struct eip_ctx *ctx, struct eip_tr *tr);
void eip_ctx_del_tr(struct eip_ctx *ctx, struct eip_tr *tr);
const struct eip_svc_entry *eip_ctx_algo_lookup(struct eip_ctx *ctx, char *algo_name);

/*
 * eip_ctx_ref()
 *	Increment context reference.
 */
static inline struct eip_ctx *eip_ctx_ref(struct eip_ctx *ctx)
{
	kref_get(&ctx->ref);
	return ctx;
}

/*
 * eip_ctx_deref()
 *	Decrement context reference.
 */
static inline void eip_ctx_deref(struct eip_ctx *ctx)
{
	kref_put(&ctx->ref, eip_ctx_final);
}

#endif /* __EIP_CTX_H */
