/*
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/crypto.h>
#include <crypto/aead.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <asm/cacheflush.h>

#include <crypto/md5.h>
#include <crypto/sha.h>
#include <crypto/sha3.h>
#include <crypto/aes.h>
#include <crypto/des.h>
#include <crypto/ghash.h>
#include <crypto/gcm.h>

#include "eip_priv.h"

/*
 * Forward declaration for tr_init functions.
 */
static bool eip_tr_ipsec_aes_cbc_init(struct eip_tr *tr, struct eip_tr_info *info, const struct eip_svc_entry *algo);
static bool eip_tr_ipsec_des_cbc_init(struct eip_tr *tr, struct eip_tr_info *info, const struct eip_svc_entry *algo);
static bool eip_tr_ipsec_aes_gcm_init(struct eip_tr *tr, struct eip_tr_info *info, const struct eip_svc_entry *algo);
static bool eip_tr_ipsec_null_hmac_init(struct eip_tr *tr, struct eip_tr_info *info, const struct eip_svc_entry *algo);

/*
 * Global constant algorith information.
 */
static const struct eip_svc_entry ipsec_algo_list[] = {
	{
		.name = "eip-aes-cbc-sha1-hmac",
		.tr_init = eip_tr_ipsec_aes_cbc_init,
		.auth_digest_len = SHA1_DIGEST_SIZE,
		.auth_block_len = SHA1_BLOCK_SIZE,
		.auth_state_len = EIP_HW_PAD_KEYSZ_SHA1,
		.ctrl_words_0 = EIP_HW_CTX_WITH_KEY | EIP_HW_CTX_ALGO_SHA1 |
			EIP_HW_CTX_AUTH_MODE_HMAC | EIP_TR_IPSEC_SPI | EIP_TR_IPSEC_SEQ_NUM,
		.ctrl_words_1 = EIP_HW_CTX_CIPHER_MODE_CBC,
		.iv_len = AES_BLOCK_SIZE,
		.cipher_blk_len = AES_BLOCK_SIZE
	},
	{
		.name = "eip-aes-cbc-md5-hmac",
		.tr_init = eip_tr_ipsec_aes_cbc_init,
		.auth_digest_len = MD5_DIGEST_SIZE,
		.auth_block_len = MD5_HMAC_BLOCK_SIZE,
		.auth_state_len = EIP_HW_PAD_KEYSZ_MD5,
		.ctrl_words_0 = EIP_HW_CTX_WITH_KEY | EIP_HW_CTX_ALGO_MD5 |
			EIP_HW_CTX_AUTH_MODE_HMAC | EIP_TR_IPSEC_SPI | EIP_TR_IPSEC_SEQ_NUM,
		.ctrl_words_1 = EIP_HW_CTX_CIPHER_MODE_CBC,
		.iv_len = AES_BLOCK_SIZE,
		.cipher_blk_len = AES_BLOCK_SIZE
	},
	{
		.name = "eip-aes-cbc-sha256-hmac",
		.tr_init = eip_tr_ipsec_aes_cbc_init,
		.auth_digest_len = SHA256_DIGEST_SIZE,
		.auth_block_len = SHA256_BLOCK_SIZE,
		.auth_state_len = EIP_HW_PAD_KEYSZ_SHA256,
		.ctrl_words_0 = EIP_HW_CTX_WITH_KEY | EIP_HW_CTX_ALGO_SHA256 |
			EIP_HW_CTX_AUTH_MODE_HMAC | EIP_TR_IPSEC_SPI | EIP_TR_IPSEC_SEQ_NUM,
		.ctrl_words_1 = EIP_HW_CTX_CIPHER_MODE_CBC,
		.iv_len = AES_BLOCK_SIZE,
		.cipher_blk_len = AES_BLOCK_SIZE
	},
	{
		.name = "eip-3des-cbc-sha1-hmac",
		.tr_init = eip_tr_ipsec_des_cbc_init,
		.auth_digest_len = SHA1_DIGEST_SIZE,
		.auth_block_len = SHA1_BLOCK_SIZE,
		.auth_state_len = EIP_HW_PAD_KEYSZ_SHA1,
		.ctrl_words_0 = EIP_HW_CTX_WITH_KEY | EIP_HW_CTX_ALGO_SHA1 |
			EIP_HW_CTX_AUTH_MODE_HMAC | EIP_TR_IPSEC_SPI | EIP_TR_IPSEC_SEQ_NUM,
		.ctrl_words_1 = EIP_HW_CTX_CIPHER_MODE_CBC,
		.iv_len = DES3_EDE_BLOCK_SIZE,
		.cipher_blk_len = DES3_EDE_BLOCK_SIZE
	},
	{
		.name = "eip-3des-cbc-md5-hmac",
		.tr_init = eip_tr_ipsec_des_cbc_init,
		.auth_digest_len = MD5_DIGEST_SIZE,
		.auth_block_len = MD5_HMAC_BLOCK_SIZE,
		.auth_state_len = EIP_HW_PAD_KEYSZ_MD5,
		.ctrl_words_0 = EIP_HW_CTX_WITH_KEY | EIP_HW_CTX_ALGO_MD5 |
			EIP_HW_CTX_AUTH_MODE_HMAC | EIP_TR_IPSEC_SPI | EIP_TR_IPSEC_SEQ_NUM,
		.ctrl_words_1 = EIP_HW_CTX_CIPHER_MODE_CBC,
		.iv_len = DES3_EDE_BLOCK_SIZE,
		.cipher_blk_len = DES3_EDE_BLOCK_SIZE
	},
	{
		.name = "eip-3des-cbc-sha256-hmac",
		.tr_init = eip_tr_ipsec_des_cbc_init,
		.auth_digest_len = SHA256_DIGEST_SIZE,
		.auth_block_len = SHA256_BLOCK_SIZE,
		.auth_state_len = EIP_HW_PAD_KEYSZ_SHA256,
		.ctrl_words_0 = EIP_HW_CTX_WITH_KEY | EIP_HW_CTX_ALGO_SHA256 |
			EIP_HW_CTX_AUTH_MODE_HMAC | EIP_TR_IPSEC_SPI | EIP_TR_IPSEC_SEQ_NUM,
		.ctrl_words_1 = EIP_HW_CTX_CIPHER_MODE_CBC,
		.iv_len = DES3_EDE_BLOCK_SIZE,
		.cipher_blk_len = DES3_EDE_BLOCK_SIZE
	},
	{
		.name = "eip-aes-gcm-rfc4106",
		.tr_init = eip_tr_ipsec_aes_gcm_init,
		.auth_digest_len = GHASH_DIGEST_SIZE,
		.auth_block_len = GHASH_BLOCK_SIZE,
		.auth_state_len = 0,
		.ctrl_words_0 = EIP_HW_CTX_WITH_KEY | EIP_HW_CTX_ALGO_GHASH |
			EIP_HW_CTX_AUTH_MODE_GMAC | EIP_TR_IPSEC_SPI | EIP_TR_IPSEC_SEQ_NUM,
		.ctrl_words_1 = EIP_HW_CTX_CIPHER_MODE_GCM | EIP_TR_IPSEC_CONTROL_IV(0x1) |
			EIP_TR_IPSEC_IV_FORMAT(0x1),
		.iv_len = 8,
		.cipher_blk_len = 4 /* IPsec ESP requires dword aligned */
	},
	{
		.name = "eip-cbc-null-sha1-hmac",
		.tr_init = eip_tr_ipsec_null_hmac_init,
		.auth_digest_len = SHA1_DIGEST_SIZE,
		.auth_block_len = SHA1_BLOCK_SIZE,
		.auth_state_len = EIP_HW_PAD_KEYSZ_SHA1,
		.ctrl_words_0 = EIP_HW_CTX_WITH_KEY | EIP_HW_CTX_ALGO_SHA1 |
			EIP_HW_CTX_AUTH_MODE_HMAC | EIP_TR_IPSEC_SPI | EIP_TR_IPSEC_SEQ_NUM,
		.ctrl_words_1 = EIP_HW_CTX_CIPHER_MODE_ECB,
		.iv_len = 0,
		.cipher_blk_len = 4
	},
	{
		.name = "eip-cbc-null-sha256-hmac",
		.tr_init = eip_tr_ipsec_null_hmac_init,
		.auth_digest_len = SHA256_DIGEST_SIZE,
		.auth_block_len = SHA256_BLOCK_SIZE,
		.auth_state_len = EIP_HW_PAD_KEYSZ_SHA256,
		.ctrl_words_0 = EIP_HW_CTX_WITH_KEY | EIP_HW_CTX_ALGO_SHA256 |
			EIP_HW_CTX_AUTH_MODE_HMAC | EIP_TR_IPSEC_SPI | EIP_TR_IPSEC_SEQ_NUM,
		.ctrl_words_1 = EIP_HW_CTX_CIPHER_MODE_ECB,
		.iv_len = 0,
		.cipher_blk_len = 4
	}
};

/*
 * eip_tr_ipsec_get_replay_words()
 */
static uint32_t eip_tr_ipsec_get_replay_words(enum eip_ipsec_replay replay)
{
	switch (replay) {
	case EIP_IPSEC_REPLAY_NONE:
		return 0;
	case EIP_IPSEC_REPLAY_32:
		return 1;
	case EIP_IPSEC_REPLAY_64:
		return 2;
	case EIP_IPSEC_REPLAY_128:
		return 4;
	case EIP_IPSEC_REPLAY_384:
	default:
		return 12;
	}
}

/*
 * eip_tr_ipsec_get_replay_seq_mask()
 */
static uint32_t eip_tr_ipsec_get_replay_seq_mask(enum eip_ipsec_replay replay)
{
	switch (replay) {
	case EIP_IPSEC_REPLAY_NONE:
		return 0;
	case EIP_IPSEC_REPLAY_32:
		return EIP_TR_IPSEC_SEQ_NUM_MASK_32;
	case EIP_IPSEC_REPLAY_64:
		return EIP_TR_IPSEC_SEQ_NUM_MASK_64;
	case EIP_IPSEC_REPLAY_128:
		return EIP_TR_IPSEC_SEQ_NUM_MASK_128;
	case EIP_IPSEC_REPLAY_384:
	default:
		return EIP_TR_IPSEC_SEQ_NUM_MASK_384;
	}
}

/*
 * eip_tr_ipsec_get_ohdr()
 */
static uint8_t eip_tr_ipsec_get_ohdr(uint32_t sa_flags)
{
	uint32_t val = sa_flags & (EIP_TR_IPSEC_FLAG_ENC | EIP_TR_IPSEC_FLAG_IPV6
			| EIP_TR_IPSEC_FLAG_UDP | EIP_TR_IPSEC_FLAG_TUNNEL);

	switch (val) {
	/*
	 * Encapsulation.
	 */
	case (EIP_TR_IPSEC_FLAG_ENC | EIP_TR_IPSEC_FLAG_TUNNEL | EIP_TR_IPSEC_FLAG_UDP):
		return EIP_TR_IPSEC_OHDR_PROTO_V4_TUNNEL_NATT_ENC;
	case (EIP_TR_IPSEC_FLAG_ENC | EIP_TR_IPSEC_FLAG_TUNNEL | EIP_TR_IPSEC_FLAG_IPV6):
		return EIP_TR_IPSEC_OHDR_PROTO_V6_TUNNEL_ENC;
	case (EIP_TR_IPSEC_FLAG_ENC | EIP_TR_IPSEC_FLAG_TUNNEL):
		return EIP_TR_IPSEC_OHDR_PROTO_V4_TUNNEL_ENC;
	case (EIP_TR_IPSEC_FLAG_ENC | EIP_TR_IPSEC_FLAG_UDP):
		return EIP_TR_IPSEC_OHDR_PROTO_V4_TRANSPORT_NATT_ENC;
	case (EIP_TR_IPSEC_FLAG_ENC | EIP_TR_IPSEC_FLAG_IPV6):
		return EIP_TR_IPSEC_OHDR_PROTO_V6_TRANSPORT_ENC;
	case (EIP_TR_IPSEC_FLAG_ENC):
		return EIP_TR_IPSEC_OHDR_PROTO_V4_TRANSPORT_ENC;

	/*
	 * Decapsulation.
	 */
	case (EIP_TR_IPSEC_FLAG_TUNNEL | EIP_TR_IPSEC_FLAG_UDP):
		return EIP_TR_IPSEC_OHDR_PROTO_V4_TUNNEL_NATT_DEC;
	case (EIP_TR_IPSEC_FLAG_TUNNEL | EIP_TR_IPSEC_FLAG_IPV6):
		return EIP_TR_IPSEC_OHDR_PROTO_V6_TUNNEL_DEC;
	case (EIP_TR_IPSEC_FLAG_TUNNEL):
		return EIP_TR_IPSEC_OHDR_PROTO_V4_TUNNEL_DEC;
	case (EIP_TR_IPSEC_FLAG_UDP):
		return EIP_TR_IPSEC_OHDR_PROTO_V4_TRANSPORT_NATT_DEC;
	case (EIP_TR_IPSEC_FLAG_IPV6):
		return EIP_TR_IPSEC_OHDR_PROTO_V6_TRANSPORT_DEC;
	default:
		return EIP_TR_IPSEC_OHDR_PROTO_V4_TRANSPORT_DEC;
	}
}

/*
 * eip_tr_ipsec_dec_cmn_init()
 *	Initialize the transform record for ipsec Decapsulation.
 */
static void eip_tr_ipsec_dec_cmn_init(struct eip_tr *tr, struct eip_tr_info *info, const struct eip_svc_entry *algo)
{
	struct eip_tr_info_ipsec *ipsec = &info->ipsec;
	struct eip_tr_base *crypto = &info->base;
	uint32_t *crypt_words = tr->hw_words;
	uint32_t *tr_words = tr->hw_words;
	uint8_t seq_num_offst;
	uint8_t mask_sz;
	uint32_t size;

	uint8_t esn = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_ESN);
	uint8_t natt = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_UDP);
	uint8_t tun = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_TUNNEL);

	/*
	 * First two words are Control words.
	 */
	tr_words[0] = algo->ctrl_words_0;
	tr_words[1] = algo->ctrl_words_1;

	/*
	 * Enable IPsec specific fields in control words 0.
	 */
	tr_words[0] |= eip_tr_ipsec_get_replay_seq_mask(ipsec->replay);
	tr_words[0] |= EIP_TR_IPSEC_EXT_SEQ_NUM(esn);
	tr_words[1] |= EIP_TR_IPSEC_PAD_TYPE;

	/*
	 * Crypto variable words starts from third words.
	 */
	crypt_words = &tr_words[2];

	/*
	 * Fill cipher key.
	 */
	memcpy(crypt_words, crypto->cipher.key_data, crypto->cipher.key_len);
	crypt_words += (crypto->cipher.key_len /  sizeof(uint32_t));

	/*
	 * Leave the space for inner & outer digest / GHASH key
	 * For GCM we store only GHASH key of size auth_block_len
	 * and for other algo we store inner & outer digest each
	 * of size auth_state_len
	 */
	size = algo->auth_state_len ? (algo->auth_state_len * 2) : algo->auth_block_len;
	crypt_words += (size / sizeof(uint32_t));

	/*
	 * IPsec specific fields for EIP96.
	 * - SPI in Host order (1 word)
	 * - Sequence number (2 words for ESN else 1 word)
	 * - Sequence mask to support window size.
	 */
	*crypt_words++ = ntohl(ipsec->spi_idx);
	*crypt_words++ = 0x0;
	seq_num_offst = crypt_words - tr_words - 1;
	if (esn)
		*crypt_words++ = 0;

	/*
	 * Set all bits to '1' in mask. For 32bit we need to set extra word to '1'.
	 */
	mask_sz = eip_tr_ipsec_get_replay_words(ipsec->replay);
	mask_sz += ipsec->replay == EIP_IPSEC_REPLAY_32 ? 1 : 0;
	while (mask_sz--) {
		*crypt_words++ = 0xFFFFFFFF;
	}

	/*
	 * Fill nonce.
	 */
	if (crypto->nonce)
		*crypt_words++ = crypto->nonce;

	/*
	 * Done with EIP crypto context words.
	 * Update relative information in control words.
	 */
	tr_words[0] |= EIP_TR_CTRL_CONTEXT_WORDS(crypt_words - tr_words - 2);

	/*
	 * Configure IPsec decapsulation specfic field in TR.
	 */
	tr_words[64] = tun ? EIP_TR_IPSEC_DECAP_TUNNEL_TOKEN_HDR : EIP_TR_IPSEC_DECAP_TRANSPORT_TOKEN_HDR;

	tr_words[65] = EIP_TR_IPSEC_IPHDR_PROC;
	tr_words[65] |= EIP_TR_IPSEC_EXT_SEQ_NUM_PROC(esn);
	if (ipsec->ip_ver == 6) {
		tr_words[65] |= EIP_TR_IPSEC_IPV6_EN;
	}

	/*
	 * Redirect packet to inline chanel after encapsulation.
	 * TODO: Round robin ring
	 */
	if (tr->svc == EIP_SVC_HYBRID_IPSEC) {
		struct eip_ctx *ctx = tr->ctx;
		tr_words[65] |= EIP_TR_REDIR_EN;
		tr_words[65] |= EIP_TR_REDIR_IFACE(ctx->dma[1].ring_id);;
		pr_debug("%px: Redirection enable to ring(%u) for decap record\n", tr, ctx->dma[0].ring_id);
	}

	tr_words[66] = EIP_TR_IPSEC_TTL(ipsec->ip_ttl);
	tr_words[66]|= (algo->cipher_blk_len / 2);

	tr_words[68] = EIP_TR_IPSEC_OHDR_PROTO(eip_tr_ipsec_get_ohdr(ipsec->sa_flags));
	tr_words[68] |= EIP_TR_IPSEC_ICV_SIZE(ipsec->icv_len);
	tr_words[68] |= EIP_TR_IPSEC_IV_SIZE(algo->iv_len);

	/*
	 * Configure outer UDP port information if natt is enabled.
	 */
	if (natt) {
		tr_words[69] = ipsec->src_port;
		tr_words[69] |= (ipsec->dst_port << 16);
	}

	/*
	 * token verify instruction
	 * Enable Sequence number verification if window is enabled
	 */
	tr_words[70] = EIP_TR_IPSEC_DECAP_TOKEN_VERIFY;
	tr_words[70] |= EIP_TR_IPSEC_DECAP_TOKEN_VERIFY_PAD | EIP_TR_IPSEC_DECAP_TOKEN_VERIFY_HMAC;
	tr_words[70] |= ipsec->replay ? EIP_TR_IPSEC_DECAP_TOKEN_VERIFY_SEQ : 0;

	/*
	 * token context instruction
	 * append sequence number location
	 */
	tr_words[71] = esn ? EIP_TR_IPSEC_DECAP_ESN_TOKEN_INST : EIP_TR_IPSEC_DECAP_TOKEN_INST;
	tr_words[71] |= seq_num_offst;

	/*
	 * Update instruction with number of words to hold window and sequence number.
	 */
	if (ipsec->replay != EIP_IPSEC_REPLAY_NONE) {
		uint8_t seq_words = esn + 1 + eip_tr_ipsec_get_replay_words(ipsec->replay);
		tr_words[71] |= EIP_TR_IPSEC_DECAP_TOKEN_INST_SEQ_UPDATE(seq_words);
	}
}

/*
 * eip_tr_ipsec_enc_cmn_init()
 *	Initialize the transform record for ipsec encapsulation.
 */
static void eip_tr_ipsec_enc_cmn_init(struct eip_tr *tr, struct eip_tr_info *info, const struct eip_svc_entry *algo)
{
	struct eip_tr_info_ipsec *ipsec = &info->ipsec;
	struct eip_tr_base *crypto = &info->base;
	uint32_t *crypt_words = tr->hw_words;
	uint32_t *tr_words = tr->hw_words;
	uint8_t seq_num_offst;
	uint32_t size;

	uint8_t esn = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_ESN);
	uint8_t natt = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_UDP);
	uint8_t copy_dscp = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_CP_TOS);
	uint8_t copy_df = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_CP_DF);

	/*
	 * First two words are Control words.
	 */
	tr_words[0] = algo->ctrl_words_0;
	tr_words[1] = algo->ctrl_words_1;

	/*
	 * Enable IPsec specific fields in control words 0.
	 */
	tr_words[0] |= EIP_TR_IPSEC_EXT_SEQ_NUM(esn);

	/*
	 * Crypto variable words starts from third words.
	 */
	crypt_words = &tr_words[2];

	/*
	 * Fill cipher key.
	 */
	memcpy(crypt_words, crypto->cipher.key_data, crypto->cipher.key_len);
	crypt_words += (crypto->cipher.key_len /  sizeof(uint32_t));

	/*
	 * Leave the space for inner & outer digest / GHASH key
	 * For GCM we store only GHASH key of size auth_block_len
	 * and for other algo we store inner & outer digest each
	 * of size auth_state_len
	 */
	size = algo->auth_state_len ? (algo->auth_state_len * 2) : algo->auth_block_len;
	crypt_words += (size / sizeof(uint32_t));

	/*
	 * IPsec specific fields for EIP96.
	 * - SPI in Host order (1 word)
	 * - Sequence number start (2 words for ESN else 1 word)
	 */
	*crypt_words++ = ntohl(ipsec->spi_idx);
	*crypt_words++ = 0x1;
	seq_num_offst = crypt_words - tr_words - 1;
	if (esn)
		*crypt_words++ = 0;

	/*
	 * Fill nonce.
	 */
	if (crypto->nonce)
		*crypt_words++ = crypto->nonce;

	/*
	 * Done with EIP crypto context words.
	 * Update relative information in control words.
	 */
	tr_words[0] |= EIP_TR_CTRL_CONTEXT_WORDS(crypt_words - tr_words - 2);
	tr_words[1] |= EIP_TR_IPSEC_SEQ_NUM_OFFSET_EN;
	tr_words[1] |= EIP_TR_IPSEC_SEQ_NUM_OFFSET(seq_num_offst);
	tr_words[1] |= EIP_TR_IPSEC_SEQ_NUM_STORE;

	/*
	 * Configure IPsec encapsulation specfic field in TR.
	 */
	if (ipsec->ip_ver == 4) {
		uint32_t csum;

		tr_words[56] = ipsec->src_ip[0];
		tr_words[60] = ipsec->dst_ip[0];

		/*
		 * Checksum require for IPv4.
		 */
		csum = (ipsec->src_ip[0] & 0xFFFF) + (ipsec->src_ip[0] >> 16);
		csum += (ipsec->dst_ip[0] & 0xFFFF) + (ipsec->dst_ip[0] >> 16);
		tr_words[57] = (csum & 0xFFFF) + (csum >> 16);

		tr_words[65] = 0;
	} else {
		tr_words[56] = ipsec->src_ip[0];
		tr_words[57] = ipsec->src_ip[1];
		tr_words[58] = ipsec->src_ip[2];
		tr_words[59] = ipsec->src_ip[3];

		tr_words[60] = ipsec->dst_ip[0];
		tr_words[61] = ipsec->dst_ip[1];
		tr_words[62] = ipsec->dst_ip[2];
		tr_words[63] = ipsec->dst_ip[3];

		tr_words[65] = EIP_TR_IPSEC_IPV6_EN;
	}

	/*
	 * Encap token header.
	 */
	tr_words[64] = EIP_TR_IPSEC_ENCAP_TOKEN_HDR;
	tr_words[64] |= EIP_TR_IPSEC_ENCAP_TOKEN_HDR_IV;

	/*
	 * Outer DF: 0 = Copy from inner, 1 = clear df, 2 = set df.
	 * DSCP En: 0 = Copy from inner, 1 = use default.
	 */
	tr_words[65] |= EIP_TR_IPSEC_DF(copy_df ? 0 : (ipsec->ip_df ? 2 : 1));
	tr_words[65] |= EIP_TR_IPSEC_DSCP_COPY_EN(!copy_dscp);
	tr_words[65] |= EIP_TR_IPSEC_IPHDR_PROC;
	tr_words[65] |= EIP_TR_IPSEC_EXT_SEQ_NUM_PROC(esn);

	/*
	 * Redirect packet to inline chanel after encapsulation.
	 */
	if (tr->svc == EIP_SVC_HYBRID_IPSEC) {
		tr_words[65] |= EIP_TR_REDIR_EN;
		tr_words[65] |= EIP_TR_REDIR_IFACE(EIP_HW_INLINE_RING);;
		pr_debug("%px: Redirection enable to inline(%u) for encap record\n", tr, EIP_HW_INLINE_RING);
	}

	tr_words[66] = copy_dscp ? 0 : EIP_TR_IPSEC_DSCP(ipsec->ip_dscp);
	tr_words[66]|= EIP_TR_IPSEC_TTL(ipsec->ip_ttl);
	tr_words[66]|= (algo->cipher_blk_len / 2);

	tr_words[68] = EIP_TR_IPSEC_OHDR_PROTO(eip_tr_ipsec_get_ohdr(ipsec->sa_flags));
	tr_words[68] |= EIP_TR_IPSEC_ICV_SIZE(ipsec->icv_len);
	tr_words[68] |= EIP_TR_IPSEC_IV_SIZE(algo->iv_len);

	/*
	 * Configure outer UDP port information if natt is enabled.
	 */
	if (natt) {
		tr_words[69] = ipsec->src_port;
		tr_words[69] |= (ipsec->dst_port << 16);
	}

	tr_words[70] = EIP_TR_IPSEC_ENCAP_TOKEN_VERIFY;
	tr_words[71] = esn ? EIP_TR_IPSEC_ENCAP_ESN_TOKEN_INST : EIP_TR_IPSEC_ENCAP_TOKEN_INST;
	tr_words[71] |= seq_num_offst;
}

/*
 * eip_tr_ipsec_cmn_init()
 *	Initialize the transform record for ipsec.
 */
static bool eip_tr_ipsec_cmn_init(struct eip_tr *tr, struct eip_tr_info *info, const struct eip_svc_entry *algo)
{
	struct eip_tr_info_ipsec *ipsec = &info->ipsec;
	uint32_t *tr_words = tr->hw_words;
	uint8_t enc;
	uint8_t esn;

	enc = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_ENC);
	esn = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_ESN);

	/*
	 * For decapsulation, Anti-replay must be enabled with ESN.
	 */
	if (!enc && esn && (ipsec->replay == EIP_IPSEC_REPLAY_NONE)) {
		pr_err("%px: Anti-replay is not enabled with ESN.\n", tr);
		return false;
	}

	/*
	 * We first do common initialization and then algo specific initialization.
	 */
	if (enc) {
		eip_tr_ipsec_enc_cmn_init(tr, info, algo);
		tr_words[0] |= EIP_TK_CTRL_OP_ENC_HMAC;
	} else {
		eip_tr_ipsec_dec_cmn_init(tr, info, algo);
		tr_words[0] |= EIP_TK_CTRL_OP_HMAC_DEC;
	}

	/*
	 * We use large record for aead.
	 */
	tr->tr_addr_type = EIP_HW_CTX_TYPE_LARGE;
	tr->tr_addr_type |= virt_to_phys(tr->hw_words);
	tr->tr_flags = ipsec->sa_flags;
	tr->digest_len = algo->auth_digest_len;
	tr->ctrl_words[0] = tr_words[0];
	tr->ctrl_words[1] = tr_words[1];
	tr->nonce = info->base.nonce;

	tr->ipsec.op.tk_fill = NULL;
	tr->ipsec.op.cb = ipsec->cb;
	tr->ipsec.op.err_cb = ipsec->err_cb;
	tr->ipsec.app_data = ipsec->app_data;
	memcpy(tr->bypass, ipsec->ppe_mdata, sizeof(tr->bypass));

	return true;
}

/*
 * eip_tr_ipsec_aes_cbc_init()
 *	Initialize the transform record for ipsec.
 */
static bool eip_tr_ipsec_aes_cbc_init(struct eip_tr *tr, struct eip_tr_info *info, const struct eip_svc_entry *algo)
{
	struct eip_tr_info_ipsec *ipsec = &info->ipsec;
	struct eip_tr_base *crypto = &info->base;
	uint32_t *tr_words = tr->hw_words;
	uint8_t enc;

	/*
	 * We first do common initialization and then algo specific initialization.
	 */
	if (!eip_tr_ipsec_cmn_init(tr, info, algo)) {
		pr_err("%px: Common initialization failed.\n", tr);
		return false;
	}

	switch(crypto->cipher.key_len) {
	case AES_KEYSIZE_128:
		tr_words[0] |= EIP_HW_CTX_ALGO_AES128;
		tr->ctrl_words[0] |= EIP_HW_CTX_ALGO_AES128;
		break;
	case AES_KEYSIZE_192:
		tr_words[0] |= EIP_HW_CTX_ALGO_AES192;
		tr->ctrl_words[0] |= EIP_HW_CTX_ALGO_AES192;
		break;
	case AES_KEYSIZE_256:
		tr_words[0] |= EIP_HW_CTX_ALGO_AES256;
		tr->ctrl_words[0] |= EIP_HW_CTX_ALGO_AES256;
		break;
	default:
		pr_err("%px: Invalid key length(%u)\n", tr, crypto->cipher.key_len);
		return false;
	}

	enc = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_ENC);
	if (enc) {
		tr_words[68] |= EIP_TR_IPSEC_ESP_PROTO(EIP_TR_IPSEC_PROTO_OUT_CBC);
	} else {
		tr_words[68] |= EIP_TR_IPSEC_ESP_PROTO(EIP_TR_IPSEC_PROTO_IN_CBC);
	}

	/*
	 * Flush record memory.
	 * Driver must not make any update in transform records words after this.
	 */
	dmac_clean_range(tr->hw_words, tr->hw_words + EIP_HW_CTX_SIZE_LARGE_WORDS);

	/*
	 * HMAC Inner and Outer Digest Precalculation
	 */
	if (!eip_tr_ahash_key2digest(tr, info, algo)) {
		pr_err("%px: Digest computation failed for algo(%s)\n", tr, algo->name);
		return false;
	}

	return true;
}

/*
 * eip_tr_ipsec_des_cbc_init()
 *	Initialize the transform record for ipsec.
 */
static bool eip_tr_ipsec_des_cbc_init(struct eip_tr *tr, struct eip_tr_info *info, const struct eip_svc_entry *algo)
{
	struct eip_tr_info_ipsec *ipsec = &info->ipsec;
	struct eip_tr_base *crypto = &info->base;
	uint32_t *tr_words = tr->hw_words;
	uint8_t enc;

	/*
	 * We first do common initialization and then algo specific initialization.
	 */
	if (!eip_tr_ipsec_cmn_init(tr, info, algo)) {
		pr_err("%px: Common initialization failed.\n", tr);
		return false;
	}

	switch(crypto->cipher.key_len) {
	case DES3_EDE_KEY_SIZE:
		tr_words[0] |= EIP_HW_CTX_ALGO_3DES;
		tr->ctrl_words[0] |= EIP_HW_CTX_ALGO_3DES;
		break;
	default:
		pr_err("%px: Invalid key length(%u)\n", tr, crypto->cipher.key_len);
		return false;
	}

	enc = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_ENC);
	if (enc) {
		tr_words[68] |= EIP_TR_IPSEC_ESP_PROTO(EIP_TR_IPSEC_PROTO_OUT_CBC);
	} else {
		tr_words[68] |= EIP_TR_IPSEC_ESP_PROTO(EIP_TR_IPSEC_PROTO_IN_CBC);
	}

	/*
	 * Flush record memory.
	 * Driver must not make any update in transform records words after this.
	 */
	dmac_clean_range(tr->hw_words, tr->hw_words + EIP_HW_CTX_SIZE_LARGE_WORDS);

	/*
	 * HMAC Inner and Outer Digest Precalculation
	 */
	if (!eip_tr_ahash_key2digest(tr, info, algo)) {
		pr_err("%px: Digest computation failed for algo(%s)\n", tr, algo->name);
		return false;
	}

	return true;
}

/*
 * eip_tr_ipsec_aes_gcm_init()
 *	Initialize the transform record for ipsec.
 */
static bool eip_tr_ipsec_aes_gcm_init(struct eip_tr *tr, struct eip_tr_info *info, const struct eip_svc_entry *algo)
{
	struct eip_tr_info_ipsec *ipsec = &info->ipsec;
	struct eip_tr_base *base = &info->base;
	uint8_t authkey[AES_BLOCK_SIZE] = {0};
	uint32_t *tr_words = tr->hw_words;
	uint32_t *doffset = NULL;
	uint32_t *key = NULL;
	uint8_t auth_key_idx;
	uint8_t enc;
	int err = 0;

	/*
	 * We first do common initialization and then algo specific initialization.
	 */
	if (!eip_tr_ipsec_cmn_init(tr, info, algo)) {
		pr_err("%px: Common initialization failed.\n", tr);
		return false;
	}

	switch(base->cipher.key_len) {
	case AES_KEYSIZE_128:
		tr_words[0] |= EIP_HW_CTX_ALGO_AES128;
		tr->ctrl_words[0] |= EIP_HW_CTX_ALGO_AES128;
		break;
	case AES_KEYSIZE_192:
		tr_words[0] |= EIP_HW_CTX_ALGO_AES192;
		tr->ctrl_words[0] |= EIP_HW_CTX_ALGO_AES192;
		break;
	case AES_KEYSIZE_256:
		tr_words[0] |= EIP_HW_CTX_ALGO_AES256;
		tr->ctrl_words[0] |= EIP_HW_CTX_ALGO_AES256;
		break;
	default:
		pr_err("%px: Invalid key length(%u)\n", tr, base->cipher.key_len);
		return false;
	}

	enc = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_ENC);
	if (enc) {
		tr_words[68] |= EIP_TR_IPSEC_ESP_PROTO(EIP_TR_IPSEC_PROTO_OUT_GCM);
	} else {
		tr_words[68] |= EIP_TR_IPSEC_ESP_PROTO(EIP_TR_IPSEC_PROTO_IN_GCM);
	}

	/*
	 * Generate and fill ghash key.
	 * Here we are using software for generation of the keys
	 */
	err = eip_tr_genkey(tr, info, (uint8_t *)authkey, algo->auth_block_len);
	if (err) {
		pr_warn("Failed to generate authentication key for GCM with err <%x>\n", err);
		return false;
	}

	auth_key_idx = EIP_HW_CTRL_WORDS + (base->cipher.key_len / sizeof(uint32_t));
	doffset = &tr_words[auth_key_idx];

	key = (uint32_t *)authkey;
	doffset[0] = ntohl(key[0]);
	doffset[1] = ntohl(key[1]);
	doffset[2] = ntohl(key[2]);
	doffset[3] = ntohl(key[3]);

	/*
	 * Flush record memory.
	 * Driver must not make any update in transform records words after this.
	 */
	dmac_clean_range(tr->hw_words, tr->hw_words + EIP_HW_CTX_SIZE_LARGE_WORDS);

	return true;
}

/*
 * eip_tr_ipsec_null_hmac_init()
 *	Initialize the transform record for ipsec.
 */
static bool eip_tr_ipsec_null_hmac_init(struct eip_tr *tr, struct eip_tr_info *info, const struct eip_svc_entry *algo)
{
	struct eip_tr_info_ipsec *ipsec = &info->ipsec;
	uint32_t *tr_words = tr->hw_words;
	uint8_t enc;

	/*
	 * We first do common initialization and then algo specific initialization.
	 */
	if (!eip_tr_ipsec_cmn_init(tr, info, algo)) {
		pr_err("%px: Common initialization failed.\n", tr);
		return false;
	}

	/*
	 * Override common encode decode bit set in common init.
	 */
	enc = !!(ipsec->sa_flags & EIP_TR_IPSEC_FLAG_ENC);
	if (enc) {
		tr_words[0] &= ~EIP_TK_CTRL_OP_ENC_HMAC;
		tr_words[0] |= EIP_TK_CTRL_OP_HMAC_ADD;
		tr->ctrl_words[0] = tr_words[0];
		tr_words[68] |= EIP_TR_IPSEC_ESP_PROTO(EIP_TR_IPSEC_PROTO_OUT_CBC);
	} else {
		tr_words[0] &= ~EIP_TK_CTRL_OP_HMAC_DEC;
		tr_words[0] |= EIP_TK_CTRL_OP_HMAC_CHK;
		tr->ctrl_words[0] = tr_words[0];
		tr_words[68] |= EIP_TR_IPSEC_ESP_PROTO(EIP_TR_IPSEC_PROTO_IN_CBC);
	}

	/*
	 * Flush record memory.
	 * Driver must not make any update in transform records words after this.
	 */
	dmac_clean_range(tr->hw_words, tr->hw_words + EIP_HW_CTX_SIZE_LARGE_WORDS);

	/*
	 * HMAC Inner and Outer Digest Precalculation
	 */
	if (!eip_tr_ahash_key2digest(tr, info, algo)) {
		pr_err("%px: Digest computation failed for algo(%s)\n", tr, algo->name);
		return false;
	}

	return true;
}

/*
 * eip_tr_ipsec_tx_done()
 *	Hardware tx completion callback API for Encapsulation/Decapsulation.
 */
static void eip_tr_ipsec_tx_done(struct eip_tr *tr, struct eip_hw_desc *hw, struct eip_sw_desc *sw)
{
	struct sk_buff *skb = eip_req2skb(sw->req);
	struct eip_ctx *ctx = tr->ctx;

	/*
	 * Free the SKB
	 */
	skb = eip_req2skb(sw->req);

	/*
	 * Free the SKB.
	 */
	consume_skb(skb);
	kmem_cache_free(ctx->sw_cache, sw);

	/*
	 * Dereference: eip_tr_ipsec_enc/dec()
	 */
	eip_tr_deref(tr);
}

/*
 * eip_tr_ipsec_get_svc()
 *	Get algo database for ipsec.
 */
const struct eip_svc_entry *eip_tr_ipsec_get_svc(void)
{
	return ipsec_algo_list;
}

/*
 * eip_tr_ipsec_get_svc_len()
 *	Get algo database length for ipsec.
 */
size_t eip_tr_ipsec_get_svc_len(void)
{
	return ARRAY_SIZE(ipsec_algo_list);
}

/*
 * eip_tr_ipsec_enc()
 *	IPsec Encapsulation.
 */
int eip_tr_ipsec_enc(struct eip_tr *tr, struct sk_buff *skb)
{
	int (*dma_tx)(struct eip_dma *dma, struct eip_sw_desc *sw, struct sk_buff *skb);
	struct eip_ctx *ctx = tr->ctx;
	struct eip_sw_desc *sw;
	struct eip_dma *dma;
	int status = 0;

	dma_tx = skb_is_nonlinear(skb) ? eip_dma_hy_tx_nonlinear_skb : eip_dma_hy_tx_linear_skb;

	/*
	 * Allocate SW descriptor.
	 */
	sw = kmem_cache_alloc(ctx->sw_cache, GFP_NOWAIT | __GFP_NOWARN);
	if (!sw) {
		pr_err("%px: Failed to allocate SW descriptor.\n", tr);
		return -ENOMEM;
	}

	/*
	 * Fill software descriptor.
	 * Dereference: eip_tr_ipsec_tx_done()
	 */
	sw->tr = eip_tr_ref(tr);
	sw->tk = NULL;
	sw->cmd_token_hdr = EIP_HW_TOKEN_HDR_EXTENDED;
	sw->tk_addr = 0;
	sw->tr_addr_type = tr->tr_addr_type;
	sw->tk_words = 0;
	sw->hw_svc = EIP_HW_CMD_HWSERVICE_LIP;
	sw->req = skb;

	/*
	 * Push the fake mac header for inline.
	 * Set handler to trasmit completion as packet will be redirected to inline.
	 */
	skb_push(skb, sizeof(struct ethhdr));
	sw->cmd_token_hdr |= skb->len;
	sw->comp = &eip_tr_ipsec_tx_done;
	sw->err_comp = NULL; /* There is no error in Transmit completion */

	dma = &ctx->dma[smp_processor_id()];

	status = dma_tx(dma, sw, skb);
	if (status < 0) {
		dma->stats.tx_error++;
		goto fail_tx;
	}

	return 0;

fail_tx:
	eip_tr_deref(tr);
	kmem_cache_free(ctx->sw_cache, sw);
	return status;
}
EXPORT_SYMBOL(eip_tr_ipsec_enc);

/*
 * eip_tr_ipsec_dec()
 *	IPsec Decapsulation.
 */
int eip_tr_ipsec_dec(struct eip_tr *tr, struct sk_buff *skb)
{
	int (*dma_tx)(struct eip_dma *dma, struct eip_sw_desc *sw, struct sk_buff *skb);
	struct eip_ctx *ctx = tr->ctx;
	struct eip_sw_desc *sw;
	struct eip_dma *dma;
	int status = 0;

	dma_tx = skb_is_nonlinear(skb) ? eip_dma_hy_tx_nonlinear_skb : eip_dma_hy_tx_linear_skb;

	/*
	 * Allocate SW descriptor.
	 */
	sw = kmem_cache_alloc(ctx->sw_cache, GFP_NOWAIT | __GFP_NOWARN);
	if (!sw) {
		pr_err("%px: Failed to allocate SW descriptor.\n", tr);
		return -ENOMEM;
	}

	/*
	 * Fill software descriptor.
	 * Dereference: eip_tr_ipsec_tx_done
	 */
	sw->tr = eip_tr_ref(tr);
	sw->tk = NULL;
	sw->cmd_token_hdr = EIP_HW_TOKEN_HDR_EXTENDED;
	sw->tk_addr = 0;
	sw->tr_addr_type = tr->tr_addr_type;
	sw->tk_words = 0;
	sw->hw_svc = EIP_HW_CMD_HWSERVICE_LIP;
	sw->req = skb;

	/*
	 * Push the fake mac header for inline.
	 * Set handler to trasmit completion as packet will be redirected to inline.
	 */
	skb_push(skb, sizeof(struct ethhdr));
	sw->cmd_token_hdr |= skb->len;
	sw->comp = &eip_tr_ipsec_tx_done;
	sw->err_comp = NULL; /* There is no error in Transmit completion */

	dma = &ctx->dma[smp_processor_id()];

	status = dma_tx(dma, sw, skb);
	if (status < 0) {
		dma->stats.tx_error++;
		goto fail_tx;
	}

	return 0;

fail_tx:
	eip_tr_deref(tr);
	kmem_cache_free(ctx->sw_cache, sw);
	return status;
}
EXPORT_SYMBOL(eip_tr_ipsec_dec);
