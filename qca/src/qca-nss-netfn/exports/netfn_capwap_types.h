/*
 * netfn_capwap_types.h
 *	Network function's CAPWAP public object definitions.
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
#ifndef __NETFN_CAPWAP_TYPES_H
#define __NETFN_CAPWAP_TYPES_H

#include <netfn_types.h>
#include <netfn_flow_cookie.h>

#define NETFN_CAPWAP_VERSION_V1			0x1
#define NETFN_CAPWAP_VERSION_V2			0x2

#define NETFN_CAPWAP_ENC_FLAG_VLAN		0x00000001
#define NETFN_CAPWAP_ENC_FLAG_PPPOE		0x00000002
#define NETFN_CAPWAP_ENC_FLAG_UDPLITE_HDR_CSUM	0x00000004

#define NETFN_CAPWAP_FEATURES_DTLS		0x00000001
#define NETFN_CAPWAP_FEATURES_INNER_SGT		0x00000002
#define NETFN_CAPWAP_FEATURES_OUTER_SGT		0x00000004
#define NETFN_CAPWAP_FEATURES_WLAN_QOS		0x00000008
#define NETFN_CAPWAP_FEATURES_FLOW_COOKIE	0x00000010

#define NETFN_CAPWAP_PKT_TYPE_INVALID		0x1000
#define NETFN_CAPWAP_PKT_TYPE_CTRL		0x0001
#define NETFN_CAPWAP_PKT_TYPE_DATA		0x0002
#define NETFN_CAPWAP_PKT_TYPE_DTLS		0x0004
#define NETFN_CAPWAP_PKT_TYPE_WINFO		0x0008
#define NETFN_CAPWAP_PKT_TYPE_80211		0x0010
#define NETFN_CAPWAP_PKT_TYPE_8023		0x0020
#define NETFN_CAPWAP_PKT_TYPE_SCS_ID_VALID	0x0040
#define NETFN_CAPWAP_PKT_TYPE_SDWF_ID_VALID	0x0080
#define NETFN_CAPWAP_PKT_TYPE_ENC_KEEPALIVE	0x0100
#define NETFN_CAPWAP_PKT_TYPE_BYPASS_KEEPALIVE	0x0200

#define NETFN_CAPWAP_WIRELESS_INFO_LEN		8
#define NETFN_CAPWAP_SNAP_HDR_LEN 		6
#define NETFN_CAPWAP_MAX_BUF_SZ			9000
#define NETFN_CAPWAP_MAX_FRAGS 			10

#define NETFN_CAPWAP_DATA_PORT			5247
#define NETFN_CAPWAP_CTRL_PORT			5246

/*
 * netfn_capwap_enc_cfg
 *	Encapsulation configuration
 */
struct netfn_capwap_enc_cfg {
	uint32_t flags;			/**< Vlan/pppoe/udplite_checksum configured. */
	uint16_t csum_cov;		/**< UDP Checksum coverage. */
	uint16_t mtu;			/**< MTU to be configured for fragmentation. */
	uint8_t ttl;			/**< TTL configured for tunnel. */
	uint8_t tos;			/**< ToS for encapsulation. */
	uint8_t bssid[ETH_ALEN];	/**< Basic service set ID. */
	uint8_t snap_hdr[NETFN_CAPWAP_SNAP_HDR_LEN];
					/**< Custom snap header. */
	uint32_t reserve[4];		/**< Reserve for future use. */
};

/*
 * netfn_capwap_dec_cfg
 *	Decapsulation configuration
 */
struct netfn_capwap_dec_cfg {
	uint32_t max_frags;		/**< Maximum number of fragments expected. */
	uint16_t max_payload_sz;	/**< Maximum size of the payload buffer. */
	uint32_t reserve[4];		/**< Reserve for future use. */
};

/*
 * netfn_capwap_tun_cfg
 *	Tunnel configuration
 */
struct netfn_capwap_tun_cfg {
	struct netfn_capwap_enc_cfg enc;	/**< Encap configuration of tunnel. */
	struct netfn_capwap_dec_cfg dec;	/**< Decap configuration of tunnel. */
	uint32_t features;			/**< Features enabled for tunnel. */
	uint16_t id;				/**< Tunnel identifier. */
	uint8_t capwap_ver;			/**< Capwap version. */
	uint32_t reserve[4];		/**< Reserve for future use. */
};

/*
 * netfn_capwap_dec_stats
 *	Capwap decap stats.
 */
struct netfn_capwap_dec_stats {
	uint64_t pkts_rcvd;			/**< Number of packets received. */
	uint64_t dtls_pkts;			/**< DTLS pkts received directly. */
	uint64_t control_pkts;			/**< Control packets. */
	uint64_t keepalive_pkts;		/**< Keep alive packets recieved. */
	uint64_t fast_reasm;			/**< Fast reassembly hits. */
	uint64_t slow_reasm;			/**< Slow reassembly hits. */
	uint64_t flow_cookie_no_db;		/**< Flow Cookie DB Fetch Failures */
	/*
	 * Drop and error stats.
	 */
	uint64_t drop_missing_frags;		/**< Drop in reassembly window because of missing fragments. */
	uint64_t drop_queue_full;		/**< Drop in during IPI to other core bcz of queue full. */
	uint64_t drop_pri_queue_full;		/**< Drop in during IPI to other core bcz of priority queue full. */
	uint64_t err_dec_failure;		/**< Failed during decapsulation. */
	uint64_t err_max_frags;			/**< Exceeds max fragments allowed(10). */
	uint64_t err_large_frags;		/**< Error packet of oversize. */
	uint64_t err_csum_fail;			/**< Error packet because of wrong checksum. */
	uint64_t err_malformed;			/**< Error packet because of malformed packet. */
	uint64_t err_excess_len;		/**< Error packet if frag size exceeds total len. */
	uint64_t err_nwireless_len;		/**< Error packets because of invalid nwireless len. */
	uint64_t reserve[2];			/**< Reserve for future use. */
};

/*
 * netfn_capwap_enc_stats
 *	Capwap encap stats.
 */
struct netfn_capwap_enc_stats {
	uint64_t pkts_rcvd;			/**< Number of packets received. */
	uint64_t num_frags;			/**< Number of segments or fragments generated. */
	uint64_t keepalive_pkts;		/**< Keep alive packets recieved. */
	/*
	 * Drop and error stats.
	 */
	uint64_t drop_mem_alloc;		/**< Packets dropped because of a memory failure. */
	uint64_t drop_queue_full;		/**< Packets dropped because the queue is full. */
	uint64_t err_dev_tx;			/**< Error packets because of dev xmit failed. */
	uint64_t err_ver_mis;			/**< Error packets because of of a version mismatch. */
	uint64_t err_direct_dtls;		/**< Error packets because of DTLS packet. */
	uint64_t err_nwireless_len;		/**< Error packets because of invalid nwireless len. */
	uint64_t err_insufficient_hroom;	/**< Error packets because of insufficent headroom. */
	uint64_t reserve[2];			/**< Reserve for future use. */
};

/*
 * netfn_capwap_pkt_stats
 *	Tunnel packet stats.
 */
struct netfn_capwap_pkt_stats {
	uint64_t tx_pkts;		/**< Total tx packets. */
	uint64_t tx_bytes;		/**< Total tx bytes. */
	uint64_t tx_errors;		/**< Total tx error. */
	uint64_t tx_dropped;		/**< Total tx dropped. */
	uint64_t rx_pkts;		/**< Total rx packets. */
	uint64_t rx_bytes;		/**< Total rx bytes. */
	uint64_t rx_errors;		/**< Total rx error. */
	uint64_t rx_dropped;		/**< Total rx dropped. */
	uint64_t reserve[2];		/**< Reserved for future use. */
};

/*
 * netfn_capwap_tun_stats
 *	Tunnel statistics
 */
struct netfn_capwap_tun_stats {
	struct netfn_capwap_pkt_stats pkts;	/**< Tunnel packet stats. */
	struct netfn_capwap_dec_stats dec;	/**< Tunnel decap stats. */
	struct netfn_capwap_enc_stats enc;	/**< Tunnel encap stats. */
};

/*
 * netfn_capwap_prehdr.
 *
 * NOTE: This structure is a copy of nss_capwap_metaheader.
 * When updating this structure, nss_capwap_metaheader should also
 * be updated.
 */
struct netfn_capwap_prehdr {
	uint8_t version;	/* CAPWAP version */
	uint8_t rid;		/* Radio ID */
	uint16_t tunnel_id;     /**< Tunnel-ID. */
	uint8_t dscp;		/* DSCP value */
	uint8_t vlan_pcp;	/* VLAN priority .P marking */
	uint16_t type;		/* Type of CAPWAP packet & What was there in CAPWAP header */
	uint16_t nwireless;	/* Number of wireless info sections in CAPWAP header */
	uint16_t wireless_qos;	/* 802.11e qos info */
	uint16_t outer_sgt;	/* Security Group Tag value in the TrustSec header */
	uint16_t inner_sgt;	/* Security Group Tag value in the TrustSec header */
	uint32_t flow_id;       /* Flow ID of the pkt fragment */
	union {
		struct {
			uint16_t vapid;		/* VAP ID info */
			uint16_t reserved;	/* Reserved for backward compatibility */
		};
		uint32_t scs_sdwf_id;	/* SCS or SDWF Identification */
	};

	/*
	 * Put the wl_info at last so we don't have copy if 802.11 to 802.3 conversion did not happen
	 */
	uint8_t wl_info[NETFN_CAPWAP_WIRELESS_INFO_LEN];
	/* Wireless info preserved from the original packet */
} __attribute((packed, aligned(4)));

#endif /* __NETFN_CAPWAP_TYPES_H */
