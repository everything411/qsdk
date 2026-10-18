/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_PPE_H
#define ATH12K_PPE_H

#ifndef PLATFORM_SDX
#include <ppe_ds_wlan.h>
#include <ppe_vp_public.h>
#include <ppe_drv_sc.h>
#include <ppe_drv.h>
#include <nss_plugins.h>
#endif
#include "dp_cmn.h"
#include "dp.h"
#include "core.h"
#include "cmn_defs.h"

struct ath12k_base;
struct ath12k_vif;
struct ath12k_vlan_iface;
struct ath12k_link_sta;

enum ppeds_irq_type {
	PPEDS_IRQ_PPE2TCL,
	PPEDS_IRQ_REO2PPE,
	PPEDS_IRQ_TX_COMPLETION,
	PPEDS_IRQ_PPE2TCL_1,
	PPEDS_IRQ_REO2PPE_1,
	PPEDS_IRQ_TX_COMPLETION_1,
};

struct dp_ppe_ds_idxs {
	u32 ppe2tcl_start_idx;
	u32 reo2ppe_start_idx;
	u32 tqm2ppe_start_idx;
};

#ifndef PPE_DS_TXCMPL_DEF_BUDGET
#define PPE_DS_TXCMPL_DEF_BUDGET 256
#endif

#define PPE_DS_MAX_NODE	            4   /* Max DS node supported */
#define DP_PPEDS_SERVICE_BUDGET     256

#define ATH12K_PPE_DEFAULT_CORE_MASK		ath12k_rfs_core_mask[0]
#define ATH12K_PPE_RFS_2GHZ_CORE_MASK		ath12k_rfs_core_mask[1]
#define ATH12K_PPE_RFS_5GHZ_CORE_MASK		ath12k_rfs_core_mask[2]
#define ATH12K_PPE_RFS_6GHZ_CORE_MASK		ath12k_rfs_core_mask[3]

#define ATH12K_INVALID_PPE_VP_NUM -1
#define ATH12K_INVALID_PPE_VP_TYPE -1
#define ATH12K_INVALID_VP_PROFILE_IDX	-1
#define ATH12K_PPEDS_INVALID_SOC_IDX -1

#define ATH12K_DP_RX_FSE_FLOW_METADATA_MASK      0xFFFF

#define PPE_VP_ENTRIES_MAX 192
#define MAX_PPEDS_IRQ_NAME_LEN 20
#define MAX_PPEDS_IRQS 6
#define ATH12K_PPEDS_IRQ_NEXT_RING 3
#define ATH12K_PPE2TCL_MAX_RINGS 2
#define ATH12K_REO2PPE_MAX_RINGS 2


struct ath12k_dp_ppe_vp_profile {
	bool is_configured;
	u8 ref_count;
	u8 vp_num;
	u8 ppe_vp_num_idx;
	u8 search_idx_reg_num;
	u8 drop_prec_enable;
	u8 to_fw;
	u8 use_ppe_int_pri;
	struct ath12k_link_vif *arvif;
	bool entry_valid;
};

struct dp_ppeds_hbm_tx_comp_ring {
	struct dp_srng tqm2ppe;
	struct  buffer_addr_info *tx_status;
	int tx_status_head;
	int tx_status_tail;
	u8 macid[DP_PPEDS_SERVICE_BUDGET];
};

struct dp_ppeds_tx_comp_ring {
	struct dp_srng ppeds_txcmpl_ring;
	struct hal_wbm_completion_ring_tx *tx_status;
	int tx_status_head;
	int tx_status_tail;
	u8 macid[DP_PPEDS_SERVICE_BUDGET];
};

struct ath12k_ppeds_stats {
	u32 tcl_prod_cnt;
	u32 tcl_cons_cnt;
	u32 reo_prod_cnt;
	u32 reo_cons_cnt;
	u32 get_tx_desc_cnt;
	u32 tx_desc_allocated;
	u32 tx_desc_alloc_fails;
	u32 tx_desc_freed;
	u32 fw2wbm_pkt_drops;
	u32 enable_intr_cnt;
	u32 disable_intr_cnt;
	u32 release_tx_single_cnt;
	u32 release_rx_desc_cnt;
	u32 num_rx_desc_freed;
	u32 num_rx_desc_realloc;
	u32 tqm_rel_reason[HAL_WBM_TQM_REL_REASON_MAX];
};

/*
 * DP arch ops to communicate from common module
 * to arch specific module
 */
struct ath12k_ppeds_arch_ops {
	irqreturn_t (*ath12k_ppeds_ppe2tcl_irq_handler)(int irq, void *ctxt);
	irqreturn_t (*ath12k_ppeds_reo2ppe_irq_handler)(int irq, void *ctxt);
	irqreturn_t (*ath12k_ppeds_ppe2tcl_tx_compln)(int irq, void *ctxt);
	int (*ath12k_ppeds_start)(struct ath12k_base *ab);
	void (*ath12k_ppeds_stop)(struct ath12k_base *ab);
	int (*ath12k_ppeds_attach)(struct ath12k_base *ab);
	int (*ath12k_ppeds_detach)(struct ath12k_base *ab);
	int (*ath12k_ppeds_register_soc)(struct ath12k_dp *dp,
						struct dp_ppe_ds_idxs *idx);
	void (*ath12k_ppeds_srng_cleanup)(struct ath12k_base *ab);
	void (*ath12k_ppeds_interrupt_start)(struct ath12k_base *ab);
	void (*ath12k_ppeds_interrupt_stop)(struct ath12k_base *ab);
	int (*ath12k_dp_ppeds_alloc_ppe_vp_profile)(struct ath12k_base *ab,
					struct ath12k_dp_ppe_vp_profile **vp_profile,
					int vp_num);
	void (*ath12k_dp_ppeds_dealloc_ppe_vp_profile)(struct ath12k_base *ab,
							     int ppe_vp_profile_idx,
							     enum nl80211_iftype type);
	int (*ath12k_dp_ppeds_alloc_vp_tbl_entry)(struct ath12k_base *ab,
						int ppe_vp_profile_idx);
	int (*ath12k_dp_ppeds_alloc_vp_search_idx_tbl_entry)(struct ath12k_base *ab,
						int ppe_vp_profile_idx);
	int (*ath12k_ppeds_srng_cmn_init)(struct ath12k_base *ab);
	int (*ath12k_ppeds_srng_cmn_alloc)(struct ath12k_base *ab);
	struct ath12k_dp_ppe_vp_profile *
		(*ath12k_dp_ppeds_get_vp_profile_from_idx)(struct ath12k_base *ab,
				uint32_t ppe_vp_idx);
	int (*ath12k_dp_ppeds_get_bank_lmac_id)(struct ath12k_base *ab,
			struct ath12k *ar,
			struct ath12k_link_vif *arvif,
			struct ath12k_dp_ppe_vp_profile *vp_profile,
			u8 *bank_id, u8 *lmac_id);
};

struct ath12k_ppeds_napi {
	struct napi_struct napi;
	struct net_device ndev;
};

struct ath12k_ppe {
	struct ath12k_base *ab;
	struct dp_srng reo2ppe_ring[ATH12K_REO2PPE_MAX_RINGS];
	struct dp_srng ppe2tcl_ring[ATH12K_PPE2TCL_MAX_RINGS];
	struct dp_ppeds_tx_comp_ring ppeds_comp_ring;
	struct ath12k_dp_ppe_vp_profile ppe_vp_profile[PPE_VP_ENTRIES_MAX];
	char ppeds_irq_name[MAX_PPEDS_IRQS][MAX_PPEDS_IRQ_NAME_LEN];
	int ppeds_irq[MAX_PPEDS_IRQS];
	int ds_node_id;
	int task_id;
	/* used for per node enumeration*/
	int ppeds_node_idx;
	int ppe_vp_tbl_registered[PPE_VP_ENTRIES_MAX];
	int ppe_vp_search_idx_tbl_set[PPE_VP_ENTRIES_MAX];
	struct ath12k_ppeds_napi ppeds_napi_ctxt;
	spinlock_t ppe_vp_tbl_lock; /* protects ppe_vp_tbl updates */
	u16 *ppeds_rx_idx[ATH12K_MAX_SOCS];
	u16 ppeds_rx_num_elem;
	int ppeds_soc_idx;
	u8 num_ppe_vp_profiles;
	u8 num_ppe_vp_search_idx_entries;
	u8 num_ppe_vp_entries;
	u8 ppeds_int_mode_enabled;
	u8 ppeds_stopped;
	u8 txrx_hw_auto_idx; /**< Auto index enabled / disabled for PPE2TCL/REO2PPE*/
	u8 hw_buff_mgmt; /**< HW buffer management */
	struct dp_srng tqm2ppe_txcmp_ring; /**< TQM2PPE tx completion ring */
	struct dp_srng ppe2wbm_refill_ring;	/**< PPE2WBM refill ring*/
	struct ath12k_ppeds_stats ppeds_stats;
	struct nss_plugins_ops *nss_plugin_ops;
	struct ppe_ds_wlan_ops_v2 *ppeds_wlanops;
	struct ath12k_ppeds_arch_ops *ppe_ops;
};

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT

extern bool ath12k_ppe_rfs_support;
extern atomic_t ath12k_num_ppeds_nodes;
extern struct ath12k_base *ds_node_map[PPE_DS_MAX_NODE];

int ath12k_ppe_rfs_get_core_mask(void);
int ath12k_change_core_mask_for_ppe_rfs(struct ath12k_base *ab,
					struct ath12k_vif *ahvif,
					int core_mask);
int ath12k_vif_update_vp_config(struct ath12k_vif *ahvif, int vp_type);
int ath12k_vif_get_vp_num(struct ath12k_vif *ahvif, struct net_device *dev);
int ath12k_vif_set_mtu(struct ath12k_vif *ahvif, int mtu);

void ath12k_dp_srng_ppeds_cleanup(struct ath12k_base *ab);
int ath12k_dp_srng_ppeds_setup(struct ath12k_base *ab);
int ath12k_dp_ppeds_register_soc(struct ath12k_dp *dp,
				 struct dp_ppe_ds_idxs *idx);
int ath12k_dp_ppeds_start(struct ath12k_base *ab);
int ath12k_ppeds_detach(struct ath12k_base *ab);
int ath12k_ppeds_attach(struct ath12k_base *ab);
void ath12k_dp_peer_ppeds_route_setup(struct ath12k *ar, struct ath12k_link_vif *arvif,
				      struct ath12k_link_sta *arsta);
int ath12k_ppeds_get_handle(struct ath12k_base *ab);
void *ath12k_dp_get_ppe_ds_ctxt(struct ath12k_base *ab);
void ath12k_dp_ppeds_update_vp_entry(struct ath12k *ar,
				     struct ath12k_link_vif *arvif);
void ath12k_dp_tx_ppeds_cfg_astidx_cache_mapping(struct ath12k_base *ab,
						 struct ath12k_link_vif *arvif,
						 bool peer_map);
void ath12k_ppe_ds_attach_vlan_vif_link(struct ath12k_vlan_iface *vlan_iface,
					int ppe_vp_num);
void ath12k_ppeds_detach_link_apvlan_vif(struct ath12k_link_vif *arvif,
					 struct ath12k_vlan_iface *vlan_iface,
					 int link_id);
int ath12k_ppeds_attach_link_vif(struct ath12k_link_vif *arvif, int vp_num,
				 int *link_ppe_vp_profile_idx,
				 struct ieee80211_vif *vif);
void ath12k_ppeds_detach_link_vif(struct ath12k_link_vif *arvif,
				  int ppe_vp_profile_idx);
void ath12k_ppeds_update_splitphy_bank_id(struct ath12k_base *ab,
					  struct ath12k_link_vif *arvif);
void ath12k_vif_free_vp(struct ath12k_vif *ahvif, struct net_device *dev);

void ath12k_dp_ppeds_service_enable_disable(struct ath12k_base *ab,
					    bool enable);
void ath12k_dp_ppeds_interrupt_stop(struct ath12k_base *ab);
void ath12k_dp_ppeds_interrupt_start(struct ath12k_base *ab);
int ath12k_nss_plugin_register_ops(struct ath12k_base *ab);
void ath12k_nss_plugin_unregister_ops(struct ath12k_base *ab);
void ath12k_ppe_ds_notify_freq_range(struct ath12k *ar, u32 freq_low, u32 freq_high);

void ath12k_dp_ppeds_tx_release_desc_list_bulk(struct ath12k_base *ab,
					       struct ath12k_dp_hw_group *dp_hw_grp,
					       struct list_head *local_list,
					       int local_list_len,
					       struct list_head *local_list_no_skb,
					       int list_no_skb_count);
void ath12k_dp_ppeds_setup_vp_entry(struct ath12k_base *ab,
				    struct ath12k *ar,
				    struct ath12k_link_vif *arvif,
				    struct ath12k_dp_ppe_vp_profile *ppe_vp_profile);

int ath12k_ppeds_attach_link_apvlan_vif(struct ath12k_link_vif *arvif, int vp_num,
					struct ath12k_vlan_iface *vlan_iface,
					int link_id);

int ath12k_dp_srng_init_idx(struct ath12k_base *ab, struct dp_srng *ring,
			    enum hal_ring_type type, int ring_num,
			    int mac_id,
			    int num_entries, u32 restore_idx);

int ath12k_ppeds_dp_srng_init(struct ath12k_base *ab, struct dp_srng *ring,
			      enum hal_ring_type type, int ring_num,
			      int mac_id, int num_entries,
			      u32 restore_idx);
void ath12k_dp_ppeds_tx_set_ppe_vp_entry(struct ath12k_base *ab,
					 struct ath12k_dp_ppe_vp_profile *ppe_vp_profile,
					 u32 ppe_vp_idx, u32 vdev_id,
					 u32 bank_id, u32 lmac_id);
int ath12k_ppeds_dp_srng_alloc(struct ath12k_base *ab, struct dp_srng *ring,
			       enum hal_ring_type type, int ring_num,
			       int num_entries);
void ath12k_dp_ppeds_stop(struct ath12k_base *ab);
#else
static inline void ath12k_dp_srng_ppeds_cleanup(struct ath12k_base *ab)
{
}

static inline int ath12k_dp_srng_ppeds_setup(struct ath12k_base *ab)
{
	return 0;
}

static inline int ath12k_dp_ppeds_register_soc(struct ath12k_dp *dp,
					       struct dp_ppe_ds_idxs *idx)
{
	return 0;
}

static inline void ath12k_dp_ppeds_stop(struct ath12k_base *ab)
{
}

static inline int ath12k_dp_ppeds_start(struct ath12k_base *ab)
{
	return 0;
}

static inline int ath12k_ppeds_detach(struct ath12k_base *ab)
{
	return 0;
}

static inline int ath12k_ppeds_attach(struct ath12k_base *ab)
{
	return 0;
}

static inline int ath12k_ppeds_attach_link_vif(struct ath12k_link_vif *arvif,
					       int vp_num,
					       int *link_ppe_vp_profile_idx,
					       struct ieee80211_vif *vif)
{
	return 0;
}

static inline void ath12k_ppeds_detach_link_vif(struct ath12k_link_vif *arvif,
						int ppe_vp_profile_idx)
{
}

static inline void ath12k_dp_peer_ppeds_route_setup(struct ath12k *ar,
						    struct ath12k_link_vif *arvif,
						    struct ath12k_link_sta *arsta)
{
}

static inline int ath12k_ppeds_get_handle(struct ath12k_base *ab)
{
	return 0;
}

static inline void *ath12k_dp_get_ppe_ds_ctxt(struct ath12k_base *ab)
{
	return NULL;
}

static inline void ath12k_dp_ppeds_service_enable_disable(struct ath12k_base *ab,
							  bool enable)
{
}

static inline void ath12k_dp_ppeds_interrupt_stop(struct ath12k_base *ab)
{
}

static inline void ath12k_dp_ppeds_interrupt_start(struct ath12k_base *ab)
{
}

static inline void ath12k_dp_ppeds_update_vp_entry(struct ath12k *ar,
						   struct ath12k_link_vif *arvif)
{
}
static inline int ath12k_vif_update_vp_config(struct ath12k_vif *ahvif,
					      int vp_type)
{
	return -EINVAL;
}

static inline void ath12k_vif_free_vp(struct ath12k_vif *ahvif,
				      struct net_device *dev)
{
}

static inline
void ath12k_dp_tx_ppeds_cfg_astidx_cache_mapping(struct ath12k_base *ab,
						 struct ath12k_link_vif *arvif,
						 bool peer_map)
{
}

static inline void ath12k_ppeds_detach_link_apvlan_vif(struct ath12k_link_vif *arvif,
						       struct ath12k_vlan_iface *vlan_iface,
						       int link_id)
{
}

static inline void ath12k_ppe_ds_attach_vlan_vif_link(struct ath12k_vlan_iface *vlan_iface,
						      int ppe_vp_num)
{
}

static inline int ath12k_nss_plugin_register_ops(struct ath12k_base *ab)
{
	return 0;
}

static inline void ath12k_nss_plugin_unregister_ops(struct ath12k_base *ab)
{
}

static inline void ath12k_ppe_ds_notify_freq_range(struct ath12k *ar,
						    u32 freq_low, u32 freq_high)
{
}
#endif /* CPTCFG_ATH12K_PPE_DS_SUPPORT */
#endif /* ATH12K_PPE_H */
