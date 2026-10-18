/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_MSDU_QUEUE_H
#define ATH12K_DP_MSDU_QUEUE_H

#include "../dp_peer.h"
#include "hal_queue.h"
#include "dp_tx_flow_info.h"
#include "dp_htt.h"

#define MAX_DEFAULT_MSDU_QUEUES    18 /* (TID(8) + HOL) x 2 (UDP + NON_UDP) */

struct ath12k_dp_msdu_q_info
*ath12k_alloc_tx_msdu_flowq(struct ath12k_dp_hw_group *dp_hw_grp,
			    struct ath12k_dp_peer *peer);
int ath12k_init_tx_msdu_flowq(struct ath12k_dp_hw_group *dp_hw_grp,
			      struct ath12k_dp_peer *peer,
			      struct ath12k_dp_msdu_q_info *sw_msduq_ptr);
struct ath12k_dp_msdu_q_info
*ath12k_init_alloc_tx_msdu_flowq(struct ath12k_dp_hw_group *dp_hw_grp,
				 struct ath12k_dp_peer *peer,
				 u8 tid_num, u8 msduq_type, u8 mgmt_msduq_type);
void ath12k_free_tx_msdu_flowq(struct ath12k_dp_hw_group *dp_hw_grp,
			       struct ath12k_dp_msdu_q_info *sw_msduq_ptr);
u8 ath12k_dp_get_chipid_bitmap(struct ath12k_dp_hw_group *dp_hw_grp,
			       struct ath12k_dp_peer *dp_peer);
#endif
