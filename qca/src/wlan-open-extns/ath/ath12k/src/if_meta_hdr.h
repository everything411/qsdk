// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __IF_META_HDR_H__
#define __IF_META_HDR_H__

#define NUM_MESH_CONFIG_RATES 1 /* should not be greater than 4 */

#if NUM_MESH_CONFIG_RATES >  4
#error "Num mesh config rates should not be greater than 4!!!"
#endif
#define MAC_ADDR_SIZE 6
enum metahdr_preamble_type {
	METAHDR_PREAMBLE_OFDM,
	METAHDR_PREAMBLE_CCK,
	METAHDR_PREAMBLE_HT,
	METAHDR_PREAMBLE_VHT,
	METAHDR_PREAMBLE_HE_SU,
	METAHDR_PREAMBLE_EHT,
};

struct metahdr_rate_info {
	uint8_t mcs;
	uint8_t nss;
	uint8_t preamble_type;
	uint8_t max_tries;
};

struct meta_hdr_s {
	uint8_t magic;
	uint8_t flags;
	uint8_t channel; /* Operating channel */
	uint8_t keyix;

	uint8_t rssi;
	uint8_t silence;
	uint8_t power;
	uint8_t retries;

	struct metahdr_rate_info rate_info[NUM_MESH_CONFIG_RATES];

	uint8_t band; /* Operating band */

	uint8_t unused[3];
};

#define MAX_CRYPTO_KEY_SIZE 48
struct mesh_recv_hdr_s {
	int8_t      rs_flags;
#define MESH_RX_FIRST_MSDU     0x01
#define MESH_RX_LAST_MSDU      0x02
#define MESH_RX_DECRYPTED      0x04
#define MESH_KEY_NOTFILLED     0x08
#define MESH_RXHDR_VER         0xF0
#define MESH_RXHDR_VER1        0x10
/* first_msdu=1, last_msdu=1  ==> non-Aggregation frame*/
/* first_msdu=1, last_msdu=0  ==> first msdu*/
/* first_msdu=0, last_msdu=0  ==> msdu frame between first and last msdu*/
/* first_msdu=0, last_msdu=1  ==> last msdu*/
	int16_t     rs_rssi;       /* RSSI (noise floor ajusted) */
	uint8_t     rs_channel; /* Operating channel */
	int         rs_ratephy1;
	int         rs_ratephy2;
	int         rs_ratephy3;
	u_int8_t    rs_decryptkey[MAX_CRYPTO_KEY_SIZE];
	u_int8_t    rs_keyix;
	uint8_t     rs_band; /* Operating band */
	uint8_t     rs_snr; /* raw RSSI/SNR read from HW tlv */
};

#define METAHDR_FLAG_TX                 (1<<0) /* packet transmission */
#define METAHDR_FLAG_TX_FAIL            (1<<1) /* transmission failed */
#define METAHDR_FLAG_TX_USED_ALT_RATE   (1<<2) /* used alternate bitrate */
#define METAHDR_FLAG_INFO_UPDATED       (1<<3)
#define METAHDR_FLAG_AUTO_RATE          (1<<5)
#define METAHDR_FLAG_NOENCRYPT          (1<<6)
#define METAHDR_FLAG_NOQOS              (1<<7)

#define METAHDR_FLAG_RX_ERR             (1<<3) /* failed crc check */
#define METAHDR_FLAG_RX_MORE            (1<<4) /* first part of a fragmented skb */
#define METAHDR_FLAG_LOG                (1<<7)

#define METAHDR_FLAG_RX_4SS             (1<<1) /* rx 4ss frame */

#define MESH_FILTER_OUT_FROMDS  (1<<0) /* filter packets with from ds bit set */
#define MESH_FILTER_OUT_TODS    (1<<1) /* filter packets with to ds bit set */
#define MESH_FILTER_OUT_NODS    (1<<2) /* filter packets with no ds bits */
#define MESH_FILTER_OUT_RA      (1<<3) /* filter packets with RA matching own RA */
#define MESH_FILTER_OUT_TA      (1<<4) /* filter packets with TA matching own TA */

#endif
