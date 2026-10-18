/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
*/
#ifndef SDC_FEATURES_EXT_H
#define SDC_FEATURES_EXT_H

#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/types.h>

/*
 * DHCP related fields.
 */
#define SDC_FEATURES_EXT_DHCP_SPORT 67
#define SDC_FEATURES_EXT_DHCP_CPORT 68
#define SDC_FEATURES_EXT_DNS_PORT	53
#define SDC_FEATURES_EXT_MDNS_PORT	5353
#define SDC_FEATURES_EXT_SSDP_PORT	1900
#define SDC_FEATURES_EXT_TLS_PORT	443

/*
 * Options field has the folloing magic at the beginning.
 */
#define SDC_FEATURES_EXT_DHCP_MAGIC 0x63825363

/*
 * DHCP message type option values (option 53).
 */
#define SDC_FEATURES_EXT_DHCP_OPN_CODE_MSG_TYPE 53
#define SDC_FEATURES_EXT_DHCP_OPN_VAL_DISCOVER  1
#define SDC_FEATURES_EXT_DHCP_OPN_VAL_OFFER     2
#define SDC_FEATURES_EXT_DHCP_OPN_VAL_REQ       3
#define SDC_FEATURES_EXT_DHCP_OPN_VAL_ACK       5
#define SDC_FEATURES_EXT_DHCP_OPN_VAL_NAK       6

#define SDC_FEATURES_EXT_DHCP_OPN_CODE_END 0xff
#define SDC_FEATURES_EXT_DHCP_OPN_CODE_PAD 0x00

/*
 * Mask to check the QR bit and RCODE in DNS flags.
 */
#define SDC_FEATURES_EXT_DNS_REQ_MASK    0x8000
#define SDC_FEATURES_EXT_DNS_QR_MASK     0x8000
#define SDC_FEATURES_EXT_DNS_RCODE_MASK  0x000f
#define SDC_FEATURES_EXT_DNS_RCODE_SERVFAIL 2
#define SDC_FEATURES_EXT_DNS_RCODE_NXDOMAIN 3

/*
 * TLS record/handshake constants for ClientHello detection.
 * Full frame is forwarded to userspace raw — no in-kernel extension parsing.
 */
#define SDC_FEATURES_EXT_TLS_CONTENT_HANDSHAKE 0x16
#define SDC_FEATURES_EXT_TLS_HS_CLIENT_HELLO   0x01

/*
 * QUIC Long Header Initial packet detection.
 * byte[0]: bit7=1 (Long Header), bit6=1 (Fixed Bit), bits5-4=00 (Initial type)
 * byte[1-4]: version field — 0x00000000 means Version Negotiation (not Initial).
 */
#define SDC_FEATURES_EXT_QUIC_PORT         443
#define SDC_FEATURES_EXT_QUIC_LONG_INITIAL 0xC0   /* mask: (byte[0] & 0xF0) == 0xC0 */
#define SDC_FEATURES_EXT_QUIC_V1           0x00000001
#define SDC_FEATURES_EXT_QUIC_V2           0x6b3343cf

/*
 * quic_version_bitmap bit definitions — stored in sdc_macdb_entry and STATS TLV.
 */
#define SDC_QUIC_VERSION_BIT_V1      BIT(0)  /* RFC 9000 */
#define SDC_QUIC_VERSION_BIT_V2      BIT(1)  /* RFC 9369 */
#define SDC_QUIC_VERSION_BIT_DRAFT   BIT(2)  /* 0xff?????? pre-RFC drafts */
#define SDC_QUIC_VERSION_BIT_GREASE  BIT(3)  /* 0x?a?a?a?a deliberate junk */
#define SDC_QUIC_VERSION_BIT_UNKNOWN BIT(4)  /* any other version value */

/*
 * Cap on unique SSDP USN TLVs stored per device.
 */
#define SDC_SSDP_USN_MAX 5

struct sdc_ctx;

/*
 * DNS frame.
 */
struct sdc_features_ext_dns_header {
	__be16 transaction_id;	/* Transaction ID */
	__be16 flags;		/* Flags */
	__be16 questions;	/* Number of questions */
	__be16 answer_rrs;	/* Number of answer resource records */
	__be16 authority_rrs;	/* Number of authority resource records */
	__be16 additional_rrs;	/* Number of additional resource records */
} __packed;

/*
 * DHCP frame format.
 */
struct sdc_features_ext_dhcp_header {
	uint8_t op;/* Message type: 1 = BOOTREQUEST, 2 = BOOTREPLY */
	uint8_t htype;/* Hardware type: 1 = Ethernet */
	uint8_t hlen;/* Hardware address length: 6 for Ethernet */
	uint8_t hops;/* Number of hops */
	uint32_t xid;/* Transaction ID */
	uint16_t secs;/* Seconds elapsed since client began address acquisition or renewal process */
	uint16_t flags;/* Flags */
	uint32_t ciaddr;/* Client IP address (if already in use) */
	uint32_t yiaddr;/* 'Your' (client) IP address */
	uint32_t siaddr;/* Next server IP address */
	uint32_t giaddr;/* Relay agent IP address */
	uint8_t chaddr[16];/* Client hardware address */
	uint8_t sname[64];/* Optional server host name */
	uint8_t file[128];/* Boot file name */
	uint32_t magic_cookie;/* Magic cookie */
	uint8_t options[0];/* Optional parameters field */
} __packed;

/*
 * sdc_features_ext_pkt_info
 *	SDC extension packet info.
 */
struct sdc_features_ext_pkt_info {
	uint8_t *smac;
	uint8_t *l4_payload;
	ssize_t l4_len;
};

extern void sdc_features_ext_dhcp(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info);
extern void sdc_features_ext_dns(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info, bool is_mdns);
extern void sdc_features_ext_dhcp_egress(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info);
extern void sdc_features_ext_dns_egress(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info, uint8_t *dst_mac);
extern void sdc_features_ext_ssdp(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info);
extern void sdc_features_ext_tls(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info);
extern void sdc_features_ext_quic(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info);
#endif
