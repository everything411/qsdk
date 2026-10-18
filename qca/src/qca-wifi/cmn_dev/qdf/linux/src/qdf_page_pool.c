/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <qdf_page_pool.h>
#include <linux/ptr_ring.h>

bool __qdf_is_pp_nbuf(struct sk_buff *skb)
{
	return !!skb->pp_recycle;
}

struct page *__qdf_page_pool_alloc_page(qdf_page_pool_t pp)
{
	return page_pool_dev_alloc_pages(pp);
}

dma_addr_t __qdf_page_pool_get_dma_addr(struct page *page)
{
	return page_pool_get_dma_addr(page);
}

bool __qdf_page_pool_full_bh(__qdf_page_pool_t pp)
{
	int i;
	int count = 0;

	if (!pp->alloc.count)
		return ptr_ring_full_bh(&pp->ring);

	for (i = 0; i < pp->ring.size; i++) {
		if (pp->ring.queue[i])
			count++;
	}

	return (pp->ring.size - count == pp->alloc.count);
}

bool __qdf_page_pool_empty(__qdf_page_pool_t pp)
{
	return !pp->alloc.count && ptr_ring_empty(&pp->ring);
}

struct page *__qdf_page_pool_alloc_frag(__qdf_page_pool_t pp, uint32_t *offset,
					size_t size)
{
	return page_pool_dev_alloc_frag(pp, offset, size);
}

void __qdf_page_pool_put_page(__qdf_page_pool_t pp, struct page *page,
			      bool direct_recycle)
{
	return page_pool_put_full_page(pp, page, direct_recycle);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 9, 0))
static void set_page_param_frag_flag(struct page_pool_params *pp_params)
{
	pp_params->flags |= PP_FLAG_PAGE_FRAG;
}
#else
static void set_page_param_frag_flag(struct page_pool_params *pp_params)
{
}
#endif

__qdf_page_pool_t __qdf_page_pool_create(qdf_device_t osdev, size_t pool_size,
					 size_t pp_page_size)
{
	struct page_pool_params pp_params = {0};
	struct page_pool *pp;
	int ret;

	pp_params.order = get_order(pp_page_size);
	pp_params.flags = PP_FLAG_DMA_MAP | PP_FLAG_DMA_SYNC_DEV,
	pp_params.nid = NUMA_NO_NODE,
	pp_params.dev = osdev->dev,
	pp_params.dma_dir = DMA_FROM_DEVICE,
	pp_params.offset = 0,
	pp_params.max_len = pp_page_size;
	pp_params.pool_size = pool_size;
	set_page_param_frag_flag(&pp_params);

	pp = page_pool_create(&pp_params);
	if (IS_ERR(pp)) {
		ret = PTR_ERR(pp);
		qdf_rl_nofl_err("Failed to create page pool: %d", ret);
		return NULL;
	}

	return pp;
}

void __qdf_page_pool_destroy(__qdf_page_pool_t pp)
{
	return page_pool_destroy(pp);
}
