/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
*/
#include <linux/tcp.h>
#include "sdc.h"

/*
 * Extract DHCP extension header info (e.g. DHCP options) from pkt_info.
 */
void sdc_features_ext_dhcp(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info)
{
	struct sdc_features_ext_dhcp_header *dh;
	uint8_t *opt_ptr, *payload_ptr;
	ssize_t remaining, payload_size;
	struct sdc_macdb_entry *entry;
	struct sdc_macdb_ctx *mctx = &ctx->macdb_ctx;

	if (!info->smac || !info->l4_payload || info->l4_len < sizeof(struct sdc_features_ext_dhcp_header)) {
		/*
		 * TODO: Add stats for this
		 */
		DEBUG_ERR("DHCP Packet info is missing or malformed.\n");
		return;
	}
	/*
	 * TODO: Optimize the lock.
	 */
	entry = sdc_macdb_get_or_create(mctx, info->smac);
	if (!entry) {
		return;
	}

	if (entry->state == SDC_MACDB_MAC_STATE_COLLECTED) {
		sdc_macdb_deref(mctx, entry);
		return;
	}

	dh = (struct sdc_features_ext_dhcp_header *)info->l4_payload;
	if (ntohl(dh->magic_cookie) != SDC_FEATURES_EXT_DHCP_MAGIC) {
		sdc_macdb_deref(mctx, entry);
		return;
	}

	/*
	 * Valid DHCP frame from this device — count it regardless of message type.
	 */

	/*
	 * Since options field is a series of TLVs. Parse it one by one.
	 */
	opt_ptr = dh->options;
	payload_ptr = dh->options;

	/*
	 * This is to get the total size of options field.
	 * The logic to check if its a dhcp request frame is to loop through the options
	 * and check for the code assocaited with the request frame.
	 */
	remaining = info->l4_len - ((uint8_t *)opt_ptr - (uint8_t *)dh);
	payload_size = remaining;
	while (remaining > 0) {
		uint8_t code = opt_ptr[0];
		if (code == SDC_FEATURES_EXT_DHCP_OPN_CODE_END) {
			sdc_macdb_deref(mctx, entry);
			return;
		}

		if (code == SDC_FEATURES_EXT_DHCP_OPN_CODE_PAD) {
			opt_ptr += 1;
			remaining -= 1;
			continue;
		}

		uint8_t len = opt_ptr[1];
		if (remaining < len) {
			sdc_macdb_deref(mctx, entry);
			return;
		}

		/*
		 * Check for DHCP request frame based on RFC.
		 * After the check is passed, and if this field contains TLV
		 * info of DHCP option field, TLV data of the DHCP options will be
		 * copied into entry->data.
		 */
		if (code == SDC_FEATURES_EXT_DHCP_OPN_CODE_MSG_TYPE && (len == 1)) {
			if (opt_ptr[2] == SDC_FEATURES_EXT_DHCP_OPN_VAL_REQ) {
				DEBUG_INFO("%pM: DHCP request frame, Adding TLV\n", entry->mac);
				struct sdc_telemetry_tlv tlv = {0};
				tlv.type = SDC_TELEMETRY_TYPE_DHCP;
				/*
				 * Length contains the both the payload and the TLV meta data ( which is type 1 byte, size 2bytes)
				 */
				tlv.len = payload_size + 3;
				tlv.value = payload_ptr;
				if (!sdc_telemetry_add_tlv(entry->data, &tlv, ctx->buf_size)) {
					atomic_inc(&entry->drop_dhcp);
					atomic_inc(&ctx->drop_dhcp);
				}
				sdc_macdb_deref(mctx, entry);
				return;
			} else if (opt_ptr[2] == SDC_FEATURES_EXT_DHCP_OPN_VAL_DISCOVER) {
				DEBUG_INFO("%pM: DHCP discover frame\n", entry->mac);
				atomic_inc(&entry->dhcp_discover_count);
				sdc_macdb_deref(mctx, entry);
				return;
			}
		}

		opt_ptr += 2 + len;
		remaining -= 2 + len;
	}

	sdc_macdb_deref(mctx, entry);
}

/*
 * sdc_features_ext_dhcp_egress
 *
 * Egress handler for DHCP server responses (Offer/ACK/NAK).
 * Matches the client device via the chaddr field in the DHCP header,
 * which carries the client hardware address regardless of the Ethernet
 * destination MAC (unicast or broadcast).
 */
void sdc_features_ext_dhcp_egress(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info)
{
	struct sdc_features_ext_dhcp_header *dh;
	struct sdc_macdb_entry *entry;
	struct sdc_macdb_ctx *mctx = &ctx->macdb_ctx;
	uint8_t *opt_ptr;
	ssize_t remaining;

	if (!info->l4_payload || info->l4_len < sizeof(struct sdc_features_ext_dhcp_header)) {
		return;
	}

	dh = (struct sdc_features_ext_dhcp_header *)info->l4_payload;
	if (ntohl(dh->magic_cookie) != SDC_FEATURES_EXT_DHCP_MAGIC) {
		return;
	}

	/*
	 * Match the client device by chaddr (client hardware address in DHCP header),
	 * not by Ethernet source/destination which carries the server MAC here.
	 */
	entry = sdc_macdb_get(mctx, dh->chaddr);
	if (!entry) {
		atomic_inc(&ctx->egress_dhcp_miss);
		return;
	}

	if (entry->state == SDC_MACDB_MAC_STATE_COLLECTED) {
		sdc_macdb_deref(mctx, entry);
		return;
	}



	opt_ptr = dh->options;
	remaining = info->l4_len - ((uint8_t *)opt_ptr - (uint8_t *)dh);

	while (remaining > 0) {
		uint8_t code = opt_ptr[0];
		if (code == SDC_FEATURES_EXT_DHCP_OPN_CODE_END)
			break;
		if (code == SDC_FEATURES_EXT_DHCP_OPN_CODE_PAD) {
			opt_ptr++;
			remaining--;
			continue;
		}
		if (remaining < 2)
			break;
		uint8_t len = opt_ptr[1];
		if (code == SDC_FEATURES_EXT_DHCP_OPN_CODE_MSG_TYPE && len == 1) {
			switch (opt_ptr[2]) {
			case SDC_FEATURES_EXT_DHCP_OPN_VAL_OFFER:
				atomic_inc(&entry->dhcp_offer_count);
				break;
			case SDC_FEATURES_EXT_DHCP_OPN_VAL_ACK:
				atomic_inc(&entry->dhcp_ack_count);
				break;
			case SDC_FEATURES_EXT_DHCP_OPN_VAL_NAK:
				atomic_inc(&entry->dhcp_nak_count);
				break;
			}
			break;
		}
		opt_ptr += 2 + len;
		remaining -= 2 + len;
	}

	sdc_macdb_deref(mctx, entry);
}

/*
 * sdc_features_ext_dns_egress
 *
 * Egress handler for DNS responses. Matches the client device by destination
 * MAC (dst_mac = eth->h_dest from the egress frame). Extracts RCODE to count
 * NXDOMAIN and SERVFAIL responses for dns_nxdomain_ratio / dns_servfail_ratio.
 */
void sdc_features_ext_dns_egress(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info, uint8_t *dst_mac)
{
	struct sdc_features_ext_dns_header *dns;
	struct sdc_macdb_entry *entry;
	struct sdc_macdb_ctx *mctx = &ctx->macdb_ctx;
	uint16_t flags;
	uint8_t rcode;

	if (!dst_mac || !info->l4_payload || info->l4_len < sizeof(struct sdc_features_ext_dns_header)) {
		return;
	}

	dns = (struct sdc_features_ext_dns_header *)info->l4_payload;
	flags = ntohs(dns->flags);

	if (!(flags & SDC_FEATURES_EXT_DNS_QR_MASK)) {
		return;
	}

	entry = sdc_macdb_get(mctx, dst_mac);
	if (!entry) {
		atomic_inc(&ctx->egress_dns_miss);
		return;
	}

	if (entry->state == SDC_MACDB_MAC_STATE_COLLECTED) {
		sdc_macdb_deref(mctx, entry);
		return;
	}

	atomic_inc(&entry->dns_response_count);

	rcode = flags & SDC_FEATURES_EXT_DNS_RCODE_MASK;
	if (rcode == SDC_FEATURES_EXT_DNS_RCODE_NXDOMAIN)
		atomic_inc(&entry->dns_nxdomain_count);
	else if (rcode == SDC_FEATURES_EXT_DNS_RCODE_SERVFAIL)
		atomic_inc(&entry->dns_servfail_count);

	sdc_macdb_deref(mctx, entry);
}

/*
 * sdc_features_ext_ssdp
 *
 * Ingress handler for SSDP (UDP port 1900). Counts SSDP packets and extracts
 * unique USN strings from device NOTIFY announcements. Each unique USN is
 * stored as a SSDP TLV, capped at SDC_SSDP_USN_MAX per device.
 */
void sdc_features_ext_ssdp(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info)
{
	struct sdc_macdb_entry *entry;
	struct sdc_macdb_ctx *mctx = &ctx->macdb_ctx;
	uint8_t *payload, *line, *end, *usn_start;
	ssize_t remaining;
	uint8_t usn_len;
	uint8_t usn_stored = 0;

	if (!info->smac || !info->l4_payload || info->l4_len == 0) {
		return;
	}

	entry = sdc_macdb_get_or_create(mctx, info->smac);
	if (!entry) {
		return;
	}

	if (entry->state == SDC_MACDB_MAC_STATE_COLLECTED) {
		sdc_macdb_deref(mctx, entry);
		return;
	}

	atomic_inc(&entry->ssdp_pkt_count);

	DEBUG_INFO("%pM: SSDP frame, pkt_count=%d\n", entry->mac,
		   atomic_read(&entry->ssdp_pkt_count));

	/*
	 * Scan the SSDP payload line by line looking for the USN header.
	 * Format: "USN: <value>\r\n"
	 */
	payload = info->l4_payload;
	end = payload + info->l4_len;
	remaining = info->l4_len;

	while (remaining > 4) {
		line = payload;
		if ((line[0] == 'U' || line[0] == 'u') &&
		    (line[1] == 'S' || line[1] == 's') &&
		    (line[2] == 'N' || line[2] == 'n') &&
		    line[3] == ':') {
			usn_start = line + 4;
			while (usn_start < end && *usn_start == ' ')
				usn_start++;

			uint8_t *usn_end = usn_start;
			while (usn_end < end && *usn_end != '\r' && *usn_end != '\n')
				usn_end++;

			usn_len = (uint8_t)(usn_end - usn_start);
			if (usn_len > 0 && usn_stored < SDC_SSDP_USN_MAX) {
				uint8_t tlv_buf[sizeof(struct sdc_telemetry_ssdp_val) + 255];
				struct sdc_telemetry_ssdp_val *sv = (struct sdc_telemetry_ssdp_val *)tlv_buf;
				struct sdc_telemetry_tlv tlv = {0};

				sv->usn_len = usn_len;
				memcpy(sv->usn, usn_start, usn_len);

				tlv.type = SDC_TELEMETRY_TYPE_SSDP;
				tlv.len = sizeof(*sv) + usn_len + SDC_TELEMETRY_TLV_MDATA_SZ;
				tlv.value = (uint8_t *)sv;
				if (!sdc_telemetry_add_tlv(entry->data, &tlv, ctx->buf_size)) {
					atomic_inc(&entry->drop_ssdp);
					atomic_inc(&ctx->drop_ssdp);
				} else
					usn_stored++;
			}
			break;
		}

		/* Advance to next line. */
		while (remaining > 0 && *payload != '\n') {
			payload++;
			remaining--;
		}
		if (remaining > 0) {
			payload++;
			remaining--;
		}
	}

	sdc_macdb_deref(mctx, entry);
}

/*
 * sdc_features_ext_tls
 *
 * Ingress handler for TLS ClientHello. Filters on the two-byte pattern
 * 0x16 (Handshake record) + 0x01 (ClientHello) and forwards the raw TCP
 * payload to userspace as a TLS TLV. Userspace performs all parsing
 * (SNI, ALPN, cipher suites, extensions, JA3 computation).
 */
void sdc_features_ext_tls(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info)
{
	struct sdc_macdb_entry *entry;
	struct sdc_macdb_ctx *mctx = &ctx->macdb_ctx;
	struct sdc_telemetry_tlv tlv = {0};

	if (!info->smac || !info->l4_payload || info->l4_len < 6)
		return;

	/* Filter: TLS Handshake record (0x16) containing a ClientHello (0x01). */
	if (info->l4_payload[0] != SDC_FEATURES_EXT_TLS_CONTENT_HANDSHAKE ||
	    info->l4_payload[5] != SDC_FEATURES_EXT_TLS_HS_CLIENT_HELLO)
		return;

	entry = sdc_macdb_get_or_create(mctx, info->smac);
	if (!entry)
		return;

	if (entry->state == SDC_MACDB_MAC_STATE_COLLECTED) {
		sdc_macdb_deref(mctx, entry);
		return;
	}

	atomic_inc(&entry->tls_ch_count);

	DEBUG_INFO("%pM: TLS ClientHello frame, tls_ch_count=%d\n", entry->mac,
		   atomic_read(&entry->tls_ch_count));

	tlv.type  = SDC_TELEMETRY_TYPE_TLS;
	tlv.len   = info->l4_len + SDC_TELEMETRY_TLV_MDATA_SZ;
	tlv.value = info->l4_payload;
	if (!sdc_telemetry_add_tlv(entry->data, &tlv, ctx->buf_size)) {
		atomic_inc(&entry->drop_tls);
		atomic_inc(&ctx->drop_tls);
	}

	sdc_macdb_deref(mctx, entry);
}

void sdc_features_ext_dns(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info, bool is_mdns)
{
	struct sdc_features_ext_dns_header *dns;
	struct sdc_macdb_entry *entry;
	struct sdc_macdb_ctx *mctx = &ctx->macdb_ctx;
	struct sdc_telemetry_tlv tlv = {0};

	if (!info->smac || !info->l4_payload || info->l4_len < sizeof(struct sdc_features_ext_dns_header)) {
		/*
		 * TODO: Add stats for this
		 */
		DEBUG_ERR("DNS Packet info is missing or malformed.\n");
		return;
	}

	/*
	 * TODO: Optimize the lock.
	 */
	entry =	sdc_macdb_get_or_create(mctx, info->smac);
	if (!entry) {
		return;
	}

	if (entry->state == SDC_MACDB_MAC_STATE_COLLECTED) {
		sdc_macdb_deref(mctx, entry);
		return;
	}


	dns = (struct sdc_features_ext_dns_header *)info->l4_payload;

	/*
	 * Check if its a DNS request frame
	 */
	if (ntohs(dns->flags) & SDC_FEATURES_EXT_DNS_REQ_MASK) {
		sdc_macdb_deref(mctx, entry);
		return;
	}

	/*
	 * Copy the entire DNS frame here.
	 * DNS is a pretty lite wieght header and we have about 10 bytes of overhead per DNS frame.
	 * Compared to parsing and copying only the reqired information, its better to just copy the whole
	 * header and let userspace handle the processing.
	 *
	 * Re-visit if we run into memory issues. We can optimize this to save 10 bytes per dns frame we
	 * send to userspace.
	 */
	if (is_mdns) {
		DEBUG_INFO("%pM: mDNS Request frame, adding TLV\n", entry->mac);
		tlv.type = SDC_TELEMETRY_TYPE_MDNS;
	} else {
		DEBUG_INFO("%pM: DNS Request frame, adding TLV\n", entry->mac);
		tlv.type = SDC_TELEMETRY_TYPE_DNS;
	}
	tlv.len = info->l4_len + 3;
	tlv.value = info->l4_payload;
	if (!sdc_telemetry_add_tlv(entry->data, &tlv, ctx->buf_size)) {
		if (is_mdns) {
			atomic_inc(&entry->drop_mdns);
			atomic_inc(&ctx->drop_mdns);
		} else {
			atomic_inc(&entry->drop_dns);
			atomic_inc(&ctx->drop_dns);
		}
	}
	sdc_macdb_deref(mctx, entry);
	return;
}

/*
 * sdc_features_ext_quic
 *
 * Detects QUIC Initial packets on UDP port 443 and tracks:
 *   - quic_initial_count: total Initial packets from this device
 *   - quic_version_bitmap: bitmask of QUIC versions seen (v1/v2/draft/grease/unknown)
 *
 * No raw frame forwarding — both features are fully captured by counters alone.
 * The QUIC Long Header is unencrypted; the payload is not read.
 *
 * Detection: byte[0] bits 7-6 = 11 (Long Header + Fixed Bit)
 *            byte[0] bits 5-4 = 00 (Initial packet type)
 *            bytes[1-4] != 0  (version 0 means Version Negotiation, not Initial)
 */
void sdc_features_ext_quic(struct sdc_ctx *ctx, struct sk_buff *skb, struct sdc_features_ext_pkt_info *info)
{
	struct sdc_macdb_entry *entry;
	struct sdc_macdb_ctx *mctx = &ctx->macdb_ctx;
	uint32_t version;
	uint8_t  version_bit;

	if (!info->smac || !info->l4_payload || info->l4_len < 5)
		return;

	/* Long Header (bit7) + Fixed Bit (bit6) + Initial type (bits5-4 = 00) */
	if ((info->l4_payload[0] & 0xF0) != SDC_FEATURES_EXT_QUIC_LONG_INITIAL)
		return;

	version = ntohl(*(uint32_t *)(info->l4_payload + 1));
	if (version == 0)
		return;  /* Version Negotiation packet — server sends this, not client Initial */

	if (version == SDC_FEATURES_EXT_QUIC_V1)
		version_bit = SDC_QUIC_VERSION_BIT_V1;
	else if (version == SDC_FEATURES_EXT_QUIC_V2)
		version_bit = SDC_QUIC_VERSION_BIT_V2;
	else if ((version & 0xff000000) == 0xff000000)
		version_bit = SDC_QUIC_VERSION_BIT_DRAFT;
	else if ((version & 0x0f0f0f0f) == 0x0a0a0a0a)
		version_bit = SDC_QUIC_VERSION_BIT_GREASE;
	else
		version_bit = SDC_QUIC_VERSION_BIT_UNKNOWN;

	entry = sdc_macdb_get_or_create(mctx, info->smac);
	if (!entry)
		return;

	if (entry->state == SDC_MACDB_MAC_STATE_COLLECTED) {
		sdc_macdb_deref(mctx, entry);
		return;
	}

	atomic_inc(&entry->quic_initial_count);
	atomic_or(version_bit, &entry->quic_version_bitmap);

	DEBUG_INFO("%pM: QUIC Initial frame, version_bitmap=0x%x count=%d\n", entry->mac,
		   atomic_read(&entry->quic_version_bitmap),
		   atomic_read(&entry->quic_initial_count));

	sdc_macdb_deref(mctx, entry);
}

