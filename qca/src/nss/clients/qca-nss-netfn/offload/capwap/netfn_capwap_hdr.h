/*
 * netfn_capwap_hdr.h
 *	Network function CAPWAP header definitions.
 *
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
#ifndef __NETFN_CAPWAP_HDR_H
#define __NETFN_CAPWAP_HDR_H

#include <linux/ieee80211.h>
#include <linux/if_vlan.h>

/*
 * First word
 */
#define NETFN_CAPWAP_HDR_PREAMBLE_SHIFT	24
#define NETFN_CAPWAP_HDR_HLEN_SHIFT	19
#define NETFN_CAPWAP_HDR_RID_SHIFT	14
#define NETFN_CAPWAP_HDR_WBID_SHIFT	9
#define NETFN_CAPWAP_HDR_TBIT_SHIFT	8
#define NETFN_CAPWAP_HDR_FBIT_SHIFT	7
#define NETFN_CAPWAP_HDR_LBIT_SHIFT	6
#define NETFN_CAPWAP_HDR_WBIT_SHIFT	5
#define NETFN_CAPWAP_HDR_MBIT_SHIFT	4
#define NETFN_CAPWAP_HDR_KBIT_SHIFT	3
#define NETFN_CAPWAP_HDR_FLAGS_SHIFT	0

/*
 * Second word
 */
#define NETFN_CAPWAP_HDR_FID_SHIFT	16
#define NETFN_CAPWAP_HDR_FOFFSET_SHIFT	3

#define NETFN_CAPWAP_HDR_PREAMBLE_MASK	GENMASK(31, 24)
#define NETFN_CAPWAP_HDR_HLEN_MASK	GENMASK(23, 19)
#define NETFN_CAPWAP_HDR_RID_MASK	GENMASK(18, 14)
#define NETFN_CAPWAP_HDR_WBID_MASK	GENMASK(13, 9)
#define NETFN_CAPWAP_HDR_TBIT_MASK	BIT_MASK(8)
#define NETFN_CAPWAP_HDR_FBIT_MASK	BIT_MASK(7)
#define NETFN_CAPWAP_HDR_LBIT_MASK	BIT_MASK(6)
#define NETFN_CAPWAP_HDR_WBIT_MASK	BIT_MASK(5)
#define NETFN_CAPWAP_HDR_MBIT_MASK	BIT_MASK(4)
#define NETFN_CAPWAP_HDR_KBIT_MASK	BIT_MASK(3)
#define NETFN_CAPWAP_HDR_FLAGS_MASK	GENMASK(2, 0)

#define NETFN_CAPWAP_HDR_FID_MASK	GENMASK(31, 16)
#define NETFN_CAPWAP_HDR_FOFFSET_MASK	GENMASK(15, 3)
#define NETFN_CAPWAP_HDR_RES_MASK	GENMASK(2, 0)


#define NETFN_CAPWAP_FLAGS_IPV6		BIT_MASK(0)
#define NETFN_CAPWAP_FLAGS_UDPLITE	BIT_MASK(1)
#define NETFN_CAPWAP_FLAGS_WINFO	BIT_MASK(2)

#define NETFN_CAPWAP_TYPE_CAPWAP	0x0
#define NETFN_CAPWAP_TYPE_DTLS		0x01
#define NETFN_CAPWAP_VERSION		0x3
#define NETFN_CAPWAP_CB(skb) ((struct netfn_capwap_hdr_mdata *)((skb)->cb))
#define NETFN_CAPWAP_HEADROOM_RESERVE	128 /* Bytes */
#define NETFN_CAPWAP_TAILROOM_RESERVE	128 /* Bytes */
#define NETFN_CAPWAP_MAX_NWIRELESS	1
#define NETFN_CAPWAP_WLAN_QOS		2

#define NETFN_CAPWAP_PKT_ENC_MASK (NETFN_CAPWAP_PKT_TYPE_WINFO \
		| NETFN_CAPWAP_PKT_TYPE_80211 | NETFN_CAPWAP_PKT_TYPE_8023)

#define NETFN_CAPWAP_HDR_IPV4_SZ (sizeof(struct iphdr) + sizeof(struct udphdr))
#define NETFN_CAPWAP_L2_HDR_SZ		(sizeof(struct ethhdr) + sizeof(struct vlan_hdr))
#define NETFN_CAPWAP_MIN_HDR_SZ (NETFN_CAPWAP_L2_HDR_SZ + sizeof(struct ipv6hdr) \
		  + sizeof(struct udphdr) + sizeof(struct netfn_capwap_hdr))
#define NETFN_CAPWAP_8023_TO_80211_SZ (sizeof(struct ieee80211_hdr_3addr) + sizeof(struct netfn_capwap_winfo) \
		+ NETFN_CAPWAP_SNAP_HDR_LEN + NETFN_CAPWAP_WLAN_QOS - sizeof(struct ethhdr))
								/* wlan header + winfo + snap + wlan_qos - ethernet */
#define NETFN_CAPWAP_MAX_HDR_SZ (NETFN_CAPWAP_MIN_HDR_SZ + NETFN_CAPWAP_8023_TO_80211_SZ)

/*
 * netfn_capwap_winfo
 *	Wireless information.
 */
struct netfn_capwap_winfo {
	uint32_t word0;
	uint32_t word1;
	char payload[0];
} __attribute((packed));

/*
 * netfn_capwap_hdr_mdata
 *	CAPWAP header information
 */
struct netfn_capwap_hdr_mdata {
	struct netfn_capwap_prehdr phdr;	/* Preheader from host */
	uint16_t frag_id;		/* Fragment - ID */
	uint16_t frag_offset;		/* Fragment offset */
	uint8_t frag:1;			/* 1=Fragment, 0=Not a fragment */
	uint8_t frag_end:1;		/* Is frag end */
	uint8_t keep_alive:1;		/* Is keepalive */
	uint8_t winfo:1;		/* Inner contains wireless info */
	uint8_t type_80211:1;		/* Inner is 802.11 */
	uint8_t exception:1;		/* Indicates pkt should be exceptioned. */
};

/*
 * netfn_capwap_enc_mdata
 *     Capwap encapsulation meta data
 */
struct netfn_capwap_enc_mdata {
	struct netfn_capwap_prehdr phdr;	/* Preheader from host */
	uint16_t frag_id;			/* Fragment - ID */
	uint16_t frag_offset;			/* Fragment offset */
	uint8_t frag:1;				/* Is fragment or not */
	uint8_t frag_end:1;			/* Frag end or not */
	uint8_t first_frag:1;			/* First frag or not */
};

/*
 * NETFN CAPWAP header
 *
 *    3                   2                   1                   0
 *  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |CAPWAP Preamble|   HLEN  |   RID   |   WBID  |T|F|L|W|M|K|Flags|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Fragment ID          |       Frag Offset       |Rsvd |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                 (optional) Radio MAC Address                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |           (optional) Wireless Specific Information            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Payload ....                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
union netfn_capwap_hdr_info {
	struct {
		uint8_t flags:3;
		uint8_t kbit:1;		/* 1=Keep Alive packet, 0=Not a keep alive packet */
		uint8_t mbit:1;		/* 1=MAC address is present, 0=not present */
		uint8_t wbit:1;		/* 1=wireless info present, 0=not present */
		uint8_t lbit:1;		/* 1=Last fragment, 0=Not the last fragment */
		uint8_t fbit:1;		/* 1=Fragmented, 0=Not fragmented */
		uint8_t tbit:1;		/* 1=Inner is 802.11, 0=Inner is 802.3 */
		uint8_t wbid:5;		/* Type of wlan packet: 0=Reserved, 1=IEEE 802.11, 2=Reserved, 3=EPCGlobal [EPCGlobal] */
		uint8_t rid2:2;		/* Radio ID */
		uint8_t rid1:3;		/* Radio ID */
		uint8_t hlen:5;		/* Header length */
		uint8_t preamble;	/* 0=CAPWAP header, 1=DTLS header */
	} fields;

	__le32 word;		/* Header information words */
} __attribute((packed));

union netfn_capwap_hdr_frag {
	struct {
		uint16_t reserve:3;	/* 13-bit Offset of the fragment in 8 byte words */
		uint16_t offset:13;	/* 13-bit Offset of the fragment in 8 byte words */
		uint16_t id;		/* Fragment ID */

	} fields;
	__le32 word;		/* Fragment information words */
} __attribute((packed));

/*
 * netfn_capwap_hdr
 *	CAPWAP header
 */
struct netfn_capwap_hdr {
	__be32 info;		/* Header fields */
	__be32 frag;		/* Fragment fields */
};

/*
 * netfn_capwap_hdr_get_preamble()
 *	Retrieve the premable from the header
 */
static inline uint8_t netfn_capwap_hdr_get_preamble(union netfn_capwap_hdr_info info)
{
	 return (uint8_t)((info.word & NETFN_CAPWAP_HDR_PREAMBLE_MASK) >> NETFN_CAPWAP_HDR_PREAMBLE_SHIFT);
}

/*
 * netfn_capwap_hdr_get_hlen()
 *	Retrieve the header length from the header.
 */
static inline uint8_t netfn_capwap_hdr_get_hlen(union netfn_capwap_hdr_info info)
{
	/*
	 * Left shift by 2 to convert one word into bytes.
	 */
	return (uint8_t)((info.word & NETFN_CAPWAP_HDR_HLEN_MASK) >> NETFN_CAPWAP_HDR_HLEN_SHIFT) << 2;
}

/*
 * netfn_capwap_hdr_get_rid()
 *	Retrieve the rid from the header
 */
static inline uint8_t netfn_capwap_hdr_get_rid(union netfn_capwap_hdr_info info)
{
	 return (uint8_t)((info.word & NETFN_CAPWAP_HDR_RID_MASK) >> NETFN_CAPWAP_HDR_RID_SHIFT);
}

/*
 * netfn_capwap_hdr_get_wbid()
 *	Retrieve the wbid from the header
 */
static inline uint8_t netfn_capwap_hdr_get_wbid(union netfn_capwap_hdr_info info)
{
	 return (uint8_t)((info.word & NETFN_CAPWAP_HDR_WBID_MASK) >> NETFN_CAPWAP_HDR_WBID_SHIFT);
}

/*
 * netfn_capwap_hdr_has_tbit()
 *	Retrieve the T flag from the header
 */
static inline bool netfn_capwap_hdr_has_tbit(union netfn_capwap_hdr_info info)
{
	 return (bool)((info.word & NETFN_CAPWAP_HDR_TBIT_MASK) >> NETFN_CAPWAP_HDR_TBIT_SHIFT);
}

/*
 * netfn_capwap_hdr_has_fbit()
 *	Retrieve the F flag from the header
 */
static inline bool netfn_capwap_hdr_has_fbit(union netfn_capwap_hdr_info info)
{
	 return (bool)((info.word & NETFN_CAPWAP_HDR_FBIT_MASK) >> NETFN_CAPWAP_HDR_FBIT_SHIFT);
}

/*
 * netfn_capwap_hdr_has_lbit()
 *	Retrieve the L flag from the header
 */
static inline bool netfn_capwap_hdr_has_lbit(union netfn_capwap_hdr_info info)
{
	 return (bool)((info.word & NETFN_CAPWAP_HDR_LBIT_MASK) >> NETFN_CAPWAP_HDR_LBIT_SHIFT);
}

/*
 * netfn_capwap_hdr_has_wbit()
 *	Retrieve the W flag from the header
 */
static inline bool netfn_capwap_hdr_has_wbit(union netfn_capwap_hdr_info info)
{
	 return (bool)((info.word & NETFN_CAPWAP_HDR_WBIT_MASK) >> NETFN_CAPWAP_HDR_WBIT_SHIFT);
}

/*
 * netfn_capwap_hdr_has_mbit()
 *	Retrieve the M flag from the header
 */
static inline bool netfn_capwap_hdr_has_mbit(union netfn_capwap_hdr_info info)
{
	 return (bool)((info.word & NETFN_CAPWAP_HDR_MBIT_MASK) >> NETFN_CAPWAP_HDR_MBIT_SHIFT);
}

/*
 * netfn_capwap_hdr_has_kbit()
 *	Retrieve the K flag from the header
 */
static inline bool netfn_capwap_hdr_has_kbit(union netfn_capwap_hdr_info info)
{
	 return (bool)((info.word & NETFN_CAPWAP_HDR_KBIT_MASK) >> NETFN_CAPWAP_HDR_KBIT_SHIFT);
}

/*
 * netfn_capwap_hdr_get_frag_id()
 * 	Get the CAPWAP header's fragment identifier.
 */
static inline __le16 netfn_capwap_hdr_get_frag_id(union netfn_capwap_hdr_frag frag)
{
	 return (__le16)((frag.word & NETFN_CAPWAP_HDR_FID_MASK) >> NETFN_CAPWAP_HDR_FID_SHIFT);
}

/*
 * netfn_capwap_hdr_get_frag_offset()
 * 	Get the CAPWAP header's fragment offset.
 */
static inline __le16 netfn_capwap_hdr_get_frag_offset(union netfn_capwap_hdr_frag frag)
{
	/*
	 * Left shift by 3 to convert two words into bytes.
	 */
	 return (__le16)((frag.word & NETFN_CAPWAP_HDR_FOFFSET_MASK) >> NETFN_CAPWAP_HDR_FOFFSET_SHIFT) << 3;
}

/*
 * netfn_capwap_hdr_set_radio_id()
 *	Set the radio ID in capwap header.
 */
static inline void netfn_capwap_hdr_set_radio_id(union netfn_capwap_hdr_info *info, uint8_t rid)
{
	info->fields.rid1 = (rid >> 2) & 0x7;   /* 3 bits */
	info->fields.rid2 = rid & 0x3;          /* 2 bits only */
}

/*
 * netfn_capwap_hdr_set_hlen()
 *	Set the header length in capwap header.
 */
static inline void netfn_capwap_hdr_set_hlen(union netfn_capwap_hdr_info *info, uint8_t hlen)
{
	info->fields.hlen = (hlen >> 2);
}

/*
 * netfn_capwap_hdr_set_frag_offset()
 *	Set the fragment offset in capwap header.
 */
static inline void netfn_capwap_hdr_set_frag_offset(union netfn_capwap_hdr_frag *frag, uint16_t offset)
{
	frag->fields.offset = (offset >> 3);
}
#endif /* __NETFN_CAPWAP_HDR_H */
