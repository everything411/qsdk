/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Generic lower-4-GB DMA bounce buffer support.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * For hardware with 32-bit DMA engines that cannot access physical addresses
 * above 4 GB. When U-Boot is relocated to a high DDR bank (> 4 GB), buffers
 * allocated from the heap or stack are above 4 GB and cannot be DMA-accessed
 * by 32-bit hardware.
 *
 * Usage:
 *   phys_addr_t bounce;
 *   void *buf = dma32_bounce_start(src, size, &bounce, DMA32_BB_RW);
 *   // use buf for DMA (guaranteed < 4 GB)
 *   dma32_bounce_stop(dst, bounce, size, DMA32_BB_RW);
 *   // caller must invalidate_dcache_range(dst) if needed after stop
 */

#ifndef __DMA_BOUNCE_H__
#define __DMA_BOUNCE_H__

#include <linux/types.h>
#include <asm/cache.h>
#include <linux/kconfig.h>
#if CONFIG_IS_ENABLED(LMB)
#include <lmb.h>
#endif

/*
 * DMA32_BB_READ  - DMA reads from buffer (copy src->bounce on start)
 * DMA32_BB_WRITE - DMA writes to buffer (copy bounce->dst on stop)
 * DMA32_BB_RW    - DMA reads and writes (copy both directions)
 */
#define DMA32_BB_READ	BIT(0)
#define DMA32_BB_WRITE	BIT(1)
#define DMA32_BB_RW	(DMA32_BB_READ | DMA32_BB_WRITE)

/**
 * dma32_bounce_start() - Get a lower-4-GB accessible buffer for 32-bit DMA.
 *
 * If @src is already below 4 GB, returns @src unchanged and sets *@bounce to 0.
 * If @src is above 4 GB and LMB is enabled, allocates a lower-4-GB buffer via
 * lmb_alloc_base(), copies @size bytes from @src if DMA32_BB_READ is set in
 * @flags, and returns the new buffer.
 *
 * In SPL (CONFIG_SPL_LMB not set), buffers are always in lower 4 GB since
 * SPL runs in-place at a fixed load address. This function is a no-op.
 *
 * @src:    Original buffer (may be above 4 GB)
 * @size:   Size of the buffer in bytes
 * @bounce: Output: LMB address of bounce buffer, or 0 if not needed
 * @flags:  DMA32_BB_READ, DMA32_BB_WRITE, or DMA32_BB_RW
 *
 * Return: lower-4-GB accessible pointer, or NULL on allocation failure
 */
static inline void *dma32_bounce_start(void *src, size_t size,
				       phys_addr_t *bounce, unsigned int flags)
{
	*bounce = 0;

	if (!src || !size)
		return NULL;

	/* Already in lower 4 GB — use as-is, no bounce needed */
	if ((uintptr_t)src <= 0xFFFFFFFFUL)
		return src;

	/* Above 4 GB — need a lower-4-GB bounce buffer */
	if (!CONFIG_IS_ENABLED(LMB))
		return src;

#if CONFIG_IS_ENABLED(LMB)
	*bounce = lmb_alloc_base(ALIGN(size, CONFIG_SYS_CACHELINE_SIZE),
				 CONFIG_SYS_CACHELINE_SIZE,
				 0xFFFFFFFFUL, LMB_NONE);
	if (!*bounce)
		return NULL;

	if (flags & DMA32_BB_READ)
		memcpy((void *)(uintptr_t)*bounce, src, size);

	return (void *)(uintptr_t)*bounce;
#endif
}

/**
 * dma32_bounce_stop() - Release a bounce buffer and copy result back.
 *
 * If @bounce is 0 (no bounce buffer was allocated), this is a no-op.
 * If DMA32_BB_WRITE is set in @flags, copies @size bytes from the bounce
 * buffer back to @dst before freeing the LMB allocation.
 *
 * The caller is responsible for any cache maintenance on @dst after this
 * call (e.g. invalidate_dcache_range) if required by the platform.
 *
 * @dst:    Original destination buffer (may be above 4 GB)
 * @bounce: LMB address of bounce buffer (0 if none)
 * @size:   Size of the buffer in bytes
 * @flags:  DMA32_BB_READ, DMA32_BB_WRITE, or DMA32_BB_RW
 */
static inline void dma32_bounce_stop(void *dst, phys_addr_t bounce,
				     size_t size, unsigned int flags)
{
	if (!CONFIG_IS_ENABLED(LMB) || !bounce)
		return;

#if CONFIG_IS_ENABLED(LMB)
	if ((flags & DMA32_BB_WRITE) && dst)
		memcpy(dst, (void *)(uintptr_t)bounce, size);

	lmb_free(bounce, ALIGN(size, CONFIG_SYS_CACHELINE_SIZE));
#endif
}

#endif /* __DMA_BOUNCE_H__ */
