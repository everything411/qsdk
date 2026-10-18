// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include "debug.h"

#include "me.h"

/*
 * Convert __TRIE_STATUS to Linux returns
 */
static inline int __trie_status_to_errno(enum __trie_status status)
{
	switch (status) {
	case __TRIE_SUCCESS:
		return 0;
	case __TRIE_FAIL_NOMEM:
		return -ENOMEM;
	case __TRIE_FAIL_INVAL:
		return -EINVAL;
	default:
		return -EINVAL;
	}
}

/*
 * Extract nibble (4 bits) from address for the given level
 */
static u32 __trie_extract_nibble(u32 *addr, int num_bits)
{
#define BITS_PER_WORD (BITS_PER_BYTE * sizeof(u32))

	u32 word_idx = num_bits / BITS_PER_WORD;
	u32 shift = BITS_PER_WORD - (num_bits % BITS_PER_WORD) - __TRIE_STRIDE;

	return ((addr[word_idx] >> shift) % __TRIE_BITS);
}

/*
 * Allocate a new trie node
 */
static inline struct __trie_node *__trie_node_alloc(void)
{
	struct __trie_node *node;
	int i;

	node = kzalloc(sizeof(*node), GFP_ATOMIC);
	if (!node)
		return NULL;

	/* Initialize all children to NULL */
	for (i = 0; i < __TRIE_BITS; i++)
		RCU_INIT_POINTER(node->child[i], NULL);

	node->value = U32_MAX;
	bitmap_zero(node->bmap, __TRIE_BITS);
	set_bit(__TRIE_NODE_INIT, node->state);

	return node;
}

/*
 * Free a trie node and all its children recursively
 * Uses bitmap to iterate only over valid children
 */
static void __trie_node_free_recursive(struct __trie_node *node)
{
	u8 bit;

	if (!node)
		return;

	/* Free all children first; iterate only over bits set in bitmap */
	for_each_set_bit(bit, node->bmap, __TRIE_BITS) {
		struct __trie_node *child = rcu_dereference_protected(node->child[bit],
								      1);

		if (child)
			__trie_node_free_recursive(child);
	}

	kfree_rcu(node, rcu);
}

/*
 * Free a specific node bit, drop the node if there are no
 * child
 */
static int __trie_node_free(struct __trie_node *node)
{
	if (test_bit(__TRIE_NODE_TERM, node->state))
		clear_bit(__TRIE_NODE_TERM, node->state);

	/*
	 * Check & return failure if there are active children
	 */
	if (!bitmap_empty(node->bmap, __TRIE_BITS))
		return __TRIE_FAIL_INVAL;

	/* Free the node only if it's a terminal node or has no children */
	kfree_rcu(node, rcu);
	return __TRIE_SUCCESS;
}

/*
 * Free the child node
 */
static int __trie_node_free_child(struct __trie_node *node, u32 bit)
{
	clear_bit(bit, node->bmap);

	rcu_assign_pointer(node->child[bit], NULL);

	/*
	 * Don't free the node if its a terminal node;
	 * Note: __trie_node_free() shd have freed the terminating node
	 */
	if (test_bit(__TRIE_NODE_TERM, node->state))
		return __TRIE_FAIL_INVAL;

	/*
	 * Don't free the node if there are children available
	 */
	if (!bitmap_empty(node->bmap, __TRIE_BITS))
		return __TRIE_FAIL_INVAL;

	kfree_rcu(node, rcu);
	return __TRIE_SUCCESS;
}

/*
 * Allocate the TRIE node recursively
 */
static int __trie_node_add(struct __trie_node **node, u32 *addr,
			   u16 max_lvl, u16 lvl, u32 val)
{
	struct __trie_node *cur;
	enum __trie_status err;
	u16 bit;

	cur = rcu_dereference_protected(*node, 1);
	if (!cur) {
		cur = __trie_node_alloc();
		if (!cur)
			return __TRIE_FAIL_NOMEM;

		rcu_assign_pointer(*node, cur);
	}

	if (lvl == max_lvl) {
		cur->value = val;
		set_bit(__TRIE_NODE_TERM, cur->state);
		return __TRIE_SUCCESS;
	}

	bit = __trie_extract_nibble(addr, lvl * __TRIE_STRIDE);
	set_bit(bit, cur->bmap);
	set_bit(__TRIE_NODE_INTR, cur->state);

	/*
	 * Recursively free the children if the add operation fails.
	 */
	err = __trie_node_add(&cur->child[bit], addr, max_lvl, ++lvl, val);
	if (err != __TRIE_SUCCESS) {
		if (__trie_node_free_child(cur, bit) == __TRIE_SUCCESS)
			rcu_assign_pointer(*node, NULL);
	}

	return err;
}

/*
 * Clear the state for the node.
 * Recursively delete the entries when relevant by backtracking.
 */
static int __trie_node_del(struct __trie_node **node, u32 *addr, u16 max_lvl, u16 lvl)
{
	struct __trie_node *cur;
	enum __trie_status ret;
	u16 bit;

	/*
	 * Check for node validity.
	 */
	cur = rcu_dereference_protected(*node, 1);
	if (!cur)
		return __TRIE_SUCCESS;

	/*
	 * When invoking ensure that max_level resets the node.
	 */
	if (lvl == max_lvl)
		return __trie_node_free(cur);

	/*
	 * Extract nibble and convert it to bit position and
	 * check if we need to inspect further down
	 */
	bit = __trie_extract_nibble(addr, lvl * __TRIE_STRIDE);

	/*
	 * Traverse to the terminal node for the given prefix
	 * to delete it from the hierarchy
	 */
	ret = __trie_node_del(&cur->child[bit], addr, max_lvl, ++lvl);

	/*
	 * If terminal node delete succeeds, try freeing up base.
	 * Note: If there are more children, the node is not deleted.
	 */
	if (ret != __TRIE_SUCCESS)
		return ret;

	ret = __trie_node_free_child(cur, bit);
	if (ret != __TRIE_SUCCESS)
		return ret;

	rcu_assign_pointer(*node, NULL);
	return ret;
}

/*
 * Internal recursive lookup function with state tracking
 * Returns the longest matching prefix value and tracks if any terminal node was found
 */
static int __trie_node_lookup(struct __trie_node *node, u32 *addr, u16 lvl, int *val)
{
	struct __trie_node *child;
	enum __trie_status ret;
	u16 bit;

	if (!node)
		return __TRIE_FAIL_INVAL;

	bit = __trie_extract_nibble(addr, lvl * __TRIE_STRIDE);

	/* Safely dereference child node under RCU protection */
	child = rcu_dereference(node->child[bit]);
	ret = __trie_node_lookup(child, addr, ++lvl, val);

	/*
	 * Only use current node's value if child lookup failed
	 * and current node is terminal
	 */
	if (ret && test_bit(__TRIE_NODE_TERM, node->state)) {
		*val = node->value;
		return __TRIE_SUCCESS;
	}

	/* Return child's result (success or failure) without overwriting val */
	return ret;
}

/*
 * Initialize HMMC/Deny list database
 */
int ath12k_me_hmmc_list_init(struct ath12k_me_db *me_db)
{
	struct ath12k_me_hmmc_list *db;

	if (!me_db)
		return -EINVAL;

	db = &me_db->hmmc_db;
	memset(db, 0, sizeof(*db));
	return 0;
}
EXPORT_SYMBOL(ath12k_me_hmmc_list_init);

/*
 * Flush the entries from HMMC/Deny list database
 */
void ath12k_me_hmmc_list_flush(struct ath12k_me_db *me_db)
{
	struct ath12k_me_hmmc_list *db;

	if (!me_db)
		return;

	db = &me_db->hmmc_db;

	if (db->v4.root) {
		__trie_node_free_recursive(rcu_dereference_protected(db->v4.root, 1));
		rcu_assign_pointer(db->v4.root, NULL);
	}

	if (db->v6.root) {
		__trie_node_free_recursive(rcu_dereference_protected(db->v6.root, 1));
		rcu_assign_pointer(db->v6.root, NULL);
	}
}
EXPORT_SYMBOL(ath12k_me_hmmc_list_flush);

/*
 * Reset the HMMC/Deny list database
 */
void ath12k_me_hmmc_list_reset(struct ath12k_me_db *me_db)
{
	struct ath12k_me_hmmc_list *db;

	if (!me_db)
		return;

	db = &me_db->hmmc_db;
	memset(db, 0, sizeof(*db));
}
EXPORT_SYMBOL(ath12k_me_hmmc_list_reset);

/*
 * Add HMMC/DENY entry to database
 */
int ath12k_me_hmmc_add(struct ath12k_me_db *me_db, u32 *addr, bool v6, u16 pfx, u32 val)
{
	struct __trie_db *trie_db;
	u32 ip_addr[4];
	u16 max_lvl;
	int ret;

	if (!me_db || !pfx || !addr)
		return -EINVAL;

	/*
	 * By default assume IPv4 as the input
	 */
	spin_lock_bh(&me_db->lock);
	trie_db = &me_db->hmmc_db.v4;
	max_lvl = pfx / __TRIE_STRIDE;
	ip_addr[0] = ntohl(addr[0]);

	if (v6) {
		trie_db = &me_db->hmmc_db.v6;
		ip_addr[1] = ntohl(addr[1]);
		ip_addr[2] = ntohl(addr[2]);
		ip_addr[3] = ntohl(addr[3]);
	}

	ret = __trie_node_add(&trie_db->root, ip_addr, max_lvl, 0, val);
	spin_unlock_bh(&me_db->lock);

	if (ret != __TRIE_SUCCESS)
		ath12k_err(NULL, "Failed to perform HMMC add, ret:%d\n", ret);

	/* Synchronize RCU after modifying the trie structure */
	synchronize_rcu();

	return __trie_status_to_errno(ret);
}
EXPORT_SYMBOL(ath12k_me_hmmc_add);

/*
 * Delete HMMC/DENY entry from database
 */
int ath12k_me_hmmc_del(struct ath12k_me_db *me_db, u32 *addr, bool v6, u16 pfx)
{
	struct __trie_db *trie_db;
	struct __trie_node *root;
	u32 ip_addr[4];
	u16 max_lvl;
	int ret;

	if (!me_db || !pfx || !addr)
		return -EINVAL;

	/*
	 * By default assume IPv4 as the input
	 */
	spin_lock_bh(&me_db->lock);
	trie_db = &me_db->hmmc_db.v4;
	max_lvl = pfx / __TRIE_STRIDE;
	ip_addr[0] = ntohl(addr[0]);

	if (v6) {
		trie_db = &me_db->hmmc_db.v6;
		ip_addr[1] = ntohl(addr[1]);
		ip_addr[2] = ntohl(addr[2]);
		ip_addr[3] = ntohl(addr[3]);
	}

	/* Check if root exists before attempting deletion */
	root = rcu_dereference_protected(trie_db->root, 1);
	if (!root) {
		spin_unlock_bh(&me_db->lock);
		return 0; /* Nothing to delete */
	}

	/*
	 * Following can return a non-zero ret even though
	 * the terminal node is cleared because of INTR nodes
	 */
	ret = __trie_node_del(&trie_db->root, ip_addr, max_lvl, 0);
	spin_unlock_bh(&me_db->lock);

	if (ret != __TRIE_SUCCESS) {
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "HMMC DEL action returns:%d, can be valid for INTR nodes\n",
			   ret);
	}

	/* Synchronize RCU after modifying the trie structure */
	synchronize_rcu();

	/*
	 * Return 0 here. For successful del operation,
	 * terminal node would have been cleared anyways.
	 */
	return 0;
}
EXPORT_SYMBOL(ath12k_me_hmmc_del);

/*
 * Lookup entry in database
 * Returns:
 * -EINVAL/-ENOMEM : For error, No entry found
 * ATH12K_ME_HMMC_ACTION : for HMMC entry
 * ATH12K_ME_DENYLIST_ACTION : for DENY entry
 */
int ath12k_me_hmmc_lookup(struct ath12k_me_db *me_db, u32 *addr, bool v6)
{
	struct __trie_db *trie_db;
	struct __trie_node *root;
	u32 ip_addr[4];
	int ret = 0;
	int val = 0;

	WARN_ON_ONCE(!me_db);

	if (!addr)
		return -EINVAL;

	/*
	 * By default assume IPv4 as the input
	 */
	rcu_read_lock_bh();
	trie_db = &me_db->hmmc_db.v4;
	ip_addr[0] = ntohl(addr[0]);

	if (v6) {
		trie_db = &me_db->hmmc_db.v6;
		ip_addr[1] = ntohl(addr[1]);
		ip_addr[2] = ntohl(addr[2]);
		ip_addr[3] = ntohl(addr[3]);
	}

	root = rcu_dereference(trie_db->root);
	if (!root) {
		rcu_read_unlock_bh();
		return -EINVAL;
	}

	ret = __trie_node_lookup(root, ip_addr, 0, &val);
	rcu_read_unlock_bh();

	/* Return the value found by the lookup, or error if lookup failed */
	return (ret == __TRIE_SUCCESS) ? val : __trie_status_to_errno(ret);
}
EXPORT_SYMBOL(ath12k_me_hmmc_lookup);
