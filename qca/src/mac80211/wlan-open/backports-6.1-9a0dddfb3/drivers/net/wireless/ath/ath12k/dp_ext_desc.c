// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "dp.h"
#include "debug.h"
#include "hal.h"

/* ATH12K WiFi7 Extension Descriptor Kmem Cache Management */

/**
 * ath12k_dp_ext_desc_cache_init() - Initialize extension descriptor cache
 *
 * Creates a dedicated kmem_cache for extension descriptors to improve
 * allocation performance and memory management.
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_dp_ext_desc_cache_init(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	char cache_name[32];

	scnprintf(cache_name, sizeof(cache_name), "ath12k_dp_ext_desc_%d", dp->device_id);
	dp->ext_cache = kmem_cache_create(cache_name,
					  ATH12K_DP_EXT_DESC_SZ,
					  0,  /* align */
					  SLAB_HWCACHE_ALIGN,
					  NULL  /* constructor */
					  );

	if (!dp->ext_cache) {
		ath12k_err(ab, "Failed to create extension descriptor cache\n");
		return -ENOMEM;
	}

	ath12k_info(ab, "Extension descriptor cache created successfully\n");
	return 0;
}
EXPORT_SYMBOL(ath12k_dp_ext_desc_cache_init);

/**
 * ath12k_dp_ext_desc_cache_deinit() - Cleanup extension descriptor cache
 *
 * Destroys the extension descriptor cache and reports any memory leaks.
 */
void ath12k_dp_ext_desc_cache_deinit(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;

	if (!dp->ext_cache)
		return;

	kmem_cache_destroy(dp->ext_cache);
	dp->ext_cache = NULL;
	ath12k_info(ab, "Extension descriptor cache destroyed\n");
}
EXPORT_SYMBOL(ath12k_dp_ext_desc_cache_deinit);

/**
 * ath12k_dp_ext_desc_map() - DMA map extension descriptor for coherent systems
 */
dma_addr_t ath12k_dp_ext_desc_map(struct ath12k_dp *dp, struct ath12k_dp_ext_desc *ext)
{
	dma_addr_t paddr = DMA_MAPPING_ERROR;

	if (dp && ext) {
#ifndef CONFIG_IO_COHERENCY
		paddr = dma_map_single(dp->dev, ext,
				       ATH12K_DP_EXT_DESC_SZ, DMA_TO_DEVICE);
		if (dma_mapping_error(dp->dev, paddr))
			return paddr;
#else
		return virt_to_phys(ext);
#endif
	}
	return paddr;
}
EXPORT_SYMBOL(ath12k_dp_ext_desc_map);

/**
 * ath12k_dp_ext_desc_unmap() - DMA unmap extension descriptor for coherent systems.
 *
 */
void ath12k_dp_ext_desc_unmap(struct ath12k_dp *dp, dma_addr_t paddr)
{
	if (!dp || !paddr || paddr == DMA_MAPPING_ERROR)
		return;
#ifndef CONFIG_IO_COHERENCY
	dma_unmap_single(dp->dev, paddr, ATH12K_DP_EXT_DESC_SZ, DMA_FROM_DEVICE);
#endif
}
EXPORT_SYMBOL(ath12k_dp_ext_desc_unmap);
