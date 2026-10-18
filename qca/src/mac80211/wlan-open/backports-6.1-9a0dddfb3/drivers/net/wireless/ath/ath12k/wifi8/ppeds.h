/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_WIFI8_PPE_H
#define ATH12K_WIFI8_PPE_H

#include "hal.h"
#include "../ppe.h"
#include "dp_rx.h"
#include "dp.h"

#define PPEDS_TX_CMPLN_RING_NUM 6
#define PPE_VP_WIFI8_ENTRIES_MAX 192
#define PPE_VP_WIFI8_START_IDX 64
#define PPE_VP_WIFI8_SEARCH_INDEX_REG_NUM_MASK 63
#define PPEDS_CLASSIFY_READ_FULL_PKT 3
#define PPEDS_TQM2PPE_TX_CMPLN_RING_NUM 0
#define PPEDS_ARCH_MODE_WIFI8 8

struct ath12k_base;
struct ath12k_vif;
struct ath12k_vlan_iface;
struct ath12k_link_sta;
struct dp_srng;
extern struct ppe_ds_wlan_ops_v2 ppeds_wlan_ops_v2_wifi8;
extern struct ath12k_ppeds_arch_ops ath12k_wifi8_arch_ppeds_ops;
extern struct sk_buff *
ath12k_dp_ppeds_tx_release_desc(struct ath12k_dp *dp,
				struct ath12k_ppeds_tx_desc_info *tx_desc);

extern unsigned int ath12k_ppeds_ppe2tcl_rings_max;
extern unsigned int ath12k_ppeds_reo2ppe_rings_max;
extern unsigned int ath12k_ppeds_txrx_hw_auto_idx;
extern unsigned int ath12k_ppeds_hw_buff_mgmt;
extern unsigned int ath12k_ppeds_ppe2wbm_ring_size;
extern unsigned int ath12k_ppeds_pkt_pre_hdr_mode;
extern unsigned int ath12k_ppe_ds_wifi8_enabled;

enum ath12k_reo2ppe_rdi {
	PPEDS_REO2PPE1_RDI = 11,
	PPEDS_REO2PPE2_RDI = 12,
	PPEDS_REO2PPE3_RDI = 13,

};

enum ppe2tcl_rings {
	PPEDS_PPE2TCL1 = 0,
	PPEDS_PPE2TCL2,
	PPEDS_PPE2TCL3,
};

enum reo2ppe_rings {
	PPEDS_REO2PPE1 = 0,
	PPEDS_REO2PPE2,
	PPEDS_REO2PPE3,
};


#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT

extern bool ath12k_ppe_rfs_support;

void ath12k_dp_srng_ppeds_cleanup(struct ath12k_base *ab);
int ath12k_dp_srng_ppeds_setup(struct ath12k_base *ab);
int ath12k_wifi8_dp_ppeds_register_soc(struct ath12k_dp *dp,
				 struct dp_ppe_ds_idxs *idx);
void ath12k_wifi8_dp_ppeds_stop(struct ath12k_base *ab);
int ath12k_wifi8_dp_ppeds_start(struct ath12k_base *ab);
int ath12k_wifi8_ppeds_detach(struct ath12k_base *ab);
int ath12k_wifi8_ppeds_attach(struct ath12k_base *ab);
int ath12k_ppeds_get_handle(struct ath12k_base *ab);
irqreturn_t ath12k_wifi8_ds_ppe2tcl_irq_handler(int irq, void *ctxt);
irqreturn_t ath12k_wifi8_ds_reo2ppe_irq_handler(int irq, void *ctxt);
irqreturn_t ath12k_wifi8_dp_ppeds_handle_tx_comp(int irq, void *ctxt);
void ath12k_wifi8_dp_ppeds_update_vp_entry(struct ath12k *ar,
				     struct ath12k_link_vif *arvif);
void ath12k_wifi8_dp_tx_ppeds_cfg_astidx_cache_mapping(struct ath12k_base *ab,
						 struct ath12k_link_vif *arvif,
						 bool peer_map);
void ath12k_wifi8_ppe_ds_attach_vlan_vif_link(struct ath12k_vlan_iface *vlan_iface,
					int ppe_vp_num);
void ath12k_wifi8_ppeds_detach_link_apvlan_vif(struct ath12k_link_vif *arvif,
					 struct ath12k_vlan_iface *vlan_iface,
					 int link_id);
int ath12k_wifi8_ppeds_attach_link_vif(struct ath12k_link_vif *arvif, int vp_num,
				 int *link_ppe_vp_profile_idx,
				 struct ieee80211_vif *vif);
void ath12k_wifi8_ppeds_detach_link_vif(struct ath12k_link_vif *arvif,
				  int ppe_vp_profile_idx);

void ath12k_wifi8_dp_ppeds_interrupt_stop(struct ath12k_base *ab);
void ath12k_wifi8_dp_ppeds_stop(struct ath12k_base *ab);
void ath12k_wifi8_dp_ppeds_interrupt_start(struct ath12k_base *ab);

struct ath12k_dp_ppe_vp_profile *
ath12k_wifi8_dp_ppeds_get_vp_profile(struct ath12k_base *ab,
		int vp_num);
int ath12k_wifi8_ppeds_attach_vif(struct ath12k_base *ab,
				struct ath12k_vif *ahvif,
				u32 vdev_id, int bank_id, u8 lmac_id);
int ath12k_wifi8_dp_srng_ppeds_alloc(struct ath12k_base *ab);
int ath12k_wifi8_dp_srng_ppeds_init(struct ath12k_base *ab);
void ath12k_ppeds_get_rxfill_ring_info_v2(int ds_node_id,
					  struct ppe_ds_wlan_rxfill_ring_info *info);
static inline struct ath12k_base *
		ath12k_wifi8_ppeds_get_central_ab(struct ath12k_base *ab)
{
	return ath12k_dp_get_ab_from_dp_hw_group(ab->dp->dp_hw_grp);
}
#else

static inline struct ath12k_base *
		ath12k_wifi8_ppeds_get_central_ab(struct ath12k_base *ab)
{
	return ab;
}

static inline int ath12k_wifi8_ppeds_attach_link_vif(struct ath12k_link_vif *arvif,
					       int vp_num,
					       int *link_ppe_vp_profile_idx,
					       struct ieee80211_vif *vif)
{
	return 0;
}

static inline void ath12k_wifi8_ppeds_detach_link_vif(struct ath12k_link_vif *arvif,
						int ppe_vp_profile_idx)
{
}


static inline irqreturn_t ath12k_dp_ppeds_handle_tx_comp(int irq, void *ctxt)
{
	return IRQ_HANDLED;
}


static inline
void ath12k_wifi8_dp_tx_ppeds_cfg_astidx_cache_mapping(struct ath12k_base *ab,
						 struct ath12k_link_vif *arvif,
						 bool peer_map)
{
}

#endif /* CPTCFG_ATH12K_PPE_DS_SUPPORT */
#endif /* ATH12K_PPE_H */
