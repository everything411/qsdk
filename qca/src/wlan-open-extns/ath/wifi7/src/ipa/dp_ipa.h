/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../../../qcn_extns/ipa/dp_ipa.h"

#define ATH12K_IPA_MAX_IFACE                MAX_IPA_IFACE

int ath12k_wifi7_dp_ipa_init(struct ath12k_base *ab);

#ifndef ETH_P_WAPI
#define ETH_P_WAPI 0x88B4
#endif

/* Nbuf CB values used by IPA component, where driver gets the required info
 * BCMC_OFFSET: rx_msdu_desc_info->da_is_mcbc stored in skb->cb[1] & 0x2
 * CHIP_ID_OFFSET: rx_msdu_desc_info->dest_chip_id stored in skb->cb[7]
 * PAMC_ID_OFFSET: rx_msdu_desc_info->dest_chip_pmac_id in skb->cb[8]
 */
#define DP_IPA_SKB_CB_VDEV_ID		0
#define DP_IPA_SKB_CB_DA_BCMC		1
#define DP_IPA_SKB_CB_PEER_ID_LO	5
#define DP_IPA_SKB_CB_PEER_ID_HI	6
#define DP_IPA_SKB_CB_DEST_CHIP_ID	7
#define DP_IPA_SKB_CB_DEST_CHIP_PMAC	8
#define DP_IPA_SKB_CB_BCMC_MASK			0x2
#define DP_IPA_SKB_CB_VDEV_ID_MASK	0x1F
#define DP_IPA_SKB_CB_PEER_ID_HI_MASK	0x3F

/**
 * ath12k_dp_ipa_skb_get_vdev_id() - Get vdev ID from skb control buffer
 * @skb: socket buffer
 *
 * Extracts the vdev ID stored in the skb control buffer by IPA component.
 *
 * Return: vdev ID as u8
 */
static inline u8 ath12k_dp_ipa_skb_get_vdev_id(struct sk_buff *skb)
{
	return (u8)skb->cb[DP_IPA_SKB_CB_VDEV_ID];
}

/**
 * ath12k_dp_ipa_skb_get_peer_id() - Get peer ID from skb control buffer
 * @skb: socket buffer
 *
 * Extracts the peer ID stored in the skb control buffer by IPA component.
 * Peer ID is 14 bits and stored in cb as follows -
 *	Lower 8 bits at offset DP_IPA_SKB_CB_PEER_ID_LO
 *	Higher 6 bits at offset DP_IPA_SKB_CB_PEER_ID_HI
 *
 * Return: peer ID as u16
 */
static inline u16 ath12k_dp_ipa_skb_get_peer_id(struct sk_buff *skb)
{
	u16 peer_id = skb->cb[DP_IPA_SKB_CB_PEER_ID_LO];

	peer_id |= (skb->cb[DP_IPA_SKB_CB_PEER_ID_HI]
		    & DP_IPA_SKB_CB_PEER_ID_HI_MASK) << 8;

	return peer_id;
}

/**
 * ath12k_dp_ipa_skb_get_is_da_bcmc() - Check if destination address is BC/MC
 * @skb: socket buffer
 *
 * Checks if the destination address is broadcast or multicast by examining
 * the DA_BCMC flag stored in the skb control buffer by IPA component.
 *
 * Return: Non-zero if DA is BC/MC, 0 otherwise
 */
static inline u8 ath12k_dp_ipa_skb_get_is_da_bcmc(struct sk_buff *skb)
{
	return (u8)(skb->cb[DP_IPA_SKB_CB_DA_BCMC]) & DP_IPA_SKB_CB_BCMC_MASK;
}

/**
 * ath12k_dp_ipa_skb_get_dst_chip_id() - Get destination chip ID from skb
 * @skb: socket buffer
 *
 * Extracts the destination chip ID stored in the skb control buffer by
 * IPA component. This is used for MLO scenarios.
 *
 * Return: destination chip ID as u8
 */
static inline u8 ath12k_dp_ipa_skb_get_dst_chip_id(struct sk_buff *skb)
{
	return (u8)skb->cb[DP_IPA_SKB_CB_DEST_CHIP_ID];
}

/**
 * ath12k_dp_ipa_skb_get_dst_chip_pmac_id() - Get destination chip PMAC ID
 * @skb: socket buffer
 *
 * Extracts the destination chip PMAC ID stored in the skb
 * control buffer by IPA component. This is used for split phy scenarios.
 *
 * Return: destination chip PMAC ID as u8
 */
static inline u8 ath12k_dp_ipa_skb_get_dst_chip_pmac_id(struct sk_buff *skb)
{
	return (u8)skb->cb[DP_IPA_SKB_CB_DEST_CHIP_PMAC];
}
