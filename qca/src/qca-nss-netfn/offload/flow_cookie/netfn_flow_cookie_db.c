/*
 *******************************************************************************
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *******************************************************************************
 */

/*
 * netfn_flow_cookie_db.c
 *     Flow Cookie Functionality File.
 *     Invokes suitable DB calls into the
 *     DB infra and perform the necessary DB operations.
 */

#include <linux/types.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/log2.h>
#include <net/udp.h>
#include <netfn_types.h>
#include <netfn_flow_cookie.h>
#include "netfn_flow_cookie_hash.h"
#include "netfn_flow_cookie_priv.h"
#include "netfn_flow_cookie_dump.h"

/*
 * netfn_flow_cookie_db_free()
 * 	Destroy the DB instance when all references are dropped
 */
static void netfn_flow_cookie_db_free(struct kref *kref)
{
	struct netfn_flow_cookie_db *db = container_of(kref, struct netfn_flow_cookie_db, ref);

	pr_debug("%p: Freeing the DB after all references are done\n", db);

	/*
	 * Cleanup all associated memory objects allocated during db_alloc
	 */
	if (db->dentry) {
		debugfs_remove_recursive(db->dentry);
		db->dentry = NULL;
	}

	netfn_flow_cookie_stats_free(db->stats);
	netfn_flow_cookie_dump_exit(db);
	kfree(db->bitmap);

	/*
	 * If, there are active nodes then its an anamoly
	 */
	BUG_ON(atomic_read(&db->active_cnt));
	kmem_cache_destroy(db->node_cache);
	kfree(db);
}

/*
 * netfn_flow_cookie_db_ref()
 * 	Increases the references of the netfn_flow_cookie_db instance provided.
 */
struct netfn_flow_cookie_db *netfn_flow_cookie_db_ref(struct netfn_flow_cookie_db *db)
{
	kref_get(&db->ref);
	return db;
}
EXPORT_SYMBOL(netfn_flow_cookie_db_ref);

/*
 * netfn_flow_cookie_db_deref()
 * 	Decreases the references of the netfn_flow_cookie_db instance provided.
 */
bool netfn_flow_cookie_db_deref(struct netfn_flow_cookie_db *db)
{
	if (kref_put(&db->ref, netfn_flow_cookie_db_free)) {
		return true;
	}

	return false;
}
EXPORT_SYMBOL(netfn_flow_cookie_db_deref);

/*
 * netfn_flow_cookie_db_match()
 *	Match the incoming tuple with the DB entry tuple.
 */
static bool netfn_flow_cookie_db_match(struct netfn_flow_cookie_db_entry *entry, struct netfn_tuple *t)
{
	struct netfn_tuple *m = &entry->tuple;
	uint8_t status = 0;

	switch (t->tuple_type) {
	case NETFN_TUPLE_5TUPLE: {

		struct netfn_tuple_5tuple *s1 = &m->tuples.tuple_5;
		struct netfn_tuple_5tuple *d1 = &t->tuples.tuple_5;

		if (t->ip_version == IPVERSION) {
			status += !!(s1->src_ip.ip4.s_addr ^ d1->src_ip.ip4.s_addr);
			status += !!(s1->dest_ip.ip4.s_addr ^ d1->dest_ip.ip4.s_addr);
		} else {
			status += !ipv6_addr_equal(&s1->src_ip.ip6, &d1->src_ip.ip6);
			status += !ipv6_addr_equal(&s1->dest_ip.ip6, &d1->dest_ip.ip6);
		}

		status += !!(s1->protocol ^ d1->protocol);
		status += !!((s1->l4_src_ident) ^ (d1->l4_src_ident));
		status += !!((s1->l4_dest_ident) ^ (d1->l4_dest_ident));
		break;
	}

	default:
		status = 1;
		pr_warn("%p: Unsupported tuple format(%d)\n", t, t->tuple_type);
		break;
	}
	return !status;
}

/*
 * netfn_flow_cookie_db_find_entry()
 *	Find the node entry for a given tuple
 */
static struct netfn_flow_cookie_db_entry *netfn_flow_cookie_db_find_entry(struct netfn_flow_cookie_db *db, struct netfn_tuple *t)
{
	struct netfn_flow_cookie_db_entry *entry;
	uint32_t hash_idx;

	hash_idx = netfn_flow_cookie_db_hash(db, t);

	hlist_for_each_entry_rcu(entry, &db->db_table[hash_idx], node) {
		if (netfn_flow_cookie_db_match(entry, t)) {
			return entry;
		}
	}

	return NULL;
}

/*
 * netfn_flow_cookie_db_lookup()
 *	Lookup flow cookie in the database
 *
 * Note: DB reference must be held and RCU must be locked
 */
struct netfn_flow_cookie *netfn_flow_cookie_db_lookup(struct netfn_flow_cookie_db *db, struct netfn_tuple *t)
{
	struct netfn_flow_cookie_db_entry *entry;
	struct netfn_flow_cookie_stats *stats;
	uint32_t hash_idx;

	/*
	 * Take reference before processing the lookup
	 */
	netfn_flow_cookie_db_ref(db);

	hash_idx = netfn_flow_cookie_db_hash(db, t);
	stats = this_cpu_ptr(db->stats);

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(), "WARNING: RCU LOCK Not Held during Cookie Lookup");

	entry = netfn_flow_cookie_db_find_entry(db, t);
	if (!entry) {
		pr_debug("%p: Failed to find entry for the tuple\n", db);
		stats->total_miss++;
		netfn_flow_cookie_db_deref(db);
		return NULL;
	}

	/*
	 * Update node and DB stats
	 */
	entry->hits++;
	stats->total_hits++;

	netfn_flow_cookie_db_deref(db);
	return &entry->cookie;
}
EXPORT_SYMBOL(netfn_flow_cookie_db_lookup);

/*
 * netfn_flow_cookie_db_add()
 * 	Add the DB entry corresponding to the 5 tuple flow info.
 */
int netfn_flow_cookie_db_add(struct netfn_flow_cookie_db *db, struct netfn_tuple *t, struct netfn_flow_cookie *cookie)
{
	struct netfn_flow_cookie_db_entry *entry = NULL;
	struct netfn_flow_cookie_stats *stats;
	uint32_t hash_idx;

	/*
	 * Take reference before processing the add
	 */
	netfn_flow_cookie_db_ref(db);
	stats = this_cpu_ptr(db->stats);

	hash_idx = netfn_flow_cookie_db_hash(db, t);

	/*
	 * Find whether the entry is already present or not
	 */
	spin_lock_bh(&db->lock);
	entry = netfn_flow_cookie_db_find_entry(db, t);
	if (entry) {
		pr_warn("%p: Failed to add flow to DB, entry already found", db);
		stats->total_add_fails++;
		spin_unlock_bh(&db->lock);

		/*
		 * drop reference taken at start
		 */
		netfn_flow_cookie_db_deref(db);
		return -EEXIST;
	}

	entry = kmem_cache_alloc(db->node_cache, GFP_NOWAIT | __GFP_NOWARN | __GFP_ZERO);
	if (!entry) {
		pr_warn("%p: Failed to allocate a flow entry in DB\n", db);
		stats->total_add_fails++;
		spin_unlock_bh(&db->lock);

		/*
		 * drop reference taken at start
		 */
		netfn_flow_cookie_db_deref(db);
		return -ENOMEM;
	}

	entry->tuple = *t;
	entry->cookie = *cookie;

	/*
	 * Insert the DB entry into the RCU based Hash List
	 */
	hlist_add_head_rcu(&entry->node, &db->db_table[hash_idx]);

	/*
	 * Take the ref as part of DB Entry Addition.
	 */
	netfn_flow_cookie_db_ref(db);

	atomic_inc(&db->active_cnt);
	stats->total_add_success++;
	set_bit(hash_idx, db->bitmap);

	pr_debug("Successfully added the DB entry node:%p", entry);
	spin_unlock_bh(&db->lock);

	/*
	 * drop the ref taken at start.
	 */
	netfn_flow_cookie_db_deref(db);
	return 0;
}
EXPORT_SYMBOL(netfn_flow_cookie_db_add);

/*
 * netfn_flow_cookie_db_del()
 * 	Delete the DB entry corresponding to the 5 tuple flow info.
 */
int netfn_flow_cookie_db_del(struct netfn_flow_cookie_db *db, struct netfn_tuple *t)
{
	struct netfn_flow_cookie_db_entry *entry;
	struct netfn_flow_cookie_stats *stats;
	struct hlist_node *hlist_head_ptr;
	uint32_t hash_idx;

	/*
	 * Take reference before processing the delete
	 */
	netfn_flow_cookie_db_ref(db);
	stats = this_cpu_ptr(db->stats);
	hash_idx = netfn_flow_cookie_db_hash(db, t);

	spin_lock_bh(&db->lock);
	entry = netfn_flow_cookie_db_find_entry(db, t);
	if (!entry) {
		pr_warn("%p: Failed to lookup DB entry\n", db);
		stats->total_del_fails++;
		spin_unlock_bh(&db->lock);

		/*
		 * drop reference taken start of function
		 */
		netfn_flow_cookie_db_deref(db);
		return -EEXIST;
	}

	/*
	 * Remove the DB entry from the RCU based Hash List
	 */
	hlist_del_rcu(&(entry->node));

	hlist_head_ptr = hlist_first_rcu(&(db->db_table[hash_idx]));

	if (unlikely(!hlist_head_ptr)) {
		clear_bit(hash_idx, db->bitmap);
	}
	spin_unlock_bh(&db->lock);

	/*
	 * drop the reference taken at the add operation
	 */
	netfn_flow_cookie_db_deref(db);

	atomic_dec(&db->active_cnt);
	stats->total_del_success++;

	/*
	 * We release the memory as allocated
	 * during the DB entry creation here.
	 */
	kmem_cache_free(db->node_cache, entry);

	/*
	 * drop the reference taken at the start
	 */
	netfn_flow_cookie_db_deref(db);
	return 0;
}
EXPORT_SYMBOL(netfn_flow_cookie_db_del);

/*
 * netfn_flow_cookie_db_alloc()
 *	Allocation of Flow DB basing on the no.of entries.
 */
struct netfn_flow_cookie_db *netfn_flow_cookie_db_alloc(uint32_t num_entries)
{
	struct netfn_flow_cookie_ctx *ctx = netfn_flow_cookie_ctx_get();
	struct netfn_flow_cookie_db *db = NULL;
	uint32_t num_hash_buckets = 0;
	char name[64];
	uint32_t i;

	if (num_entries <= 0) {
		pr_warn("Invalid Hash table size passed from user, num_entries:%d", num_entries);
		return NULL;
	}

	num_hash_buckets = roundup_pow_of_two(num_entries);

	db = kzalloc(sizeof(*db) + num_hash_buckets * sizeof(struct hlist_head), GFP_ATOMIC);
	if (!db) {
		pr_warn("Failed to allocate memory for the DB instance");
		return NULL;
	}

	db->max_size = num_hash_buckets;
	db->max_bits = __ffs(db->max_size);

	/*
	 * We initialize the DB Hash table here.
	 */
	for (i = 0; i < db->max_size; i++) {
		INIT_HLIST_HEAD(&db->db_table[i]);
	}

	atomic_set(&db->active_cnt, 0);

	db->node_cache = kmem_cache_create("db_node_cache", sizeof(struct netfn_flow_cookie_db_entry), 0, 0, NULL);
	if (!db->node_cache) {
		pr_warn("%p: Failed to create node cache from KMEM\n", db);
		goto fail1;
	}

	snprintf(name, sizeof(name), "flow_cookie_db@%lx", (uintptr_t)db);

	db->dentry = debugfs_create_dir(name, ctx->dentry);
	if (!db->dentry) {
		pr_warn("%p: Failed to create the flow_db(%s) dentry\n", db, name);
	}

	db->stats = netfn_flow_cookie_stats_alloc(db);
	if (!db->stats) {
		pr_warn("%p: Failed to create the stats entry for db(%s)\n", db, name);
		goto fail2;
	}

	if (netfn_flow_cookie_dump_init(db) == -1) {
		goto fail2;
	}

	db->bitmap = kzalloc(db->max_size, GFP_ATOMIC);
	if (!db->bitmap) {
		pr_warn("%p: Memory allocation failed for Bit Map.\n", db);
		goto fail3;
	}

        /*
	 * Initialize all bits to 0.
	 */
	memset(db->bitmap, 0, db->max_size);

	/*
	 * We initialize the reference structure here
	 * for the DB instance.
	 */
	kref_init(&db->ref);
	pr_info("%p: Database handle created\n", db);
	return db;

fail3:
	netfn_flow_cookie_dump_exit(db);
fail2:
	if (db->dentry)
		debugfs_remove_recursive(db->dentry);

	kmem_cache_destroy(db->node_cache);
fail1:
	kfree(db);
	return NULL;
}
EXPORT_SYMBOL(netfn_flow_cookie_db_alloc);
