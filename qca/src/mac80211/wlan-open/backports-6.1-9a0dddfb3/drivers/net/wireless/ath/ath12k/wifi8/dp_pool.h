/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_POOL_H
#define ATH12K_DP_POOL_H

#include <linux/mm.h>
#include "../stailq.h"
#include "../core.h"
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/gfp.h>

#define ATH12K_DP_POOL_CTXT_ALIGN	8
#define ATH12K_DP_POOL_INDEX_INVALID	0xFFFF

struct mem_page_t {
	dma_addr_t page_p_addr;
	char *page_v_addr_start;
	char *page_v_addr_end;
};

struct mem_multi_page_t {
	struct mem_page_t *mem_pages_unaligned;
	struct mem_page_t *mem_pages;
	size_t page_size;
	u16 num_element_per_page;
	u16 num_pages;
};

struct node_t {
	STAILQ_ENTRY(node_t) node_entry;
	void *element;
};

struct pool_ctxt_t {
	struct mem_multi_page_t pages;
	/* Free node list */
	STAILQ_HEAD(, node_t) free_node_list;

	STAILQ_HEAD(, node_t) node_list;

	struct node_t *node_pool;
	void    *unaligned_ptr;
	void    *mem_start;
	void    *start_node;
	void    *end_node;
	int     elem_sz;
	int     num_of_elem;
	int     num_free_nodes;
	int     total_num_allocs;
	bool    is_cacheable_memory;
};

void mem_multi_page_alloc(struct ath12k_base *ab, struct mem_multi_page_t *pages,
			  u16 element_size, u16 element_num, bool cacheable);
void free_memory_pool(struct pool_ctxt_t *ctxt, void *element);
// Important: pool_destroy() does NOT nullify caller's pointer.
// Caller must set pointer to NULL after cleanup.
void *alloc_memory_pool(struct pool_ctxt_t *ctxt);
void mem_multi_pages_free(struct ath12k_base *ab, struct pool_ctxt_t *ctxt);
void pool_destroy(struct ath12k_base *ab, struct pool_ctxt_t *ctxt);
struct pool_ctxt_t *init_memory_pool(struct ath12k_base *ab, u16 elem_size,
				     u16 num_of_elem, bool cacheable);
u16 pool_node_to_id(struct pool_ctxt_t *ctxt, void *node);
void *pool_node_from_id(struct pool_ctxt_t *ctxt, u16 node_id);
void pool_addr_from_id(struct pool_ctxt_t *ctxt, u16 node_id, void **v_addr,
		       dma_addr_t *p_addr);
int dma_map_pages(struct ath12k_base *ab, struct pool_ctxt_t *ctxt);
void dma_unmap_pages(struct ath12k_base *ab, struct pool_ctxt_t *ctxt);
#endif
