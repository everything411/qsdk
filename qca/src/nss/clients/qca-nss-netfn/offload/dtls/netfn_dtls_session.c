/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/rculist.h>
#include <net/addrconf.h>
#include <crypto/aes.h>
#include <linux/ip.h>

#include "netfn_dtls_priv.h"

/*
 * netfn_dtls_session_reset_stats()
 *	reset the summary stats.
 */
static void netfn_dtls_session_reset_stats(struct netfn_dtls_session *ses)
{
	int words;
	int cpu;
	int i;

	words = (sizeof(struct netfn_dtls_session_stats) / sizeof(uint64_t));

	/*
	 * All statistics are 64bit. So we can just iterate by words.
	 */
	for_each_possible_cpu(cpu) {
		const struct netfn_dtls_session_stats *sp = per_cpu_ptr(ses->stats_pcpu, cpu);
		uint64_t *sp_ptr = (uint64_t *)sp;

		for (i = 0; i < words; i++, sp_ptr++)
			*sp_ptr = 0;
	}
}

/*
 * netfn_dtls_session_get_stats()
 *	Update the summary stats.
 */
static void netfn_dtls_session_get_stats(struct netfn_dtls_session *ses,
		struct netfn_dtls_session_stats *stats)
{
	int words;
	int cpu;
	int i;

	words = (sizeof(*stats) / sizeof(uint64_t));
	memset(stats, 0, sizeof(*stats));

	/*
	 * All statistics are 64bit. So we can just iterate by words.
	 */
	for_each_possible_cpu(cpu) {
		const struct netfn_dtls_session_stats *sp = per_cpu_ptr(ses->stats_pcpu, cpu);
		uint64_t *stats_ptr = (uint64_t *)stats;
		uint64_t *sp_ptr = (uint64_t *)sp;

		for (i = 0; i < words; i++, stats_ptr++, sp_ptr++)
			*stats_ptr += *sp_ptr;
	}
}

/*
 * netfn_dtls_session_read()
 *	Read the all Session statistics in provided buffer.
 */
static ssize_t netfn_dtls_session_read(struct file *fp, char __user *ubuf, size_t sz, loff_t *ppos)
{
	struct netfn_dtls_session *ses = fp->private_data;
	struct netfn_dtls_session_stats stats;
	struct netfn_tuple_5tuple *t;
	char *hdr, *buf, *format;
	ssize_t max_len = 0;
	uint8_t ip_version;
	ssize_t len = 0;
	int i;

	t = &ses->tuple.tuples.tuple_5;
	ip_version = ses->tuple.ip_version;

	hdr = (ses->flags & NETFN_DTLS_FLAG_ENC) ? "Encap" : "Decap";
	netfn_dtls_session_get_stats(ses, &stats);

	/*
	 * We need to calculate required string buffer for stats, else full stats may not be captured.
	 */
	max_len = (sizeof(stats) / sizeof(uint64_t)) * NETFN_DTLS_MAX_STR_LEN;
	max_len += NETFN_DTLS_MAX_STR_LEN; /* heading */

	buf = vzalloc(max_len);
	if (!buf) {
		pr_warn("%px: failed to allocate print buffer (%zu)", ses, max_len);
		return 0;
	}


	if (ip_version == IPVERSION)
		format = "%s Session%u (src:%pI4n dst:%pI4n epoch:0x%X sport:%u dport:%u proto:%u version:0x%X flags:0x%X):\n";
	else
		format = "%s Session%u (src:%pI6 dst:%pI6 epoch:0x%X sport:%u dport:%u proto:%u version:0x%X flags:0x%X):\n";

	len += snprintf(buf + len, max_len - len, format,
				hdr, ++i, &t->src_ip, &t->dest_ip, ntohs(ses->epoch), ntohs(t->l4_src_ident),
				ntohs(t->l4_dest_ident), t->protocol, ip_version, ses->flags);

	len += snprintf(buf + len, max_len - len, "\tTx packets: %llu\n", stats.tx_pkts);
	len += snprintf(buf + len, max_len - len, "\tTx bytes: %llu\n", stats.tx_bytes);
	len += snprintf(buf + len, max_len - len, "\tRx packets: %llu\n", stats.rx_pkts);
	len += snprintf(buf + len, max_len - len, "\tRx bytes: %llu\n", stats.rx_bytes);
	len += snprintf(buf + len, max_len - len, "\tEnqueue error: %llu\n", stats.fail_enqueue);
	len += snprintf(buf + len, max_len - len, "\tTransformation error: %llu\n", stats.fail_transform);

	len = simple_read_from_buffer(ubuf, sz, ppos, buf, len);
	vfree(buf);

	return len;
}

/*
 * netfn_dtls_session_write()
 *	Read the all Session statistics in provided buffer.
 */
static ssize_t netfn_dtls_session_write(struct file *fp, const char __user *ubuf, size_t sz, loff_t *ppos)
{
	struct netfn_dtls_session *ses = fp->private_data;
	uint32_t reset;

	if (kstrtou32_from_user(ubuf, sz, 0, &reset)) {
		return -EINVAL;
	}

	if (reset != 0) {
		return -EINVAL;
	}

	netfn_dtls_session_reset_stats(ses);

	return sz;
}

/*
 * stats callback.
 */
const struct file_operations netfn_dtls_ses_file_ops = {
	.open = simple_open,
	.llseek = default_llseek,
	.read = netfn_dtls_session_read,
	.write = netfn_dtls_session_write
};

/*
 * netfn_dtls_session_fill_tr()
 *	Fill TR infor using ses data passed by user.
 */
static void netfn_dtls_session_fill_tr(struct netfn_dtls_cfg *cfg, netfn_tuple_t *t, struct eip_tr_info *tr_info)
{
	struct netfn_tuple_5tuple *t5 = &t->tuples.tuple_5;
	struct eip_tr_info_dtls *dtls = &tr_info->dtls;
	struct eip_tr_base *base = &tr_info->base;
	uint32_t flags = cfg->flags;

	base->svc = NETFN_DTLS_DEFAULT_SVC;
	strlcpy(base->alg_name, cfg->base.algo_name, CRYPTO_MAX_ALG_NAME);
	base->cipher.key_data = cfg->base.cipher.key_data;
	base->cipher.key_len = cfg->base.cipher.key_len;
	base->auth.key_data = cfg->base.auth.key_data;
	base->auth.key_len = cfg->base.auth.key_len;
	base->nonce = cfg->base.nonce;

	dtls->flags |= (flags & NETFN_DTLS_FLAG_ENC) ? EIP_TR_DTLS_FLAG_ENC : 0;
	dtls->flags |= (flags & NETFN_DTLS_FLAG_IPV6) ? EIP_TR_DTLS_FLAG_IPV6 : 0;
	dtls->flags |= (flags & NETFN_DTLS_FLAG_UDPLITE) ? EIP_TR_DTLS_FLAG_UDPLITE : 0;
	dtls->flags |= (flags & NETFN_DTLS_FLAG_UDPLITE_CSUM) ? EIP_TR_DTLS_FLAG_UDPLITE_CSUM: 0;
	dtls->flags |= (flags & NETFN_DTLS_FLAG_CAPWAP) ? EIP_TR_DTLS_FLAG_CAPWAP : 0;
	dtls->flags |= (flags & NETFN_DTLS_FLAG_CP_TOS) ? EIP_TR_DTLS_FLAG_CP_TOS : 0;
	dtls->flags |= (flags & NETFN_DTLS_FLAG_CP_DF) ? EIP_TR_DTLS_FLAG_CP_DF : 0;
	dtls->ip_ttl = cfg->hop_limit;
	dtls->ip_df = (flags & NETFN_DTLS_FLAG_CP_DF) ? 0 : cfg->df;
	dtls->ip_dscp = (flags & NETFN_DTLS_FLAG_CP_TOS) ? 0 : cfg->tos;

	switch (cfg->replay_win) {
	case 0:
		dtls->replay = EIP_DTLS_REPLAY_NONE;
		break;
	case 8:
		dtls->replay = EIP_DTLS_REPLAY_64;
		break;
	default:
		/*
		 * We already have replay check in caller function.
		 */
		WARN_ON(1);
		break;
	}

	dtls->version = ntohs(cfg->version);	/* Version is stored in host format */
	dtls->epoch = cfg->epoch;
	dtls->src_port = t5->l4_src_ident;
	dtls->dst_port = t5->l4_dest_ident;

	if (t->ip_version == IPVERSION) {
		dtls->ip_ver = IPVERSION;
		dtls->src_ip[0] = t5->src_ip.ip4.s_addr;
		dtls->dst_ip[0] = t5->dest_ip.ip4.s_addr;
	} else {
		dtls->ip_ver = 6;
		dtls->src_ip[0] = t5->src_ip.ip6.s6_addr32[0];
		dtls->src_ip[1] = t5->src_ip.ip6.s6_addr32[1];
		dtls->src_ip[2] = t5->src_ip.ip6.s6_addr32[2];
		dtls->src_ip[3] = t5->src_ip.ip6.s6_addr32[3];
		dtls->dst_ip[0] = t5->dest_ip.ip6.s6_addr32[0];
		dtls->dst_ip[1] = t5->dest_ip.ip6.s6_addr32[1];
		dtls->dst_ip[2] = t5->dest_ip.ip6.s6_addr32[2];
		dtls->dst_ip[3] = t5->dest_ip.ip6.s6_addr32[3];
	}
}

/*
 * netfn_dtls_session_get_overhead()
 *	Get Header overhead.
 */
uint16_t netfn_dtls_session_get_overhead(struct netfn_dtls_session *ses)
{
	bool is_capwap = ses->flags & NETFN_DTLS_FLAG_CAPWAP;
	bool is_ipv6 = ses->flags & NETFN_DTLS_FLAG_IPV6;
	struct eip_tr_algo_info algo = {0};
	uint16_t overhead;

	/*
	 * Calculate Encapsulation overhead.
	 */
	overhead = sizeof(struct udphdr) + sizeof(struct eip_dtls_hdr);
	overhead += is_ipv6 ? sizeof(struct ipv6hdr) : sizeof(struct iphdr);
	overhead += is_capwap ? sizeof(struct eip_capwap_hdr) : 0;

	eip_tr_get_algo_info(ses->tr, &algo);
	overhead += (algo.blk_len + algo.iv_len + algo.hmac_len);

	return overhead;
}

/*
 * netfn_dtls_session_alloc()
 *	Create a dtls sesison under netdevice.
 */
struct netfn_dtls_session *netfn_dtls_session_alloc(struct netfn_dtls_cfg *cfg, netfn_tuple_t *t, struct netfn_dtls_tun *tun)
{
	struct netfn_tuple_5tuple *t5 = &t->tuples.tuple_5;
	struct netfn_dtls_drv *drv = &g_dtls_drv;
	struct net_device *dev = tun->dev;
	struct eip_tr_info tr_info = {0};
	struct netfn_dtls_session *ses;
	char name[] = "epochXXX";
	bool udplite;
	bool capwap;
	bool encap;

	encap = (cfg->flags & NETFN_DTLS_FLAG_ENC);
	capwap = (cfg->flags & NETFN_DTLS_FLAG_CAPWAP);
	udplite = (cfg->flags & NETFN_DTLS_FLAG_UDPLITE);

	pr_debug("%p: algo_name %s, flags %u replay_win %u df %u tos %u hop_limit %u\n",
		dev, cfg->base.algo_name, cfg->flags, cfg->replay_win, cfg->df,
		cfg->tos, cfg->hop_limit);

	if (t->ip_version == IPVERSION) {
		pr_debug("%p: src:%pI4n dst:%pI4n epoch:0x%X sport:%u dport:%u proto:%u\n",
				dev, &t5->src_ip.ip4.s_addr, &t5->dest_ip.ip4.s_addr, ntohs(cfg->epoch), ntohs(t5->l4_src_ident), ntohs(t5->l4_dest_ident), t5->protocol);
	} else {
		pr_debug("%p: src:%pI6 dst:%pI6 epoch:0x%X sport:%u dport:%u proto:%u\n",
				dev, t5->src_ip.ip6.s6_addr32, t5->dest_ip.ip6.s6_addr32, ntohs(cfg->epoch), ntohs(t5->l4_src_ident), ntohs(t5->l4_dest_ident), t5->protocol);
	}

	/*
	 * Sanity checks:
	 *	Algorithm is supported by hardware.
	 *	epoch index has to be unique for database.
	 */
	if (!eip_ctx_algo_supported(drv->ctx, cfg->base.algo_name)) {
		pr_err("%px: Session algorithm not supported(%s)\n", dev, cfg->base.algo_name);
		return NULL;
	}

	if (cfg->replay_win != 0 && cfg->replay_win != 8) {
		pr_err("%px: Invalid replay size(%u)\n", dev, cfg->replay_win);
		return NULL;
	}

	ses = kzalloc(sizeof(*ses), GFP_KERNEL);
	if (!ses) {
		pr_err("%px: Failed to allocate session\n", dev);
		return NULL;
	}

	ses->stats_pcpu = alloc_percpu_gfp(struct netfn_dtls_session_stats, GFP_KERNEL | __GFP_ZERO);
	if (!ses->stats_pcpu) {
		pr_err("%px: Failed to allocate stats memory for Session\n", dev);
		goto fail_pcpu;
	}

	/*
	 * Dereference: netfn_dtls_session_free()
	 * Tunnel uses netdevice referencing.
	 */
	dev_hold(dev);
	ses->tun = tun;
	ses->tuple = *t;
	ses->flags = cfg->flags;

	/*
	 * Cache part of tuple used in datapath in network order.
	 */
	ses->epoch = cfg->epoch;
	netfn_dtls_session_fill_tr(cfg, t, &tr_info);

	/*
	 * Set completion handler.
	 */
	if (encap) {
		netfn_dtls_enc_init(ses, &tr_info);
		snprintf(name, sizeof(name), "enc%u", ntohs(cfg->epoch));
	} else {
		netfn_dtls_dec_init(ses, &tr_info);
		snprintf(name, sizeof(name), "dec%u", ntohs(cfg->epoch));
	}

	ses->tr = eip_tr_alloc(drv->ctx, &tr_info);
	if (!ses->tr) {
		pr_err("%px: Failed to allocate HW record\n", dev);
		goto fail_tr;
	}

	/*
	 * create debugfs
	 */
	ses->dentry = debugfs_create_file(name, S_IRUGO, tun->dentry, ses, &netfn_dtls_ses_file_ops);

	return ses;

fail_tr:
	dev_put(dev);
	free_percpu(ses->stats_pcpu);
fail_pcpu:
	kfree(ses);
	return NULL;
}

/*
 * netfn_dtls_session_free()
 *	Free dtls Session.
 */
void netfn_dtls_session_free(struct netfn_dtls_session *ses)
{
	/*
	 * Release the TR. This also ensure no new packet being sent by driver.
	 */
	eip_tr_free(ses->tr);

	/*
	 * Reference: netfn_dtls_session_alloc()
	 */
	dev_put(ses->tun->dev);
	ses->tun = NULL;
	free_percpu(ses->stats_pcpu);
	kfree(ses);
}
