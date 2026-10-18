/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __ATH12K_WIFI8_EXTNS_DP_IPA_H__
#define __ATH12K_WIFI8_EXTNS_DP_IPA_H__

#include "../../../core.h"
#include "../../dp.h"
#include "../../hal.h"
#include "../../../qcn_extns/ipa/dp_ipa.h"

#include <linux/ipa_wdi3.h>

#define ath12k_wifi8_get_ipa_dp(__dp_wifi8) ((__dp_wifi8)->extn.ipa)

#define ATH12K_DP_WIFI8_IPA_REO2SW5_IDX		4
#define ATH12K_DP_WIFI8_REO2SW5_RDI		5

/* WiFi8: single cumac ipa_ctx handles 3 SoCs × 16 ifaces each */
#define ATH12K_IPA_MAX_IFACE_WIFI8          (3 * MAX_IPA_IFACE)

/**
 * enum ath12k_dp_wifi8_ipa2wbm_ring_e - IPA to WBM ring types
 * @ATH12K_DP_WIFI8_IPA2WBM_IPA: ring given to IPA for refill
 * @ATH12K_DP_WIFI8_IPA2WBM_HOST: ring used by HOST to refill err pkts
 * @ATH12K_DP_WIFI8_IPA2WBM_MAX: Maximum number of ring types
 */
enum ath12k_dp_wifi8_ipa2wbm_ring_e {
	ATH12K_DP_WIFI8_IPA2WBM_IPA,
	ATH12K_DP_WIFI8_IPA2WBM_HOST,
	ATH12K_DP_WIFI8_IPA2WBM_MAX,
};

/**
 * struct ath12k_dp_wifi8_ipa - WiFi8 IPA datapath context
 * @dp_wifi8: Pointer to the WiFi8 datapath structure
 * @ipa2wbm_idle_buf_ring: ring for providing initial rx bufs to IPA WBM pool
 * @ipa2wbm_ring: Array of IPA to WBM rings indexed by ring type
 * @reo2sw5: reo dst ring used by IPA for processing RX packets
 * @ipa2tcl_ring: TX data ring to which IPA enqueues packets
 * @tqm2ipa_ring: TX completion ring from which IPA consumes free buffers
 */
struct ath12k_dp_wifi8_ipa {
	struct ath12k_dp_wifi8 *dp_wifi8;
	struct dp_srng ipa2wbm_idle_buf_ring;
	struct dp_srng ipa2wbm_ring[ATH12K_DP_WIFI8_IPA2WBM_MAX];
	struct dp_srng reo2sw5;
	struct dp_srng ipa2tcl_ring;
	struct dp_srng tqm2ipa_ring;
};

int ath12k_wifi8_dp_ipa_init(struct ath12k_base *ab);

#define ATH12K_DP_IPA_VP_MIN	64
#define ATH12K_DP_IPA_VP_MAX	255
#define ATH12K_DP_IPA_VP_RANGE	(ATH12K_DP_IPA_VP_MAX - ATH12K_DP_IPA_VP_MIN + 1)
/*Since AHVIF_ID starts from 1 to make vp number start from 64 we are substracting 1*/
#define ATH12K_DP_IPA_GET_VP_FROM_AHVIFID(__ahvifid) (__ahvifid + ATH12K_DP_IPA_VP_MIN - 1)
#define ATH12K_DP_IPA_GET_VP_IDX_FROM_VP(__vp) (__vp - ATH12K_DP_IPA_VP_MIN)
#define ATH12K_DP_IPA_VP_NUM_INVALID(vp_num) \
	(((vp_num) < ATH12K_DP_IPA_VP_MIN) || ((vp_num) > ATH12K_DP_IPA_VP_MAX))

/**
 * struct ath12k_dp_ipa_vp_profile - IPA PPE VP entry profile
 * @drop_prec_enable: Whether precedence-drop is enabled for this VP
 * @to_fw_sw: Whether packets should be routed to FW/SW path for this VP
 * @use_ppe_int_pri: Whether to use PPE internal priority for this VP
 * @search_idx_reg_num: Search-index register number used for VP programming
 * @vdev_id: VDEV identifier associated with this VP
 * @bank_id: PPE bank identifier used for VP programming
 * @lmac_id: LMAC identifier corresponding to the radio/PHY
 * @entry_valid: True if this VP profile entry is valid
*/
struct ath12k_ipa_vp_profile {
	u8 drop_prec_enable;
	u8 to_fw_sw;
	u8 use_ppe_int_pri;
	u8 search_idx_reg_num;
	u32 vdev_id;
	u32 bank_id;
	u32 lmac_id;
	bool entry_valid;
};

int ath12k_dp_ipa_alloc_vp_num(struct ath12k_vif *ahvif, struct net_device *dev);
void ath12k_dp_ipa_dealloc_vp_num(struct ath12k_vif *ahvif);
int ath12k_dp_ipa_get_vp_from_dev(struct net_device *dev);
struct net_device *ath12k_dp_ipa_get_netdev_from_vp(u8 vp_num);
void ath12k_dp_ipa_set_ppe_vp_entry_config(struct ath12k_ipa_vp_profile *vp_profile,
					   struct net_device *dev,
					   struct ath12k_link_vif *arvif);
void ath12k_dp_ipa_hal_tx_set_ppe_vp_entry(struct ath12k_base *ab,
					   u8 vp_num,
					   struct ath12k_ipa_vp_profile *vp_profile);
int ath12k_wifi8_dp_rx_ring_setup_ipa(struct ath12k_base *ab);
void ath12k_wifi8_dp_rx_ring_free_ipa(struct ath12k_base *ab);
int ath12k_wifi8_dp_ipa_ipa2wbm_idle_buf_ring_init(struct ath12k_base *ab);
void ath12k_wifi8_dp_fill_rx_src(struct ath12k_base *ab);
int ath12k_wifi8_dp_rx_ipa2wbm_idle_buf_config_qcn9625(struct ath12k_base *ab);
int ath12k_wifi8_dp_tx_ring_setup_ipa(struct ath12k_base *ab);
void ath12k_wifi8_dp_tx_ring_cleanup_ipa(struct ath12k_base *ab);
int ath12k_wifi8_dp_fill_tx_src(struct ath12k_base *ab);
int ath12k_wifi8_dp_ipa_rx_buf_smmu_map_unmap(struct ath12k_base *ab, bool create);
void ath12k_wifi8_dp_ipa_iface_setup_set_vp_num(u8 link_id,
						struct ipa_wdi_reg_intf_in_params *in);
void ath12k_wifi8_dp_ipa_iface_cleanup_vp_num(const char *dev_name, u8 link_id);
void ath12k_wifi8_dp_ipa_iface_setup_set_intf_idx(struct ipa_wdi_reg_intf_in_params *in,
						  int intf_idx);

static inline u8
ath12k_ipa_cb_get_vap_id(const struct ipa_wdi6_rx_skb_cb *cb)
{
	return (u8)((cb->packed >> IPA_WDI6_CB_VAP_ID_SHIFT) &
		    IPA_WDI6_CB_VAP_ID_MASK);
}

static inline u16
ath12k_ipa_cb_get_ta_peer_id(const struct ipa_wdi6_rx_skb_cb *cb)
{
	return (u16)((cb->packed >> IPA_WDI6_CB_TA_PEER_ID_SHIFT) &
		     IPA_WDI6_CB_TA_PEER_ID_MASK);
}

static inline u8
ath12k_ipa_cb_get_hw_link_id(const struct ipa_wdi6_rx_skb_cb *cb)
{
	return (u8)((cb->packed >> IPA_WDI6_CB_HW_LINK_ID_SHIFT) &
		    IPA_WDI6_CB_HW_LINK_ID_MASK);
}

/**
 * ath12k_wifi8_dp_ipa_xmit() - IPA TX bypass entry point
 *
 * Called from ath12k_wifi8_mac_op_tx() for every unicast data frame.
 * Performs eligibility checks, in-flight flow control, and IPA submission.
 *
 * Returns 0 if skb was consumed (IPA success or flow-control drop with
 * queues stopped); negative if skb was not consumed (fall through to TCL).
 */
int ath12k_wifi8_dp_ipa_xmit(struct ath12k *ar, struct sk_buff *skb,
			      const struct ath12k_skb_cb *skb_cb,
			      struct ath12k_vif *ahvif,
			      struct ath12k_sta *ahsta,
			      struct ath12k_link_vif *arvif,
			      bool is_mcast);

void ath12k_wifi8_dp_ipa_debugfs_init(struct dentry *root,
				      struct ath12k_ipa *ipa_ctx);

/* TX stats counters (defined in dp_ipa.c, used in dp_ipa_debugfs.c) */
extern atomic64_t ipa_tx_ipa_count;
extern atomic64_t ipa_tx_sw_count;
extern atomic64_t ipa_tx_qstop_count;
extern atomic64_t ipa_tx_qwake_count;
extern atomic64_t ipa_tx_pm_wake;
extern atomic64_t ipa_tx_busy_count;
extern atomic64_t ipa_tx_total_sw_count;
extern atomic64_t ipa_tx_queue_count[];

#endif /* __ATH12K_WIFI8_EXTNS_DP_IPA_H__ */
