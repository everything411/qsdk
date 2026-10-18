/*
 * hostapd / IEEE 1905.1 CMDU transport — public API
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ETH_P_1905_H
#define ETH_P_1905_H

/* IEEE 1905.1 EtherType */
#ifndef ETH_P_1905_1
#define ETH_P_1905_1                  0x893AU
#endif

/* Raw-frame byte offsets (SOCK_RAW, l2_hdr=1) */
#define ETH_P_1905_ETHERTYPE_OFFSET   12   /* halfword: EtherType */
#define ETH_P_1905_MSGTYPE_OFFSET     16   /* halfword: 1905 Message Type
					    * = 14 (eth hdr) + 1 (ver) + 1 (rsvd) */

/* IEEE 1905.1 CMDU header — immediately after the 14-byte Ethernet header */
struct ieee1905_hdr {
	u8   message_version;  /* 0x00 for IEEE 1905.1 */
	u8   reserved;         /* 0x00 */
	be16 message_type;     /* CMDU type, big-endian */
	be16 message_id;       /* TX tag / response correlation, big-endian */
	u8   fragment_id;      /* 0x00 for single-fragment CMDUs */
	u8   flags;            /* bit0=last_fragment, bit7=relay_indicator */
} STRUCT_PACKED;

#define IEEE1905_FLAG_LAST_FRAGMENT   BIT(0)
#define IEEE1905_FLAG_RELAY           BIT(7)

/* Maximum distinct message types that can be registered in a single context */
#define ETH_P_1905_MAX_MSG_TYPES      16

/* Duplicate-detection ring buffer depth */
#define ETH_P_1905_DEDUP_SIZE         32

/* Maximum number of peer entries (exact + wildcard combined) */
#define ETH_P_1905_MAX_PEERS          16

/* SMD IAP message types carried in the 1905 message_type field.
 * These replace the ETH_P_OUI suffixes (0x06 / 0x07) used by
 * uhr_oui_transport.  Additional EasyMesh message types can be
 * registered later via eth_p_1905_rebuild_bpf(). */
#define ETH_P_1905_IAP_MSG_REQUEST    0x0006
#define ETH_P_1905_IAP_MSG_RESPONSE   0x0007

/* SMD ST Preparation/Execution 1905 message types (Wi-Fi 8 / SMD feature).
 * TODO: Replace placeholder values with actual 1905 spec message type IDs
 * once the spec assigns them. */
#define ETH_P_1905_SMD_ST_PREP_REQ_MSG      0xFF01  /* placeholder */
#define ETH_P_1905_SMD_ST_PREP_REP_MSG      0xFF02  /* placeholder */
#define ETH_P_1905_SMD_ST_EXEC_REQ_MSG      0xFF03  /* placeholder */
#define ETH_P_1905_SMD_ST_EXEC_REP_MSG      0xFF04  /* placeholder */
#define ETH_P_1905_SMD_ST_PREP_CTX_MSG      0xFF05  /* placeholder */
#define ETH_P_1905_SMD_ST_ROAM_CLEANUP_MSG       0xFF06  /* placeholder */
#define ETH_P_1905_SMD_ST_CTX_REQ_MSG            0xFF07  /* placeholder */
#define ETH_P_1905_SMD_ST_CTX_REP_MSG            0xFF0A  /* placeholder */
#define ETH_P_1905_SMD_ST_EXEC_VIA_TGT_DONE_MSG  0xFF0B  /* placeholder */

/* SMD Neighbor Update/Fetch 1905 message types. */
#define ETH_P_1905_SMD_NEIGHBOR_UPDATE_MSG   0xFF08
#define ETH_P_1905_SMD_NEIGHBOR_FETCH_MSG    0xFF09

struct eth_p_1905_ctx;
struct hostapd_data;

/*
 * eth_p_1905_init - Initialise a 1905 CMDU transport context.
 *
 * @hapd:           hostapd instance; the bridge interface name is taken from
 *                  hapd->conf->bridge (same as uhr_oui_init).
 * @msg_types:      array of 1905 message_type values to receive via BPF;
 *                  NULL or num_msg_types == 0 means wildcard (receive all
 *                  ETH_P_1905_1 frames).
 * @num_msg_types:  length of msg_types array (max ETH_P_1905_MAX_MSG_TYPES).
 *
 * Opens a PF_PACKET/SOCK_RAW socket on the bridge interface, attaches a BPF
 * filter for the requested message types, and returns an opaque context.
 *
 * Returns the context on success, NULL on failure.
 */
struct eth_p_1905_ctx *
eth_p_1905_init(struct hostapd_data *hapd,
		const u16 *msg_types,
		size_t num_msg_types);

/*
 * eth_p_1905_deinit - Tear down a 1905 transport context.
 *
 * Closes the L2 socket, frees the peer list, and releases all memory.
 */
void eth_p_1905_deinit(struct eth_p_1905_ctx *ctx);

/*
 * eth_p_1905_rebuild_bpf - Rebuild and re-attach the BPF filter.
 *
 * Call this after modifying the message-type list stored in the context
 * (e.g. when adding EasyMesh message types at runtime).  The filter is
 * rebuilt from ctx->msg_types / ctx->num_msg_types; num_msg_types == 0
 * installs a wildcard (EtherType-only) filter.
 *
 * Returns 0 on success, -1 on failure.
 */
int eth_p_1905_rebuild_bpf(struct eth_p_1905_ctx *ctx);

/*
 * eth_p_1905_peer_exists - Check whether a peer is reachable.
 *
 * Returns 1 if an exact-match entry exists OR a wildcard entry
 * (mac_addr == 00:00:00:00:00:00) is present, 0 otherwise.
 */
int eth_p_1905_peer_exists(struct eth_p_1905_ctx *ctx, const u8 *mac_addr);

/*
 * eth_p_1905_add_peer - Add a peer AP to the configured list.
 *
 * @mac_addr:  peer MLD address.  Use 00:00:00:00:00:00 to register a
 *             wildcard entry that supplies the shared key for peers not
 *             yet individually configured.
 * @key:       32-byte AES-SIV-256 key, or NULL when has_key is false.
 * @has_key:   true if the peer uses encryption.
 *
 * The duplicate check uses an exact-match lookup so that adding a concrete
 * entry while a wildcard is already present is never incorrectly suppressed.
 * The total peer list size (exact + wildcard) is bounded by
 * ETH_P_1905_MAX_PEERS.
 *
 * Returns 0 on success (including "already exists"), -1 on error.
 */
int eth_p_1905_add_peer(struct eth_p_1905_ctx *ctx,
			const u8 *mac_addr,
			const u8 *key,
			bool has_key);

/*
 * eth_p_1905_clone_peer - Register new_mac with the key from existing_mac.
 *
 * Called from uhr_iap_rx() to register the MLD address carried in the IAP
 * frame body.  Falls back to the wildcard entry when existing_mac has no
 * exact entry (first-contact via wildcard path).
 *
 * @ctx:          1905 transport context.
 * @existing_mac: Ethernet source address of the received frame.
 * @new_mac:      MLD address from the IAP frame body to register as a
 *                concrete peer entry.
 *
 * Returns 0 on success (including "already exists"), -1 on error.
 */
int eth_p_1905_clone_peer(struct eth_p_1905_ctx *ctx,
			  const u8 *existing_mac,
			  const u8 *new_mac);

/*
 * eth_p_1905_send - Transmit a 1905 CMDU.
 *
 * Optionally AES-SIV-256 encrypts the payload if the destination peer has a
 * key configured (AD = [src_addr || message_type-as-BE16]).  Prepends the
 * standard 1905 CMDU header and auto-assigns a message_id (1..0xFFFF,
 * wraps, never uses 0).
 *
 * Returns the message_id used on success (1..65535), -1 on failure.
 */
int eth_p_1905_send(struct eth_p_1905_ctx *ctx,
		    const u8 *dst_addr,
		    const u8 *src_addr,
		    u16 message_type,
		    const u8 *data,
		    size_t data_len);

/*
 * eth_p_1905_load_partners - Load configured SMD partner APs.
 *
 * Reads hapd->conf->smd_partners and calls eth_p_1905_add_peer() for each
 * entry.  Mirrors uhr_load_partners().
 *
 * @ctx:  1905 transport context returned by eth_p_1905_init().
 * @hapd: hostapd instance whose conf->smd_partners list is read.
 *
 * Returns the number of partners loaded, or -1 on error.
 */
int eth_p_1905_load_partners(struct eth_p_1905_ctx *ctx,
			     struct hostapd_data *hapd);

#endif /* ETH_P_1905_H */

