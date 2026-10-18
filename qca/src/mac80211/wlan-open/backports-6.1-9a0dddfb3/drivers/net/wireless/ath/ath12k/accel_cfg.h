/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. 
 */

#ifndef ACCEL_CFG_H
#define ACCEL_CFG_H

#include <ath/ath_dp_accel_cfg.h>

#define ATH12K_DP_MSCS_VALID_TID_MASK 0x7
/**
 * ath12k_dp_mscs_peer_lookup_status - mscs lookup status
 * @ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_ALLOW_MSCS_QOS_TAG_UPDATE
 * @ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_DENY_QOS_TAG_UPDATE
 * @ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_ALLOW_INVALID_QOS_TAG_UPDATE
 * @ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_PEER_NOT_FOUND
 */
enum ath12k_dp_mscs_peer_lookup_status {
	ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_ALLOW_MSCS_QOS_TAG_UPDATE,
	ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_ALLOW_INVALID_QOS_TAG_UPDATE,
	ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_DENY_QOS_TAG_UPDATE,
	ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_PEER_NOT_FOUND,
};

/**
 * ath_encode_metadata() - Encode combined MLO and SDWF metadata
 * @link_id: MLO link ID; use ATH12k_MLO_LINK_ID_INVALID to skip MLO encoding
 * @msduq_peer: SDWF peer MSDUQ value; use SDWF_PEER_MSDUQ_INVALID to skip SDWF encoding
 *
 * Return: Combined metadata bitmask
 */
u32 ath_encode_metadata(u8 link_id, u16 msduq_peer);

/**
 * ath12k_dp_accel_cfg_init() - Initialize dp_accel_cfg context
 * @ab: ath12k_base handle
 *
 * Return: None
 */
void ath12k_dp_accel_cfg_init(struct ath12k_base *ab);

/**
 * ath12k_dp_accel_cfg_deinit() - Deinitialize dp_accel_cfg context
 * @ab: ath12k_base handle
 *
 * Return: None
 */
void ath12k_dp_accel_cfg_deinit(struct ath12k_base *ab);
#endif
