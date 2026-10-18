// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/bitmap.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/jhash.h>
#include <linux/kref.h>
#include <linux/seqlock.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <net/ipv6.h>

#include "extap_db.h"

/**
 * __key2hash() - Generate hash index from MAC hash key
 * @k: Pointer to hash key
 *
 * Computes hash index for IPv4 or IPv6 address using jhash.
 *
 * Return: Hash index in range [0, MAC_DB_HASHES)
 */
static u32 __key2hash(const struct mac_hash_key *k)
{
	BUILD_BUG_ON_NOT_POWER_OF_2(MAC_DB_HASHES);

	if (k->is_v6)
		return jhash(&k->ip.v6, sizeof(k->ip.v6), 0) & MAC_DB_HASHES_MASK;

	return jhash(&k->ip.v4, sizeof(k->ip.v4), 0) & MAC_DB_HASHES_MASK;
}

/**
 * __key_compare() - Compare two MAC hash keys
 * @a: First hash key
 * @b: Second hash key
 *
 * Compares keys for equality, checking IP version and address.
 *
 * Return: true if keys are equal, false otherwise
 */
static bool __key_compare(const struct mac_hash_key *a,
			  const struct mac_hash_key *b)
{
	if (a->is_v6 != b->is_v6)
		return false;

	if (a->is_v6)
		return ipv6_addr_equal(&a->ip.v6, &b->ip.v6);

	return a->ip.v4.s_addr == b->ip.v4.s_addr;
}

/* MAC hash entry set */
static void mac_hash_set(struct mac_db *db, const struct mac_hash_key *k,
			 u16 bit)
{
	lockdep_assert_held(&db->lock);

	write_seqcount_begin(&db->hash_seq);
	set_bit(bit, db->hash[__key2hash(k)].bmap);
	write_seqcount_end(&db->hash_seq);
}

/* MAC hash entry clear */
static void mac_hash_clear(struct mac_db *db, const struct mac_hash_key *k,
			   u16 bit)
{
	lockdep_assert_held(&db->lock);

	write_seqcount_begin(&db->hash_seq);
	clear_bit(bit, db->hash[__key2hash(k)].bmap);
	write_seqcount_end(&db->hash_seq);
}

/**
 * mac_hash_key_gen() - Generate a hash key from skb
 * @skb: Socket buffer to extract IP and MAC from
 * @k: Pointer to hash key structure to fill
 * @use_dest: If true, use destination addresses; if false, use source addresses
 *
 * Extracts IP and MAC address from skb for both IPv4 and IPv6 packets.
 *
 * Return: 0 on success, -EINVAL if packet version is unsupported
 */
int mac_hash_key_gen(struct sk_buff *skb, struct mac_hash_key *k, bool use_dest)
{
	switch (ip_hdr(skb)->version) {
	case 4:
		k->is_v6 = false;

		/* Key generated using destination addresses */
		if (use_dest) {
			k->ip.v4.s_addr = ip_hdr(skb)->daddr;
			ether_addr_copy(k->mac_addr, eth_hdr(skb)->h_dest);
			return 0;
		}

		k->ip.v4.s_addr = ip_hdr(skb)->saddr;
		ether_addr_copy(k->mac_addr, eth_hdr(skb)->h_source);
		break;
	case 6:
		k->is_v6 = true;

		/* Key generated using destination addresses */
		if (use_dest) {
			memcpy(&k->ip.v6, &ipv6_hdr(skb)->daddr, sizeof(k->ip.v6));
			ether_addr_copy(k->mac_addr, eth_hdr(skb)->h_dest);
			return 0;
		}

		memcpy(&k->ip.v6, &ipv6_hdr(skb)->saddr, sizeof(k->ip.v6));
		ether_addr_copy(k->mac_addr, eth_hdr(skb)->h_source);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * mac_db_free() - Free the MAC database
 * @ref: Reference count pointer (called via kref_put)
 *
 * Callback for kref_put. Frees database when reference count reaches zero.
 * Use mac_db_deref() instead of calling directly.
 */
void mac_db_free(struct kref *ref)
{
	struct mac_db *db = container_of(ref, struct mac_db, ref);

	WARN_ONCE(!bitmap_empty(db->data_bmap, db->max_entries), "mac_db freed with active entries");
	kfree(db);
}

/**
 * mac_db_alloc_ref() - Allocate and initialize a MAC database
 * @num_entries: Desired capacity; capped to MAC_DB_ENTRIES
 *
 * Allocates new database
 *
 * Return: Pointer to new database or NULL on allocation failure
 */
struct mac_db *mac_db_alloc_ref(u16 num_entries)
{
	struct mac_db *db;

	if (num_entries > MAC_DB_ENTRIES) {
		pr_err("MAC_DB num_entries(%d) out-of-range\n", num_entries);
		return NULL;
	}

	db = kzalloc(sizeof(*db), GFP_KERNEL);
	if (!db)
		return NULL;

	spin_lock_init(&db->lock);
	seqcount_init(&db->hash_seq);
	kref_init(&db->ref); /* No direct free, use db_deref to free the entry */

	memset(db->hash, 0, sizeof(db->hash));
	memset(db->data, 0, sizeof(db->data));

	/* First mark all entries unavailable, then open only num_entries */
	bitmap_fill(db->data_bmap, MAC_DB_ENTRIES);
	db->max_entries = num_entries ? num_entries : MAC_DB_ENTRIES;
	bitmap_zero(db->data_bmap, db->max_entries);

	return db;
}

/**
 * mac_db_entry_free() - Free a MAC database entry
 * @ref: Reference count pointer (called via kref_put)
 *
 * Callback for kref_put. Clears entry fields and marks as available.
 * Use mac_db_entry_deref() instead of calling directly.
 */
void mac_db_entry_free(struct kref *ref)
{
	struct mac_db_entry *me = container_of(ref, struct mac_db_entry, ref);
	struct mac_db *db = me->db;

	memset(&me->key, 0, sizeof(me->key));
	memset(me->mac_addr, 0, sizeof(me->mac_addr));
	me->hits = 0;

	/* Clear the bitmap to indicate the entry is now free */
	clear_bit(me->bmap_idx, db->data_bmap);
	mac_db_deref(db); /* Drop the reference taken during add */
}

/**
 * mac_db_entry_get() - Lookup a MAC database entry
 * @db: Pointer to MAC database
 * @k: Hash key to lookup
 *
 * Finds entry matching the key using hash table with sequence locking.
 * Returns entry with incremented reference count. caller must release via
 * mac_db_entry_deref().
 *
 * Return: Pointer to entry with incremented refcount or NULL if not found
 */
struct mac_db_entry *mac_db_entry_get(struct mac_db *db, struct mac_hash_key *k)
{
	struct mac_db_entry *me = NULL;
	struct mac_hash *mh;
	unsigned long bit;
	unsigned int seq;
	u16 max_entries;

	mh = &db->hash[__key2hash(k)];
	max_entries = db->max_entries;

	/*
	 * We need to retry the bitmap read just to ensure if there is
	 * any update to the bitmap registered during the operation, hence
	 * ensure the updated status. For example if something got deleted
	 * during the first read then we will retry again to check that.
	 */
	do {
		/* Check if we are retrying the read */
		if (me)
			me = mac_db_entry_deref(me);

		seq = read_seqcount_begin(&db->hash_seq);

		for_each_set_bit(bit, mh->bmap, max_entries) {
			me = mac_db_entry_ref(db, (u16)bit);
			if (!me)
				continue;

			if (__key_compare(&me->key, k))
				break;

			me = mac_db_entry_deref(me);
		}

	} while (read_seqcount_retry(&db->hash_seq, seq));

	return me;
}

/**
 * mac_db_entry_flush() - Flush all entries from the MAC database
 * @db: Pointer to MAC database
 *
 * Removes all entries from database. Takes bitmap snapshot to avoid corruption
 * during iteration. Safe even if entries are in use.
 */
void mac_db_entry_flush(struct mac_db *db)
{
	DECLARE_BITMAP(snapshot, MAC_DB_ENTRIES);
	u16 max_entries = db->max_entries;
	struct mac_db_entry *me;
	unsigned long bit;

	spin_lock_bh(&db->lock);

	/* Snapshot the bitmap so that mac_db_entry_free's clear_bit does not
	 * corrupt the iterator's view of data_bmap.
	 */
	bitmap_copy(snapshot, db->data_bmap, MAC_DB_ENTRIES);

	for_each_set_bit(bit, snapshot, max_entries) {
		me = mac_db_entry_ref(db, (u16)bit);
		if (!me)
			continue;

		mac_hash_clear(db, &me->key, (u16)bit);
		mac_db_entry_deref(me); /* Drop reference from entry_add */

		mac_db_entry_deref(me); /* Drop reference taken above */
	}

	spin_unlock_bh(&db->lock);
}

/**
 * mac_db_entry_del() - Delete a MAC database entry
 * @db: Pointer to MAC database
 * @mac_addr: MAC address to delete
 *
 * Removes entry with specified MAC address from database.
 * Clears hash entry and drops references.
 */
void mac_db_entry_del(struct mac_db *db, u8 mac_addr[])
{
	struct mac_db_entry *me;
	unsigned long bit;

	spin_lock_bh(&db->lock);

	for_each_set_bit(bit, db->data_bmap, db->max_entries) {
		me = mac_db_entry_ref(db, (u16)bit);
		if (!me)
			continue;

		/* Compare the MAC address */
		if (ether_addr_equal(me->mac_addr, mac_addr)) {
			mac_hash_clear(db, &me->key, (u16)bit);
			mac_db_entry_deref(me); /* Drop reference from; entry_add */
		}

		mac_db_entry_deref(me); /* Drop reference taken at the beginning */
	}

	spin_unlock_bh(&db->lock);
}

/**
 * mac_db_entry_add() - Add a new MAC database entry
 * @db: Pointer to MAC database
 * @mac_addr: MAC address to store
 * @k: Hash key for lookup
 *
 * Adds new IP-to-MAC mapping to database. Checks if entry exists, finds
 * available slot, initializes entry with reference count and adds to hash table.
 *
 * Return: 0 on success, -EEXIST if entry exists, -ENOMEM if database is full
 */
int mac_db_entry_add(struct mac_db *db, u8 mac_addr[], struct mac_hash_key *k)
{
	struct mac_db_entry *me;
	unsigned long bit;
	int err = 0;

	spin_lock_bh(&db->lock);

	/*
	 * Check if the entry exists for the key
	 *
	 * FIXME: If, MAC address changes which ideally should never happen
	 * is __entry_add supposed to update the entry's MAC also.
	 */
	me = mac_db_entry_get(db, k);
	if (me) {
		mac_db_entry_deref(me);
		err = -EEXIST;
		goto done;
	}

	/* Find an empty slot to add the entry */
	bit = find_first_zero_bit(db->data_bmap, db->max_entries);
	if (bit >= db->max_entries) {
		err = -ENOMEM;
		goto done;
	}

	me = &db->data[(u16)bit];
	kref_init(&me->ref);		/* set refcount to 1; released in mac_db_entry_del */

	me->key = *k;
	me->db = db;
	me->bmap_idx = (u16)bit;
	ether_addr_copy(me->mac_addr, mac_addr);

	/* Mark the entries valid in the bitmap */
	mac_db_ref(db); /* Take reference; release it in entry_put */
	set_bit(bit, db->data_bmap);
	mac_hash_set(db, k, (u16)bit);

done:
	spin_unlock_bh(&db->lock);
	return err;
}
