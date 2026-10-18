/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#ifndef SAC_FEATURE_H
#define SAC_FEATURE_H

#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/udp.h>
#include <linux/workqueue.h>

#define SAC_FEATURE_IPV4_LEN		(4)
#define SAC_FEATURE_IPV6_LEN		(16)
#define SAC_FEATURE_IS_IP_ADDR_NULL(a) \
	((a[0] | a[1] | a[2] | a[3]) == 0)

/*
 * DNS specific defines.
 */
#define SAC_FEATURE_DNS_RESPONSE_MAX_SIZE	(280)
#define SAC_FEATURE_DNS_RESPONSE_MASK		(0x8000)
#define SAC_FEATURE_DNS_NAME_SIZE_MAX		(254)
#define SAC_FEATURE_DNS_RESPONSE_COUNT_MAX	(1)
#define SAC_FEATURE_DNS_ANSWER_FIXED_LEN	(24)
#define SAC_FEATURE_DNS_RESPONSE_FIXED_LEN	(4)
#define SAC_FEATURE_DNS_COMPRESSION_MASK	(0xC0)
#define SAC_FEATURE_DNS_NAME_POINTER_MASK	(0x3F)

/*
 * Message type to be sent to user space.
 */
#define SAC_FEATURE_MSG_TYPE_DNS_IPV4	(1)
#define SAC_FEATURE_MSG_TYPE_DNS_IPV6	(2)
#define SAC_FEATURE_MSG_TYPE_TLS_SNI	(3)

/*
 * UIO MQTT message-type token for SNI frames.
 */
#define MQTT_UIO_SNI_DATA		(0xffff000a)

/*
 * DNS resource record types.
 */
#define SAC_FEATURE_DNS_RR_TYPE_A	(1)
#define SAC_FEATURE_DNS_RR_TYPE_CNAME	(5)
#define SAC_FEATURE_DNS_RR_TYPE_TXT	(16)
#define SAC_FEATURE_DNS_RR_TYPE_AAAA	(28)

extern struct sac_ctx sac_ctx_g;

/*
 * sac_feature_dns_header
 *	DNS header fields.
 *
 * +---------------------------+
 * |    ID (2B)                |
 * +---------------------------+
 * |    Flags (2B)             |
 * +---------------------------+
 * |    Question count (2B)    |
 * +---------------------------+
 * |    Answer count (2B)      |
 * +---------------------------+
 * |    Authority count (2B)   |
 * +---------------------------+
 * |    Additional count (2B)  |
 * +---------------------------+
 */
struct sac_feature_dns_header {
	__be16 transaction_id;	/* Transaction ID */
	__be16 flags;		/* Flags */
	__be16 questions;	/* Number of questions */
	__be16 answer_rrs;	/* Number of answer resource records */
	__be16 authority_rrs;	/* Number of authority resource records */
	__be16 additional_rrs;	/* Number of additional resource records */
} __packed;

/*
 * sac_feature_dns_answer_info
 *	Fixed fields in DNS answer.
 */
struct sac_feature_dns_answer_info {
	__be16 type;		/* Type of data */
	__be16 dns_class;	/* Class of data */
	__be32 ttl;		/* TTL */
	__be16 data_len;	/* Data length */
} __packed;

/*
 * sac_feature_dns_info
 *	Domain name details.
 */
struct sac_feature_dns_info {
	uint16_t tot_len;		/* Total length of data */
	uint16_t type;			/* Message type */
	uint32_t ip_addr[4];		/* WAN IP address */
	uint32_t ttl;			/* TTL for the DNS record */
	uint8_t domain_name[SAC_FEATURE_DNS_NAME_SIZE_MAX];
					/* Store domain name from DNS response */
} __packed;

/*
 * sac_feature_dns_response
 *	DNS response.
 */
struct sac_feature_dns_response {
	uint16_t count;			/* Total number of DNS responses */
	uint16_t tot_len;		/* Total length of the responses */
	struct sac_feature_dns_info
		info[SAC_FEATURE_DNS_RESPONSE_COUNT_MAX];
					/* DNS response details (TODO: allocate dynamically) */
};

/*
 * TLS SNI specific defines.
 */
#define SAC_FEATURE_SNI_NAME_SIZE_MAX		(256)
#define SAC_FEATURE_SNI_INFO_FIXED_LEN		(26)
#define SAC_FEATURE_SNI_RESPONSE_FIXED_LEN	(4)
#define SAC_FEATURE_SNI_RESPONSE_MAX_SIZE	\
	(SAC_FEATURE_SNI_RESPONSE_FIXED_LEN +	\
	 SAC_FEATURE_SNI_INFO_FIXED_LEN +	\
	 SAC_FEATURE_SNI_NAME_SIZE_MAX)
#define SAC_FEATURE_SNI_RESPONSE_COUNT_MAX	(1)

/*
 * sac_feature_tls_sni_info
 *	TLS SNI details sent kernel to userspace via SAC mmap buffer.
 *	ABI must stay identical with sac_cmn.hpp in the userspace tree.
 */
struct sac_feature_tls_sni_info {
	uint16_t tot_len;	/* SAC_FEATURE_SNI_INFO_FIXED_LEN + sni_len */
	uint16_t type;		/* SAC_FEATURE_MSG_TYPE_TLS_SNI */
	uint8_t ip_version;	/* 4 or 6 */
	uint8_t pad;		/* reserved, must be zero */
	uint16_t dst_port;	/* destination port, host byte order */
	uint32_t dst_ip[4];	/* destination IP, network byte order */
	uint16_t sni_len;	/* bytes in sni_name */
	uint8_t  sni_name[SAC_FEATURE_SNI_NAME_SIZE_MAX];
					/* SNI hostname, not NUL-terminated */
} __packed;

/*
 * sac_feature_tls_sni_response
 *	SNI response container.
 */
struct sac_feature_tls_sni_response {
	uint16_t count;		/* always 1 per enqueue */
	uint16_t tot_len;	/* SAC_FEATURE_SNI_RESPONSE_FIXED_LEN + info[0].tot_len */
	struct sac_feature_tls_sni_info info[SAC_FEATURE_SNI_RESPONSE_COUNT_MAX];
					/* SNI info entries */
} __packed;

void sac_feature_extract_dns(struct sac_ctx *ctx, struct udphdr *uh, bool is_mdns);

#define SAC_FEATURE_SNI_FLOW_HASH_BITS		10
#define SAC_FEATURE_SNI_FLOW_MAX_ENTRIES	4096
#define SAC_FEATURE_SNI_FLOW_EXPIRE_SEC		60
#define SAC_FEATURE_SNI_DEFAULT_MAX_ATTEMPTS	3
#define SAC_FEATURE_SNI_MAX_PARSE_BYTES		1024
#define SAC_FEATURE_SNI_AGE_INTERVAL_SEC	30

/*
 * sac_feature_sni_flow_state
 *	Inspection state for a tracked TCP flow.
 */
enum sac_feature_sni_flow_state {
	SAC_FEATURE_SNI_STATE_UNKNOWN = 0,	/* not yet inspected */
	SAC_FEATURE_SNI_STATE_PARSING,		/* parse attempt in progress */
	SAC_FEATURE_SNI_STATE_FOUND,		/* SNI successfully extracted */
	SAC_FEATURE_SNI_STATE_GIVEUP,		/* max attempts exhausted */
};

/*
 * sac_feature_sni_flow_entry
 *	Per-connection SNI inspection state.
 */
struct sac_feature_sni_flow_entry {
	struct hlist_node	hnode;		/* hash table linkage */
	uint32_t		src_ip;		/* source IP, network byte order */
	uint32_t		dst_ip;		/* destination IP, network byte order */
	uint16_t		src_port;	/* source port, host byte order */
	uint16_t		dst_port;	/* destination port, host byte order */
	unsigned long		last_seen;	/* jiffies at last packet */
	uint8_t			state;		/* enum sac_feature_sni_flow_state */
	uint8_t			attempts;	/* ClientHello parse attempt count */
};

/*
 * sac_feature_sni_ctx
 *	SNI subsystem state.
 */
struct sac_feature_sni_ctx {
	DECLARE_HASHTABLE(flow_table, SAC_FEATURE_SNI_FLOW_HASH_BITS);
					/* per-flow inspection state table */
	spinlock_t		flow_lock;	/* protects flow_table */
	atomic_t		flow_count;	/* current number of tracked flows */
	atomic64_t		flow_evicted;	/* entries removed by age-out */
	struct delayed_work	age_work;	/* periodic age-out work */
	bool			running;	/* set false to stop age_work */
};

int sac_feature_sni_init(struct sac_feature_sni_ctx *ctx, struct dentry *dbg_dir);
void sac_feature_sni_deinit(struct sac_feature_sni_ctx *ctx);
void sac_feature_sni_process(struct sac_ctx *sac, struct sac_feature_sni_ctx *sni,
			     struct sk_buff *skb);

extern struct sac_feature_sni_ctx sac_feature_sni_ctx_g;

#endif /* SAC_FEATURE_H */
