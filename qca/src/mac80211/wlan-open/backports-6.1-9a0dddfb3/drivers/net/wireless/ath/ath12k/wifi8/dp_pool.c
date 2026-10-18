// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_pool.h"
#include "../debug.h"

void mem_multi_page_alloc(struct ath12k_base *ab,
			  struct mem_multi_page_t *pages,
			  u16 element_size, u16 element_num,
			  bool cacheable)
{
	u16 page_idx, i;
	struct mem_page_t *mem_pages;
	int size;

	if (!pages->page_size)
		pages->page_size = ATH12K_PAGE_SIZE;

	pages->num_element_per_page = pages->page_size / element_size;
	if (!pages->num_element_per_page) {
		ath12k_warn(ab, "Invalid page %d or element size %d",
			    (int)pages->page_size, (int)element_size);
		goto out_fail;
	}

	pages->num_pages = element_num / pages->num_element_per_page;
	if (element_num % pages->num_element_per_page)
		pages->num_pages++;

	size = (pages->num_pages * sizeof(struct mem_page_t)) +
		ATH12K_DP_POOL_CTXT_ALIGN - 1;
	mem_pages = kzalloc(size, GFP_ATOMIC);
	if (!mem_pages)
		goto out_fail;
	pages->mem_pages_unaligned = mem_pages;
	pages->mem_pages = PTR_ALIGN(mem_pages, ATH12K_DP_POOL_CTXT_ALIGN);

	for (page_idx = 0; page_idx < pages->num_pages; page_idx++) {
		mem_pages = &pages->mem_pages[page_idx];
		if (cacheable) {
			mem_pages->page_v_addr_start = kzalloc(pages->page_size,
							       GFP_ATOMIC);
		} else {
			mem_pages->page_v_addr_start =
				dma_alloc_coherent(ab->dev,
						   pages->page_size,
						   &mem_pages->page_p_addr,
						   GFP_KERNEL);
		}
		if (!mem_pages->page_v_addr_start) {
			ath12k_warn(ab, "page alloc fail for idx %d\n", page_idx);
			goto page_alloc_fail;
		}
		mem_pages->page_v_addr_end = mem_pages->page_v_addr_start +
					     pages->page_size;
	}
	return;
page_alloc_fail:
	for (i = 0; i < page_idx; i++) {
		mem_pages = &pages->mem_pages[i];
		if (cacheable) {
			kfree(mem_pages->page_v_addr_start);
		} else {
			dma_free_coherent(ab->dev, pages->page_size,
					  mem_pages->page_v_addr_start,
					  mem_pages->page_p_addr);
		}
	}
	kfree(pages->mem_pages_unaligned);
out_fail:
	pages->mem_pages_unaligned = NULL;
	pages->mem_pages = NULL;
	pages->num_pages = 0;
}

void *alloc_memory_pool(struct pool_ctxt_t *ctxt)
{
	struct node_t *node = NULL;
	void *block = NULL;

	if (!ctxt)
		return NULL;

	node = STAILQ_FIRST(&ctxt->node_list);
	if (node) {
		STAILQ_REMOVE_HEAD(&ctxt->node_list, node_entry);
		block = node->element;
		memset(block, 0, ctxt->elem_sz);

		node->element = NULL;
		STAILQ_INSERT_TAIL(&ctxt->free_node_list, node, node_entry);

		ctxt->total_num_allocs++;
		ctxt->num_free_nodes--;
	}

	return block;
}

void free_memory_pool(struct pool_ctxt_t *ctxt, void *element)
{
	struct node_t *node = NULL;

	if (!ctxt || !element)
		return;

	node = STAILQ_FIRST(&ctxt->free_node_list);
	if (!node)
		return;

	STAILQ_REMOVE_HEAD(&ctxt->free_node_list, node_entry);

	node->element = element;
	STAILQ_INSERT_TAIL(&ctxt->node_list, node, node_entry);
	ctxt->num_free_nodes++;
}

void mem_multi_pages_free(struct ath12k_base *ab, struct pool_ctxt_t *ctxt)
{
	u16 page_idx;
	struct mem_page_t *mem_pages;
	struct mem_multi_page_t *pages = &ctxt->pages;

	if (!pages->page_size)
		pages->page_size = ATH12K_PAGE_SIZE;

	mem_pages = pages->mem_pages;
	for (page_idx = 0; page_idx < pages->num_pages; page_idx++) {
		if (ctxt->is_cacheable_memory) {
			kfree(mem_pages->page_v_addr_start);
		} else {
			dma_free_coherent(ab->dev, pages->page_size,
					  mem_pages->page_v_addr_start,
					  mem_pages->page_p_addr);
		}
		mem_pages++;
	}
	kfree(pages->mem_pages_unaligned);

	pages->mem_pages = NULL;
	pages->mem_pages_unaligned = NULL;
	pages->num_pages = 0;
}

void pool_destroy(struct ath12k_base *ab, struct pool_ctxt_t *ctxt)
{
	struct node_t *node = NULL;
	struct node_t *tmp_node = NULL;

	if (!ctxt)
		return;

	if (ctxt->num_of_elem != ctxt->num_free_nodes) {
		ath12k_warn(ab, "Error: leak detected num_of_elem %d, free_nodes %d\n",
			    ctxt->num_of_elem, ctxt->num_free_nodes);
	}

	STAILQ_FOREACH_SAFE(node, &ctxt->node_list, node_entry, tmp_node) {
		STAILQ_REMOVE(&ctxt->node_list, node, node_t, node_entry);
	}

	STAILQ_FOREACH_SAFE(node, &ctxt->free_node_list, node_entry, tmp_node) {
		STAILQ_REMOVE(&ctxt->free_node_list, node, node_t, node_entry);
	}

	if (ctxt->pages.num_pages)
		mem_multi_pages_free(ab, ctxt);

	kfree(ctxt->node_pool);

	ctxt->mem_start = NULL;
	ctxt->start_node = NULL;
	ctxt->end_node = NULL;
	kfree(ctxt->unaligned_ptr);
}

struct pool_ctxt_t *init_memory_pool(struct ath12k_base *ab, u16 elem_size,
				     u16 num_of_elem, bool cacheable)
{
	struct pool_ctxt_t *ctxt = NULL;
	void *unaligned_ctxt;
	void *block, *page_info;
	void *current_elem = NULL;
	u16 i, j;
	u16 num_elem = 0;
	int size = sizeof(struct pool_ctxt_t) + ATH12K_DP_POOL_CTXT_ALIGN - 1;
	struct node_t *node = NULL;
	struct node_t *tmp_node = NULL;

#ifdef CONFIG_IO_COHERENCY
	cacheable = true;
#endif

	unaligned_ctxt = kzalloc(size, GFP_ATOMIC);

	if (unaligned_ctxt) {
		ctxt = (struct pool_ctxt_t *)PTR_ALIGN(unaligned_ctxt,
						       ATH12K_DP_POOL_CTXT_ALIGN);
		memset(ctxt, 0, sizeof(*ctxt));
		ctxt->unaligned_ptr = unaligned_ctxt;
	} else {
		goto error;
	}
	STAILQ_INIT(&ctxt->free_node_list);
	STAILQ_INIT(&ctxt->node_list);
	ctxt->elem_sz = elem_size;
	ctxt->num_of_elem = num_of_elem;
	ctxt->is_cacheable_memory = cacheable;

	ctxt->node_pool = kzalloc((sizeof(struct node_t) * num_of_elem), GFP_ATOMIC);
	if (!ctxt->node_pool)
		goto error1;

	for (i = 0; i < num_of_elem; i++) {
		ctxt->node_pool[i].element = NULL;
		STAILQ_INSERT_TAIL(&ctxt->free_node_list, &ctxt->node_pool[i],
				   node_entry);
	}

	mem_multi_page_alloc(ab, &ctxt->pages, elem_size, num_of_elem, cacheable);

	if (!ctxt->pages.num_pages)
		goto error2;

	block = (void *)ctxt->pages.mem_pages[0].page_v_addr_start;

	ctxt->mem_start  = block;
	ctxt->start_node = block;

	for (i = 0; i < ctxt->pages.num_pages; i++) {
		page_info = ctxt->pages.mem_pages[i].page_v_addr_start;
		if (!page_info)
			goto error3;
		for (j = 0; j < ctxt->pages.num_element_per_page; j++) {
			if (num_elem >= num_of_elem)
				break;
			current_elem = (void *)((char *)page_info + (j * elem_size));
			free_memory_pool(ctxt, current_elem);
			num_elem++;
		}
	}

	ctxt->end_node = (num_elem > 0) ? current_elem : NULL;
	return ctxt;

error3:
	STAILQ_FOREACH_SAFE(node, &ctxt->node_list, node_entry, tmp_node) {
		STAILQ_REMOVE(&ctxt->node_list, node, node_t, node_entry);
	}

	STAILQ_FOREACH_SAFE(node, &ctxt->free_node_list, node_entry, tmp_node) {
		STAILQ_REMOVE(&ctxt->free_node_list, node, node_t, node_entry);
	}

	mem_multi_pages_free(ab, ctxt);
error2:
	kfree(ctxt->node_pool);
error1:
	kfree(ctxt->unaligned_ptr);
error:
	return NULL;
}

u16 pool_node_to_id(struct pool_ctxt_t *ctxt, void *node)
{
	void *current_elem;
	void *page_info;
	u16 i, j;
	u16 num_elem = 0;

	for (i = 0; i < ctxt->pages.num_pages; i++) {
		page_info = ctxt->pages.mem_pages[i].page_v_addr_start;
		if (!page_info)
			return ATH12K_DP_POOL_INDEX_INVALID;
		for (j = 0; j < ctxt->pages.num_element_per_page; j++) {
			if (num_elem >= ctxt->num_of_elem)
				break;

			current_elem = (void *)((char *)page_info + (j * ctxt->elem_sz));
			if (current_elem == node)
				return num_elem;
			num_elem++;
		}
	}

	return ATH12K_DP_POOL_INDEX_INVALID;
}

void *pool_node_from_id(struct pool_ctxt_t *ctxt, u16 node_id)
{
	int page_idx, elem_idx;
	struct mem_page_t *page_info;

	if (!ctxt || !ctxt->pages.mem_pages)
		return NULL;

	if (node_id >= (ctxt->pages.num_element_per_page * ctxt->pages.num_pages))
		return NULL;

	page_idx = node_id / ctxt->pages.num_element_per_page;
	elem_idx = node_id % ctxt->pages.num_element_per_page;

	page_info = &ctxt->pages.mem_pages[page_idx];
	return page_info->page_v_addr_start + (elem_idx * ctxt->elem_sz);
}

void pool_addr_from_id(struct pool_ctxt_t *ctxt, u16 node_id,
		       void **v_addr, dma_addr_t *p_addr)
{
	struct mem_page_t *page_info;
	int page_idx, elem_idx;

	if (node_id >= (ctxt->pages.num_element_per_page * ctxt->pages.num_pages))
		return;

	page_idx = node_id / ctxt->pages.num_element_per_page;
	elem_idx = node_id % ctxt->pages.num_element_per_page;

	page_info = &ctxt->pages.mem_pages[page_idx];
	*v_addr = page_info->page_v_addr_start + elem_idx * ctxt->elem_sz;
	*p_addr = page_info->page_p_addr + elem_idx * ctxt->elem_sz;
}

int dma_map_pages(struct ath12k_base *ab, struct pool_ctxt_t *ctxt)
{
	int i, j;
	dma_addr_t paddr;
	struct mem_page_t *mem_pages;

	if (ctxt->is_cacheable_memory) {
		mem_pages = &ctxt->pages.mem_pages[0];
		for (i = 0; i < ctxt->pages.num_pages; i++) {
			paddr = ath12k_core_dma_map_single(ab->dev,
							   mem_pages->page_v_addr_start,
							   ctxt->pages.page_size,
							   DMA_BIDIRECTIONAL);
			if (!paddr) {
				ath12k_err(ab, "failed to dma map page %d\n", i);
				/* Unmap previously mapped pages */
				for (j = 0; j < i; j++) {
					ath12k_core_dma_unmap_single(
						ab->dev,
						ctxt->pages.mem_pages[j].page_p_addr,
						ctxt->pages.page_size,
						DMA_BIDIRECTIONAL);
				}
				return -ENOMEM;
			}
			mem_pages->page_p_addr = paddr;
			mem_pages++;
		}
	}
	return 0;
}

void dma_unmap_pages(struct ath12k_base *ab, struct pool_ctxt_t *ctxt)
{
	int i;
	struct mem_page_t *mem_pages;

	if (ctxt->is_cacheable_memory) {
		mem_pages = &ctxt->pages.mem_pages[0];
		for (i = 0; i < ctxt->pages.num_pages; i++) {
			ath12k_core_dma_unmap_single(ab->dev,
						     mem_pages->page_p_addr,
						     ctxt->pages.page_size,
						     DMA_BIDIRECTIONAL);
			mem_pages++;
		}
	}
}
