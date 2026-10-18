/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __MESH_UTIL_H__
#define __MESH_UTIL_H__
#include "if_meta_hdr.h"
#include "../vendor.h"
#include <linux/skbuff.h>

extern unsigned int mmeshsim;

struct ath12k_dp_tx_msdu_info_s {
	int exception_fw;
	int no_enc_frame;
};

#define HTT_NON_QOS_TID     16
#define HTT_TX_EXT_TID_NON_QOS_MCAST_BCAST HTT_NON_QOS_TID
#define IEEE80211_RX_FIRST_MSDU 0x1

/**
 * enum ath12k_extn_mesh_conf_cmd_type: Mesh peer config command types
 *
 * @ATH12K_EXTN_MESH_CONF_ADD_LOCAL_PEER: Add local mesh peer command
 * @ATH12K_EXTN_MESH_CONF_ALLOW_DATA: Authorize local mesh peer command
 * @ATH12K_EXTN_MESH_CONF_DEL_LOCAL_PEER: Delete local mesh peer command
 * @ATH12K_EXTN_MESH_CONF_SET_PEER_TIMEOUT_CNT: Configure Peer timeout count
 * @ATH12K_EXTN_MESH_CONF_PEER_TIMEOUT_EN: Configure mesh peer cleanup timer
 * @ATH12K_EXTN_MESH_CONF_PEER_DUMP: Dump mesh peer information
 */
enum ath12k_extn_mesh_conf_cmd_type {
	ATH12K_EXTN_MESH_CONF_ADD_LOCAL_PEER = 1,
	ATH12K_EXTN_MESH_CONF_ALLOW_DATA = 2,
	ATH12K_EXTN_MESH_CONF_DEL_LOCAL_PEER = 3,
	ATH12K_EXTN_MESH_CONF_SET_PEER_TIMEOUT_CNT = 4,
	ATH12K_EXTN_MESH_CONF_PEER_TIMEOUT_EN = 5,
	ATH12K_EXTN_MESH_CONF_PEER_DUMP = 6,
};

/* Per-peer binary payload for the mesh peer_dump vendor reply. */
struct ieee80211_mesh_peer_dump_entry {
	u8   mac[ETH_ALEN];
	u16  aid;
	u8   is_mesh_peer;
	u32  phymode;               /* enum wmi_phy_mode value */
	u32  nss;
	u8   beacon_miss_count;
	u64  last_beacon_jiffies;
	u64  current_jiffies;
	u64  beacon_age_sec;
	u8   intersection_done;
	u32  intersect_attempts;
	u32  intersect_successes;
	u32  cap_update_failures;
	u32  rsn_parse_failures;
} __packed;

/* ABI size guard: keep in sync with the userspace definition. */
static_assert(sizeof(struct ieee80211_mesh_peer_dump_entry) == 59,
	      "ieee80211_mesh_peer_dump_entry size mismatch");

#define MESH_BYTE_MASK 0xFF
#define MESH_NIBBLE_MASK 0xF
#define MESH_DBG_MCS_OFFSET 0
#define MESH_DBG_NSS_OFFSET 8
#define MESH_DBG_PRAMBLE_OFFSET 12
#define MESH_DBG_RETRIES_OFFSET 16
#define MESH_DBG_KEYIX_OFFSET 20
#define MESH_DBG_FLAGS_OFFSET 24
#define MESH_DBG_HDR 0x000f0004

struct ath12k_vif_mesh_params {
	u_int32_t mhdr;
	u_int32_t mdbg;
	u_int8_t mhdr_len;
};

static inline int add_mesh_meta_hdr(struct sk_buff *skb, struct ath12k_vif_mesh_params *params)
{
	struct meta_hdr_s *mhdr;
	u_int32_t hdrsize;
	u_int32_t dbg_mhdr;

	dbg_mhdr = params->mhdr ? params->mhdr : MESH_DBG_HDR;
	hdrsize = params->mhdr_len;
	skb->priority = (params->mdbg >> 16) & 0x7;

	if (skb_push(skb, hdrsize) == NULL) {
		pr_err("couldn't add meta header skb %p\n", skb);
		return -1;
	}

	memset(skb->data, 0, hdrsize);
	mhdr = (struct meta_hdr_s *) skb->data;

	mhdr->power = 0xff;
	mhdr->rate_info[0].mcs = (dbg_mhdr >> MESH_DBG_MCS_OFFSET) & MESH_BYTE_MASK;
	mhdr->rate_info[0].nss = (dbg_mhdr >> MESH_DBG_NSS_OFFSET) & MESH_NIBBLE_MASK;
	mhdr->rate_info[0].preamble_type = (dbg_mhdr >> MESH_DBG_PRAMBLE_OFFSET) & MESH_NIBBLE_MASK;
	mhdr->rate_info[0].max_tries = (dbg_mhdr >> MESH_DBG_RETRIES_OFFSET) & MESH_NIBBLE_MASK;
	mhdr->retries = (dbg_mhdr >> MESH_DBG_RETRIES_OFFSET) & MESH_NIBBLE_MASK;
	mhdr->keyix = (dbg_mhdr >> MESH_DBG_KEYIX_OFFSET) & MESH_NIBBLE_MASK;
	mhdr->flags = (dbg_mhdr >> MESH_DBG_FLAGS_OFFSET) & MESH_BYTE_MASK;
	params->mhdr &= ~(METAHDR_FLAG_INFO_UPDATED << MESH_DBG_FLAGS_OFFSET);

	return 0;
}

int ath12k_dp_add_mesh_meta_hdr(struct sk_buff  *skb,
				struct ath12k_vif *ahvif, bool print,
				bool *checkhdr);

void ath12k_dp_rx_fill_mesh_metadata(struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_dp_peer *peer,
				     struct sk_buff *skb,
				     struct hal_rx_spd_data *rx_spd);

bool ath12k_dp_mesh_rx_filter_mesh_packets(struct ath12k_base *ab,
					   struct ath12k_dp_peer *peer,
					   struct hal_rx_spd_data *rx_spd);
#endif /* __MESH_UTIL_H__ */
