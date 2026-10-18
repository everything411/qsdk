// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*
 * HMMC Dump functionality - Extension module
 *
 * Provides the unified dump API (ath12k_me_hmmc_dump_extn) and all associated
 * helper infrastructure for traversing and printing HMMC/Deny list trie
 * database entries for IPV4/IPV6.
 *
 * All formatted output is produced via seq_printf() into a temporary
 * seq_file backed by a kmalloc'd buffer.  Once the traversal is complete
 * the buffer is emitted through ath12k_info() and freed.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <me.h>
#include <me_hmmc.h>
#include <debug.h>
#include "me_hmmc_extn.h"

/* ------------------------------------------------------------------ */
/* Trie Dump Helper Functions                                         */
/* ------------------------------------------------------------------ */

/*
 * __trie_action_to_string - Convert action value to string representation
 * @value: Action value (such as ATH12K_ME_HMMC_ACTION, ATH12K_ME_DENYLIST_ACTION)
 *
 * Returns a human-readable string for the given action value.
 */
static inline const char *__trie_action_to_string(u32 value)
{
	switch (value) {
	case ATH12K_ME_HMMC_ACTION:
		return "HMMC";
	case ATH12K_ME_DENYLIST_ACTION:
		return "DENY";
	default:
		return "UNKNOWN";
	}
}

/*
 * __trie_format_ipv4_addr - Write IPv4 address in dotted decimal notation
 * @seq:  seq_file to write into
 * @addr: IPv4 address in host byte order
 */
static inline void __trie_format_ipv4_addr(struct seq_file *seq, u32 addr)
{
	u32 addr_be = htonl(addr);
	u8 *bytes = (u8 *)&addr_be;

	seq_printf(seq, "%u.%u.%u.%u",
		   bytes[0], bytes[1], bytes[2], bytes[3]);
}

/*
 * __trie_format_ipv6_addr - Write IPv6 address in standard notation
 * @seq:  seq_file to write into
 * @addr: IPv6 address as array of 4 u32s in host byte order
 *
 * Applies RFC 5952 :: compression for the longest run of consecutive
 * zero 16-bit groups (minimum run length of 2).
 */
static inline void __trie_format_ipv6_addr(struct seq_file *seq, u32 *addr)
{
	int max_zero_start = -1, max_zero_len = 0;
	int i, zero_start = -1, zero_len = 0;
	bool after_double_colon = false;
	bool compressed = false;
	bool first_field = true;
	u16 words[8];

	/* Convert u32 array to u16 array - addr is already in host order */
	for (i = 0; i < 4; i++) {
		words[i * 2]     = (u16)(addr[i] >> 16);
		words[i * 2 + 1] = (u16)(addr[i] & 0xFFFF);
	}

	/* Find longest run of consecutive zeros for :: compression */
	for (i = 0; i < 8; i++) {
		if (words[i] == 0) {
			if (zero_start == -1) {
				zero_start = i;
				zero_len = 1;
			} else {
				zero_len++;
			}
		} else {
			if (zero_len > max_zero_len && zero_len >= 2) {
				max_zero_start = zero_start;
				max_zero_len = zero_len;
			}
			zero_start = -1;
			zero_len = 0;
		}
	}

	/* Check final run */
	if (zero_len > max_zero_len && zero_len >= 2) {
		max_zero_start = zero_start;
		max_zero_len = zero_len;
	}

	/* Format the address */
	for (i = 0; i < 8; i++) {
		if (max_zero_len >= 2 && i == max_zero_start && !compressed) {
			/* Start of :: compression */
			seq_printf(seq, "::");
			compressed = true;
			after_double_colon = true;
			first_field = false;
			i += max_zero_len - 1; /* Skip compressed zeros */
		} else {
			/* Regular 16-bit field */
			if (!first_field && !after_double_colon)
				seq_printf(seq, ":");
			seq_printf(seq, "%x", words[i]);
			first_field = false;
			after_double_colon = false;
		}
	}

	/* Handle special case where compression is at the end */
	if (max_zero_len >= 2 && max_zero_start + max_zero_len == 8 && !compressed)
		seq_printf(seq, "::");
}

/*
 * __trie_dump_add_entry - Write a single trie entry to the seq_file
 * @ctx:   Dump context carrying the seq_file and current address state
 * @value: Action value stored at the terminal node
 *
 * Formats the current IP address (built up during recursive traversal) and
 * its prefix length into a human-readable line and writes it via seq_printf().
 *
 * Returns: 0 on success, -ENOSPC if the seq_file buffer has overflowed
 */
static int __trie_dump_add_entry(struct __trie_dump_ctx *ctx, u32 value)
{
	if (!ctx || !ctx->seq)
		return -EINVAL;

	/* Write "IP: <addr>/<prefix>, Action: <action>\n" directly to seq */
	seq_printf(ctx->seq, "IP: ");

	if (ctx->is_v6)
		__trie_format_ipv6_addr(ctx->seq, ctx->current_addr);
	else
		__trie_format_ipv4_addr(ctx->seq, ctx->current_addr[0]);

	seq_printf(ctx->seq, "/%u, Action: %s\n",
		   ctx->current_prefix, __trie_action_to_string(value));

	ctx->entries_dumped++;

	return seq_has_overflowed(ctx->seq) ? -ENOSPC : 0;
}

/*
 * __trie_dump_recursive_filtered - Recursively traverse trie and dump entries
 * @node:          Current trie node
 * @ctx:           Dump context (seq_file, current address, counters)
 * @level:         Current depth in the trie (0 = root)
 * @max_level:     Maximum depth (__TRIE_LEVEL_V4 or __TRIE_LEVEL_V6)
 * @action_filter: Only dump terminal nodes whose value matches this action
 *
 * Walks the trie depth-first.  At each level the nibble contributed by the
 * current child index is OR-ed into ctx->current_addr; it is cleared again
 * after the child subtree has been fully visited so that sibling subtrees
 * start with a clean state.
 *
 * Must be called inside an RCU read-side critical section.
 *
 * Returns: 0 on success, -ENOSPC if the seq_file output is exhausted
 */
static int __trie_dump_recursive_filtered(struct __trie_node *node,
					  struct __trie_dump_ctx *ctx, u16 level,
					  u16 max_level, u32 action_filter)
{
	u32 bit_position;
	int ret = 0;
	u8 bit;

	if (!node || level > max_level)
		return 0;

	/* If this is a terminal node that matches the filter, record it */
	if (test_bit(__TRIE_NODE_TERM, node->state) &&
	    node->value == action_filter) {
		ctx->current_prefix = level * __TRIE_STRIDE;
		ret = __trie_dump_add_entry(ctx, node->value);
		if (ret)
			return ret;
	}

	/* Continue traversing children if not yet at max depth */
	if (level < max_level) {
		bit_position = level * __TRIE_STRIDE;

		for_each_set_bit(bit, node->bmap, __TRIE_BITS) {
			struct __trie_node *child;

			child = rcu_dereference(node->child[bit]);
			if (!child)
				continue;

			/* Accumulate this nibble into the current address */
			if (ctx->is_v6) {
				/* IPv6: 128 bits across four u32 words */
				u32 word_idx  = bit_position / 32;
				u32 bit_shift = 28 - (bit_position % 32); /* MSB first */

				if (word_idx < 4)
					ctx->current_addr[word_idx] |=
						(u32)bit << bit_shift;
			} else {
				/* IPv4: single u32 word */
				u32 bit_shift = 28 - bit_position; /* MSB first */

				ctx->current_addr[0] |= (u32)bit << bit_shift;
			}

			/* Recurse into child subtree */
			ret = __trie_dump_recursive_filtered(child, ctx, level + 1,
							     max_level, action_filter);

			/* Clear the nibble before visiting the next sibling */
			if (ctx->is_v6) {
				u32 word_idx  = bit_position / 32;
				u32 bit_shift = 28 - (bit_position % 32);

				if (word_idx < 4)
					ctx->current_addr[word_idx] &=
						~((u32)0xF << bit_shift);
			} else {
				u32 bit_shift = 28 - bit_position;

				ctx->current_addr[0] &= ~((u32)0xF << bit_shift);
			}

			if (ret)
				return ret;
		}
	}

	return 0;
}

/*
 * __trie_dump_add_section_header - Write a section header to the seq_file
 * @ctx:    Dump context
 * @header: Null-terminated header string to write
 *
 * Returns: 0 on success, -ENOSPC if the seq_file buffer has overflowed
 */
static int __trie_dump_add_section_header(struct __trie_dump_ctx *ctx,
					  const char *header)
{
	if (!ctx || !ctx->seq)
		return -EINVAL;

	seq_printf(ctx->seq, "%s", header);

	return seq_has_overflowed(ctx->seq) ? -ENOSPC : 0;
}

/* ------------------------------------------------------------------ */
/* Internal core dump logic                                           */
/* ------------------------------------------------------------------ */

/*
 * __me_hmmc_dump_core - Core dump logic writing into a seq_file
 * @me_db:        ME database
 * @filter_flags: ATH12K_ME_DUMP_* flags
 * @ctx:          Pre-initialised dump context (seq already set)
 *
 * Acquires me_db->lock and traverses the requested sub-databases.
 * All output goes through seq_printf() via @ctx->seq.
 *
 * Returns 0 on success, negative error code on failure.
 */
static int __me_hmmc_dump_core(struct ath12k_me_db *me_db, u32 filter_flags,
			       struct __trie_dump_ctx *ctx)
{
	struct ath12k_me_hmmc_list *db = &me_db->hmmc_db;
	bool first_section = true;
	struct __trie_node *root;
	int ret = 0;

	spin_lock_bh(&me_db->lock);

	/* Main header */
	ret = __trie_dump_add_section_header(ctx,
					     "ATH12K_ME_DUMP_EXTS: Filtered HMMC/Deny List Dump:\n");
	if (ret)
		goto cleanup;

	/* ---- HMMC IPv4 ---- */
	if (filter_flags & ATH12K_ME_DUMP_HMMC_V4) {
		if (!first_section) {
			ret = __trie_dump_add_section_header(ctx, "\n");
			if (ret)
				goto done;
		}
		first_section = false;

		ret = __trie_dump_add_section_header(ctx,
						     "=== HMMC IPv4 Entries ===\n");
		if (ret)
			goto done;

		ctx->is_v6 = false;
		memset(ctx->current_addr, 0, sizeof(ctx->current_addr));

		rcu_read_lock_bh();
		root = rcu_dereference(db->v4.root);
		if (root)
			ret = __trie_dump_recursive_filtered(root, ctx,
							     0, __TRIE_LEVEL_V4,
							     ATH12K_ME_HMMC_ACTION);
		rcu_read_unlock_bh();

		if (ret)
			goto done;
	}

	/* ---- HMMC IPv6 ---- */
	if (filter_flags & ATH12K_ME_DUMP_HMMC_V6) {
		if (!first_section) {
			ret = __trie_dump_add_section_header(ctx, "\n");
			if (ret)
				goto done;
		}
		first_section = false;

		ret = __trie_dump_add_section_header(ctx,
						     "=== HMMC IPv6 Entries ===\n");
		if (ret)
			goto done;

		ctx->is_v6 = true;
		memset(ctx->current_addr, 0, sizeof(ctx->current_addr));

		rcu_read_lock_bh();
		root = rcu_dereference(db->v6.root);
		if (root)
			ret = __trie_dump_recursive_filtered(root, ctx,
							     0, __TRIE_LEVEL_V6,
							     ATH12K_ME_HMMC_ACTION);
		rcu_read_unlock_bh();

		if (ret)
			goto done;
	}

	/* ---- Deny List IPv4 ---- */
	if (filter_flags & ATH12K_ME_DUMP_DENY_V4) {
		if (!first_section) {
			ret = __trie_dump_add_section_header(ctx, "\n");
			if (ret)
				goto done;
		}
		first_section = false;

		ret = __trie_dump_add_section_header(ctx,
						     "=== Deny List IPv4 Entries ===\n");
		if (ret)
			goto done;

		ctx->is_v6 = false;
		memset(ctx->current_addr, 0, sizeof(ctx->current_addr));

		rcu_read_lock_bh();
		root = rcu_dereference(db->v4.root);
		if (root)
			ret = __trie_dump_recursive_filtered(root, ctx,
							     0, __TRIE_LEVEL_V4,
							     ATH12K_ME_DENYLIST_ACTION);
		rcu_read_unlock_bh();

		if (ret)
			goto done;
	}

	/* ---- Deny List IPv6 ---- */
	if (filter_flags & ATH12K_ME_DUMP_DENY_V6) {
		if (!first_section) {
			ret = __trie_dump_add_section_header(ctx, "\n");
			if (ret)
				goto done;
		}
		first_section = false;

		ret = __trie_dump_add_section_header(ctx,
						     "=== Deny List IPv6 Entries ===\n");
		if (ret)
			goto done;

		ctx->is_v6 = true;
		memset(ctx->current_addr, 0, sizeof(ctx->current_addr));

		rcu_read_lock_bh();
		root = rcu_dereference(db->v6.root);
		if (root)
			ret = __trie_dump_recursive_filtered(root, ctx,
							     0, __TRIE_LEVEL_V6,
							     ATH12K_ME_DENYLIST_ACTION);
		rcu_read_unlock_bh();

		if (ret)
			goto done;
	}

done:
	/* Print total entry count */
	seq_printf(ctx->seq, "ath12k_me_dump: Total entries dumped: %u\n",
		   ctx->entries_dumped);

cleanup:
	spin_unlock_bh(&me_db->lock);
	return ret;
}

/* ------------------------------------------------------------------ */
/* Exported Dump API                                                   */
/* ------------------------------------------------------------------ */

/**
 * ath12k_me_hmmc_dump_extn - Dump HMMC/Deny list entries based on filter flags
 * @me_db:          ME database containing the HMMC/Deny list trie
 * @filter_flags:   Bitwise OR of ATH12K_ME_DUMP_* flags selecting specific
 *                  entries to include in the output
 * @entries_dumped: Receives the total number of entries printed (may be NULL)
 *
 * Traverses the requested trie sub-databases (HMMC IPv4/IPv6, Deny IPv4/IPv6)
 * and writes all matching terminal nodes to a temporary buffer using
 * seq_printf().  The formatted text is then emitted via ath12k_info() so
 * that callers do not need to manage any seq_file or output buffer themselves.
 *
 * The function acquires me_db->lock (BH-safe spinlock) for the duration of
 * the dump to serialise against concurrent add/del operations.  Individual
 * trie traversals additionally take the RCU read lock (BH variant) as
 * required by rcu_dereference().
 *
 * Returns:
 *  0        - success
 *  -EINVAL  - @me_db is NULL or @filter_flags is zero
 *  -ENOMEM  - temporary output buffer could not be allocated
 *  -ENOSPC  - output buffer exhausted before all entries were written
 */
int ath12k_me_hmmc_dump_extn(struct ath12k_me_db *me_db, u32 filter_flags,
			u16 *entries_dumped)
{
	struct __trie_dump_ctx ctx = {0};
	struct seq_file seq = {};
	int ret;

	if (!me_db || !filter_flags)
		return -EINVAL;

	/* Allocate a temporary output buffer */
	seq.buf = kmalloc(HMMC_DUMP_BUFFER_SIZE, GFP_KERNEL);
	if (!seq.buf)
		return -ENOMEM;

	seq.size  = HMMC_DUMP_BUFFER_SIZE;
	seq.count = 0;
	mutex_init(&seq.lock);

	/* Initialise dump context */
	ctx.seq            = &seq;
	ctx.entries_dumped = 0;
	memset(ctx.current_addr, 0, sizeof(ctx.current_addr));

	/* Run the core dump logic */
	ret = __me_hmmc_dump_core(me_db, filter_flags, &ctx);

	if (entries_dumped)
		*entries_dumped = ctx.entries_dumped;

	/* NUL-terminate and emit whatever was written */
	if (seq.count < seq.size)
		seq.buf[seq.count] = '\0';
	else
		seq.buf[seq.size - 1] = '\0';

	if (seq.count > 0)
		ath12k_info(NULL, "%s", seq.buf);

	mutex_destroy(&seq.lock);
	kfree(seq.buf);

	return ret;
}
EXPORT_SYMBOL(ath12k_me_hmmc_dump_extn);
