/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __EXTAP_DB_H
#define __EXTAP_DB_H

#include <linux/bitmap.h>
#include <linux/etherdevice.h>
#include <linux/in.h>
#include <linux/kref.h>
#include <linux/seqlock.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <net/ipv6.h>

#define MAC_DB_ENTRIES		512
#define MAC_DB_HASHES		128
#define MAC_DB_HASHES_MASK	(MAC_DB_HASHES - 1)

struct mac_db;

/**
 * struct mac_hash - IP to MAC hash entry
 * @bmap: Bitmap where each bit corresponds to an index in MAC DB
 *
 * Hash table entry for fast lookup of MAC addresses by IP address.
 */
struct mac_hash {
	DECLARE_BITMAP(bmap, MAC_DB_ENTRIES);
};

/**
 * struct mac_hash_key - IP key object for lookup
 * @is_v6: Address is v4 or v6
 * @ip: IPv4 or IPv6 address
 * @mac_addr: MAC address associated with the key
 *
 * Key used to lookup MAC database entries. Supports both IPv4 and IPv6 addresses.
 */
struct mac_hash_key {
	bool is_v6;
	union {
		struct in_addr  v4;
		struct in6_addr v6;
	} ip;
	u8 mac_addr[ETH_ALEN];
};

/**
 * struct mac_db_entry - MAC database entry
 * @key: Hash key for lookup
 * @mac_addr: MAC address to replace
 * @ref: Reference count; non-zero when in use
 * @db: Back pointer to parent database
 * @bmap_idx: Bitmap index for this entry
 * @hits: Number of successful DMAC rewrites using this entry
 *
 * Individual entry in the MAC database. Tracks IP-to-MAC mappings with
 * reference counting for safe concurrent access.
 */
struct mac_db_entry {
	struct mac_hash_key key;
	u8 mac_addr[ETH_ALEN];
	struct kref ref;

	struct mac_db *db;
	u16 bmap_idx;
	u32 hits;
};

/**
 * struct mac_db - MAC database
 * @ref: Reference count for the database
 * @lock: Spinlock for add/delete operations
 * @max_entries: Capacity, capped to MAC_DB_ENTRIES
 * @data_bmap: Bitmap tracking available objects
 * @data: Array of MAC database entries
 * @hash_seq: Sequence lock for hash table updates
 * @hash: Hash table for fast lookup using key
 *
 * Main database structure for storing IP-to-MAC mappings. Uses hash table
 * for O(1) lookup and reference counting for safe memory management.
 */
struct mac_db {
	struct kref ref;
	spinlock_t lock;
	u16 max_entries;
	DECLARE_BITMAP(data_bmap, MAC_DB_ENTRIES);
	struct mac_db_entry data[MAC_DB_ENTRIES];

	seqcount_t hash_seq;
	struct mac_hash hash[MAC_DB_HASHES];
};

int mac_hash_key_gen(struct sk_buff *skb, struct mac_hash_key *k, bool use_dest);
struct mac_db *mac_db_alloc_ref(u16 num_entries);
void mac_db_free(struct kref *ref);

static inline struct mac_db *mac_db_ref(struct mac_db *db)
{
	return db && kref_get_unless_zero(&db->ref) ? db : NULL;
}

static inline void *mac_db_deref(struct mac_db *db)
{
	kref_put(&db->ref, mac_db_free);
	return NULL;
}

int mac_db_entry_add(struct mac_db *db, u8 mac_addr[], struct mac_hash_key *k);
void mac_db_entry_del(struct mac_db *db, u8 mac_addr[]);
void mac_db_entry_flush(struct mac_db *db);
struct mac_db_entry *mac_db_entry_get(struct mac_db *db, struct mac_hash_key *k);
void mac_db_entry_free(struct kref *ref);

static inline struct mac_db_entry *mac_db_entry_ref(struct mac_db *db, u16 idx)
{
	struct mac_db_entry *me = &db->data[idx];

	return kref_get_unless_zero(&me->ref) ? me : NULL;
}

static inline void *mac_db_entry_deref(struct mac_db_entry *me)
{
	kref_put(&me->ref, mac_db_entry_free);
	return NULL;
}

#endif /* !__EXTAP_DB_H */
