/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/netdevice.h>
#include <netfn_flow_cookie.h>
#include "netfn_flow_cookie_priv.h"

static const char *netfn_flow_cookie_stats_str[] = {
	"Flow add success",			/* Flow Cookie add operations successful */
	"Flow add failed",			/* Flow Cookie add operations failed */
	"Flow del success",			/* Flow Cookie delete operations successful */
	"Flow del failed",			/* Flow Cookie delete operations failed */
	"Total Hits",				/* Total Hits for all the Flows */
	"Total Miss",				/* Total Miss */
};

/*
 * netfn_flow_cookie_stats_reset()
 *	API to reset the Flow cookie DB stats
 */
static void netfn_flow_cookie_stats_reset(struct netfn_flow_cookie_db *db)
{
	int dwords, cpu;

	dwords = (sizeof(struct netfn_flow_cookie_stats) / sizeof(uint64_t));

	/*
	 * All statistics are 64bit. So we can just iterate by words.
	 */
	for_each_possible_cpu(cpu) {
		struct netfn_flow_cookie_stats *sp = per_cpu_ptr(db->stats, cpu);
		uint64_t *sp_ptr = (uint64_t *)sp;
		int i;

		for (i = 0; i < dwords; i++, sp_ptr++)
			*sp_ptr = 0;
	}
}

/*
 * netfn_flow_cookie_stats_get()
 *	API to retrieve the Flow cookie stats DB specifically.
 *
 * Note: All the Per CPU stats will be accumulated here.
 */
static void netfn_flow_cookie_stats_get(struct netfn_flow_cookie_db *db, struct netfn_flow_cookie_stats *stats)
{
	int dwords, cpu;

	dwords = (sizeof(*stats) / sizeof(uint64_t));
	memset(stats, 0, sizeof(*stats));

	/*
	 * All statistics are 64bit. So we can just iterate by words.
	 */
	for_each_possible_cpu(cpu) {
		struct netfn_flow_cookie_stats *sp = per_cpu_ptr(db->stats, cpu);
		uint64_t *stats_ptr = (uint64_t *)stats;
		uint64_t *sp_ptr = (uint64_t *)sp;
		int i;

		for (i = 0; i < dwords; i++, stats_ptr++, sp_ptr++)
			*stats_ptr += *sp_ptr;
	}
}

/*
 * netfn_flow_cookie_stats_clear()
 *	Clear Flow Cookie Stats
 */
static ssize_t netfn_flow_cookie_stats_clear(struct file *fp, const char __user *ubuf, size_t sz, loff_t *ppos)
{
	struct netfn_flow_cookie_db *db = fp->private_data;
	uint32_t reset;

	if (kstrtou32_from_user(ubuf, sz, 0, &reset)) {
		return -EINVAL;
	}

	if (reset != 0) {
		return -EINVAL;
	}

	netfn_flow_cookie_stats_reset(db);

	return sz;
}

/*
 * netfn_flow_cookie_stats_print()
 *	API to dump the flow cookie stats in the appropriate file.
 */
static ssize_t netfn_flow_cookie_stats_print(struct file *fp, char __user *ubuf, size_t sz, loff_t *ppos)
{
	struct netfn_flow_cookie_db *db = fp->private_data;
	struct netfn_flow_cookie_stats stats;
	uint64_t *stats_shadow;
	ssize_t len = 0;
	ssize_t max_len;
	uint32_t i;
	char *buf;

	netfn_flow_cookie_stats_get(db, &stats);

	/*
	 * We need to calculate required string buffer for stats, else full stats may not be captured.
	 */
	max_len = (sizeof(stats) / sizeof(uint64_t)) * NETFN_FLOW_COOKIE_MAX_STR_LEN; /* Members */
	max_len += NETFN_FLOW_COOKIE_MAX_STR_LEN; /* DB stats start heading */
	max_len += NETFN_FLOW_COOKIE_MAX_STR_LEN; /* DB stats end heading */

	buf = vzalloc(max_len);
	if (!buf) {
		pr_warn("%px: Failed to allocate stats print buffer (%zu)\n", db, max_len);
		return 0;
	}

	len = snprintf(buf, max_len, "\n%lx:Flow cookie stats start:\n", (uintptr_t)db);

	stats_shadow = (uint64_t *)&stats;

	for (i = 0; i < (sizeof(struct netfn_flow_cookie_stats) / sizeof(uint64_t)); i++) {
		len += snprintf(buf + len, max_len - len, "\t[%s]: %llu\n", netfn_flow_cookie_stats_str[i], stats_shadow[i]);
	}

	len += snprintf(buf + len, max_len - len, "\n%lx:Flow cookie stats end:\n", (uintptr_t)db);

	len = simple_read_from_buffer(ubuf, sz, ppos, buf, len);
	vfree(buf);

	return len;
}

/*
 * netfn_flow_cookie_stats_ops()
 *	File operations for Netfn Flow Cookie DB Specific stats.
 */
const struct file_operations netfn_flow_cookie_stats_ops = {
	.open = simple_open,
	.llseek = default_llseek,
	.read = netfn_flow_cookie_stats_print,
	.write = netfn_flow_cookie_stats_clear,
};

/*
 * netfn_flow_cookie_stats_alloc()
 *	Allocate the Netfn Flow Cookie DB Stats and the Infra.
 */
struct netfn_flow_cookie_stats *netfn_flow_cookie_stats_alloc(struct netfn_flow_cookie_db *db)
{
	struct netfn_flow_cookie_stats *stats;

	stats = alloc_percpu_gfp(struct netfn_flow_cookie_stats, GFP_KERNEL | __GFP_ZERO);
	if (!stats) {
		pr_warn("%px: Failed to allocate stats memory\n", db);
		return NULL;
	}

	if (!debugfs_create_file("stats", S_IRUGO, db->dentry, db, &netfn_flow_cookie_stats_ops)) {
		pr_warn("%p: Failed to create file entry for flow_db_dir(%p)\n", db, db->dentry);
		goto fail;
	}

	pr_info("%p:Successfully initialized Flow Cookie Stats\n", db);
	return stats;

fail:
	debugfs_remove_recursive(db->dentry);
	free_percpu(stats);
	return NULL;
}

/*
 * netfn_flow_cookie_stats_free()
 *	De-Initialize the Netfn Flow Cookie DB Stats Infra.
 */
void netfn_flow_cookie_stats_free(struct netfn_flow_cookie_stats *stats)
{
	free_percpu(stats);
}
