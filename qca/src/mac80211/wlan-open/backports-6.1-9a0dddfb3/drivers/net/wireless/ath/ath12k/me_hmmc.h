/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*
 * HMMC and Deny list-based ME override header
 * Trie-based database with RCU protection for concurrent access
 * Performs Recursive implementation
 */

#ifndef _HMMC_DENY_LIST_H_
#define _HMMC_DENY_LIST_H_

#include <linux/types.h>
#include <linux/rcupdate.h>
#include <linux/in.h>
#include <linux/in6.h>

/*
 * HMMC and Deny list-based ME override constants
 */
#define __TRIE_STRIDE 4
#define __TRIE_BITS BIT(__TRIE_STRIDE)
#define __TRIE_LEVEL_V4 (BITS_PER_TYPE(struct in_addr) / __TRIE_STRIDE)
#define __TRIE_LEVEL_V6 (BITS_PER_TYPE(struct in6_addr) / __TRIE_STRIDE)

/*
 * Trie node type enumeration for bit-based state management
 */
enum __trie_node_type {
	__TRIE_NODE_INIT = 0,		/* Initial state bit position */
	__TRIE_NODE_INTR,		/* Intermediate node bit position */
	__TRIE_NODE_TERM,		/* Terminal node bit position */
	__TRIE_NODE_MAX
};

/*
 * Return types for Trie Operations.
 */
enum __trie_status {
	__TRIE_SUCCESS = 0,		/* Success */
	__TRIE_FAIL_NOMEM,		/* Memory Failure */
	__TRIE_FAIL_INVAL,		/* Invalid */
	__TRIE_FAIL_MAX
};

struct ath12k_me_db;

/*
 * Trie node for prefix-based search
 */
struct __trie_node {
	struct __trie_node __rcu *child[__TRIE_BITS];	/* child per node */
	DECLARE_BITMAP(bmap, __TRIE_BITS);		/* node bitmap */
	DECLARE_BITMAP(state, __TRIE_NODE_MAX);		/* State of the node */
	u32 value;					/* Value stored */
	struct rcu_head rcu;				/* For kfree_rcu */
};

/*
 * Trie database (Population trie)
 */
struct __trie_db {
	struct __trie_node __rcu *root; /* Root of the trie */
};

/*
 * Database to hold HMMC / denylist conversion, group-ID can be either denied
 * or marked for HMMC conversion
 */
struct ath12k_me_hmmc_list {
	struct __trie_db v4;   /* IPv4 data base */
	struct __trie_db v6;   /* IPv6 data base */
};

/* Public API functions */

/**
 * ath12k_me_hmmc_list_init - Initialize HMMC/Deny list database
 * @me_db: Database structure to initialize HMMC DB.
 *
 * Returns: 0 on success, negative error code on failure
 */
int ath12k_me_hmmc_list_init(struct ath12k_me_db *me_db);

/**
 * ath12k_me_hmmc_list_flush - Flush the HMMC/Deny list database
 * @me_db: Database structure for HMMC entries to be flushed.
 */
void ath12k_me_hmmc_list_flush(struct ath12k_me_db *me_db);

/**
 * ath12k_me_hmmc_list_reset - Reset the HMMC/Deny list database
 * @me_db: Database structure to reset
 */
void ath12k_me_hmmc_list_reset(struct ath12k_me_db *me_db);

/**
 * ath12k_me_hmmc_add - Add HMMC/DENY entry to database
 * @me_db: Database structure
 * @addr: IP address (IPv4 as single u32, IPv6 as array of 4 u32s in host order)
 * @v6: true for IPv6, false for IPv4
 * @pfx: Prefix length in bits
 * @val : HMMC or DENY action
 *
 * Returns: 0 on success, negative error code on failure
 */
int ath12k_me_hmmc_add(struct ath12k_me_db *me_db, u32 *addr, bool v6, u16 pfx, u32 val);

/**
 * ath12k_me_hmmc_del - Delete HMMC/DENY entry from database
 * @me_db: Database structure
 * @addr: IP address (IPv4 as single u32, IPv6 as array of 4 u32s in host order)
 * @v6: true for IPv6, false for IPv4
 * @pfx: Prefix length in bits
 *
 * Returns: 0 on success, negative error code on failure
 */
int ath12k_me_hmmc_del(struct ath12k_me_db *me_db, u32 *addr, bool v6, u16 pfx);

/**
 * ath12k_me_hmmc_lookup - Lookup entry in database
 * @me_db: Database structure
 * @addr: IP address to lookup (IPv4 as single u32, IPv6 as array of 4 u32s in host order)
 * @v6: true for IPv6, false for IPv4
 *
 * Returns: -EINVAL/-ENOMEM for error,
 * ATH12K_ME_HMMC_ACTION for HMMC entry, ATH12K_ME_DENYLIST_ACTION for deny entry
 */
int ath12k_me_hmmc_lookup(struct ath12k_me_db *me_db, u32 *addr, bool v6);

#endif /* _HMMC_DENY_LIST_H_ */
