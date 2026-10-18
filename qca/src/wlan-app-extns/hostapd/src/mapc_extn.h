/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef HOSTAPD_MAPC_EXTN_H
#define HOSTAPD_MAPC_EXTN_H

/* Forward declarations needed by both active and no-op paths */
struct hostapd_data;
struct sta_info;
struct wpabuf;

#include "utils/common.h"

#ifdef CONFIG_IEEE80211BN

#define MAPC_MAX_E2E_ENTRIES  8

/**
 * struct mapc_cotdma_e2e_entry - One active Co-TDMA E2E config entry.
 * Parsed from QCA_WLAN_VENDOR_ATTR_MAPC_GET_PARAMS_E2E_ENTRIES nested attr.
 * Reuses qca_wlan_vendor_attr_mapc_cotdma_e2e_config_entry field layout.
 */
struct mapc_cotdma_e2e_entry {
	u8   config_mode;        /* 0 = remove, 1 = add */
	u16  qmid;
	u8   peer_mac[ETH_ALEN];
	bool bsta_mac_valid;
	u8   bsta_mac[ETH_ALEN];
};

/**
 * struct mapc_peer_params_result - Driver-cache MAPC params for one peer.
 * Populated by nl80211_get_mapc_peer_params_extn() and consumed by
 * mapc_get_peer_params() for formatting.
 */
struct mapc_peer_params_result {
	u32 cap_bitmap;
	u32 apid_to, apid_from;
	u32 q2q_to, q2q_from;
	u32 ch_width, ccfs, bss_color, rx_txop, dsb;
	u32 primary_ac, nbr_prio;
	u32 svc_start, svc_interval, svc_end;
	u32 latency_threshold, max_txop, min_txop;
	/* attr 22 — active Co-TDMA E2E config entries; absent = zero entries */
	struct mapc_cotdma_e2e_entry e2e_entries[MAPC_MAX_E2E_ENTRIES];
	u8  e2e_entry_count;
};

/**
 * struct mapc_vendor_peer_ctx - QCA vendor-specific per-peer state.
 */
struct mapc_vendor_peer_ctx {
	u16  vendor_apid;               /* locally allocated Q2Q AID */
	u16  remote_assigned_vendor_apid; /* Q2Q AID assigned by peer */
	bool set_vendor_apid;           /* peer signalled QCA OUI */
};

/**
 * mapc_vendor_append_subelement - Append QCA vendor subelement to MAPC IE.
 * @buf:  wpabuf being built
 * @sta:  peer sta_info
 * @ctx:  frame context (action code)
 */
void mapc_vendor_append_subelement(struct wpabuf *buf,
				   const struct sta_info *sta,
				   u8 action_code);

/**
 * mapc_vendor_parse_subelement - Parse QCA vendor subelement from MAPC IE.
 * @v:       pointer to first byte of subelement body
 * @sub_len: length of subelement body
 * @target:  peer sta_info to write parsed state into
 */
void mapc_vendor_parse_subelement(const u8 *v, size_t sub_len,
				  struct sta_info *target);

/**
 * mapc_vendor_alloc_peer_aid - Allocate QCA vendor AID for peer if applicable.
 * @hapd: BSS context
 * @sta:  peer sta_info
 */
void mapc_vendor_alloc_peer_aid(struct hostapd_data *hapd,
				struct sta_info *sta);

/**
 * mapc_set_vendor_params - Deliver QCA vendor MAPC params for a peer to driver.
 * @hapd: BSS context
 * @sta:  MAPC peer sta_info
 *
 * Returns 0 on success, negative on failure.
 */
int mapc_set_vendor_params(struct hostapd_data *hapd,
			   struct sta_info *sta);

/**
 * mapc_alloc_vendor_aid - Allocate a QCA vendor AID from the AID pool.
 * @hapd: BSS context
 *
 * Returns the allocated AID, or 0 if the pool is full.
 */
u16 mapc_alloc_vendor_aid(struct hostapd_data *hapd);

/**
 * mapc_release_vendor_aid - Release a previously allocated QCA vendor AID.
 * @hapd: BSS context
 * @sta:  peer whose vendor AID should be freed
 */
void mapc_release_vendor_aid(struct hostapd_data *hapd,
			     struct sta_info *sta);

/**
 * mapc_get_peer_params - Retrieve stored MAPC params for a peer.
 * @hapd:       BSS context
 * @peer_addr:  BSSID of the MAPC peer to query
 * @reply:      caller-provided buffer to receive the result
 * @reply_size: size of @reply in bytes
 *
 * Returns number of bytes written on success, or negative on failure.
 */
int mapc_get_peer_params(struct hostapd_data *hapd, const u8 *peer_addr,
			 char *reply, size_t reply_size);

/**
 * nl80211_get_mapc_peer_params_extn - Send vendor GET command and parse reply.
 * Defined in driver_nl80211_extn.c.
 */
int nl80211_get_mapc_peer_params_extn(struct hostapd_data *hapd,
				      const u8 *peer_addr,
				      struct mapc_peer_params_result *res);

/**
 * nl80211_set_mapc_vendor_params_extn - Send QCA vendor MAPC params SET command.
 * Defined in driver_nl80211_extn.c.
 */
int nl80211_set_mapc_vendor_params_extn(struct hostapd_data *hapd,
					const u8 *peer_addr,
					u16 apid_to, u16 apid_from);

#else /* CONFIG_IEEE80211BN */

static inline int mapc_set_vendor_params(struct hostapd_data *hapd,
					       struct sta_info *sta)
{
	return 0;
}

static inline void mapc_vendor_append_subelement(struct wpabuf *buf,
						 const struct sta_info *sta,
						 u8 action_code)
{
}

static inline void mapc_vendor_parse_subelement(const u8 *v, size_t sub_len,
						struct sta_info *target)
{
}

static inline void mapc_vendor_alloc_peer_aid(struct hostapd_data *hapd,
					      struct sta_info *sta)
{
}

static inline u16 mapc_alloc_vendor_aid(struct hostapd_data *hapd)
{
	return 0;
}

static inline void mapc_release_vendor_aid(struct hostapd_data *hapd,
					    struct sta_info *sta)
{
}

static inline int mapc_get_peer_params(struct hostapd_data *hapd,
				       const u8 *peer_addr,
				       char *reply, size_t reply_size)
{
	return -1;
}

#endif /* CONFIG_IEEE80211BN */

#endif /* HOSTAPD_MAPC_EXTN_H */
