/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_sk_offload_stats.c
 *      Network Sock Offload Flow Statistics.
 */

#include <linux/ip.h>
#include <linux/debugfs.h>
#include <linux/netdevice.h>

#include "netfn_sk_offload_priv.h"

/*
 * netfn_sk_offload_stats_str
 *      Debug statistics global.
 */
static const char *netfn_sk_offload_stats_str[] __attribute__((unused)) = {
	"Total connections",		/* Total connections */
	"Ctx Alloc Success",		/* Context alloc success */
	"Ctx Alloc fail No Memory",	/* Context alloc fail due to no memory */
	"Ctx Alloc fail Max Conns",	/* Context alloc fail due to Max Conns */
	"Ctx Alloc fail Stats",		/* Context alloc fail due to stats failure */
	"Ctx Alloc fail Rule",		/* Context alloc fail due to Rule failure */
	"Ctx Alloc fail Sanity",	/* Context alloc fail due to Sanity failure */
	"Ctx Dealloc Success",		/* Context dealloc success */
	"Rx Packets",			/* Rx pkts received */
	"Rx Bytes",			/* Rx bytes received */
#ifdef NETFN_SK_OFFLOAD_HW_GRO
	"HW GRO Rx Packets",		/* GRO Rx pkts received */
	"HW GRO Rx Bytes",			/* GRO Rx bytes received */
#endif
	"Tx Packets",			/* Tx pkts sent */
	"Tx Bytes",			/* Tx bytes sent */
	"Ctx retrieve failures",	/* Context retrieve failures */
	"Insufficient hdr_len failures",	/* Insufficient header length failures */
	"Total packets dropped",	/* Total pkts dropped */
};

/*
 * netfn_sk_offload_stats_ctx_str
 *      Debug statistics per connection.
 */
static const char *netfn_sk_offload_stats_ctx_str[] __attribute__((unused)) = {
	"Rx packets",				/* Rx pkts received */
	"Rx Bytes",				/* Rx Bytes received */
#ifdef NETFN_SK_OFFLOAD_HW_GRO
	"HW GRO Rx packets",			/* Rx pkts received */
	"HW GRO Rx Bytes",				/* Rx Bytes received */
#endif
	"Dst miss packets",			/* Destination failure packets */
	"Tx packets",				/* Tx pkts sent */
	"Tx Bytes",				/* Tx Bytes sent */
	"Coalesced packets",			/* Total pkts coalesced to head */
	"Coalesced start packets",		/* Total pkts started coalesced */
	"Coalesce failed packets",		/* Total pkts failed to coalesced */
	"Coalesce start failed packets",	/* Total pkts failed to start coalescing (GRO_SKIP: no gro_more) */
	"Napi flush packets",			/* GRO flushed by Linux Napi complete */
	"XDP frame convert failed",		/* xdp_convert_buff_to_frame() failed (tailroom check) */
	"SKB build failed",			/* napi_build_skb() failed for page pool SKB */
	"Scatter-gather fragments received",	/* Scatter-gather (multi-buffer) fragments received */
	"Scatter-gather fragments delivered",	/* Scatter-gather fragments delivered to Linux */
};

/*
 * netfn_sk_offload_stats_get()
 *      API to retrieve the Global Sock Offload stats.
 *
 * Note: All the Per CPU stats will be accumulated here.
 */
static void netfn_sk_offload_stats_get(struct netfn_sk_offload *so, struct netfn_sk_offload_stats *stats)
{
        int dwords, cpu;

        dwords = (sizeof(*stats) / sizeof(uint64_t));

        /*
         * All statistics are 64bit. So we can just iterate by words.
         */
        for_each_possible_cpu(cpu) {
                struct netfn_sk_offload_stats *sp = per_cpu_ptr(so->stats, cpu);
                uint64_t *stats_ptr = (uint64_t *)stats;
                uint64_t *sp_ptr = (uint64_t *)sp;
                int i;

                for (i = 0; i < dwords; i++, stats_ptr++, sp_ptr++)
                        *stats_ptr += *sp_ptr;
        }
}

/*
 * netfn_sk_offload_stats_show()
 *      API to dump the sock offload stats in the appropriate file.
 */
static int netfn_sk_offload_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct netfn_sk_offload *so = &g_sk_offload;
	struct netfn_sk_offload_stats *stats;
	uint64_t *stats_shadow;
	uint32_t i;

#define __MAX_STATS_ENTRY (sizeof(struct netfn_sk_offload_stats) / sizeof(uint64_t))
#define __MAX_STATS_STR_ENTRY ARRAY_SIZE(netfn_sk_offload_stats_str)

	BUILD_BUG_ON(__MAX_STATS_ENTRY != __MAX_STATS_STR_ENTRY);

	stats = vzalloc(sizeof(struct netfn_sk_offload_stats));
	if (!stats) {
		return -ENOMEM;
	}

	netfn_sk_offload_stats_get(so, stats);

	seq_printf(m, "\n:Global Sock Offload stats start:\n");

	stats_shadow = (uint64_t *)stats;

	for (i = 0; i < __MAX_STATS_ENTRY; i++) {
		seq_printf(m, "\t[%s]: %llu\n", netfn_sk_offload_stats_str[i], stats_shadow[i]);
	}

	seq_printf(m, "\n:Global Sock Offload stats end:\n");
	vfree(stats);

	return 0;
}

/*
 * netfn_sk_offload_stats_open()
 *      Netfn Sock Offload stats open callback
 */
static int netfn_sk_offload_stats_open(struct inode *inode, struct file *file)
{
        return single_open(file, netfn_sk_offload_stats_show, inode->i_private);
}

/*
 * netfn_sk_offload_stats_ops()
 *      File operations for Netfn Sock Offload Global stats.
 */
const struct file_operations netfn_sk_offload_stats_ops = {
        .open = netfn_sk_offload_stats_open,
	.read = seq_read,
        .llseek = seq_lseek,
	.release = seq_release,
};

/*
 * netfn_sk_offload_stats_ctx_get()
 *      API to retrieve the Connection specific Sock Offload stats.
 *
 * Note: All the Per CPU stats will be accumulated here.
 */
static void netfn_sk_offload_stats_ctx_get(struct netfn_sk_offload_ctx *ctx, struct netfn_sk_offload_stats_ctx *stats)
{
        int dwords, cpu;

        dwords = (sizeof(*stats) / sizeof(uint64_t));

        /*
         * All statistics are 64bit. So we can just iterate by words.
         */
        for_each_possible_cpu(cpu) {
                struct netfn_sk_offload_stats_ctx *sp = per_cpu_ptr(ctx->stats, cpu);
                uint64_t *stats_ptr = (uint64_t *)stats;
                uint64_t *sp_ptr = (uint64_t *)sp;
                int i;

                for (i = 0; i < dwords; i++, stats_ptr++, sp_ptr++)
                        *stats_ptr += *sp_ptr;
        }
}

/*
 * netfn_sk_offload_stats_ctx_print()
 *      API to print the sock offload stats in the appropriate file.
 */
static int netfn_sk_offload_stats_ctx_print(struct seq_file *m, struct netfn_sk_offload_ctx *ctx,
					struct netfn_sk_offload_stats_ctx *stats)
{
	struct netfn_tuple_5tuple *t;
        uint64_t *stats_shadow;
        uint32_t i;

#define __MAX_STATS_CTX_ENTRY (sizeof(struct netfn_sk_offload_stats_ctx) / sizeof(uint64_t))
#define __MAX_STATS_CTX_STR_ENTRY ARRAY_SIZE(netfn_sk_offload_stats_ctx_str)

	BUILD_BUG_ON(__MAX_STATS_CTX_ENTRY != __MAX_STATS_CTX_STR_ENTRY);

	t = &ctx->tuple.tuples.tuple_5;

	memset(stats, 0, sizeof(*stats));
        netfn_sk_offload_stats_ctx_get(ctx, stats);

        seq_printf(m, "\n:Connection_%d Sock Offload stats start:\n", ctx->db_idx);

	if(ctx->tuple.ip_version == IPVERSION) {
		seq_printf(m, "\nsip_address: %pI4", &t->src_ip);
		seq_printf(m, "\ndip_address: %pI4", &t->dest_ip);
	} else if (ctx->tuple.ip_version == 6) {
		seq_printf(m, "\nsip_address: %pI6", &t->src_ip);
		seq_printf(m, "\ndip_address: %pI6", &t->dest_ip);
	}

	seq_printf(m, "\nsrc_port: %u", ntohs(t->l4_src_ident));
	seq_printf(m, "\ndst_port: %u", ntohs(t->l4_dest_ident));
	seq_printf(m, "\nprotocol: %u", t->protocol);
	seq_printf(m, "\nip_ver: %u\n", ctx->tuple.ip_version);

        stats_shadow = (uint64_t *)stats;

        for (i = 0; i < __MAX_STATS_CTX_ENTRY; i++) {
                seq_printf(m, "\t[%s]: %llu\n", netfn_sk_offload_stats_ctx_str[i], stats_shadow[i]);
        }

        seq_printf(m, "\n:Connection_%d Sock Offload stats end:\n", ctx->db_idx);

        return 0;
}

/*
 * netfn_sk_offload_stats_ctx_show()
 *      API to dump the sock offload stats in the appropriate file.
 */
static int netfn_sk_offload_stats_ctx_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct netfn_sk_offload *so = &g_sk_offload;
	struct netfn_sk_offload_db_entry *db = so->db;
        struct netfn_sk_offload_stats_ctx *stats;
	struct netfn_sk_offload_ctx *ctx = NULL;
	uint8_t index;

	stats = vzalloc(sizeof(struct netfn_sk_offload_stats_ctx));
	if (!stats) {
		return -ENOMEM;
	}

	rcu_read_lock_bh();
	for_each_set_bit(index, so->sk_bitmap, NETFN_SK_OFFLOAD_MAX_CONNS) {
		ctx = rcu_dereference(db[index].ctx);
		netfn_sk_offload_stats_ctx_print(m, ctx, stats);
	}
	rcu_read_unlock_bh();

	vfree(stats);
	return 0;
}

/*
 * netfn_sk_offload_stats_ctx_open()
 *      Netfn Sock Offload stats Rx open callback
 */
static int netfn_sk_offload_stats_ctx_open(struct inode *inode, struct file *file)
{
        return single_open(file, netfn_sk_offload_stats_ctx_show, inode->i_private);
}

/*
 * netfn_sk_offload_stats_ctx_ops()
 *      File operations for Netfn Sock Offload Connection Specific stats.
 */
const struct file_operations netfn_sk_offload_stats_ctx_ops = {
        .open = netfn_sk_offload_stats_ctx_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release,
};

/*
 * netfn_sk_offload_stats_init()
 *	Initialize the Netfn Sock Offload Global Stats.
 */
int netfn_sk_offload_stats_init(void)
{
	struct netfn_sk_offload *so = &g_sk_offload;

	if (!debugfs_create_file("stats", S_IRUGO, so->dentry, so, &netfn_sk_offload_stats_ops)) {
		pr_warn("%p: Failed to create stats file entry for dir(%p)\n", so, so->dentry);
		return -1;
	}

	pr_info("%p:Successfully initialized Global Sock Offload Stats\n", so);

	if (!debugfs_create_file("ctx_stats", S_IRUGO, so->dentry, so, &netfn_sk_offload_stats_ctx_ops)) {
		pr_warn("%p: Failed to create ctx stats file entry for dir(%p)\n", so, so->dentry);
		return -1;
	}

	pr_info("%p:Successfully initialized Connection specific Sock Offload Stats\n", so);

	return 0;
}

/*
 * netfn_sk_offload_stats_deinit()
 *	De-Initialize the Netfn Sock Offload Global Stats.
 */
void netfn_sk_offload_stats_deinit(void)
{
	struct netfn_sk_offload *so = &g_sk_offload;

	if (so->dentry)
		debugfs_remove_recursive(so->dentry);
	return;
}
