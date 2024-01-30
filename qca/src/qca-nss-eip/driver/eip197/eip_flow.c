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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/dma-mapping.h>
#include <linux/crypto.h>
#include <linux/cache.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <linux/dmapool.h>
#include <linux/dma-direct.h>
#include <linux/spinlock.h>

#include "eip_priv.h"
#include "eip_flow.h"

/*
 * eip_flow_get_tail
 * 	Obtain the flow at the tail
 */
static struct eip_flow *eip_flow_get_tail(struct hlist_head *flow_head)
{
	struct hlist_node *entry;
	hlist_for_each(entry, flow_head) {
		if (entry->next == NULL) {
			return hlist_entry(entry, struct eip_flow, node);
		}
	}
	return NULL;
}

/*
 * eip_flow_get_next
 * 	Obtain the next flow
 */
static struct eip_flow *eip_flow_get_next(struct eip_flow *flow)
{
	return hlist_entry(flow->node.next, struct eip_flow, node);

}

/*
 * eip_flow_get_prev
 * 	Obtain the previous flow
 */
static struct eip_flow *eip_flow_get_prev(struct eip_flow *flow)
{
	return hlist_entry(flow->node.pprev, struct eip_flow, node.next);
}

/*
 * eip_flow_get_hash()
 * 	Calculate hash for flow tuple
 */
static eip_flow_hash_t eip_flow_get_hash(struct eip_flow_tuple *flow)
{
	struct eip_pdev *ep = platform_get_drvdata(eip_drv_g.pdev);
	uint32_t data[EIP_FLOW_HASH_DATA_SZ];
	eip_flow_hash_t hash;
	void __iomem *base_addr = ep->dev_vaddr;
	int i;

	memset(data, 0, sizeof(data));

	data[0] = 0;
	data[1] = flow->ip_proto << 8;

	if (flow->ip_ver == 6) {
		data[1] |= (1 << 25);
	}

	/*
	 * TODO: Write the port field for inner flow.
	 */
	data[2] = ntohl(flow->spi);
	data[3] = flow->epoch;
	data[4] = 0;

	if (flow->ip_ver == IPVERSION) {
		data[5] = flow->dst_ip[0];
		data[6] = data[7] = data[8] = 0;
		data[9] = flow->src_ip[0];
		data[10] = data[11] = data[12] = 0;
	} else if (flow->ip_ver == 6) {
		data[5] = flow->dst_ip[0];
		data[6] = flow->dst_ip[1];
		data[7] = flow->dst_ip[2];
		data[8] = flow->dst_ip[3];
		data[9] = flow->src_ip[0];
		data[10] = flow->src_ip[1];
		data[11] = flow->src_ip[2];
		data[12] = flow->src_ip[3];
	}

	hash.words[0] = ioread32(base_addr + EIP_HW_FHASH_IV0);
	hash.words[1] = ioread32(base_addr + EIP_HW_FHASH_IV1);
	hash.words[2] = ioread32(base_addr + EIP_HW_FHASH_IV2);
	hash.words[3] = ioread32(base_addr + EIP_HW_FHASH_IV3);

	/*
	 * Algorithm referred from Security-IP-197_HW3.3_Programmer-Manual_RevC
	 */
	for (i = 0; i < EIP_FLOW_HASH_DATA_SZ; i++) {
		uint32_t *hash_words = hash.words;

		hash_words[0] += data[i];
		hash_words[0] += (hash_words[0] << 10);
		hash_words[0] ^= (hash_words[0] >> 6);
		hash_words[1+(i % 3)] ^= data[i];

		if (((i % 3)==2) && (i<(EIP_FLOW_HASH_DATA_SZ - 1))) {
			hash_words[1] -= hash_words[2]; hash_words[1] -= hash_words[3]; hash_words[1] ^= (hash_words[3] >> 13);
			hash_words[2] -= hash_words[3]; hash_words[2] -= hash_words[1]; hash_words[2] ^= (hash_words[1] << 8);
			hash_words[3] -= hash_words[1]; hash_words[3] -= hash_words[2]; hash_words[3] ^= (hash_words[2] >> 13);
			hash_words[1] -= hash_words[2]; hash_words[1] -= hash_words[3]; hash_words[1] ^= (hash_words[3] >> 12);
			hash_words[2] -= hash_words[3]; hash_words[2] -= hash_words[1]; hash_words[2] ^= (hash_words[1] << 16);
			hash_words[3] -= hash_words[1]; hash_words[3] -= hash_words[2]; hash_words[3] ^= (hash_words[2] >> 5);
			hash_words[1] -= hash_words[2]; hash_words[1] -= hash_words[3]; hash_words[1] ^= (hash_words[3] >> 3);
			hash_words[2] -= hash_words[3]; hash_words[2] -= hash_words[1]; hash_words[2] ^= (hash_words[1] << 10);
			hash_words[3] -= hash_words[1]; hash_words[3] -= hash_words[2]; hash_words[3] ^= (hash_words[2] >> 15);
		}
	}
	return hash;
}

/*
 * _eip_flow_lookup()
 *      Lookup the Flow in table using the provided hash.
 */
static struct eip_flow *_eip_flow_lookup(struct eip_flow_tbl *flow_table, eip_flow_hash_t hash)
{
	struct hlist_head *flow_head;
	struct hlist_node *entry;
	struct eip_flow *flow;
	uint32_t idx;

	assert_spin_locked(&flow_table->lock);

	idx = EIP_FLOW_HASH_IDX(hash);
	flow_head = &flow_table->sw_head[idx];

	if (hlist_empty(flow_head)) {
		return NULL;
	}

	hlist_for_each(entry, flow_head) {
		flow = hlist_entry (entry, struct eip_flow, node);
		if (EIP_HASH_EQUAL(flow->hash,hash))
			return flow;
	}

	return NULL;
}

/*
 * eip_flow_alloc
 * 	Allocate flow
 */
static struct eip_flow *eip_flow_alloc(struct eip_flow_tuple *flow_tuple, struct eip_tr *tr)
{
	struct eip_pdev *ep = platform_get_drvdata(eip_drv_g.pdev);
	struct hlist_head *flow_head = NULL;
	struct eip_flow_hw *hflow = NULL;
	struct eip_flow_tbl *tbl = NULL;
	struct eip_flow *flow = NULL;
	eip_flow_hash_t hash;
	uint32_t idx;
	int i;

	hash = eip_flow_get_hash(flow_tuple);
	idx = EIP_FLOW_HASH_IDX(hash);
	tbl = &ep->flow_table;
	flow_head = &tbl->sw_head[idx];

	assert_spin_locked(&tbl->lock);
	if (_eip_flow_lookup(tbl, hash) != NULL) {
		pr_warn("%px: Flow already exists \n", tr);
		return NULL;
	}

	/*
	 * Allocate the software flow.
	 */
	flow = kmem_cache_alloc(ep->flow_swcache, GFP_ATOMIC);
	if (!flow) {
		return NULL;
	}

	memset(flow, 0, sizeof(struct eip_flow));
	INIT_HLIST_NODE(&flow->node);
	flow->hash = hash;
	flow->tuple = *flow_tuple;

	/*
	 * Check if hardware flow is available in the hash table
	 */
	if(hlist_empty(flow_head)) {
		hflow = tbl->hw_head + idx;
		memset(hflow, 0, sizeof(struct eip_flow_hw));
		hflow->tr_addr_type_1 = tr->tr_addr_type;
		hflow->next_flow_offset = 0;
		hflow->hashid_1 = hash;


		flow->hflow = hflow ;
		flow->hflow_paddr = tbl->hw_head_paddr + (idx * sizeof(*hflow));
		flow->sentinel = true;
		pr_debug("%px: Hardware flow allocated %pad\n", tr, &flow->hflow_paddr);

		return flow;
	}

	/*
	 * Look up the available hardware flow memory from the collision pool.
	 */
	for (i = 0; i < EIP_FLOW_MAX_COLLISION; i++) {
		hflow = ep->flow_hwcache + i;
		if (hflow->tr_addr_type_1 == 0) {
			hflow->tr_addr_type_1 = tr->tr_addr_type;
			hflow->next_flow_offset = 0;
			hflow->hashid_1 = hash;

			flow->hflow = hflow;
			flow->hflow_paddr = ep->hwcache_paddr + (i * sizeof(*hflow));
			pr_debug("%px: Hardware flow allocated from collision memory %pad\n", tr, &flow->hflow_paddr);

			return flow;
		}
	}

	kmem_cache_free(ep->flow_swcache, flow);
	return NULL;
}

/*
 * eip_flow_free()
 *      Reset and free the flow
 */
static void eip_flow_free(struct eip_flow *flow, struct eip_pdev *ep)
{
	hlist_del_init(&flow->node);

	flow->hflow->tr_addr_type_1 = EIP_FLOW_TR_DISABLE;
	memset(flow->hflow, 0, sizeof(*(flow->hflow)));
	memset(flow, 0, sizeof(struct eip_flow));
	kmem_cache_free(ep->flow_swcache, flow);
}

/*
 * eip_flow_swap_hflow()
 *      copy and swap hflow memory for next_flow->hflow to flow->hflow.
 */
static void eip_flow_swap_hflow(struct eip_flow *flow, struct eip_flow *next_flow)
{
	flow->hflow->tr_addr_type_1 = EIP_FLOW_TR_DISABLE;
	flow->hflow->hashid_1 = next_flow->hflow->hashid_1;
	flow->hflow->tr_addr_type_1 = next_flow->hflow->tr_addr_type_1;
	flow->hflow->next_flow_offset = next_flow->hflow->next_flow_offset;

	flow->hflow = xchg(&next_flow->hflow, flow->hflow);
	flow->hflow_paddr = xchg(&next_flow->hflow_paddr, flow->hflow_paddr);
	next_flow->sentinel = flow->sentinel;
	flow->sentinel =  false;
}

/*
 * eip_flow_add()
 */
struct eip_flow *eip_flow_add(struct eip_flow_tuple *flow_tuple, struct eip_tr *tr)
{
	struct eip_pdev *ep = platform_get_drvdata(eip_drv_g.pdev);
	struct eip_flow *flow, *tail_flow;
	struct hlist_head *flow_head;
	struct eip_flow_tbl *tbl;
	uint32_t idx;

	tbl = &ep->flow_table;

	spin_lock_bh(&tbl->lock);
	flow = eip_flow_alloc(flow_tuple, tr);
	if (!flow) {
		spin_unlock_bh(&tbl->lock);
		pr_err("%px: Failed to allocate EIP flow\n", tr);
		return NULL;
	}

	idx = EIP_FLOW_HASH_IDX(flow->hash);
	flow_head = &tbl->sw_head[idx];

	/*
	 * Fast addition: Addition of a new node at the begining of hlist
	 */
	if (flow->sentinel) {
		hlist_add_head(&flow->node, flow_head);
		tbl->stats.alloc++;

		spin_unlock_bh(&tbl->lock);
		if (flow_tuple->ip_ver == 6) {
			pr_debug("%px Flow (src:%pI6n dst:%pI6n spi:0x%X) added at the head at index %u\n", flow, flow_tuple->src_ip, flow_tuple->dst_ip, ntohl(flow_tuple->spi), idx);
		} else {
			pr_debug("%px Flow (src:%pI4n dst:%pI4n spi:0x%X) added at the head at index %u\n", flow, flow_tuple->src_ip, flow_tuple->dst_ip, ntohl(flow_tuple->spi), idx);
		}

		return flow;
	}

	/*
	 * Slow addition : Allocate a new hardware flow at tail.
	 * Obtain the tail of the current chain. Here, atleast one node will be already present,hence tail can't be null.
	 */
	tail_flow = eip_flow_get_tail(flow_head);
	BUG_ON(tail_flow == NULL);

	pr_debug("%px: Physical address of hardware head: %pad\n", tr, &tbl->hw_head_paddr);
	tail_flow->hflow->next_flow_offset = (flow->hflow_paddr - tbl->hw_head_paddr) | EIP_FLOW_VALID_BIT;

	hlist_add_behind(&flow->node, &tail_flow->node);
	tbl->stats.alloc++;
	tbl->stats.collision++;
	tbl->stats.active_collision++;

	spin_unlock_bh(&tbl->lock);
	if (flow_tuple->ip_ver == 6) {
		pr_debug("%px Flow (src:%pI6n dst:%pI6n spi:0x%X) added after collision at index %u\n", flow, flow_tuple->src_ip, flow_tuple->dst_ip, ntohl(flow_tuple->spi), idx);
	} else {
		pr_debug("%px Flow (src:%pI4n dst:%pI4n spi:0x%X) added after collision at index %u\n", flow, flow_tuple->src_ip, flow_tuple->dst_ip, ntohl(flow_tuple->spi), idx);
	}

	return flow;
}

/*
 * eip_flow_del()
 * 	Deletion of flow from the hardware table
 */
void eip_flow_del(struct eip_flow_tuple *flow_tuple, struct eip_tr *tr)
{
	struct eip_pdev *ep = platform_get_drvdata(eip_drv_g.pdev);
	struct eip_flow *flow, *prev_flow;
	struct hlist_head *flow_head;
	struct eip_flow_tbl *tbl;
	eip_flow_hash_t hash;
	uint32_t idx;

	hash = eip_flow_get_hash(flow_tuple);
	idx = EIP_FLOW_HASH_IDX(hash);
	tbl = &ep->flow_table;
	flow_head = &tbl->sw_head[idx];

	spin_lock_bh(&tbl->lock);
	flow = _eip_flow_lookup(tbl, hash);
	if (flow == NULL) {
		spin_unlock_bh(&tbl->lock);
		pr_err("%px: Flow doesn't exist \n", tr);
		return;
	}

	/*
	 * This the only flow in the chain, Reset & free the flow.
	 */
	if (hlist_is_singular_node(&flow->node, flow_head)) {
		ASSERT(flow->sentinel);
		pr_debug("%px Flow (src:%pI4n dst:%pI4n spi:0x%X) deleted at head at index %u\n",
				flow, flow_tuple->src_ip, flow_tuple->dst_ip, ntohl(flow_tuple->spi), idx);
		goto free;
	}

	/*
	 * There are more flows in the chain, But we are deleting the head flow.
	 * Move the second hardware flow to the head. Then, reset & free the flow.
	 */
	if (flow->sentinel) {
		struct eip_flow *next_flow;

		next_flow = eip_flow_get_next(flow);
		eip_flow_swap_hflow(flow, next_flow);
		pr_debug("%px Flow (src:%pI4n dst:%pI4n spi:0x%X) deleted at head (with collision) at index %u\n",
				flow, flow_tuple->src_ip, flow_tuple->dst_ip, ntohl(flow_tuple->spi), idx);
		tbl->stats.active_collision--;
		goto free;
	}

	/*
	 * We are deleting the non-sentinel (not at the Head) flow.
	 * Make previous hflow to point the next flow (or NULL). Then reset and free the flow.
	 */
	prev_flow = eip_flow_get_prev(flow);
	prev_flow->hflow->next_flow_offset = flow->hflow->next_flow_offset;
	tbl->stats.active_collision--;
	pr_debug("%px Flow (src:%pI4n dst:%pI4n spi:0x%X) deleted (after collision) at index %u\n",
			flow, flow_tuple->src_ip, flow_tuple->dst_ip, ntohl(flow_tuple->spi), idx);

free:
	tbl->stats.free++;
	eip_flow_free(flow, ep);
	spin_unlock_bh(&tbl->lock);
}

/*
 * eip_flow_table_init()
 * 	Allocate flow entries for hardware table
 */
bool eip_flow_table_init(struct platform_device *pdev)
{
	struct eip_pdev *ep = platform_get_drvdata(pdev);
	void __iomem *base_addr = ep->dev_vaddr;
	struct eip_flow_tbl *tbl;
	size_t tbl_sz, collision_sz;
	dma_addr_t paddr;
	void *addr;

	int i;

	ep->flow_swcache = kmem_cache_create("eip_sw_flow", sizeof(struct eip_flow), 0, 0, NULL);
	if (!ep->flow_swcache) {
		pr_err("%px: Failed to allocate flow swcache\n",pdev);
		return false;
	}

	tbl = &ep->flow_table;
	tbl_sz = EIP_FLOW_MAX * sizeof(struct eip_flow_hw);
	collision_sz = EIP_FLOW_MAX_COLLISION * sizeof(struct eip_flow_hw);

	/*
	 * Allocate contiguous memory for hash table and collision list
	 */
	addr = dma_alloc_coherent(&pdev->dev, (tbl_sz + collision_sz), &paddr, GFP_DMA);
	if (!addr) {
		pr_err("%px: Failed to allocate entries\n",pdev);
		goto fail_dma;
	}

	memset(addr, 0, (tbl_sz + collision_sz));
	tbl->hw_head = addr;
	tbl->hw_head_paddr = paddr;
	ep->flow_hwcache = addr + tbl_sz;
	ep->hwcache_paddr = paddr + tbl_sz;

	pr_debug("%px: DMA Address %px\n", pdev, tbl->hw_head);
	pr_debug("%px: DMA Physical address %pad\n", pdev, &tbl->hw_head_paddr);

	iowrite32(EIP_HW_FLUE_CONFIG_VAL, base_addr + EIP_HW_FLUE_CONFIG(0));
	iowrite32(0x0, base_addr + EIP_HW_FLUE_CACHEBASE_LO(0));
	iowrite32(0x0, base_addr + EIP_HW_FLUE_CACHEBASE_HI(0));
	iowrite32(tbl->hw_head_paddr, base_addr + EIP_HW_FLUE_HASHBASE_LO(0));
	iowrite32(0x0, base_addr + EIP_HW_FLUE_HASHBASE_HI(0));

	for (i = 0; i < EIP_FLOW_MAX; i++){
		INIT_HLIST_HEAD(&tbl->sw_head[i]);
	}

	return true;

fail_dma:
	kmem_cache_destroy(ep->flow_swcache);
	return false;
}

/*
 * eip_flow_table_deinit()
 * 	Free the flow entries
 */
void eip_flow_table_deinit(struct platform_device *pdev)
{
	struct eip_pdev *ep = platform_get_drvdata(pdev);
	void __iomem *base_addr = ep->dev_vaddr;
	struct eip_flow_tbl *tbl;

	tbl = &ep->flow_table;
	iowrite32(0x0, base_addr + EIP_HW_FLUE_HASHBASE_LO(0));
	kmem_cache_destroy(ep->flow_swcache);
	dma_free_coherent(&pdev->dev, EIP_FLOW_MAX * sizeof(struct eip_flow_hw), tbl->hw_head, tbl->hw_head_paddr);
}

