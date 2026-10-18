/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include <linux/debugfs.h>
#include <linux/ip.h>
#include <linux/jhash.h>
#include <linux/netfilter_ipv4.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/tcp.h>
#include "sac.h"
#include "sac_feature.h"
#include "sac_telemetry.h"

/*
 * sac_feature_skip_answer_name()
 *	Skips name while parsing answer fields in the DNS response.
 *
 * Answer section of DNS response may contain names in uncompressed or
 * compressed format.
 * Uncompressed format follows the same format as in the DNS query
 * section as below:
 * +--------+--------------+
 * |  0x03  |   www        |
 * +--------+--------------+
 * |  0x07  |   example    |
 * +--------+--------------+
 * |  0x03  |   com        |
 * +--------+--------------+
 * |  0x00  |              |  <-- End
 * +--------+--------------+
 *
 * Compressed name is identified by 0xC0 and the pointer indicates the
 * offset in the L4 payload buffer where the name starts.
 */
static int sac_feature_skip_answer_name(const uint8_t *l4_data, int offset, uint16_t len)
{
	int jumped = 0;
	int pointer = 0;
	int label_len = 0;
	int jump_offset = offset;

	while ((offset < len) && (l4_data[offset] != 0)) {
		/*
		 * Compressed name.
		 */
		if ((l4_data[offset] & SAC_FEATURE_DNS_COMPRESSION_MASK) == SAC_FEATURE_DNS_COMPRESSION_MASK) {
			/*
			 * Skip the compression mask and pointer.
			 */
			if (offset + 1 >= len) {
				return jumped ? jump_offset : offset;
			}

			if (!jumped) {
				jump_offset = offset + 2;
			}

			/*
			 * Pointer indicates the offset at which the name starts in the data buffer.
			 */
			pointer = ((l4_data[offset] & SAC_FEATURE_DNS_NAME_POINTER_MASK) << 8) | l4_data[offset + 1];
			if (pointer >= len) {
				return jumped ? jump_offset : len;
			}

			offset = pointer;
			jumped = 1;
		} else {
			label_len = l4_data[offset++];
			offset += label_len;
		}
	}

	/*
	 * Advance for the end character 00.
	 */
	if (!jumped) {
		offset++;
	}

	return jumped ? jump_offset : offset;
}

/*
 * sac_feature_parse_dns_name_and_ip()
 *	Parse domain name and IP address from DNS response frame.
 *
 * DNS response frame consists of DNS header followed by Query
 * and answer section.
 *
 * DNS query section format:
 * +-------------------------------+
 * |           QNAME               |  Domain name (labels with length prefixes)
 * +-------------------------------+
 * |           QTYPE               |  Type of query (2B)
 * +-------------------------------+
 * |           QCLASS              |  Class of query (2B)
 * +-------------------------------+
 *
 * DNS answer section format
 * +-------------------------------+
 * |           NAME                |  Domain name (can be compressed using 0xC0 pointer)
 * +-------------------------------+
 * |           TYPE                |  Type of resource record (2B)
 * +-------------------------------+
 * |           CLASS               |  Class of resource record (2B)
 * +-------------------------------+
 * |           TTL                 |  Time to live (4B)
 * +-------------------------------+
 * |           RDLENGTH            |  Length of RDATA field (2B)
 * +-------------------------------+
 * |           RDATA               |  Resource data (e.g., IP address)
 * +-------------------------------+
 */
static bool sac_feature_parse_dns_name_and_ip(const uint8_t *l4_data,
					      uint16_t len,
					      struct sac_feature_dns_info *dns)
{
	struct sac_stats *stats = &sac_ctx_g.stats;
	struct sac_feature_dns_header *dns_hdr;
	uint16_t ans_count = 0;
	uint16_t domain_len = 0;
	uint16_t label_len = 0;
	uint16_t pos = 0;

	dns_hdr = (struct sac_feature_dns_header *)l4_data;
	ans_count = ntohs(dns_hdr->answer_rrs);

	/*
	 * Domain name starts after DNS header.
	 */
	pos += sizeof(struct sac_feature_dns_header);

	label_len = l4_data[pos++];
	while (label_len && pos < len) {
		if (unlikely((domain_len + label_len + 1) > SAC_FEATURE_DNS_NAME_SIZE_MAX)) {
			atomic64_inc(&stats->invalid_l4);
			DEBUG_TRACE("Domain name too long for buffer\n");
			return false;
		}

		if (unlikely((uint32_t)pos + label_len > len)) {
			atomic64_inc(&stats->invalid_l4);
			DEBUG_TRACE("DNS label exceeds packet boundary\n");
			return false;
		}

		memcpy(&dns->domain_name[domain_len], &l4_data[pos], label_len);
		pos += label_len;
		domain_len += label_len;
		dns->domain_name[domain_len++] = '.';
		label_len = l4_data[pos++];
	}

	/*
	 * Remove the last dot.
	 */
	dns->domain_name[domain_len-1] = '\0';
	dns->tot_len += domain_len;

	/*
	 * Skip QTYPE and QCLASS.
	 */
	pos += 4;

	/*
	 * Parse answer section to get IP address.
	 */
	while (ans_count-- && pos < len) {
		uint16_t data_len;
		struct sac_feature_dns_answer_info *ainfo;

		pos = sac_feature_skip_answer_name(l4_data, pos, len);

		/*
		 * Safety check.
		 */
		if (unlikely(pos + sizeof(struct sac_feature_dns_answer_info) > len)) {
			atomic64_inc(&stats->invalid_l4);
			DEBUG_TRACE("Invalid DNS frame\n");
			return false;
		}

		ainfo = (struct sac_feature_dns_answer_info *)&l4_data[pos];
		pos += sizeof(struct sac_feature_dns_answer_info);
		data_len = ntohs(ainfo->data_len);

		/*
		 * Get the IPv4/IPv6 address, currently we take the first IP address from answer.
		 */
		switch (ntohs(ainfo->type)) {
		case SAC_FEATURE_DNS_RR_TYPE_A:
			if (data_len != SAC_FEATURE_IPV4_LEN) {
				atomic64_inc(&stats->dns_no_valid_ip4);
				return false;
			}

			memcpy(&dns->ip_addr, &l4_data[pos], SAC_FEATURE_IPV4_LEN);
			if (SAC_FEATURE_IS_IP_ADDR_NULL(dns->ip_addr)) {
				atomic64_inc(&stats->dns_no_valid_ip4);
				DEBUG_TRACE("Invalid IPv4 address\n");
				return false;
			}

			dns->type = SAC_FEATURE_MSG_TYPE_DNS_IPV4;
			dns->ttl = ntohl(ainfo->ttl);
			DEBUG_TRACE("Resolved ipv4 address: %pI4 \n", &dns->ip_addr[0]);
			return true;

		case SAC_FEATURE_DNS_RR_TYPE_AAAA:
			if (data_len != SAC_FEATURE_IPV6_LEN) {
				atomic64_inc(&stats->dns_no_valid_ip6);
				return false;
			}

			memcpy(&dns->ip_addr, &l4_data[pos], SAC_FEATURE_IPV6_LEN);
			if (SAC_FEATURE_IS_IP_ADDR_NULL(dns->ip_addr)) {
				atomic64_inc(&stats->dns_no_valid_ip6);
				DEBUG_TRACE("Invalid IPv6 address\n");
				return false;
			}

			dns->ttl = ntohl(ainfo->ttl);
			dns->type = SAC_FEATURE_MSG_TYPE_DNS_IPV6;
			DEBUG_TRACE("Resolved ipv6 address: %pI6 \n", &dns->ip_addr[0]);
			return true;

		case SAC_FEATURE_DNS_RR_TYPE_CNAME:
			DEBUG_TRACE("Skip CNAME, data_len: %u\n", data_len);
			break;

		default:
			atomic64_inc(&stats->dns_unsupp_ans_type);
			DEBUG_TRACE("Unhandled type!\n");
			return false;
		}

		pos += data_len;
	}

	atomic64_inc(&stats->dns_ans_noip);
	return false;
}

/*
 * sac_feature_extract_dns()
 *	Extract DNS details from the received packet.
 *
 * Both DNS and mDNS use the same frame format, so the same
 * parsing function handles both.
 */
void sac_feature_extract_dns(struct sac_ctx *ctx, struct udphdr *uh, bool is_mdns)
{
	uint16_t l4_len = ntohs(uh->len) - sizeof(struct udphdr);
	uint8_t *l4_payload = (uint8_t *)uh + sizeof(struct udphdr);
	struct sac_feature_dns_response *response;
	struct sac_feature_dns_header *dns_hdr;
	struct sac_feature_dns_info *dns_info;
	struct sac_stats *stats = &ctx->stats;
	uint16_t query_count = 0;
	uint16_t ans_count = 0;

	if (!l4_payload || l4_len < sizeof(struct sac_feature_dns_header)) {
		DEBUG_TRACE("Invalid L4 payload or length\n");
		atomic64_inc(&stats->invalid_l4);
		return;
	}

	/*
	 * Check if it's a DNS response frame
	 */
	dns_hdr = (struct sac_feature_dns_header *)l4_payload;
	if (!(ntohs(dns_hdr->flags) & SAC_FEATURE_DNS_RESPONSE_MASK)) {
		DEBUG_TRACE("Not a DNS response frame: 0x%X\n", dns_hdr->flags);
		return;
	}

	/*
	 * Check if there are valid query and answer.
	 */
	query_count = ntohs(dns_hdr->questions);
	ans_count = ntohs(dns_hdr->answer_rrs);
	if (!query_count || !ans_count) {
		DEBUG_TRACE("No query <%u>/answer <%u>\n", query_count, ans_count);
		return;
	}

	response = kzalloc(sizeof(*response), GFP_ATOMIC);
	if (!response) {
		atomic64_inc(&stats->dns_tx_fail);
		return;
	}

	/*
	 * Initialize response data and parse DNS frame.
	 */
	response->tot_len = SAC_FEATURE_DNS_RESPONSE_FIXED_LEN;
	dns_info = &response->info[0];
	dns_info->tot_len = SAC_FEATURE_DNS_ANSWER_FIXED_LEN;

	if (!sac_feature_parse_dns_name_and_ip(l4_payload, l4_len, dns_info)) {
		DEBUG_TRACE("Failed to parse DNS response\n");
		atomic64_inc(&stats->dns_parse_fail);
		kfree(response);
		return;
	}

	atomic64_inc(&stats->dns_parse_success);
	response->count++;
	response->tot_len += dns_info->tot_len;
	DEBUG_TRACE("Queried domain name: %s, domain_len: %d, ttl: %d, mmap_offset: %d\n",
				dns_info->domain_name, dns_info->tot_len, dns_info->ttl, ctx->mmap_offset);

	/*
	 * Check for offset overflow.
	 */
	spin_lock_bh(&ctx->lock);
	if ((ctx->mmap_offset + SAC_FEATURE_DNS_RESPONSE_MAX_SIZE) > ctx->mmap_size) {
		ctx->mmap_offset = 0;
	}

	/*
	 * Try to send data to user space.
	 */
	if (unlikely(!sac_telemetry_send_data(ctx, (uint8_t *)response, response->tot_len))) {
		DEBUG_TRACE("Failed to send data through telemetry\n");
		atomic64_inc(&stats->dns_tx_fail);
		spin_unlock_bh(&ctx->lock);
		kfree(response);
		return;
	}

	ctx->mmap_offset += SAC_FEATURE_DNS_RESPONSE_MAX_SIZE;
	atomic64_inc(&stats->dns_tx_success);
	atomic64_set(&stats->mmap_offset, ctx->mmap_offset);
	spin_unlock_bh(&ctx->lock);
	kfree(response);
}

struct sac_feature_sni_ctx sac_feature_sni_ctx_g;

/*
 * sac_feature_sni_flow_hash()
 *	Compute hash key for a TCP 4-tuple.
 */
static u32 sac_feature_sni_flow_hash(u32 src_ip, u32 dst_ip, u16 sport, u16 dport)
{
	return jhash_3words(src_ip, dst_ip, ((u32)sport << 16) | dport, 0);
}

/*
 * sac_feature_sni_flow_lookup()
 *	Find a flow entry. Must be called with flow_lock held.
 */
static struct sac_feature_sni_flow_entry *
sac_feature_sni_flow_lookup(struct sac_feature_sni_ctx *ctx,
			    u32 src_ip, u32 dst_ip, u16 sport, u16 dport)
{
	struct sac_feature_sni_flow_entry *fe;
	u32 key = sac_feature_sni_flow_hash(src_ip, dst_ip, sport, dport);

	hash_for_each_possible(ctx->flow_table, fe, hnode, key) {
		if (fe->src_ip == src_ip &&
		    fe->dst_ip == dst_ip &&
		    fe->src_port == sport &&
		    fe->dst_port == dport) {
			return fe;
		}
	}
	return NULL;
}

/*
 * sac_feature_sni_flow_alloc()
 *	Allocate and insert a new flow entry. Must be called with flow_lock held.
 */
static struct sac_feature_sni_flow_entry *
sac_feature_sni_flow_alloc(struct sac_feature_sni_ctx *ctx, struct sac_ctx *sac,
			   u32 src_ip, u32 dst_ip, u16 sport, u16 dport)
{
	struct sac_feature_sni_flow_entry *fe;
	u32 key;

	if (atomic_read(&ctx->flow_count) >= SAC_FEATURE_SNI_FLOW_MAX_ENTRIES) {
		atomic64_inc(&sac->stats.sni_flow_cache_full);
		DEBUG_WARN("SNI flow cache full, dropping new entry\n");
		return NULL;
	}

	fe = kzalloc(sizeof(*fe), GFP_ATOMIC);
	if (!fe) {
		DEBUG_ERROR("SNI flow entry alloc failed\n");
		return NULL;
	}

	fe->src_ip = src_ip;
	fe->dst_ip = dst_ip;
	fe->src_port = sport;
	fe->dst_port = dport;
	fe->last_seen = jiffies;
	fe->state = SAC_FEATURE_SNI_STATE_UNKNOWN;

	key = sac_feature_sni_flow_hash(src_ip, dst_ip, sport, dport);
	hash_add(ctx->flow_table, &fe->hnode, key);
	atomic_inc(&ctx->flow_count);
	return fe;
}

/*
 * sac_feature_sni_parse_clienthello()
 *	Extract SNI hostname from a TLS ClientHello at payload_off within skb.
 *	Returns SNI length on success, 0 otherwise.
 */
static int sac_feature_sni_parse_clienthello(struct sk_buff *skb,
					     u32 payload_off, u32 avail,
					     u8 *sni_out, u16 sni_max)
{
	u8 *tmp = sni_out;
	const u8 *p;
	u32 off = payload_off;
	u32 lim = payload_off + min_t(u32, avail, SAC_FEATURE_SNI_MAX_PARSE_BYTES);
	u16 rec_len, session_len, cs_len, cm_len;
	u16 ext_total, ext_type, ext_len, sni_name_len;
	u32 ext_end;

	p = skb_header_pointer(skb, off, 5, tmp);
	if (!p || p[0] != 0x16 || p[1] != 0x03) {
		DEBUG_TRACE("SNI parse: not a TLS record\n");
		return 0;
	}
	rec_len = ((u16)p[3] << 8) | p[4];
	if (rec_len < 42 || rec_len > 16384) {
		return 0;
	}
	off += 5;

	p = skb_header_pointer(skb, off, 4, tmp);
	if (!p || p[0] != 0x01) {
		return 0;
	}
	{
		u32 hs_len = ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];

		if (hs_len < 38) {
			return 0;
		}
		lim = min_t(u32, lim, off + 4 + hs_len);
	}
	off += 4;

	off += 34;
	if (off >= lim) {
		return 0;
	}

	p = skb_header_pointer(skb, off, 1, tmp);
	if (!p) {
		return 0;
	}
	session_len = *p;
	if (session_len > 32) {
		return 0;
	}
	off += 1 + session_len;
	if (off + 2 > lim) {
		return 0;
	}

	p = skb_header_pointer(skb, off, 2, tmp);
	if (!p) {
		return 0;
	}
	cs_len = ((u16)p[0] << 8) | p[1];
	if (!cs_len || cs_len > 512) {
		return 0;
	}
	off += 2 + cs_len;
	if (off + 1 > lim) {
		return 0;
	}

	p = skb_header_pointer(skb, off, 1, tmp);
	if (!p) {
		return 0;
	}
	cm_len = *p;
	off += 1 + cm_len;
	if (off + 2 > lim) {
		return 0;
	}

	p = skb_header_pointer(skb, off, 2, tmp);
	if (!p) {
		return 0;
	}
	ext_total = ((u16)p[0] << 8) | p[1];
	off += 2;

	ext_end = off + ext_total;
	if (ext_end > lim) {
		ext_end = lim;
	}

	while (off + 4 <= ext_end) {
		p = skb_header_pointer(skb, off, 4, tmp);
		if (!p) {
			return 0;
		}
		ext_type = ((u16)p[0] << 8) | p[1];
		ext_len = ((u16)p[2] << 8) | p[3];
		off += 4;

		if (ext_type == 0x0000) {
			if (ext_len < 5 || off + 5 > ext_end) {
				return 0;
			}
			p = skb_header_pointer(skb, off, 5, tmp);
			if (!p || p[2] != 0x00) {
				return 0;
			}
			sni_name_len = ((u16)p[3] << 8) | p[4];
			if (!sni_name_len || sni_name_len >= sni_max) {
				return 0;
			}
			if (off + 5 + sni_name_len > ext_end) {
				return 0;
			}
			p = skb_header_pointer(skb, off + 5,
					       sni_name_len, sni_out);
			if (!p) {
				return 0;
			}
			if (p != sni_out) {
				memcpy(sni_out, p, sni_name_len);
			}
			return (int)sni_name_len;
		}
		if (ext_len == 0 || off + ext_len > ext_end) {
			break;
		}
		off += ext_len;
	}
	return 0;
}

/*
 * sac_feature_sni_emit_frame()
 *	Build and enqueue an sac_feature_tls_sni_response to the SAC mmap buffer.
 */
static void sac_feature_sni_emit_frame(struct sac_ctx *sac,
				       u32 dst_ip_nbo, u8 ip_version, u16 dst_port,
				       const u8 *sni_name, u16 sni_len)
{
	struct sac_feature_tls_sni_response *resp;
	struct sac_feature_tls_sni_info *info;

	resp = kzalloc(sizeof(*resp), GFP_ATOMIC);
	if (!resp) {
		DEBUG_WARN("SNI resp alloc failed\n");
		atomic64_inc(&sac->stats.sni_enqueue_fail);
		return;
	}

	info = &resp->info[0];

	info->tot_len = SAC_FEATURE_SNI_INFO_FIXED_LEN + sni_len;
	info->type = SAC_FEATURE_MSG_TYPE_TLS_SNI;
	info->ip_version = ip_version;
	info->dst_port = dst_port;
	info->dst_ip[0] = dst_ip_nbo;
	info->sni_len = sni_len;
	memcpy(info->sni_name, sni_name, sni_len);

	resp->count = 1;
	resp->tot_len = SAC_FEATURE_SNI_RESPONSE_FIXED_LEN + info->tot_len;

	spin_lock_bh(&sac->lock);

	if ((sac->mmap_offset + SAC_FEATURE_SNI_RESPONSE_MAX_SIZE) >
	    sac->mmap_size) {
		sac->mmap_offset = 0;
	}

	if (unlikely(!sac_telemetry_send_data(sac, (uint8_t *)resp,
					      resp->tot_len))) {
		DEBUG_TRACE("SNI enqueue failed at offset %u\n", sac->mmap_offset);
		atomic64_inc(&sac->stats.sni_enqueue_fail);
		spin_unlock_bh(&sac->lock);
		kfree(resp);
		return;
	}

	sac->mmap_offset += SAC_FEATURE_SNI_RESPONSE_MAX_SIZE;
	atomic64_inc(&sac->stats.sni_enqueue_ok);
	DEBUG_TRACE("SNI enqueued: len=%u offset=%u\n", sni_len, sac->mmap_offset);
	spin_unlock_bh(&sac->lock);
	kfree(resp);
}

/*
 * sac_feature_sni_process()
 *	Process an ingress packet for TLS SNI extraction.
 *	Called from sac_feature_sni_pre_routing_hook() in softirq context.
 */
void sac_feature_sni_process(struct sac_ctx *sac, struct sac_feature_sni_ctx *sni,
			     struct sk_buff *skb)
{
	struct iphdr *iph;
	struct tcphdr *th;
	struct sac_feature_sni_flow_entry *fe;
	u8 *sni_buf;
	u32 ip_hdr_len, tcp_hdr_len, payload_off, payload_len;
	u32 src_ip, dst_ip;
	u16 sport, dport;
	int sni_len;

	if (!pskb_may_pull(skb, sizeof(struct iphdr))) {
		return;
	}

	if (unlikely(skb_network_offset(skb) != 0)) {
		return;
	}

	iph = (struct iphdr *)skb->data;
	if (iph->protocol != IPPROTO_TCP) {
		return;
	}

	if (iph->ihl < 5) {
		return;
	}

	ip_hdr_len = iph->ihl * 4;

	if (!pskb_may_pull(skb, ip_hdr_len + sizeof(struct tcphdr))) {
		return;
	}

	iph = (struct iphdr *)skb->data;
	th = (struct tcphdr *)((u8 *)skb->data + ip_hdr_len);
	dport = ntohs(th->dest);
	if (dport != 443) {
		return;
	}

	atomic64_inc(&sac->stats.sni_tcp443_seen);

	tcp_hdr_len = th->doff * 4;
	if (tcp_hdr_len < sizeof(struct tcphdr)) {
		return;
	}

	payload_off = ip_hdr_len + tcp_hdr_len;
	payload_len = ntohs(iph->tot_len);
	if (payload_len < payload_off + 10) {
		return;
	}
	payload_len -= payload_off;

	src_ip = iph->saddr;
	dst_ip = iph->daddr;
	sport = ntohs(th->source);

	spin_lock_bh(&sni->flow_lock);

	fe = sac_feature_sni_flow_lookup(sni, src_ip, dst_ip, sport, dport);
	if (!fe) {
		fe = sac_feature_sni_flow_alloc(sni, sac, src_ip, dst_ip,
						sport, dport);
	}

	if (!fe || fe->state != SAC_FEATURE_SNI_STATE_UNKNOWN) {
		spin_unlock_bh(&sni->flow_lock);
		return;
	}

	fe->attempts++;
	fe->last_seen = jiffies;

	if (fe->attempts > sac->sni_max_attempts) {
		fe->state = SAC_FEATURE_SNI_STATE_GIVEUP;
		atomic64_inc(&sac->stats.sni_giveup);
		DEBUG_TRACE("SNI giveup after %u attempts\n", fe->attempts);
		spin_unlock_bh(&sni->flow_lock);
		return;
	}

	fe->state = SAC_FEATURE_SNI_STATE_PARSING;

	spin_unlock_bh(&sni->flow_lock);

	sni_buf = kmalloc(SAC_FEATURE_SNI_NAME_SIZE_MAX, GFP_ATOMIC);
	if (!sni_buf) {
		DEBUG_WARN("SNI buf alloc failed\n");
		spin_lock_bh(&sni->flow_lock);
		fe = sac_feature_sni_flow_lookup(sni, src_ip, dst_ip, sport, dport);
		if (fe && fe->state == SAC_FEATURE_SNI_STATE_PARSING) {
			fe->state = SAC_FEATURE_SNI_STATE_UNKNOWN;
		}
		spin_unlock_bh(&sni->flow_lock);
		return;
	}

	atomic64_inc(&sac->stats.sni_parse_attempts);

	sni_len = sac_feature_sni_parse_clienthello(skb, payload_off, payload_len,
						    sni_buf, SAC_FEATURE_SNI_NAME_SIZE_MAX);

	{
		uint8_t attempts_copy = 0;

		spin_lock_bh(&sni->flow_lock);
		fe = sac_feature_sni_flow_lookup(sni, src_ip, dst_ip, sport, dport);
		if (fe && fe->state == SAC_FEATURE_SNI_STATE_PARSING) {
			fe->state = (sni_len > 0) ? SAC_FEATURE_SNI_STATE_FOUND
						  : SAC_FEATURE_SNI_STATE_UNKNOWN;
			attempts_copy = fe->attempts;
		}
		spin_unlock_bh(&sni->flow_lock);

		if (sni_len > 0) {
			atomic64_inc(&sac->stats.sni_found);
			DEBUG_TRACE("SNI found: len=%d\n", sni_len);
			sac_feature_sni_emit_frame(sac, dst_ip, 4, dport,
						   sni_buf, (u16)sni_len);
		} else {
			atomic64_inc(&sac->stats.sni_parse_fail);
			DEBUG_TRACE("SNI parse failed for dport=%u attempt=%u\n",
				    dport, attempts_copy);
		}
	}

	kfree(sni_buf);
}

/*
 * sac_feature_sni_age_work_fn()
 *	Periodic work to evict idle flow entries.
 */
static void sac_feature_sni_age_work_fn(struct work_struct *work)
{
	struct sac_feature_sni_ctx *ctx =
		container_of(work, struct sac_feature_sni_ctx, age_work.work);
	struct sac_feature_sni_flow_entry *fe;
	struct hlist_node *tmp;
	unsigned long expire;
	int bkt;

	expire = jiffies - (unsigned long)(SAC_FEATURE_SNI_FLOW_EXPIRE_SEC * HZ);

	spin_lock_bh(&ctx->flow_lock);
	hash_for_each_safe(ctx->flow_table, bkt, tmp, fe, hnode) {
		if (time_before(fe->last_seen, expire)) {
			hash_del(&fe->hnode);
			kfree(fe);
			atomic_dec(&ctx->flow_count);
			atomic64_inc(&ctx->flow_evicted);
		}
	}
	spin_unlock_bh(&ctx->flow_lock);

	if (READ_ONCE(ctx->running)) {
		schedule_delayed_work(&ctx->age_work,
				      SAC_FEATURE_SNI_AGE_INTERVAL_SEC * HZ);
	}
}

/*
 * sac_feature_sni_stats_show()
 *	Called when reading the debugfs sni_stats file.
 */
static int sac_feature_sni_stats_show(struct seq_file *s, void *data)
{
	struct sac_feature_sni_ctx *ctx = (struct sac_feature_sni_ctx *)s->private;
	struct sac_ctx *sac = &sac_ctx_g;

	if (!ctx) {
		DEBUG_ERROR("SNI context not available.\n");
		return 0;
	}

	seq_printf(s, "\nSAC SNI stats:\n");
	seq_printf(s, "sni_enabled:          %d\n", sac->sni_enable ? 1 : 0);
	seq_printf(s, "sni_max_attempts:     %u\n", sac->sni_max_attempts);
	seq_printf(s, "flow_cache_entries:   %d\n", atomic_read(&ctx->flow_count));
	seq_printf(s, "flow_evicted:         %llu\n",
		   atomic64_read(&ctx->flow_evicted));
	seq_printf(s, "tcp_443_seen:         %llu\n",
		   atomic64_read(&sac->stats.sni_tcp443_seen));
	seq_printf(s, "parse_attempts:       %llu\n",
		   atomic64_read(&sac->stats.sni_parse_attempts));
	seq_printf(s, "sni_found:            %llu\n",
		   atomic64_read(&sac->stats.sni_found));
	seq_printf(s, "parse_fail:           %llu\n",
		   atomic64_read(&sac->stats.sni_parse_fail));
	seq_printf(s, "giveup:               %llu\n",
		   atomic64_read(&sac->stats.sni_giveup));
	seq_printf(s, "flow_cache_full:      %llu\n",
		   atomic64_read(&sac->stats.sni_flow_cache_full));
	seq_printf(s, "enqueue_ok:           %llu\n",
		   atomic64_read(&sac->stats.sni_enqueue_ok));
	seq_printf(s, "enqueue_fail:         %llu\n",
		   atomic64_read(&sac->stats.sni_enqueue_fail));
	return 0;
}

/*
 * sac_feature_sni_stats_open()
 *	Called when opening the debugfs sni_stats file.
 */
static int sac_feature_sni_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, sac_feature_sni_stats_show, inode->i_private);
}

static const struct file_operations sac_feature_sni_stats_ops = {
	.owner   = THIS_MODULE,
	.open    = sac_feature_sni_stats_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

/*
 * sac_feature_sni_enable_write()
 *	Write handler for the sni_enable debugfs file.
 */
static ssize_t sac_feature_sni_enable_write(struct file *file,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	u8 c;

	if (!count)
		return -EINVAL;
	if (get_user(c, (const u8 __user *)ubuf))
		return -EFAULT;
	if (c != '0' && c != '1')
		return -EINVAL;
	WRITE_ONCE(sac_ctx_g.sni_enable, c == '1');
	return count;
}

/*
 * sac_feature_sni_enable_read()
 *	Read handler for the sni_enable debugfs file.
 */
static ssize_t sac_feature_sni_enable_read(struct file *file, char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	const char *str = READ_ONCE(sac_ctx_g.sni_enable) ? "1\n" : "0\n";

	return simple_read_from_buffer(ubuf, count, ppos, str, 2);
}

static const struct file_operations sac_feature_sni_enable_ops = {
	.owner  = THIS_MODULE,
	.read   = sac_feature_sni_enable_read,
	.write  = sac_feature_sni_enable_write,
	.llseek = default_llseek,
};

/*
 * sac_feature_sni_pre_routing_hook()
 *	Global IPv4 PRE_ROUTING hook for SNI extraction.
 *	Fires for all forwarded IPv4 packets - device-independent.
 *	This is needed because TLS ClientHello from LAN clients ingresses
 *	on the LAN bridge (br-lan), not the WAN interface tracked for DNS.
 */
static unsigned int sac_feature_sni_pre_routing_hook(void *priv,
						     struct sk_buff *skb,
						     const struct nf_hook_state *state)
{
	if (!READ_ONCE(sac_ctx_g.enable) || !READ_ONCE(sac_ctx_g.sni_enable)) {
		return NF_ACCEPT;
	}

	sac_feature_sni_process(&sac_ctx_g, &sac_feature_sni_ctx_g, skb);
	return NF_ACCEPT;
}

static const struct nf_hook_ops sac_feature_sni_nf_ops = {
	.hook     = sac_feature_sni_pre_routing_hook,
	.pf       = NFPROTO_IPV4,
	.hooknum  = NF_INET_PRE_ROUTING,
	.priority = NF_IP_PRI_FIRST,
};

/*
 * sac_feature_sni_init()
 *	Initialize SNI subsystem.
 */
int sac_feature_sni_init(struct sac_feature_sni_ctx *ctx, struct dentry *dbg_dir)
{
	int ret;

	BUILD_BUG_ON(offsetof(struct sac_feature_tls_sni_info, dst_port) != 6);
	BUILD_BUG_ON(offsetof(struct sac_feature_tls_sni_info, dst_ip) != 8);
	BUILD_BUG_ON(offsetof(struct sac_feature_tls_sni_info, sni_name) != 26);
	BUILD_BUG_ON(sizeof(struct sac_feature_tls_sni_info) != 282);

	hash_init(ctx->flow_table);
	spin_lock_init(&ctx->flow_lock);
	atomic_set(&ctx->flow_count, 0);
	atomic64_set(&ctx->flow_evicted, 0);
	WRITE_ONCE(ctx->running, true);

	ret = nf_register_net_hook(&init_net, &sac_feature_sni_nf_ops);
	if (ret) {
		DEBUG_ERROR("Failed to register SNI pre-routing hook\n");
		return ret;
	}

	INIT_DELAYED_WORK(&ctx->age_work, sac_feature_sni_age_work_fn);
	schedule_delayed_work(&ctx->age_work, SAC_FEATURE_SNI_AGE_INTERVAL_SEC * HZ);

	if (dbg_dir) {
		debugfs_create_file("sni_stats", 0444, dbg_dir, ctx,
				    &sac_feature_sni_stats_ops);
		debugfs_create_file("sni_enable", 0644, dbg_dir, NULL,
				    &sac_feature_sni_enable_ops);
	}

	DEBUG_INFO("SNI subsystem initialized\n");
	return 0;
}

/*
 * sac_feature_sni_deinit()
 *	Deinitialize SNI subsystem and free all flow entries.
 */
void sac_feature_sni_deinit(struct sac_feature_sni_ctx *ctx)
{
	struct sac_feature_sni_flow_entry *fe;
	struct hlist_node *tmp;
	int bkt;

	nf_unregister_net_hook(&init_net, &sac_feature_sni_nf_ops);

	WRITE_ONCE(ctx->running, false);
	cancel_delayed_work_sync(&ctx->age_work);

	spin_lock_bh(&ctx->flow_lock);
	hash_for_each_safe(ctx->flow_table, bkt, tmp, fe, hnode) {
		hash_del(&fe->hnode);
		kfree(fe);
	}
	atomic_set(&ctx->flow_count, 0);
	spin_unlock_bh(&ctx->flow_lock);
	DEBUG_INFO("SNI subsystem deinitialized\n");
}
