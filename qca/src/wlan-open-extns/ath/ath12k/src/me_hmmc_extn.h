/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*
 * HMMC Dump infrastructure header
 *
 * Declares the public dump API (ath12k_me_hmmc_dump_extn), the dump filter
 * flags, the output buffer size constant (HMMC_DUMP_BUFFER_SIZE), and
 * the internal trie-traversal context (struct __trie_dump_ctx).
 *
 * HMMC_DUMP_BUFFER_SIZE and struct __trie_dump_ctx are guarded by
 * CPTCFG_QCN_EXTN and are only visible when the extension module is
 * compiled in.  Callers outside the extension module (e.g. vendor.c)
 * do not need to know about seq_file or the dump context at all.
 *
 * When CPTCFG_QCN_EXTN is not selected a stub fn is provided so that
 * callers in the main driver compile cleanly without the EXTNS module.
 */

#ifndef _ME_DUMP_H_
#define _ME_DUMP_H_

#include <linux/types.h>

/* Forward declaration - full definition is in me.h */
struct ath12k_me_db;

#ifdef CPTCFG_QCN_EXTN

#include <linux/seq_file.h>

/*
 * HMMC_DUMP_BUFFER_SIZE - Size of the temporary output buffer used by
 * ath12k_me_hmmc_dump_extn() to accumulate seq_printf() output before emitting
 * it via ath12k_info().  Sized to accommodate a full HMMC/Deny list dump.
 */
#define HMMC_DUMP_BUFFER_SIZE	(16 * 1024)

/**
 * struct __trie_dump_ctx - Internal context for trie traversal and dump
 * @seq:            seq_file backed by the temporary output buffer
 * @current_addr:   IP address being built up during recursive traversal
 *                  (4 x u32 to support both IPv4 and IPv6)
 * @current_prefix: Prefix length at the current terminal node
 * @entries_dumped: Running count of entries written so far
 * @is_v6:          true when traversing an IPv6 trie, false for IPv4
 *
 * Used internally by me_dump.c during trie traversal.
 */
struct __trie_dump_ctx {
	struct seq_file *seq;
	u32              current_addr[4];
	u16              current_prefix;
	u16              entries_dumped;
	bool             is_v6;
};

#endif /* CPTCFG_QCN_EXTN */

/*
 * Dump filter flags for selective dumping.
 * Pass one or more of these (OR-ed together) as the filter_flags
 * argument to ath12k_me_hmmc_dump_extn().
 */
#define ATH12K_ME_DUMP_HMMC_V4    BIT(0)  /* Dump HMMC IPv4 entries */
#define ATH12K_ME_DUMP_HMMC_V6    BIT(1)  /* Dump HMMC IPv6 entries */
#define ATH12K_ME_DUMP_DENY_V4    BIT(2)  /* Dump Deny list IPv4 entries */
#define ATH12K_ME_DUMP_DENY_V6    BIT(3)  /* Dump Deny list IPv6 entries */

/* Convenience macros for common flag combinations */
#define ATH12K_ME_DUMP_ALL_HMMC   (ATH12K_ME_DUMP_HMMC_V4 | ATH12K_ME_DUMP_HMMC_V6)
#define ATH12K_ME_DUMP_ALL_DENY   (ATH12K_ME_DUMP_DENY_V4 | ATH12K_ME_DUMP_DENY_V6)
#define ATH12K_ME_DUMP_ALL_V4     (ATH12K_ME_DUMP_HMMC_V4 | ATH12K_ME_DUMP_DENY_V4)
#define ATH12K_ME_DUMP_ALL_V6     (ATH12K_ME_DUMP_HMMC_V6 | ATH12K_ME_DUMP_DENY_V6)
#define ATH12K_ME_DUMP_ALL        (ATH12K_ME_DUMP_ALL_HMMC | ATH12K_ME_DUMP_ALL_DENY)

#ifndef CPTCFG_QCN_EXTN

/**
 * ath12k_me_hmmc_dump_extn - Stub when EXTNS module is not compiled in
 *
 * Returns -EOPNOTSUPP to indicate the dump feature is unavailable.
 */
static inline int ath12k_me_hmmc_dump_extn(struct ath12k_me_db *me_db,
				      u32 filter_flags,
				      u16 *entries_dumped)
{
	return -EOPNOTSUPP;
}

#else /* CPTCFG_QCN_EXTN */

/**
 * ath12k_me_hmmc_dump_extn - Dump HMMC/Deny list entries based on filter flags
 * @me_db:          ME database containing the HMMC/Deny list trie DBs
 * @filter_flags:   Bitwise OR of ATH12K_ME_DUMP_* flags
 * @entries_dumped: Receives the total number of entries printed (may be NULL)
 *
 * Traverses the requested trie sub-databases and writes all matching terminal
 * nodes to a temporary buffer using seq_printf().  The formatted text is then
 * emitted via ath12k_info() so that callers do not need to manage any
 * seq_file or output buffer themselves.
 *
 * This unified API allows selective dumping of HMMC/Deny entries for IPv4/IPv6
 * based on the filter_flags parameter. Multiple flags can be combined using OR.
 *
 * Examples:
 * - ATH12K_ME_DUMP_HMMC_V4: Dump only HMMC IPv4 entries
 * - ATH12K_ME_DUMP_DENY_V6: Dump only Deny list IPv6 entries
 * - ATH12K_ME_DUMP_ALL_HMMC: Dump all HMMC entries (both IPv4 and IPv6)
 * - ATH12K_ME_DUMP_ALL: Dump all entries
 *
 * Returns 0 on success, negative error code on failure.
 * Implemented in me_dump.c (EXTNS).
 */
int ath12k_me_hmmc_dump_extn(struct ath12k_me_db *me_db, u32 filter_flags,
			u16 *entries_dumped);

#endif /* CPTCFG_QCN_EXTN */
#endif /* _ME_DUMP_H_ */
