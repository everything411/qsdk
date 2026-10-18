/*
 * hostapd / IEEE 1905.1 CMDU transport
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redesigned from the original multi-receiver framework to a single-context
 * model that mirrors the uhr_oui_transport API while preserving:
 *
 *   - IEEE 1905.1 CMDU framing (header, message_id auto-assignment, flags)
 *   - Spec-required duplicate-CMDU detection via a per-context ring buffer
 *   - Flexible BPF filtering: wildcard or per-message-type JEQ chain,
 *     rebuildable at runtime (e.g. to add EasyMesh message types later)
 *   - AES-SIV-256 per-peer encryption, ported from uhr_oui_transport
 *
 * Design:
 *   One PF_PACKET/SOCK_RAW socket per eth_p_1905_ctx, opened on the MLD
 *   bridge interface (hapd->conf->bridge).  A BPF filter is attached at
 *   init time and can be rebuilt via eth_p_1905_rebuild_bpf() whenever the
 *   message-type list changes.
 *
 */

#include "utils/includes.h"
#include <linux/filter.h>

#include "utils/common.h"
#include "l2_packet/l2_packet.h"
#include "crypto/aes.h"
#include "crypto/aes_siv.h"
#include "hostapd.h"
#include "ap_config.h"
#include "eth_p_1905.h"
#include "uhr_iap.h"

/* ------------------------------------------------------------------ */
/* Internal structures                                                  */
/* ------------------------------------------------------------------ */

/**
 * struct eth_p_1905_peer - Per-peer entry with optional AES-SIV-256 key
 *
 * Mirrors uhr_peer_entry from uhr_oui_transport.c.
 */
struct eth_p_1905_peer {
	u8 mac_addr[ETH_ALEN];
	u8 key[32];    /* AES-SIV-256 key; valid only when has_key is true */
	bool has_key;
	struct eth_p_1905_peer *next;
};

/**
 * struct eth_p_1905_dedup_entry - One slot in the duplicate-detection cache
 */
struct eth_p_1905_dedup_entry {
	u8  src_mac[ETH_ALEN];
	u16 message_type;
	u16 message_id;
	int valid;
};

/**
 * struct eth_p_1905_ctx - Per-MLD IEEE 1905.1 transport context
 *
 * @hapd:           owning hostapd instance
 * @l2:             PF_PACKET/SOCK_RAW socket on the bridge interface
 * @own_addr:       own MLD address (copied from hapd->mld->mld_addr at init)
 * @next_msg_id:    TX message-ID counter; 1..0xFFFF, wraps, never 0
 * @msg_types:      BPF filter: message types to accept (0 entries = wildcard)
 * @num_msg_types:  number of valid entries in msg_types[]
 * @peers:          singly-linked list of configured peer APs
 * @peer_count:     number of entries in @peers (enforced against ETH_P_1905_MAX_PEERS)
 * @dedup_cache:    ring buffer for duplicate-CMDU detection
 * @dedup_idx:      next slot to overwrite in dedup_cache[]
 */
struct eth_p_1905_ctx {
	struct hostapd_data *hapd;
	struct l2_packet_data *l2;
	u8 own_addr[ETH_ALEN];
	u16 next_msg_id;

	/* BPF filter state — same semantics as the original multi-receiver
	 * design: num_msg_types == 0 means wildcard (EtherType check only). */
	u16 msg_types[ETH_P_1905_MAX_MSG_TYPES];
	size_t num_msg_types;

	/* Peer list */
	struct eth_p_1905_peer *peers;
	int peer_count;  /* total entries; bounded by ETH_P_1905_MAX_PEERS */

	/* Duplicate-detection ring buffer (spec-required) */
	struct eth_p_1905_dedup_entry dedup_cache[ETH_P_1905_DEDUP_SIZE];
	unsigned int dedup_idx;
};

/* ------------------------------------------------------------------ */
/* Duplicate detection                                                  */
/* ------------------------------------------------------------------ */

static int eth_p_1905_is_duplicate(struct eth_p_1905_ctx *ctx,
				   const u8 *src_mac,
				   u16 message_type, u16 message_id)
{
	unsigned int i;

	for (i = 0; i < ETH_P_1905_DEDUP_SIZE; i++) {
		const struct eth_p_1905_dedup_entry *e = &ctx->dedup_cache[i];

		if (!e->valid)
			continue;
		if (e->message_type == message_type &&
		    e->message_id   == message_id &&
		    os_memcmp(e->src_mac, src_mac, ETH_ALEN) == 0)
			return 1;
	}
	return 0;
}

static void eth_p_1905_record_seen(struct eth_p_1905_ctx *ctx,
				   const u8 *src_mac,
				   u16 message_type, u16 message_id)
{
	struct eth_p_1905_dedup_entry *e =
		&ctx->dedup_cache[ctx->dedup_idx];

	os_memcpy(e->src_mac, src_mac, ETH_ALEN);
	e->message_type = message_type;
	e->message_id   = message_id;
	e->valid        = 1;

	ctx->dedup_idx = (ctx->dedup_idx + 1) % ETH_P_1905_DEDUP_SIZE;
}

/* ------------------------------------------------------------------ */
/* BPF filter construction                                              */
/* ------------------------------------------------------------------ */
/*
 * BPF program layout for N message types (N > 0):
 *
 *   insn 0:   LD  H [12]                  load EtherType
 *   insn 1:   JEQ 0x893A, 0, N+1          if != 0x893A → DROP
 *   insn 2:   LD  H [16]                  load 1905 Message Type
 *   insn 3+i: JEQ types[i], N-i, 0        if match → PASS, else continue
 *   insn 3+N: RET 0                        DROP
 *   insn 4+N: RET ~0                       PASS
 *
 * BPF program for wildcard (N == 0):
 *   insn 0:   LD  H [12]
 *   insn 1:   JEQ 0x893A, 0, 1            if != 0x893A → DROP
 *   insn 2:   RET ~0                       PASS
 *   insn 3:   RET 0                        DROP
 *
 * Jump offsets are relative to the instruction AFTER the jump.
 */
static int eth_p_1905_attach_bpf(struct eth_p_1905_ctx *ctx,
				 const u16 *types, size_t ntypes)
{
	/* Max size: ntypes + 5 instructions */
	struct sock_filter insns[ETH_P_1905_MAX_MSG_TYPES + 5];
	unsigned int n = 0;
	size_t i;

	/* Validate ntypes to prevent u8 cast overflow in BPF jump offsets */
	if (ntypes > ETH_P_1905_MAX_MSG_TYPES) {
		wpa_printf(MSG_ERROR,
			   "1905: Too many types for BPF filter (%zu > %d)",
			   ntypes, ETH_P_1905_MAX_MSG_TYPES);
		return -1;
	}

	if (ntypes == 0) {
		/* Wildcard: EtherType check only */
		insns[n++] = (struct sock_filter)
			BPF_STMT(BPF_LD | BPF_H | BPF_ABS,
				 ETH_P_1905_ETHERTYPE_OFFSET);
		insns[n++] = (struct sock_filter)
			BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,
				 ETH_P_1905_1, 0, 1);
		insns[n++] = (struct sock_filter)
			BPF_STMT(BPF_RET | BPF_K, ~0U);  /* PASS */
		insns[n++] = (struct sock_filter)
			BPF_STMT(BPF_RET | BPF_K, 0);    /* DROP */
	} else {
		/* EtherType + message_type filter */
		insns[n++] = (struct sock_filter)
			BPF_STMT(BPF_LD | BPF_H | BPF_ABS,
				 ETH_P_1905_ETHERTYPE_OFFSET);
		/*
		 * insn 1: if EtherType != 0x893A, jump (ntypes+1) forward
		 * to land on the DROP instruction at position 3+ntypes.
		 * From insn 1, next is insn 2; DROP is at 3+ntypes.
		 * jf = (3+ntypes) - 2 = ntypes+1.
		 */
		insns[n++] = (struct sock_filter)
			BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,
				 ETH_P_1905_1, 0, (u8)(ntypes + 1));
		insns[n++] = (struct sock_filter)
			BPF_STMT(BPF_LD | BPF_H | BPF_ABS,
				 ETH_P_1905_MSGTYPE_OFFSET);
		/*
		 * insns 3..3+ntypes-1: one JEQ per message type.
		 * For insn at position 3+i:
		 *   PASS is at position 4+ntypes.
		 *   jt = (4+ntypes) - (3+i+1) = ntypes - i.
		 */
		for (i = 0; i < ntypes; i++) {
			insns[n++] = (struct sock_filter)
				BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,
					 types[i],
					 (u8)(ntypes - i), 0);
		}
		insns[n++] = (struct sock_filter)
			BPF_STMT(BPF_RET | BPF_K, 0);    /* DROP */
		insns[n++] = (struct sock_filter)
			BPF_STMT(BPF_RET | BPF_K, ~0U);  /* PASS */
	}

	return l2_packet_set_bpf_filter(ctx->l2, (const void *) insns,
					(unsigned short) n);
}

/*
 * eth_p_1905_rebuild_bpf - Rebuild and re-attach the BPF filter from the
 * message-type list stored in the context.
 *
 * num_msg_types == 0 installs a wildcard (EtherType-only) filter.
 * Call this whenever ctx->msg_types or ctx->num_msg_types is updated.
 */
int eth_p_1905_rebuild_bpf(struct eth_p_1905_ctx *ctx)
{
	int ret;

	if (!ctx) {
		wpa_printf(MSG_ERROR,
			   "1905: rebuild_bpf called with NULL ctx");
		return -1;
	}

	if (ctx->num_msg_types == 0)
		ret = eth_p_1905_attach_bpf(ctx, NULL, 0);
	else
		ret = eth_p_1905_attach_bpf(ctx, ctx->msg_types,
					    ctx->num_msg_types);

	if (ret < 0)
		wpa_printf(MSG_ERROR,
			   "1905: Failed to rebuild BPF filter on %s",
			   ctx->hapd->conf->bridge);
	return ret;
}

/* ------------------------------------------------------------------ */
/* Peer management                                                      */
/* ------------------------------------------------------------------ */

/**
 * eth_p_1905_get_peer - Internal: look up a peer by MAC address (exact match only).
 */
static struct eth_p_1905_peer *
eth_p_1905_get_peer(struct eth_p_1905_ctx *ctx, const u8 *mac_addr)
{
	struct eth_p_1905_peer *peer;

	if (!ctx || !mac_addr)
		return NULL;

	for (peer = ctx->peers; peer; peer = peer->next) {
		if (os_memcmp(peer->mac_addr, mac_addr, ETH_ALEN) == 0)
			return peer;
	}
	return NULL;
}

/**
 * eth_p_1905_get_wildcard_peer - Internal: find the wildcard peer entry.
 *
 * A wildcard entry has mac_addr == 00:00:00:00:00:00 and supplies the
 * shared key for peers not yet individually registered.  On first contact
 * the wildcard is cloned into a concrete entry keyed by the peer's MLD addr
 * via eth_p_1905_clone_peer().
 */
static struct eth_p_1905_peer *
eth_p_1905_get_wildcard_peer(struct eth_p_1905_ctx *ctx)
{
	struct eth_p_1905_peer *peer;

	if (!ctx)
		return NULL;

	for (peer = ctx->peers; peer; peer = peer->next) {
		if (is_zero_ether_addr(peer->mac_addr))
			return peer;
	}
	return NULL;
}

/**
 * eth_p_1905_peer_exists - Check whether a peer is reachable.
 *
 * Returns 1 if an exact-match entry exists OR a wildcard entry
 * (mac_addr == 00:00:00:00:00:00) is present, 0 otherwise.
 */
int eth_p_1905_peer_exists(struct eth_p_1905_ctx *ctx, const u8 *mac_addr)
{
	return eth_p_1905_get_peer(ctx, mac_addr) != NULL ||
	       eth_p_1905_get_wildcard_peer(ctx) != NULL;
}

/**
 * eth_p_1905_add_peer - Add a peer AP to the configured list.
 *
 * Uses an exact-match duplicate check so that adding a concrete entry while
 * a wildcard is already present is never incorrectly suppressed.
 * Enforces ETH_P_1905_MAX_PEERS across the total peer list.
 *
 * Returns 0 on success (including "already exists"), -1 on error.
 */
int eth_p_1905_add_peer(struct eth_p_1905_ctx *ctx,
			const u8 *mac_addr,
			const u8 *key,
			bool has_key)
{
	struct eth_p_1905_peer *peer;

	if (!ctx || !mac_addr)
		return -1;

	/* Exact-match check only — do not suppress concrete entries because
	 * a wildcard is present. */
	if (eth_p_1905_get_peer(ctx, mac_addr)) {
		wpa_printf(MSG_DEBUG,
			   "1905: Peer " MACSTR " already exists",
			   MAC2STR(mac_addr));
		return 0;
	}

	if (ctx->peer_count >= ETH_P_1905_MAX_PEERS) {
		wpa_printf(MSG_WARNING,
			   "1905: Peer list full (%d), not adding " MACSTR,
			   ETH_P_1905_MAX_PEERS, MAC2STR(mac_addr));
		return -1;
	}

	peer = os_zalloc(sizeof(*peer));
	if (!peer) {
		wpa_printf(MSG_ERROR,
			   "1905: Failed to allocate peer entry");
		return -1;
	}

	os_memcpy(peer->mac_addr, mac_addr, ETH_ALEN);
	if (has_key && key) {
		os_memcpy(peer->key, key, sizeof(peer->key));
		peer->has_key = true;
	}
	peer->next = ctx->peers;
	ctx->peers = peer;
	ctx->peer_count++;

	wpa_printf(MSG_DEBUG, "1905: Added peer " MACSTR " (%s)",
		   MAC2STR(mac_addr),
		   peer->has_key ? "encrypted" : "plain");
	return 0;
}

/**
 * eth_p_1905_clone_peer - Register new_mac with the key from existing_mac.
 *
 * Called from uhr_iap_rx() to register the MLD address carried in the IAP
 * frame body.  Falls back to the wildcard entry when existing_mac has no
 * exact entry (first-contact via wildcard path).
 *
 * Returns 0 on success (including "already exists"), -1 on error.
 */
int eth_p_1905_clone_peer(struct eth_p_1905_ctx *ctx,
			  const u8 *existing_mac,
			  const u8 *new_mac)
{
	struct eth_p_1905_peer *existing;

	if (!ctx || !existing_mac || !new_mac)
		return -1;

	/* No-op if new_mac already has a concrete entry */
	if (eth_p_1905_get_peer(ctx, new_mac))
		return 0;

	/* Try exact match for existing_mac first */
	existing = eth_p_1905_get_peer(ctx, existing_mac);
	if (!existing) {
		/* Fall back to wildcard (first-contact path) */
		existing = eth_p_1905_get_wildcard_peer(ctx);
		if (existing)
			wpa_printf(MSG_INFO,
				   "1905: Adding " MACSTR
				   " to peer list (promoted via wildcard key)",
				   MAC2STR(new_mac));
	}
	if (!existing) {
		wpa_printf(MSG_WARNING,
			   "1905: No key source for MLD addr " MACSTR
			   " — no exact peer and no wildcard",
			   MAC2STR(new_mac));
		return -1;
	}

	return eth_p_1905_add_peer(ctx, new_mac,
				   existing->key, existing->has_key);
}

/* ------------------------------------------------------------------ */
/* RX path                                                              */
/* ------------------------------------------------------------------ */

/*
 * eth_p_1905_rx - l2_packet RX callback.
 *
 * Called by the eloop with the full raw frame (l2_hdr=1):
 *   buf[0..13]  = Ethernet header
 *   buf[14..21] = IEEE 1905.1 CMDU header (struct ieee1905_hdr, 8 bytes)
 *   buf[22..]   = CMDU payload (optionally AES-SIV-256 encrypted)
 *
 * Processing:
 *   1. Validate minimum frame length.
 *   2. Parse message_type and message_id from the 1905 header.
 *   3. Duplicate-detection check; drop silently if duplicate.
 *   4. Record the CMDU in the dedup ring buffer.
 *   5. If the source peer has a key, AES-SIV-256 decrypt the payload.
 *      AD = [src_addr (6 bytes) || message_type as BE16 (2 bytes)].
 *   6. Dispatch to uhr_iap_rx().
 */
static void eth_p_1905_rx(void *priv, const u8 *src_addr,
			  const u8 *buf, size_t len)
{
	struct eth_p_1905_ctx *ctx = priv;
	const struct l2_ethhdr *ethhdr;
	const struct ieee1905_hdr *hdr;
	struct eth_p_1905_peer *peer;
	u16 msg_type, msg_id;
	const u8 *payload;
	size_t payload_len;
	u8 *plain = NULL;

	if (!buf) {
		wpa_printf(MSG_ERROR, "1905: RX called with NULL buffer");
		return;
	}

	/* Minimum: 14-byte eth header + 8-byte 1905 header */
	if (len < sizeof(*ethhdr) + sizeof(*hdr)) {
		wpa_printf(MSG_DEBUG,
			   "1905: RX frame too short (%zu bytes)", len);
		return;
	}

	ethhdr = (const struct l2_ethhdr *) buf;
	hdr    = (const struct ieee1905_hdr *) (buf + sizeof(*ethhdr));

	msg_type = WPA_GET_BE16((const u8 *) &hdr->message_type);
	msg_id   = WPA_GET_BE16((const u8 *) &hdr->message_id);

	/* IEEE 1905.1 spec: silently discard duplicate CMDUs */
	if (eth_p_1905_is_duplicate(ctx, ethhdr->h_source,
				    msg_type, msg_id)) {
		wpa_printf(MSG_DEBUG,
			   "1905: Drop duplicate CMDU type=0x%04x mid=%u "
			   "from " MACSTR,
			   msg_type, msg_id, MAC2STR(ethhdr->h_source));
		return;
	}
	eth_p_1905_record_seen(ctx, ethhdr->h_source, msg_type, msg_id);

	payload     = buf + sizeof(*ethhdr) + sizeof(*hdr);
	payload_len = len - sizeof(*ethhdr) - sizeof(*hdr);

	wpa_printf(MSG_DEBUG,
		   "1905: RX type=0x%04x mid=%u from " MACSTR " len=%zu",
		   msg_type, msg_id, MAC2STR(ethhdr->h_source), payload_len);

	peer = eth_p_1905_get_peer(ctx, ethhdr->h_source);
	if (is_broadcast_ether_addr(ethhdr->h_dest)) {
		/* Broadcast-destination CMDUs use the wildcard/shared key path. */
		struct eth_p_1905_peer *wildcard =
			eth_p_1905_get_wildcard_peer(ctx);

		if (wildcard && wildcard->has_key) {
			wpa_printf(MSG_INFO,
				   "1905: Broadcast frame from " MACSTR
				   " - decrypting with wildcard key",
				   MAC2STR(ethhdr->h_source));
			peer = wildcard;
		}
	}
	if (!peer) {
		/* No exact match — try wildcard (all-zero MAC).  The wildcard
		 * supplies the shared key; the sender's MLD address is promoted
		 * to a concrete entry by eth_p_1905_clone_peer() once the IAP
		 * frame body is parsed by uhr_iap_rx(). */
		struct eth_p_1905_peer *wildcard =
			eth_p_1905_get_wildcard_peer(ctx);

		if (wildcard) {
			wpa_printf(MSG_INFO,
				   "1905: No exact peer for " MACSTR
				   " — decrypting with wildcard key",
				   MAC2STR(ethhdr->h_source));
			peer = wildcard;
		}
	}
	if (peer && peer->has_key) {
		/*
		 * AES-SIV-256 decrypt.
		 *
		 * AD = [src_addr (6 bytes) || message_type as BE16 (2 bytes)]
		 *
		 * This mirrors the uhr_oui_transport AD = [src_addr, oui_suffix]
		 * but uses the 2-byte 1905 message_type instead of the 1-byte
		 * OUI suffix, which is the natural 1905 equivalent.
		 */
		u8 msg_type_bytes[2];
		const u8 *ad[2];
		size_t ad_len[2];
		size_t plain_len;

		WPA_PUT_BE16(msg_type_bytes, msg_type);
		ad[0]     = ethhdr->h_source;
		ad_len[0] = ETH_ALEN;
		ad[1]     = msg_type_bytes;
		ad_len[1] = sizeof(msg_type_bytes);

		if (payload_len < AES_BLOCK_SIZE) {
			wpa_printf(MSG_DEBUG,
				   "1905: Encrypted frame too short (%zu)",
				   payload_len);
			return;
		}

		plain_len = payload_len - AES_BLOCK_SIZE;

		/*
		 * Allocate at least 1 byte so os_malloc() never returns NULL
		 * for a zero-length plaintext (edge case: SIV tag only).
		 */
		plain = os_malloc(plain_len + 1);
		if (!plain)
			return;

		if (aes_siv_decrypt(peer->key, sizeof(peer->key),
				    payload, payload_len,
				    2, ad, ad_len,
				    plain) < 0) {
			wpa_printf(MSG_DEBUG,
				   "1905: AES-SIV decrypt failed from " MACSTR,
				   MAC2STR(ethhdr->h_source));
			os_free(plain);
			return;
		}

		if (is_zero_ether_addr(peer->mac_addr)) {
			wpa_printf(MSG_INFO,
				   "1905: Wildcard decryption succeeded for "
				   MACSTR, MAC2STR(ethhdr->h_source));
			if (!is_broadcast_ether_addr(ethhdr->h_dest))
				wpa_printf(MSG_INFO,
					   "1905: MLD addr "MACSTR " will be promoted",
					   MAC2STR(ethhdr->h_source));
		}

		uhr_iap_rx(ctx->hapd, ethhdr->h_source, ethhdr->h_dest,
			   msg_type, plain, plain_len);
		os_free(plain);
	} else {
		uhr_iap_rx(ctx->hapd, ethhdr->h_source, ethhdr->h_dest,
			   msg_type, payload, payload_len);
	}
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/**
 * eth_p_1905_init - Initialise a 1905 CMDU transport context.
 *
 * Opens a PF_PACKET/SOCK_RAW socket on hapd->conf->bridge, attaches a BPF
 * filter for the requested message types, and returns an opaque context.
 *
 * @hapd:          hostapd instance (bridge name taken from hapd->conf->bridge)
 * @msg_types:     array of 1905 message_type values to accept via BPF;
 *                 NULL or num_msg_types == 0 installs a wildcard filter.
 * @num_msg_types: length of msg_types[] (max ETH_P_1905_MAX_MSG_TYPES)
 *
 * Returns the context on success, NULL on failure.
 */
struct eth_p_1905_ctx *
eth_p_1905_init(struct hostapd_data *hapd,
		const u16 *msg_types,
		size_t num_msg_types)
{
	struct eth_p_1905_ctx *ctx;
	size_t i;

	if (!hapd || !hapd->conf || !hapd->conf->bridge[0]) {
		wpa_printf(MSG_ERROR,
			   "1905: init called with invalid hapd or no bridge configured");
		return NULL;
	}

	if (num_msg_types > ETH_P_1905_MAX_MSG_TYPES) {
		wpa_printf(MSG_ERROR,
			   "1905: Too many message types (%zu > %d)",
			   num_msg_types, ETH_P_1905_MAX_MSG_TYPES);
		return NULL;
	}

	wpa_printf(MSG_DEBUG,
		   "1905: Initializing transport on bridge %s",
		   hapd->conf->bridge);

	ctx = os_zalloc(sizeof(*ctx));
	if (!ctx) {
		wpa_printf(MSG_ERROR,
			   "1905: Failed to allocate context");
		return NULL;
	}

	ctx->hapd        = hapd;
	ctx->next_msg_id = 1;
	/* Use the MLD address as the own address (consistent with
	 * uhr_oui_init() after the wildcard-peer patch) */
	os_memcpy(ctx->own_addr, hapd->mld->mld_addr, ETH_ALEN);

	/* Store the requested message-type filter */
	ctx->num_msg_types = num_msg_types;
	for (i = 0; i < num_msg_types; i++)
		ctx->msg_types[i] = msg_types[i];

	/* Open L2 socket on the bridge interface (same as uhr_oui_init) */
	ctx->l2 = l2_packet_init(hapd->conf->bridge, NULL, ETH_P_1905_1,
				 eth_p_1905_rx, ctx, 1);
	if (!ctx->l2) {
		wpa_printf(MSG_ERROR,
			   "1905: Failed to open l2_packet on %s",
			   hapd->conf->bridge);
		os_free(ctx);
		return NULL;
	}

	/* Attach the initial BPF filter */
	if (eth_p_1905_rebuild_bpf(ctx) < 0) {
		wpa_printf(MSG_ERROR,
			   "1905: Failed to attach BPF filter on %s",
			   hapd->conf->bridge);
		l2_packet_deinit(ctx->l2);
		os_free(ctx);
		return NULL;
	}

	wpa_printf(MSG_INFO,
		   "1905: Initialized on %s (MAC " MACSTR
		   ", %zu msg type(s) in BPF filter)",
		   hapd->conf->bridge, MAC2STR(ctx->own_addr),
		   num_msg_types);

	return ctx;
}

/**
 * eth_p_1905_deinit - Tear down a 1905 transport context.
 *
 * Closes the L2 socket, frees the peer list, and releases all memory.
 */
void eth_p_1905_deinit(struct eth_p_1905_ctx *ctx)
{
	struct eth_p_1905_peer *peer, *next;

	if (!ctx)
		return;

	wpa_printf(MSG_DEBUG, "1905: Deinitializing transport");

	if (ctx->l2) {
		l2_packet_deinit(ctx->l2);
		ctx->l2 = NULL;
	}

	/* Free peer list */
	peer = ctx->peers;
	while (peer) {
		next = peer->next;
		os_free(peer);
		peer = next;
	}
	ctx->peers = NULL;

	os_free(ctx);
}

/**
 * eth_p_1905_send - Transmit a 1905 CMDU.
 *
 * If the destination peer has a key, the payload is AES-SIV-256 encrypted
 * before transmission.  The 1905 CMDU header is always prepended and a
 * message_id is auto-assigned (1..0xFFFF, wraps, never 0).
 *
 * AD for AES-SIV = [src_addr (6 bytes) || message_type as BE16 (2 bytes)]
 *
 * Returns the message_id used on success (1..65535), -1 on failure.
 */
int eth_p_1905_send(struct eth_p_1905_ctx *ctx,
		    const u8 *dst_addr,
		    const u8 *src_addr,
		    u16 message_type,
		    const u8 *data,
		    size_t data_len)
{
	struct eth_p_1905_peer *peer;
	u8 *payload = NULL;
	size_t payload_len;
	u8 *packet, *p;
	size_t packet_len;
	int ret;
	u16 used_msg_id;
	struct l2_ethhdr *ethhdr;
	struct ieee1905_hdr *hdr;
	bool encrypted = false;

	if (!ctx || !dst_addr || !src_addr) {
		wpa_printf(MSG_ERROR,
			   "1905: send called with NULL parameter");
		return -1;
	}

	peer = eth_p_1905_get_peer(ctx, dst_addr);
	if (!peer) {
		/* No exact match — fall back to wildcard key */
		peer = eth_p_1905_get_wildcard_peer(ctx);
		if (peer)
			wpa_printf(MSG_DEBUG,
				   "1905: No exact peer for " MACSTR
				   " — sending with wildcard key",
				   MAC2STR(dst_addr));
	}

	if (peer && peer->has_key) {
		/*
		 * AES-SIV-256 encrypt.
		 * AD = [src_addr (6 bytes) || message_type as BE16 (2 bytes)]
		 */
		u8 msg_type_bytes[2];
		const u8 *ad[2];
		size_t ad_len[2];

		WPA_PUT_BE16(msg_type_bytes, message_type);
		ad[0]     = src_addr;
		ad_len[0] = ETH_ALEN;
		ad[1]     = msg_type_bytes;
		ad_len[1] = sizeof(msg_type_bytes);

		payload_len = data_len + AES_BLOCK_SIZE;
		payload = os_malloc(payload_len);
		if (!payload)
			return -1;

		if (aes_siv_encrypt(peer->key, sizeof(peer->key),
				    data, data_len,
				    2, ad, ad_len,
				    payload) < 0) {
			wpa_printf(MSG_ERROR,
				   "1905: AES-SIV encrypt failed for " MACSTR,
				   MAC2STR(dst_addr));
			os_free(payload);
			return -1;
		}
		encrypted = true;
	} else {
		/* No encryption — send plaintext */
		payload     = (u8 *) data;
		payload_len = data_len;
	}

	/* Auto-assign message_id (1..0xFFFF, wraps, never 0) */
	used_msg_id = ctx->next_msg_id;
	ctx->next_msg_id++;
	if (ctx->next_msg_id == 0)
		ctx->next_msg_id = 1;

	/* Build the full frame: eth header + 1905 header + payload */
	packet_len = sizeof(*ethhdr) + sizeof(*hdr) + payload_len;
	packet = os_zalloc(packet_len);
	if (!packet) {
		if (encrypted)
			os_free(payload);
		return -1;
	}
	p = packet;

	/* Ethernet header */
	ethhdr = (struct l2_ethhdr *) p;
	os_memcpy(ethhdr->h_dest,   dst_addr, ETH_ALEN);
	os_memcpy(ethhdr->h_source, src_addr, ETH_ALEN);
	ethhdr->h_proto = host_to_be16(ETH_P_1905_1);
	p += sizeof(*ethhdr);

	/* IEEE 1905.1 CMDU header */
	hdr = (struct ieee1905_hdr *) p;
	hdr->message_version = 0x00;
	hdr->reserved        = 0x00;
	WPA_PUT_BE16((u8 *) &hdr->message_type, message_type);
	WPA_PUT_BE16((u8 *) &hdr->message_id,   used_msg_id);
	hdr->fragment_id     = 0x00;
	/* Always set LAST_FRAGMENT for single-fragment CMDUs */
	hdr->flags           = IEEE1905_FLAG_LAST_FRAGMENT;
	p += sizeof(*hdr);

	/* Payload (plaintext or ciphertext) */
	os_memcpy(p, payload, payload_len);

	ret = l2_packet_send(ctx->l2, NULL, 0, packet, packet_len);

	os_free(packet);
	if (encrypted)
		os_free(payload);

	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "1905: TX failed type=0x%04x to " MACSTR,
			   message_type, MAC2STR(dst_addr));
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "1905: TX type=0x%04x mid=%u to " MACSTR " len=%zu",
		   message_type, used_msg_id, MAC2STR(dst_addr), payload_len);

	return ret;
}

/**
 * eth_p_1905_load_partners - Load configured SMD partner APs into the context.
 *
 * Reads hapd->conf->smd_partners and calls eth_p_1905_add_peer() for each
 * entry.  Mirrors uhr_load_partners() from uhr_oui_transport.c.
 *
 * @ctx:  1905 transport context returned by eth_p_1905_init().
 * @hapd: hostapd instance whose conf->smd_partners list is read.
 *
 * Returns the number of partners loaded, or -1 on error.
 */
int eth_p_1905_load_partners(struct eth_p_1905_ctx *ctx,
			     struct hostapd_data *hapd)
{
	struct smd_partner_entry *partner;
	int count = 0;

	if (!ctx) {
		wpa_printf(MSG_ERROR,
			   "1905: load_partners called with NULL ctx");
		return -1;
	}

	if (!hapd || !hapd->conf) {
		wpa_printf(MSG_ERROR,
			   "1905: load_partners called with NULL hapd");
		return -1;
	}

	if (!hapd->conf->smd_partners) {
		wpa_printf(MSG_WARNING, "1905: No partners configured");
		return 0;
	}

	for (partner = hapd->conf->smd_partners;
	     partner;
	     partner = partner->next) {
		if (eth_p_1905_add_peer(ctx,
					partner->mac_addr,
					partner->key,
					partner->has_key) < 0) {
			wpa_printf(MSG_ERROR,
				   "1905: Failed to add partner " MACSTR,
				   MAC2STR(partner->mac_addr));
			continue;
		}
		count++;
	}

	wpa_printf(MSG_INFO, "1905: Loaded %d partner(s)", count);
	return count;
}
