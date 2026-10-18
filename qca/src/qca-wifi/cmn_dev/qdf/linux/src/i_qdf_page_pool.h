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

#if  !defined(_I_QDF_PAGE_POOL_H)
#define _I_QDF_PAGE_POOL_H

#include <linux/version.h>
#include <linux/skbuff.h>
#include "qdf_types.h"
#include "i_qdf_trace.h"

#ifdef DP_FEATURE_RX_BUFFER_RECYCLE

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#include <net/page_pool/helpers.h>
#include <net/page_pool/types.h>
#else
#include <net/page_pool.h>
#endif

typedef struct page_pool *__qdf_page_pool_t;

/**
 * __qdf_is_pp_nbuf: Check if SKB memory is from page pool
 *
 * @skb: SKB reference
 *
 * Return: True/False
 */
bool __qdf_is_pp_nbuf(struct sk_buff *skb);

/**
 * __qdf_page_pool_alloc_page: Allocate full page from page pool
 *
 * @pp: Page Pool reference
 *
 * Return: Page reference
 */
struct page *__qdf_page_pool_alloc_page(__qdf_page_pool_t pp);

/**
 * __qdf_page_pool_get_dma_addr: Get DMA address of the page pool page
 *
 * @page: Reference to the page
 *
 * Return: DMA address of the page
 */
dma_addr_t __qdf_page_pool_get_dma_addr(struct page *page);

/**
 * __qdf_page_pool_full_bh() - Check page pool full condition
 *
 * @pp: Page Pool Reference
 *
 * Return: true/false
 */
bool __qdf_page_pool_full_bh(__qdf_page_pool_t pp);

/**
 * __qdf_page_pool_empty() - Check page pool empty condition
 *
 * @pp: Page Pool Reference
 *
 * Return: true/false
 */
bool __qdf_page_pool_empty(__qdf_page_pool_t pp);

/**
 * __qdf_page_pool_alloc_frag() - Allocate frag buffer from page pool
 *
 * @pp: Page Pool Reference
 * @offset: Buffer offset reference within the page
 * @size: Buffer size
 *
 * Return: Allocated page reference
 */
struct page *__qdf_page_pool_alloc_frag(__qdf_page_pool_t pp, uint32_t *offset,
					size_t size);

/**
 * __qdf_page_pool_put_page() - Decrement frag reference count of page pool page
 *
 * @pp: Page Pool eference
 * @page: Page reference
 * @direct_recycle: Direct recycle to lockless cache in page pool
 *
 * Return: None
 */
void __qdf_page_pool_put_page(__qdf_page_pool_t pp, struct page *page,
			      bool direct_recycle);

/**
 * __qdf_page_pool_create() - Create page_pool
 *
 * @osdev: Device handle
 * @pool_size: Pool Size
 * @pp_page_size: Page pool page size
 *
 * Return: Page Pool Reference
 */
__qdf_page_pool_t __qdf_page_pool_create(qdf_device_t osdev, size_t pool_size,
					 size_t pp_page_size);

/**
 * __qdf_page_pool_destroy() - Destroy page_pool
 * @pp: Page Pool Reference
 *
 * Return: None
 */
void __qdf_page_pool_destroy(__qdf_page_pool_t pp);

#else

typedef void *__qdf_page_pool_t;

/**
 * __qdf_is_pp_nbuf: Check if SKB memory is from page pool
 *
 * @skb: SKB reference
 *
 * Return: True/False
 */
static inline bool __qdf_is_pp_nbuf(struct sk_buff *skb)
{
	return false;
}

/**
 * __qdf_page_pool_alloc_page: Allocate full page from page pool
 *
 * @pp: Page Pool reference
 *
 * Return: Page reference
 */
static inline struct page *__qdf_page_pool_alloc_page(__qdf_page_pool_t pp)
{
	return NULL;
}

/**
 * __qdf_page_pool_get_dma_addr: Get DMA address of the page pool page
 *
 * @page: Reference to the page
 *
 * Return: DMA address of the page
 */
static inline dma_addr_t __qdf_page_pool_get_dma_addr(struct page *page)
{
	return 0;
}

/**
 * __qdf_page_pool_full_bh() - Check page pool full condition
 *
 * @pp: Page Pool Reference
 *
 * Return: true/false
 */
static inline bool __qdf_page_pool_full_bh(__qdf_page_pool_t pp)
{
	return false;
}

/**
 * __qdf_page_pool_empty() - Check page pool empty condition
 *
 * @pp: Page Pool Reference
 *
 * Return: true/false
 */
static inline bool __qdf_page_pool_empty(__qdf_page_pool_t pp)
{
	return false;
}

/**
 * __qdf_page_pool_alloc_frag() - Allocate frag buffer from page pool
 *
 * @pp: Page Pool Reference
 * @offset: Buffer offset reference within the page
 * @size: Buffer size
 *
 * Return: Allocated page reference
 */
static inline struct page *
__qdf_page_pool_alloc_frag(__qdf_page_pool_t pp, uint32_t *offset, size_t size)
{
	return NULL;
}

/**
 * __qdf_page_pool_put_page() - Decrement frag reference count of page pool page
 *
 * @pp: Page Pool eference
 * @page: Page reference
 * @direct_recycle: Direct recycle to lockless cache in page pool
 *
 * Return: None
 */
static inline void
__qdf_page_pool_put_page(__qdf_page_pool_t pp, struct page *page,
			 bool direct_recycle)
{
}

/**
 * __qdf_page_pool_create() - Create page_pool
 *
 * @osdev: Device handle
 * @pool_size: Pool Size
 * @pp_page_size: Page pool page size
 *
 * Return: Page Pool Reference
 */
static inline __qdf_page_pool_t
__qdf_page_pool_create(qdf_device_t osdev, size_t pool_size,
		       size_t pp_page_size)
{
	return NULL;
}

/**
 * __qdf_page_pool_destroy() - Destroy page_pool
 * @pp: Page Pool Reference
 *
 * Return: None
 */
static inline void __qdf_page_pool_destroy(__qdf_page_pool_t pp)
{
}
#endif /* DP_FEATURE_RX_BUFFER_RECYCLE */
#endif /* _I_QDF_PAGE_POOL_H */
