/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_MGMT_RX_WIFI8_H
#define ATH12K_MGMT_RX_WIFI8_H

#include "hw.h"
#include "../mgmt_rx.h"
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/ath12k_cmn_extn.h"
#endif /* CPTCFG_QCN_EXTN */

struct ath12k_base;

struct ath12k_mgmt_wifi8 {
	struct mgmt_srng reo_dst_rx_ring;
	struct mgmt_srng reo_dst_rx_err_ring;
	struct mgmt_srng wbm_refill_ring;
	struct mgmt_srng wbm_idle_buf_ring;
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_mgmt_wifi8_extn mgmt_wifi8_extn;
#endif
};

struct ath12k_mgmt *ath12k_wifi8_mgmt_init(struct ath12k_base *ab);
void ath12k_wifi8_mgmt_deinit(struct ath12k_mgmt *mgmt);
void ath12k_wifi8_mgmt_rx_refill_ring_init(struct ath12k_base *ab);
int ath12k_wifi8_mgmt_rx_ring_setup(struct ath12k_base *ab);
void ath12k_wifi8_srng_hw_mgmt_rings_disable(struct ath12k_base *ab);
void ath12k_mgmt_srng_hw_disable(struct ath12k_base *ab, struct mgmt_srng *ring);
void ath12k_wifi8_mgmt_refill_rings_deinit(struct ath12k_base *ab);
int ath12k_wifi8_mgmt_rx_refill_ring_setup(struct ath12k_base *ab);

int ath12k_wifi8_mgmt_wbm_ring_sel_config_qcn9625(struct ath12k_base *ab);

struct ath12k_mgmt *ath12k_wifi8_get_cumac_mgmt(struct ath12k_mgmt *mgmt);

void ath12k_wifi8_cu_mem_update(struct ath12k_base *ab,
				struct ath12k_link_vif *arvif,
				bool is_probe_req);

static inline struct ath12k_mgmt_wifi8 *ath12k_get_mgmt_wifi8(struct ath12k_mgmt *mgmt)
{
	return (struct ath12k_mgmt_wifi8 *)mgmt->arch_priv;
}

static inline
void ath12k_wifi8_mgmt_extract_rx_desc_data(struct ath12k_mgmt *mgmt,
					    struct hal_rx_desc_data *rx_desc_data,
					    struct hal_rx_desc *rx_desc,
					    struct hal_rx_desc *ldesc)
{
	mgmt->ab->hal.hal_ops->extract_rx_desc_data(rx_desc_data, rx_desc, ldesc);
}

void ath12k_wifi8_mgmt_rx_replenish_buffs(struct ath12k_mgmt *mgmt,
					  struct mgmt_srng *rx_refill_ring,
					  struct list_head *desc_used_list,
					  bool reuse);
int ath12k_wifi8_mgmt_rx_ring_setup(struct ath12k_base *ab);

static inline
bool ath12k_wifi8_mgmt_op_override_mld_tx(struct ath12k_mgmt *mgmt)
{
	return false;
}

static inline
u32 ath12k_wifi8_mgmt_rx_h_peer_meta_data(struct ath12k_mgmt *mgmt,
					  struct hal_rx_desc *desc)
{
	return mgmt->ab->hal.hal_ops->rx_h_peer_meta_data(desc);
}

#endif
