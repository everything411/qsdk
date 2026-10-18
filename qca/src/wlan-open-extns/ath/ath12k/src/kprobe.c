// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * kprobe support for ath12k - packet 5-tuple logging via debugfs
 *
 * Usage:
 *   Register:          echo "1 <symbol> skb_arg=<n>" > \
 *                          /sys/kernel/debug/ath12k/kprobe
 *   Unregister one:    echo "0 <symbol>" > /sys/kernel/debug/ath12k/kprobe
 *   Unregister all:    echo "0" > /sys/kernel/debug/ath12k/kprobe
 *   Status:            cat /sys/kernel/debug/ath12k/kprobe
 *
 * skb_arg is mandatory - the 1-based position of the sk_buff argument
 * in the probed function's prototype (1 = first argument, 2 = second, ...).
 */

#include <linux/kprobes.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <net/ip.h>

#include "../core.h"
#include "../debug.h"
#include "kprobe.h"

#ifdef CONFIG_KPROBES

#define ATH12K_KPROBE_SYMBOL_LEN	128
/*
 * symbol, skb ptr, 5-tuple, transport hdr, IP metadata, skb metadata:
 * ~550 B worst-case (IPv6/TCP); remainder reserved for future fields.
 */
#define ATH12K_KPROBE_BUF_SIZE		1024
/*
 * Upper bound on skb_arg, custom modules may have long list of argumnets.
 */
#define ATH12K_KPROBE_MAX_ARGS		15

/**
 * struct ath12k_kprobe_ctx - per-probe context
 * @kp:      embedded kprobe structure
 * @symbol:  symbol name the kprobe is attached to
 * @skb_arg: 1-based argument index of the sk_buff in the probed function
 * @list:    linkage in ath12k_kprobe_list
 *
 * Lifetime: allocated on register, freed only after unregister_kprobe()
 * returns. unregister_kprobe() waits for any in-flight pre-handler to
 * complete, so container_of() access in the pre-handler is always safe.
 */
struct ath12k_kprobe_ctx {
	struct kprobe kp;
	char symbol[ATH12K_KPROBE_SYMBOL_LEN];
	int skb_arg;
	struct list_head list;
};

/* Protects ath12k_kprobe_list for all register/unregister/read operations. */
static DEFINE_MUTEX(ath12k_kprobe_mutex);
static LIST_HEAD(ath12k_kprobe_list);
static struct dentry *kprobe_debugfs_entry;

/**
 * ath12k_kprobe_pre_handler()
 * @p: pointer to the embedded kprobe structure
 * @regs: CPU register state at the probe point
 *
 * Extracts the sk_buff from the argument position given by ctx->skb_arg,
 * then formats and emits a single log line containing skb fields.
 *
 * Only IPv4 and IPv6 packets are logged; IPv6 extension headers are not walked;
 * nexthdr is taken from the fixed IPv6 header only.
 *
 * Return: 0 always (kprobe pre-handlers must return 0)
 */
static int ath12k_kprobe_pre_handler(struct kprobe *p,
				     struct pt_regs *regs)
{
	struct ath12k_kprobe_ctx *ctx =
		container_of(p, struct ath12k_kprobe_ctx, kp);
	struct sk_buff *skb;
	struct iphdr _iph, *iph;
	struct ipv6hdr _ip6h, *ip6h;
	struct tcphdr _tcph, *tcph;
	struct udphdr _udph, *udph;
	__be32 _esp[2], *esp;
	int nh_off, th_off, len = 0;
	u16 frag_off;
	u8 tc, dscp, ecn;
	char *buf;

	/* skb_arg is 1-based; regs_get_kernel_argument() is 0-based */
	skb = (struct sk_buff *)regs_get_kernel_argument(regs, ctx->skb_arg - 1);
	if (!skb)
		return 0;

	nh_off = skb_network_offset(skb);

	/* skb_header_pointer() handles non-linear skbs without pulling data */
	iph = skb_header_pointer(skb, nh_off, sizeof(_iph), &_iph);
	if (!iph)
		return 0;

	buf = kmalloc(ATH12K_KPROBE_BUF_SIZE, GFP_ATOMIC);
	if (!buf)
		return 0;

	len += scnprintf(buf + len, ATH12K_KPROBE_BUF_SIZE - len,
			 "%s: skb=%pK", ctx->symbol, skb);

	if (iph->version == 4) {
		th_off   = nh_off + iph->ihl * 4;
		frag_off = ntohs(iph->frag_off); /* DF/MF flags + 13-bit fragment offset */
		dscp     = (iph->tos >> 2) & 0x3f; /* upper 6 bits of TOS */
		ecn      = iph->tos & 0x3;          /* lower 2 bits of TOS */

		/* Only parse transport header for non-fragmented or first-fragment packets */
		if (iph->protocol == IPPROTO_TCP && !(frag_off & IP_OFFSET)) {
			tcph = skb_header_pointer(skb, th_off,
						  sizeof(_tcph), &_tcph);
			if (tcph)
				len += scnprintf(buf + len,
						 ATH12K_KPROBE_BUF_SIZE - len,
						 " IPv4/TCP src=%pI4:%u dst=%pI4:%u"
						 " tcp_seq=%u tcp_ack=%u tcp_csum=0x%04x",
						 &iph->saddr, ntohs(tcph->source),
						 &iph->daddr, ntohs(tcph->dest),
						 ntohl(tcph->seq), ntohl(tcph->ack_seq),
						 ntohs(tcph->check));
			else
				len += scnprintf(buf + len,
						 ATH12K_KPROBE_BUF_SIZE - len,
						 " IPv4/TCP src=%pI4 dst=%pI4",
						 &iph->saddr, &iph->daddr);
		} else if (iph->protocol == IPPROTO_UDP && !(frag_off & IP_OFFSET)) {
			udph = skb_header_pointer(skb, th_off,
						  sizeof(_udph), &_udph);
			if (udph)
				len += scnprintf(buf + len,
						 ATH12K_KPROBE_BUF_SIZE - len,
						 " IPv4/UDP src=%pI4:%u dst=%pI4:%u"
						 " udp_csum=0x%04x",
						 &iph->saddr, ntohs(udph->source),
						 &iph->daddr, ntohs(udph->dest),
						 ntohs(udph->check));
			else
				len += scnprintf(buf + len,
						 ATH12K_KPROBE_BUF_SIZE - len,
						 " IPv4/UDP src=%pI4 dst=%pI4",
						 &iph->saddr, &iph->daddr);
		} else if (iph->protocol == IPPROTO_ESP && !(frag_off & IP_OFFSET)) {
			esp = skb_header_pointer(skb, th_off,
						 sizeof(_esp), &_esp);
			if (esp)
				len += scnprintf(buf + len,
						 ATH12K_KPROBE_BUF_SIZE - len,
						 " IPv4/ESP src=%pI4 dst=%pI4"
						 " esp_spi=0x%08x esp_seq=%u",
						 &iph->saddr, &iph->daddr,
						 ntohl(esp[0]), ntohl(esp[1]));
			else
				len += scnprintf(buf + len,
						 ATH12K_KPROBE_BUF_SIZE - len,
						 " IPv4/ESP src=%pI4 dst=%pI4",
						 &iph->saddr, &iph->daddr);
		} else {
			len += scnprintf(buf + len, ATH12K_KPROBE_BUF_SIZE - len,
					 " IPv4 src=%pI4 dst=%pI4 ip_proto=%u",
					 &iph->saddr, &iph->daddr, iph->protocol);
		}

		len += scnprintf(buf + len, ATH12K_KPROBE_BUF_SIZE - len,
				 " skb_mark=%u skb_prio=%u",
				 skb->mark, skb->priority);

		len += scnprintf(buf + len, ATH12K_KPROBE_BUF_SIZE - len,
				 " ip_tos=0x%02x dscp=%u ecn=%u ip_ttl=%u"
				 " ip_id=0x%04x dont_frag=%d more_frags=%d"
				 " frag_off_bytes=%u",
				 iph->tos, dscp, ecn, iph->ttl, ntohs(iph->id),
				 !!(frag_off & IP_DF), !!(frag_off & IP_MF),
				 (frag_off & IP_OFFSET) << 3);

	} else if (iph->version == 6) {
		ip6h = skb_header_pointer(skb, nh_off, sizeof(_ip6h), &_ip6h);
		if (!ip6h) {
			kfree(buf);
			return 0;
		}

		th_off = nh_off + sizeof(struct ipv6hdr);

		tc   = (ip6h->priority << 4) | (ip6h->flow_lbl[0] >> 4);
		dscp = tc >> 2;
		ecn  = tc & 0x3;

		if (ip6h->nexthdr == IPPROTO_TCP) {
			tcph = skb_header_pointer(skb, th_off,
						  sizeof(_tcph), &_tcph);
			if (tcph)
				len += scnprintf(buf + len,
						 ATH12K_KPROBE_BUF_SIZE - len,
						 " IPv6/TCP src=%pI6c:%u dst=%pI6c:%u"
						 " tcp_seq=%u tcp_ack=%u tcp_csum=0x%04x",
						 &ip6h->saddr, ntohs(tcph->source),
						 &ip6h->daddr, ntohs(tcph->dest),
						 ntohl(tcph->seq), ntohl(tcph->ack_seq),
						 ntohs(tcph->check));
			else
				len += scnprintf(buf + len,
						 ATH12K_KPROBE_BUF_SIZE - len,
						 " IPv6/TCP src=%pI6c dst=%pI6c",
						 &ip6h->saddr, &ip6h->daddr);
		} else if (ip6h->nexthdr == IPPROTO_UDP) {
			udph = skb_header_pointer(skb, th_off,
						  sizeof(_udph), &_udph);
			if (udph)
				len += scnprintf(buf + len,
						 ATH12K_KPROBE_BUF_SIZE - len,
						 " IPv6/UDP src=%pI6c:%u dst=%pI6c:%u"
						 " udp_csum=0x%04x",
						 &ip6h->saddr, ntohs(udph->source),
						 &ip6h->daddr, ntohs(udph->dest),
						 ntohs(udph->check));
			else
				len += scnprintf(buf + len,
						 ATH12K_KPROBE_BUF_SIZE - len,
						 " IPv6/UDP src=%pI6c dst=%pI6c",
						 &ip6h->saddr, &ip6h->daddr);
		} else if (ip6h->nexthdr == IPPROTO_ESP) {
			esp = skb_header_pointer(skb, th_off,
						 sizeof(_esp), &_esp);
			if (esp)
				len += scnprintf(buf + len,
						 ATH12K_KPROBE_BUF_SIZE - len,
						 " IPv6/ESP src=%pI6c dst=%pI6c"
						 " esp_spi=0x%08x esp_seq=%u",
						 &ip6h->saddr, &ip6h->daddr,
						 ntohl(esp[0]), ntohl(esp[1]));
			else
				len += scnprintf(buf + len,
						 ATH12K_KPROBE_BUF_SIZE - len,
						 " IPv6/ESP src=%pI6c dst=%pI6c",
						 &ip6h->saddr, &ip6h->daddr);
		} else {
			len += scnprintf(buf + len, ATH12K_KPROBE_BUF_SIZE - len,
					 " IPv6 src=%pI6c dst=%pI6c ip6_nexthdr=%u",
					 &ip6h->saddr, &ip6h->daddr, ip6h->nexthdr);
		}

		len += scnprintf(buf + len, ATH12K_KPROBE_BUF_SIZE - len,
				 " skb_mark=%u skb_prio=%u",
				 skb->mark, skb->priority);

		len += scnprintf(buf + len, ATH12K_KPROBE_BUF_SIZE - len,
				 " ip6_tc=0x%02x dscp=%u ecn=%u"
				 " hop_limit=%u ip6_payload_len=%u",
				 tc, dscp, ecn, ip6h->hop_limit,
				 ntohs(ip6h->payload_len));
	} else {
		kfree(buf);
		return 0;
	}

	len += scnprintf(buf + len, ATH12K_KPROBE_BUF_SIZE - len,
			 " eth_proto=0x%04x queue_mapping=%u ip_summed=%u",
			 ntohs(skb->protocol), skb->queue_mapping, skb->ip_summed);

	len += scnprintf(buf + len, ATH12K_KPROBE_BUF_SIZE - len,
			 " skb_len=%u data_len=%u skb_headlen=%u truesize=%u",
			 skb->len, skb->data_len, skb_headlen(skb), skb->truesize);

	len += scnprintf(buf + len, ATH12K_KPROBE_BUF_SIZE - len,
			 " is_clone=%d is_shared=%d is_nonlinear=%d nr_frags=%u",
			 skb_cloned(skb), skb_shared(skb),
			 skb_is_nonlinear(skb), skb_shinfo(skb)->nr_frags);

	ath12k_info(NULL, "%s\n", buf);

	kfree(buf);
	return 0;
}

/**
 * ath12k_kprobe_register() - allocate and register a single kprobe
 * @symbol: symbol name to probe
 * @skb_arg: 1-based skb argument index
 *
 * Must be called with ath12k_kprobe_mutex held.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_kprobe_register(const char *symbol, int skb_arg)
{
	struct ath12k_kprobe_ctx *ctx;
	int ret;

	/* Reject duplicate registrations for the same symbol */
	list_for_each_entry(ctx, &ath12k_kprobe_list, list) {
		if (!strcmp(ctx->symbol, symbol)) {
			pr_warn("kprobe already registered for %s\n",
				symbol);
			return -EEXIST;
		}
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	strscpy(ctx->symbol, symbol, ATH12K_KPROBE_SYMBOL_LEN);
	ctx->skb_arg = skb_arg;
	ctx->kp.symbol_name = ctx->symbol;
	ctx->kp.pre_handler = ath12k_kprobe_pre_handler;
	INIT_LIST_HEAD(&ctx->list);

	ret = register_kprobe(&ctx->kp);
	if (ret < 0) {
		ath12k_err(NULL, "failed to register kprobe for %s: %d\n",
			   symbol, ret);
		kfree(ctx);
		return ret;
	}

	list_add_tail(&ctx->list, &ath12k_kprobe_list);
	ath12k_info(NULL, "kprobe registered for %s skb_arg=%d at %pK\n",
		    ctx->symbol, ctx->skb_arg, ctx->kp.addr);
	return 0;
}

/**
 * ath12k_kprobe_unregister_symbol() - unregister kprobe for a specific symbol
 * @symbol: symbol name to unregister, or NULL to unregister all
 *
 * Must be called with ath12k_kprobe_mutex held.
 * unregister_kprobe() waits for in-flight handlers before returning,
 * so kfree(ctx) after it is always safe.
 */
static void ath12k_kprobe_unregister_symbol(const char *symbol)
{
	struct ath12k_kprobe_ctx *ctx, *tmp;

	list_for_each_entry_safe(ctx, tmp, &ath12k_kprobe_list, list) {
		if (symbol && strcmp(ctx->symbol, symbol))
			continue;

		list_del(&ctx->list);
		/*
		 * unregister_kprobe() waits for any in-flight pre-handler
		 * to complete before returning - safe to free ctx after this.
		 */
		unregister_kprobe(&ctx->kp);
		ath12k_info(NULL, "kprobe unregistered for %s\n", ctx->symbol);
		kfree(ctx);

		if (symbol)
			break;
	}
}

/**
 * ath12k_kprobe_write() - debugfs write handler
 * @file: file pointer
 * @ubuf: user buffer
 * @count: number of bytes to write
 * @ppos: file position
 *
 * Accepts the following forms:
 *   "1 <symbol> skb_arg=<n>"  - register kprobe (skb_arg is 1-based, mandatory)
 *   "0 <symbol>"               - unregister kprobe for <symbol>
 *   "0"                        - unregister all kprobes
 *
 * Return: number of bytes consumed, or negative error code
 */
static ssize_t ath12k_kprobe_write(struct file *file,
				   const char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	char buf[ATH12K_KPROBE_SYMBOL_LEN + 32];
	char symbol[ATH12K_KPROBE_SYMBOL_LEN];
	int enable, skb_arg, ret, n;

	if (count == 0 || count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, count))
		return -EFAULT;

	buf[count] = '\0';
	/* Strip trailing newline if present */
	if (buf[count - 1] == '\n')
		buf[count - 1] = '\0';

	/* Parse: "1 <symbol> skb_arg=<n>" or "0 [<symbol>]" */
	symbol[0] = '\0';
	skb_arg = 0;
	n = sscanf(buf, "%d %127s skb_arg=%d", &enable, symbol, &skb_arg);
	if (n < 1)
		return -EINVAL;

	if (enable == 1) {
		if (n < 2 || symbol[0] == '\0') {
			ath12k_err(NULL,
				   "usage: echo \"1 <symbol> skb_arg=<n>\" > kprobe\n");
			return -EINVAL;
		}
		if (n < 3) {
			pr_warn("skb_arg not specified for '%s': "
				"use: echo \"1 <symbol> skb_arg=<n>\" > kprobe"
				" (n is 1-based: 1=first arg, 2=second, ...)\n",
				symbol);
			return -EINVAL;
		}
		if (skb_arg < 1 || skb_arg > ATH12K_KPROBE_MAX_ARGS) {
			pr_warn("skb_arg must be between 1 and %d\n",
				ATH12K_KPROBE_MAX_ARGS);
			return -EINVAL;
		}

		mutex_lock(&ath12k_kprobe_mutex);
		ret = ath12k_kprobe_register(symbol, skb_arg);
		mutex_unlock(&ath12k_kprobe_mutex);

		if (ret && ret != -EEXIST)
			return ret;
	} else if (enable == 0) {
		mutex_lock(&ath12k_kprobe_mutex);
		/* "0 <symbol>" unregisters one; bare "0" unregisters all */
		ath12k_kprobe_unregister_symbol(symbol[0] ? symbol : NULL);
		mutex_unlock(&ath12k_kprobe_mutex);
	} else {
		return -EINVAL;
	}

	return count;
}

/**
 * ath12k_kprobe_read() - debugfs read handler
 * @file: file pointer
 * @ubuf: user buffer to write into
 * @count: size of user buffer
 * @ppos: file position
 *
 * Lists all registered kprobes, one per line:
 *   "1 <symbol> skb_arg=<n>"
 * Returns "0\n" if no kprobes are registered.
 *
 * Return: number of bytes read, or negative error code
 */
static ssize_t ath12k_kprobe_read(struct file *file,
				  char __user *ubuf,
				  size_t count, loff_t *ppos)
{
	struct ath12k_kprobe_ctx *ctx;
	char *buf;
	int buf_size = PAGE_SIZE;
	int len = 0;
	ssize_t ret;

	buf = kzalloc(buf_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mutex_lock(&ath12k_kprobe_mutex);
	if (list_empty(&ath12k_kprobe_list)) {
		len = scnprintf(buf, buf_size, "0\n");
	} else {
		list_for_each_entry(ctx, &ath12k_kprobe_list, list) {
			len += scnprintf(buf + len, buf_size - len,
					 "1 %s skb_arg=%d\n",
					 ctx->symbol, ctx->skb_arg);
		}
	}
	mutex_unlock(&ath12k_kprobe_mutex);

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

static const struct file_operations ath12k_kprobe_fops = {
	.read   = ath12k_kprobe_read,
	.write  = ath12k_kprobe_write,
	.open   = simple_open,
	.owner  = THIS_MODULE,
	.llseek = default_llseek,
};

/**
 * ath12k_kprobe_init() - initialize kprobe debugfs entry
 *
 * Creates /sys/kernel/debug/ath12k/kprobe under the top-level ath12k
 * debugfs directory. The ath12k directory must already exist (created
 * by the main ath12k driver); if it is absent this function returns
 * -ENOENT without creating anything.
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_kprobe_init(void)
{
	struct dentry *debugfs_ath12k;

	/*
	 * The top-level ath12k debugfs directory is owned by the main
	 * ath12k driver. debugfs_lookup() increments its reference count;
	 * dput() is required after we are done with the dentry.
	 */
	debugfs_ath12k = debugfs_lookup("ath12k", NULL);
	if (!debugfs_ath12k) {
		ath12k_err(NULL, "ath12k debugfs dir not found\n");
		return -ENOENT;
	}

	kprobe_debugfs_entry = debugfs_create_file("kprobe", 0600,
						   debugfs_ath12k, NULL,
						   &ath12k_kprobe_fops);
	dput(debugfs_ath12k);

	if (IS_ERR(kprobe_debugfs_entry)) {
		ath12k_err(NULL, "failed to create kprobe debugfs entry\n");
		kprobe_debugfs_entry = NULL;
		return PTR_ERR(kprobe_debugfs_entry);
	}

	ath12k_info(NULL, "kprobe debugfs entry created\n");
	return 0;
}

/**
 * ath12k_kprobe_exit() - clean up all kprobes and the debugfs entry
 *
 * Unregisters all active kprobes and removes the debugfs entry.
 * Called during module unload.
 */
void ath12k_kprobe_exit(void)
{
	mutex_lock(&ath12k_kprobe_mutex);
	ath12k_kprobe_unregister_symbol(NULL);
	mutex_unlock(&ath12k_kprobe_mutex);

	debugfs_remove(kprobe_debugfs_entry);
	kprobe_debugfs_entry = NULL;
}

#endif /* CONFIG_KPROBES */
