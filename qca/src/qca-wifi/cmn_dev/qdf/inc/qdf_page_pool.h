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

#if  !defined(_QDF_PAGE_POOL_H)
#define _QDF_PAGE_POOL_H

#include <i_qdf_page_pool.h>
#include <qdf_util.h>

typedef __qdf_page_pool_t qdf_page_pool_t;

/**
 * qdf_page_pool_alloc_page: Allocate full page from page pool
 *
 * @pp: Page Pool reference
 *
 * Return: Page reference
 */
static inline qdf_page_t qdf_page_pool_alloc_page(qdf_page_pool_t pp)
{
	return __qdf_page_pool_alloc_page(pp);
}

/**
 * qdf_page_pool_get_dma_addr: Get DMA address of the page pool page
 *
 * @page: Reference to the page
 *
 * Return: DMA address of the page
 */
static inline dma_addr_t qdf_page_pool_get_dma_addr(qdf_page_t page)
{
	return __qdf_page_pool_get_dma_addr(page);
}

/**
 * qdf_page_pool_full_bh() - Check page pool full condition
 *
 * @pp: Page Pool Reference
 *
 * Return: true/false
 */
static inline bool qdf_page_pool_full_bh(qdf_page_pool_t pp)
{
	return __qdf_page_pool_full_bh(pp);
}

/**
 * qdf_page_pool_empty() - Check page pool empty condition
 *
 * @pp: Page Pool Reference
 *
 * Return: true/false
 */
static inline bool qdf_page_pool_empty(qdf_page_pool_t pp)
{
	return __qdf_page_pool_empty(pp);
}

/**
 * qdf_page_pool_alloc_frag() - Allocate frag buffer from page pool
 *
 * @pp: Page Pool Reference
 * @offset: Buffer offset reference within the page
 * @size: Buffer size
 *
 * Return: Allocated page reference
 */
static inline qdf_page_t
qdf_page_pool_alloc_frag(qdf_page_pool_t pp, uint32_t *offset, size_t size)
{
	return __qdf_page_pool_alloc_frag(pp, offset, size);
}

/**
 * qdf_page_pool_put_page() - Decrement frag reference count of page pool page
 *
 * @pp: Page Pool eference
 * @page: Page reference
 * @direct_recycle: Direct recycle to lockless cache in page pool
 *
 * Return: None
 */
static inline void
qdf_page_pool_put_page(qdf_page_pool_t pp, qdf_page_t page,
		       bool direct_recycle)
{
	return __qdf_page_pool_put_page(pp, page, direct_recycle);
}

/**
 * qdf_page_pool_create() - Create page_pool
 *
 * @osdev: Device handle
 * @pool_size: Pool Size
 * @pp_page_size: Page pool page size
 *
 * Return: Page Pool Reference
 */
static inline qdf_page_pool_t
qdf_page_pool_create(qdf_device_t osdev, size_t pool_size, size_t pp_page_size)
{
	return __qdf_page_pool_create(osdev, pool_size, pp_page_size);
}

/**
 * qdf_page_pool_destroy() - Destroy page_pool
 * @pp: Page Pool Reference
 *
 * Return: None
 */
static inline void qdf_page_pool_destroy(qdf_page_pool_t pp)
{
	return __qdf_page_pool_destroy(pp);
}
#endif /* _QDF_PAGE_POOL_H */
