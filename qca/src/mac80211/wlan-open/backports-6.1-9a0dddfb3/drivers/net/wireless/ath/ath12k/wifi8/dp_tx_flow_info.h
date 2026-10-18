/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_TX_CLASSIFY_H
#define ATH12K_DP_TX_CLASSIFY_H

#include "dp_ast.h"
#include "hal_desc.h"
#include "../core.h"
#include "../peer.h"

#define ATH12K_NUM_MSDU_Q_PER_TID	2
#define ATH12K_MAX_DP_MSDUQ_PER_TID \
	(ATH12K_NUM_TX_CLASSIFY_BANKS * ATH12K_NUM_MSDU_Q_PER_TID)

#define ATH12K_MAX_MSDU_COUNT		0xFFFF
#define ATH12K_INVALID_TID		0xFF
#define ATH12K_NUM_TID_PER_BANK		9
#define ATH12K_MAX_NUM_DATA_TIDS	8
#define ATH12K_SIZE_OF_TID_INFO		12
#define ATH12K_TX_CLASSIFY_INFO_SIZE_SINGLE \
	(ATH12K_NUM_TID_PER_BANK * ATH12K_SIZE_OF_TID_INFO)

#define ATH12K_DP_PN_COUNTER_ALIGN	8
#define ATH12K_DP_PN_COUNTER_SIZE	16
#define ATH12K_DP_PN_NUM_PAGE		(ATH12K_MAX_PEER_ID / \
					(PAGE_SIZE / ATH12K_DP_PN_COUNTER_SIZE))
#define ATH12K_DP_PN_PAGE_INDEX		GENMASK(15, 8)
#define ATH12K_DP_PN_PAGE_OFFSET	GENMASK(7, 0)

/* HW requirement for these structures are 48 dword size and aligned. */
#define MSDU_STRUCT_SZ                  256
/* HW requirement for these structures are 48 dword size and aligned. */
#define MPDU_STRUCT_SZ                  256
#define NUM_TOTAL_MPDU_QUEUES           4096
#define NUM_TOTAL_MSDU_QUEUES           8192

enum ath12k_mgmt_msduq_type {
	MGMT_MSDUQ_LINK_CMN,
	MGMT_MSDUQ_NON_ML,
	MGMT_MSDUQ_LINK_0,
	MGMT_MSDUQ_LINK_1,
	MGMT_MSDUQ_LINK_2,
	MGMT_MSDUQ_LINK_3,
	MGMT_MSDUQ_LINK_4,
	MGMT_MSDUQ_TYPE_MAX,
};

struct ath12k_pn_page_info {
	dma_addr_t paddr;
	void *vaddr;
};

enum ath12k_tx_q_state {
	ATH12K_TX_Q_INVALID,
	ATH12K_TX_Q_CREATED,
	ATH12K_TX_Q_INIT_DONE,
	ATH12K_TX_Q_DELETED,
	ATH12K_TX_Q_MODIFIED,
};

struct ath12k_flow_metadata {
	u32 peer_id :12,
	    tid_num:5,
	    flow_type:5,
	    reserved:10;
};

struct ath12k_dp_msdu_q_info  {
	void *msdu_q_vaddr;
	dma_addr_t msdu_q_paddr;
	u32 msduq_idx;
	u16 msduq_sam_id;
	enum ath12k_tx_q_state msduq_state;
	union {
		u32 queue_number:24,
		    reserved:8;
		struct ath12k_flow_metadata flow_info;
	};
	struct list_head list;
	u8 svc_id;
	u8 qm_id;
	bool txop_intent;
	u8 bitmap;
	u8 mlo:1,
	   qos:1,
	   tqm_send:1,
	   allocated:1;
	enum hal_tqm_service_category svc;
	unsigned long last_drop_jiffies;
	u32 consecutive_drop_count;
	bool in_threshold_list;
	struct list_head threshold_node;
};

struct ath12k_dp_mpdu_q_info {
	void *mpdu_q_vaddr;
	dma_addr_t mpdu_q_paddr;
	dma_addr_t pn_addr;
	u32 mpduq_id;
	u16 mpduq_sam_id;
	enum ath12k_tx_q_state mpduq_state;
	union {
		u32 queue_number:24,
		    reserved:8;
		struct ath12k_flow_metadata flow_info;
	};
	bool tqm_send;
	struct list_head list;
};

struct ath12k_dp_tx_tid_info {
	/* to hold q info for all the banks */
	struct ath12k_dp_msdu_q_info *msduq[ATH12K_MAX_DP_MSDUQ_PER_TID];
	struct ath12k_dp_mpdu_q_info *mpduq;
	u16 peer_id;
	u8 tid_num;
	u8 num_of_active_msdu_queues;
};

struct ath12k_dp_tx_flow_info {
	/* tx flow_q lock */
	spinlock_t tx_q_lock;
	/* for all regular data tids */
	struct ath12k_dp_tx_tid_info tid_info[ATH12K_MAX_NUM_DATA_TIDS];

	struct ath12k_dp_msdu_q_info *mcast_msduq;
	struct ath12k_dp_mpdu_q_info *mcast_mpduq;
	struct ath12k_dp_mpdu_q_info *mgmt_mpduq;
	struct ath12k_dp_msdu_q_info *mgmt_msduq[MGMT_MSDUQ_TYPE_MAX];

	struct ath12k_dp_msdu_q_info *hol_msduq;
	u8 holq_tid;

	void *hw_who_classify_info_vaddr;
	dma_addr_t hw_who_classify_info_paddr;

	unsigned long assoc_hw_links_bitmap;
	unsigned long txq_hw_links_bitmap;
};

int ath12k_dp_tx_classify_info_alloc(struct ath12k_dp_hw_group *dp_hw_grp,
				     dma_addr_t *tx_classify_info_paddr,
				     void **tx_classify_info_vaddr);
void ath12k_dp_tx_classify_info_free(struct ath12k_dp_hw_group *dp_hw_grp,
				     dma_addr_t tx_classify_info_paddr,
				     void *tx_classify_info_vaddr);
dma_addr_t ath12k_dp_get_page_paddr(struct ath12k_dp_hw_group *dp_hw_grp,
				    u16 sw_peer_id);
void *ath12k_dp_get_page_vaddr(struct ath12k_dp_hw_group *dp_hw_grp,
			       u16 sw_peer_id);
int ath12k_dp_pn_counter_page_init(struct ath12k_dp_hw_group *dp_hw_grp);
void ath12k_dp_pn_counter_page_free(struct ath12k_dp_hw_group *dp_hw_grp);
int ath12k_dp_tx_peer_msduq_mpduq_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					struct ath12k_dp_peer *dp_peer,
					u8 link_id);
int ath12k_dp_tx_mcast_msduq_mpduq_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					 struct ath12k_dp_peer *dp_peer);
int ath12k_wifi8_dp_tx_pool_create(struct ath12k_dp_hw_group *dp_hw_grp);
void ath12k_wifi8_dp_tx_pool_destroy(struct ath12k_dp_hw_group *dp_hw_grp);
int ath12k_wifi8_qos_queue_setup(struct ath12k_dp_hw_group *dp_hw_grp,
				 struct ath12k_dp_peer *dp_peer,
				 u8 msduq, u16 qos_id,
				 u8 qm_id, bool txop_intent);
#endif
