/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_TX_QUEUE_H
#define ATH12K_DP_TX_QUEUE_H

#include "dp_tx_tid.h"
#include "../wmi.h"

enum ath12k_txpt_classify_flag {
	TXPT_DEFAULT_FLOWQ_EMPTY = 0x0,
	TXPT_DEFAULT_FLOWQ_ALLOC = 0x1,
};

enum ath12k_classify_bank_subid {
	CLASSIFY_BANK_SUBID_NON_UDP,
	CLASSIFY_BANK_SUBID_UDP,
	CLASSIFY_BANK_SUBID_MIXED,
};

struct ath12k_dp_tx_metdata {
	u8 svc_id; //bit 0
	u32 bitmap; //bitmap is used to track if fields are modified
	u8 qm_id;
	bool txop_intent;
};

struct ath12k_dp_tx_queue_metadata {
	u8 tidno;
	u8 flow_type;
	enum hal_tcl_encap_type encap_type;
	struct ath12k_dp_tx_metdata q_params;
};

extern u32 link_to_mgmt_type_map[];

#define MGMT_MSDUQ_LINK(n) \
	MGMT_MSDUQ_LINK_##n

#define HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC(n) \
	HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC_##n

static inline u32 ath12k_link_num_to_mgmt_type(u8 link_num)
{
	switch (link_num) {
	case 0:  return MGMT_MSDUQ_LINK(0);
	case 1:  return MGMT_MSDUQ_LINK(1);
	case 2:  return MGMT_MSDUQ_LINK(2);
	case 3:  return MGMT_MSDUQ_LINK(3);
	default: return MGMT_MSDUQ_TYPE_MAX;
	}
}

static inline u32 ath12k_link_num_to_msduq_type(u8 link_num)
{
	switch (link_num) {
	case 0:  return HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC(0);
	case 1:  return HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC(1);
	case 2:  return HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC(2);
	case 3:  return HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC(3);
	default: return HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC_4;
	}
}

#define ATH12K_LINK_TO_MGMT_TYPE(link_num) \
	ath12k_link_num_to_mgmt_type((u8)(link_num))

#define ATH12K_LINK_TO_MSDUQ_TYPE(link_num) \
	ath12k_link_num_to_msduq_type((u8)(link_num))

#define HTT_MSDUQ_INDEX_TO_CLASSIFY_BANK_ID(htt_msduq_idx)    ((htt_msduq_idx) >> 0x1)
#define HTT_MSDUQ_INDEX_TO_CLASSIFY_BANK_SUBID(htt_msduq_idx) ((htt_msduq_idx) &  0x1)

void ath12k_set_txpt_flow_ptr(struct ath12k_dp_hw_group *dp_hw_grp,
			      struct ath12k_dp_peer *peer);
u8 ath12k_tx_get_bank_id(struct ath12k_dp_peer *peer,
			 enum htt_tx_tid_msduq_mpdu_type msduq_idx,
			 enum ath12k_classify_bank_subid *bank_sub_id);
struct hal_txpt_classify_info *ath12k_get_txpt_info_ptr(struct ath12k_dp_peer *peer,
							u8 bank_id, u8 tidpos);
dma_addr_t ath12k_get_txpt_paddr(struct ath12k_dp_peer *peer, u8 bank_id, u8 tidpos);

int ath12k_tx_classify_info_alloc(struct ath12k_dp_hw_group *dp_hw_grp,
				  struct ath12k_dp_peer *peer, u8 tid_num,
				  enum ath12k_txpt_classify_flag flag, u8 flow_mask);
void ath12k_dp_txpt_update_to_all(struct ath12k_dp_peer *peer,
				  struct hal_txpt_classify_info *txpt_info_ptr);
int ath12k_peer_alloc_default_queues(struct ath12k_dp_hw_group *dp_hw_grp,
				     struct ath12k_dp_peer *peer,
				     struct ath12k_dp_vif *dp_vif, bool is_qos);
int ath12k_peer_alloc_mgmt_queues(struct ath12k_dp_hw_group *dp_hw_grp,
				  struct ath12k_dp_peer *peer,
				  struct ath12k_dp_vif *dp_vif);
int ath12k_peer_alloc_mcast_queues(struct ath12k_dp_hw_group *dp_hw_grp,
				   struct ath12k_dp_peer *peer,
				   struct ath12k_dp_vif *dp_vif);
int ath12k_tx_alloc_hol_flow_ptr(struct ath12k_dp_hw_group *dp_hw_grp,
				 struct ath12k_dp_peer *peer);
int ath12k_peer_alloc_hol_queues(struct ath12k_dp_hw_group *dp_hw_grp,
				 struct ath12k_dp_peer *peer,
				 struct ath12k_dp_vif *dp_vif, bool is_qos);
struct hal_txpt_classify_info
*ath12k_tx_alloc_mcast_flow_ptr(struct ath12k_dp_hw_group *dp_hw_grp,
				struct ath12k_dp_peer *peer);
void ath12k_tx_classify_info_free(struct ath12k_dp_hw_group *dp_hw_grp,
				  struct hal_txpt_classify_info **tx_tid_ptr,
				  dma_addr_t txpt_paddr);
void ath12k_peer_free_static_queues(struct ath12k_dp_hw_group *dp_hw_grp,
				    struct ath12k_dp_peer *peer);
void ath12k_dp_peer_free_queues(struct ath12k_dp_hw_group *dp_hw_grp,
				struct ath12k_dp_peer *peer);
int ath12k_peer_alloc_dynamic_queue(struct ath12k_dp_hw_group *dp_hw_grp,
				    struct ath12k_dp_peer *peer,
				    struct ath12k_dp_tx_queue_metadata *tx_queue_params);
#endif
