/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _ATH12K_DEBUGFS_H_
#define _ATH12K_DEBUGFS_H_

#include "core.h"

struct ath12k;
#define ATH12K_UDP_TCP_START_PORT		0
#define ATH12K_UDP_TCP_END_PORT			65535
#define ATH12K_RX_FSE_FLOW_MATCH_DEBUGFS	0xBBBB

#define ATH12K_BUF_SIZE_32 32

#define ATH12K_HTT_PEER_STATS_RESET BIT(16)

#define ATH12K_HTT_STATS_BUF_SIZE (1024 * 512)

#define ATH12K_QOS_MAP_LEN_MIN 16

#define SMART_MON_FILTER_V	BIT(0)
#define SMART_MON_FILTER_D	BIT(1)
#define SMART_MON_FILTER_M	BIT(2)
#define SMART_MON_FILTER_C	BIT(3)

#define ATH12K_DBG_RESET_STR "reset"
#define ATH12K_DBG_RESET_STR_LEN sizeof("reset")

enum ath12k_dbg_aggr_mode {
	ATH12K_DBG_AGGR_MODE_AUTO,
	ATH12K_DBG_AGGR_MODE_MANUAL,
	ATH12K_DBG_AGGR_MODE_MAX,
};

void ath12k_wmi_crl_path_stats_list_free(struct ath12k *ar, struct list_head *head);

u32 ath12k_dbg_dump_qos_profile(struct ath12k_base *ab,
				char *buf, u8 qos_id, u32 size);

#define ATH12K_MAX_NRPS 8
#define MAC_UNIT_LEN 3

struct ath12k_neighbor_peer {
	struct list_head list;
	int vdev_id;
	u8 addr[ETH_ALEN];
	u8 rssi;
	u8 avg_rssi;
	s64 timestamp;
	int pdev_id;
};

#ifdef CPTCFG_ATH12K_DEBUGFS
void ath12k_debugfs_soc_create(struct ath12k_base *ab);
void ath12k_debugfs_soc_destroy(struct ath12k_base *ab);
void ath12k_debugfs_register(struct ath12k *ar);
void ath12k_debugfs_unregister(struct ath12k *ar);
void ath12k_hw_debugfs_register(struct ath12k_hw *ah);
void ath12k_debugfs_pdev_destroy(struct ath12k_base *ab);
void ath12k_debugfs_fw_stats_init(struct ath12k *ar);

static inline int ath12k_extd_tx_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return ((dp_pdev->dp_stats_mask & DP_ENABLE_STATS) &&
		(dp_pdev->dp_stats_mask & DP_ENABLE_EXT_TX_STATS));
}

/**
 * ath12k_htt_tx_ppdu_stats_enabled - Check if extended HTT TX stats are enabled
 * @dp_pdev: DP pdev handle
 *
 * Returns true when DP_ENABLE_TX_PPDU_STATS is set. This knob independently
 * controls struct ath12k_htt_tx_ppdu_stats allocation and updates.
 */
static inline bool
ath12k_htt_tx_ppdu_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return ((dp_pdev->dp_stats_mask & DP_ENABLE_STATS) &&
		(dp_pdev->dp_stats_mask & DP_ENABLE_TX_PPDU_STATS));
}

static inline bool ath12k_extd_rx_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return ((dp_pdev->dp_stats_mask &  DP_ENABLE_STATS) &&
		(dp_pdev->dp_stats_mask & DP_ENABLE_EXT_RX_STATS));
}

static inline bool ath12k_dp_rx_ppdu_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return ((dp_pdev->dp_stats_mask & DP_ENABLE_STATS) &&
		(dp_pdev->dp_stats_mask & DP_ENABLE_RX_PPDU_STATS));
}

static inline int ath12k_debugfs_rx_filter(struct ath12k *ar)
{
	return ar->debug.rx_filter;
}

static inline bool
ath12k_dp_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_STATS);
}

static inline bool
ath12k_dp_debug_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_DEBUG_STATS);
}

static inline bool
ath12k_dp_advance_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_ADVANCE_STATS);
}

static inline bool
ath12k_dp_vow_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_VOW_STATS);
}

static inline bool
ath12k_dp_latency_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_LATENCY_STATS);
}

static inline bool
ath12k_dp_qos_msduq_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_QOS_MSDUQ_STATS);
}

static inline bool
ath12k_dp_scs_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_SCS_STATS);
}

static inline bool
ath12k_dp_fetch_reo_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_FETCH_REO_STATS);
}

static inline u8 ath12k_debugfs_is_qos_stats_enabled(struct ath12k *ar)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if ((dp_pdev->dp_stats_mask & (DP_ENABLE_QOS_STATS)))
		return (dp_pdev->qos_stats & ATH12K_QOS_STATS_CATEG_MASK);
	return 0;
}

static inline bool ath12k_tid_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_TID_STATS);
}

static inline bool ath12k_proto_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_PROTO_STATS);
}

void ath12k_tid_tx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason);
void ath12k_tid_tx_drop_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason);
void ath12k_tid_rx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason);
void ath12k_tid_drop_rx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason);
void ath12k_debugfs_nrp_clean(struct ath12k *ar, const u8 *addr, int num_nrp);
void ath12k_debugfs_nrp_cleanup_all(struct ath12k *ar);

void ath12k_debugfs_op_vif_add(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif);
void ath12k_debugfs_pdev_create(struct ath12k_base *ab);

void ath12k_debugfs_add_interface(struct ath12k_link_vif *arvif);
void ath12k_debugfs_remove_interface(struct ath12k_link_vif *arvif);
struct dentry *ath12k_debugfs_erp_create(void);

#define ATH12K_CCK_RATES			4
#define ATH12K_OFDM_RATES			8
#define ATH12K_HT_RATES				8
#define ATH12K_VHT_RATES			12
#define ATH12K_HE_RATES				12
#define ATH12K_HE_RATES_WITH_EXTRA_MCS		14
#define ATH12K_EHT_RATES			16
#define HE_EXTRA_MCS_SUPPORT			GENMASK(31, 16)
#define ATH12K_NSS_1				1
#define ATH12K_NSS_4				4
#define ATH12K_NSS_5				5
#define ATH12K_NSS_8				8
#define ATH12K_HW_NSS(_rcode)			(((_rcode) >> 5) & 0x7)
#define TPC_STATS_WAIT_TIME			(1 * HZ)
#define MAX_TPC_PREAM_STR_LEN			7
#define TPC_INVAL				-128
#define TPC_MAX					127
#define TPC_STATS_WAIT_TIME			(1 * HZ)
#define TPC_STATS_TOT_ROW			700
#define TPC_STATS_TOT_COLUMN			100
#define MODULATION_LIMIT			126

#define ATH12K_TPC_STATS_BUF_SIZE	(TPC_STATS_TOT_ROW * TPC_STATS_TOT_COLUMN)

/*
 * enum qca_wlan_priority_type - priority mask
 * This enum defines priority mask that user can configure
 * over BT traffic type which can be passed through
 * QCA_WLAN_VENDOR_ATTR_BTCOEX_CONFIG_WLAN_PRIORITY attribute.
 *
 * @QCA_WLAN_PRIORITY_BE: Bit mask for WLAN Best effort traffic
 * @QCA_WLAN_PRIORITY_BK: Bit mask for WLAN Background traffic
 * @QCA_WLAN_PRIORITY_VI: Bit mask for WLAN Video traffic
 * @QCA_WLAN_PRIORITY_VO: Bit mask for WLAN Voice traffic
 * @QCA_WLAN_PRIORITY_BEACON: Bit mask for WLAN BEACON frame
 * @QCA_WLAN_PRIORITY_MGMT: Bit mask for WLAN Management frame
 */
enum qca_wlan_priority_type {
	QCA_WLAN_PRIORITY_BE = BIT(0),
	QCA_WLAN_PRIORITY_BK = BIT(1),
	QCA_WLAN_PRIORITY_VI = BIT(2),
	QCA_WLAN_PRIORITY_VO = BIT(3),
	QCA_WLAN_PRIORITY_BEACON = BIT(4),
	QCA_WLAN_PRIORITY_MGMT = BIT(5),
};

#define BTCOEX_ENABLE                    1
#define BTCOEX_DISABLE                   0
#define BTCOEX_CONFIGURE_DEFAULT        -1
#define BTCOEX_THREE_WIRE_MODE           1
#define BTCOEX_PTA_MODE                  2
#define BTCOEX_MAX_PKT_WEIGHT            255
#define BTCOEX_MAX_WLAN_PRIORITY    ((QCA_WLAN_PRIORITY_MGMT << 1) - 1)

enum wmi_tpc_pream_bw {
	WMI_TPC_PREAM_CCK,
	WMI_TPC_PREAM_OFDM,
	WMI_TPC_PREAM_HT20,
	WMI_TPC_PREAM_HT40,
	WMI_TPC_PREAM_VHT20,
	WMI_TPC_PREAM_VHT40,
	WMI_TPC_PREAM_VHT80,
	WMI_TPC_PREAM_VHT160,
	WMI_TPC_PREAM_HE20,
	WMI_TPC_PREAM_HE40,
	WMI_TPC_PREAM_HE80,
	WMI_TPC_PREAM_HE160,
	WMI_TPC_PREAM_EHT20,
	WMI_TPC_PREAM_EHT40,
	WMI_TPC_PREAM_EHT60,
	WMI_TPC_PREAM_EHT80,
	WMI_TPC_PREAM_EHT120,
	WMI_TPC_PREAM_EHT140,
	WMI_TPC_PREAM_EHT160,
	WMI_TPC_PREAM_EHT200,
	WMI_TPC_PREAM_EHT240,
	WMI_TPC_PREAM_EHT280,
	WMI_TPC_PREAM_EHT320,
	/* UHR preambles: FW sends one shared EHT/UHR rates array;
	 * host indexes into rates_array2 identically to EHT.
	 */
	WMI_TPC_PREAM_UHR20,
	WMI_TPC_PREAM_UHR40,
	WMI_TPC_PREAM_UHR60,
	WMI_TPC_PREAM_UHR80,
	WMI_TPC_PREAM_UHR120,
	WMI_TPC_PREAM_UHR140,
	WMI_TPC_PREAM_UHR160,
	WMI_TPC_PREAM_UHR200,
	WMI_TPC_PREAM_UHR240,
	WMI_TPC_PREAM_UHR280,
	WMI_TPC_PREAM_UHR320,
	WMI_TPC_PREAM_MAX
};

enum ath12k_debug_tpc_stats_ctl_mode {
	ATH12K_TPC_STATS_CTL_MODE_LEGACY_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HT_VHT20_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HE_EHT20_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HT_VHT40_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HE_EHT40_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_VHT80_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HE_EHT80_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_VHT160_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HE_EHT160_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HE_EHT320_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_CCK_2GHZ,
	ATH12K_TPC_STATS_CTL_MODE_LEGACY_2GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HT20_2GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HT40_2GHZ,

	ATH12K_TPC_STATS_CTL_MODE_EHT80_SU_PUNC20 = 23,
	ATH12K_TPC_STATS_CTL_MODE_EHT160_SU_PUNC20,
	ATH12K_TPC_STATS_CTL_MODE_EHT320_SU_PUNC40,
	ATH12K_TPC_STATS_CTL_MODE_EHT320_SU_PUNC80,
	ATH12K_TPC_STATS_CTL_MODE_EHT320_SU_PUNC120
};

enum ath12k_debug_tpc_stats_support_modes {
	ATH12K_TPC_STATS_SUPPORT_160 = 0,
	ATH12K_TPC_STATS_SUPPORT_320,
	ATH12K_TPC_STATS_SUPPORT_AX,
	ATH12K_TPC_STATS_SUPPORT_AX_EXTRA_MCS,
	ATH12K_TPC_STATS_SUPPORT_BE,
	ATH12K_TPC_STATS_SUPPORT_BE_PUNC,
};

struct ath12k_pktlog_hdr {
        u16 flags;
        u16 missed_cnt;
        u16 log_type;
        u16 size;
        u32 timestamp;
        u32 type_specific_data;
        struct mlo_timestamp m_timestamp;
        u8 payload[];
} __packed;
#else

static inline void ath12k_debugfs_pdev_destroy(struct ath12k_base *ab)
{
}

static inline void ath12k_debugfs_fw_stats_init(struct ath12k *ar)
{
}

static inline void ath12k_debugfs_soc_create(struct ath12k_base *ab)
{
}

static inline void ath12k_debugfs_soc_destroy(struct ath12k_base *ab)
{
}

static inline void ath12k_debugfs_register(struct ath12k *ar)
{
}

static inline void ath12k_debugfs_unregister(struct ath12k *ar)
{
}

static inline void ath12k_hw_debugfs_register(struct ath12k_hw *ah)
{
}

static inline int ath12k_extd_tx_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return 0;
}

static inline bool
ath12k_htt_tx_ppdu_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return false;
}

static inline bool ath12k_extd_rx_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return false;
}

static inline bool ath12k_dp_rx_ppdu_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return false;
}

static inline int ath12k_debugfs_rx_filter(struct ath12k *ar)
{
	return 0;
}

static inline void ath12k_debugfs_set_rx_filter(struct ath12k *ar, u32 rx_filter)
{
}

static inline void ath12k_debugfs_op_vif_add(struct ieee80211_hw *hw,
					     struct ieee80211_vif *vif)
{
}

static inline void ath12k_debugfs_pdev_create(struct ath12k_base *ab)
{
}

static inline void ath12k_debugfs_add_interface(struct ath12k_link_vif *arvif)
{
}

static inline void ath12k_debugfs_remove_interface(struct ath12k_link_vif *arvif)
{
}

static inline bool
ath12k_dp_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return false;
}

static inline bool
ath12k_dp_debug_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return false;
}

static inline bool
ath12k_tid_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return false;
}

static inline bool
ath12k_dp_qos_msduq_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return false;
}

static inline bool
ath12k_dp_scs_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return false;
}

static inline bool
ath12k_dp_fetch_reo_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return false;
}

static inline u8 ath12k_debugfs_is_qos_stats_enabled(struct ath12k *ar)
{
	return 0;
}

static inline
void ath12k_debugfs_nrp_clean(struct ath12k *ar, const u8 *addr, int num_nrp)
{
}

static inline void ath12k_debugfs_nrp_cleanup_all(struct ath12k *ar)
{
}

struct dentry *ath12k_debugfs_erp_create(void)
{
       return NULL;
}

/**
 * struct ath12k_debugfs_ctx - inode->i_private for every file created
 *                             via ath12k_debugfs_create_file().
 * @ab:   base device pointer used for the is_bypassed pre-check.
 * @data: original per-file private pointer forwarded to read/write.
 */
#endif /* CPTCFG_ATH12K_DEBUGFS */

void ath12k_init_pktlog(struct ath12k *ar);

void ath12k_deinit_pktlog(struct ath12k *ar);
void ath12k_htt_pktlog_process(struct ath12k *ar, u8 *data, u32 len);
void ath12k_htt_ppdu_pktlog_process(struct ath12k *ar, u8 *data, u32 len);
void ath12k_dp_txrx_stats_buf_pktlog_process(struct ath12k *ar, u8 *data,
					     u16 log_type, u32 len);
void ath12k_cbf_pktlog_process(struct ath12k *ar, u8 *data, u32 len,
			       struct htt_t2h_ppdu_stats_ind_hdr *htt_hdr,
			       struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv *cbf_tlv);
/**
 * struct ath12k_debugfs_ctx - inode->i_private for every file created
 *                             via ath12k_debugfs_create_file().
 * @ab:   base device pointer used for the is_bypassed pre-check.
 * @data: original per-file private pointer forwarded to read/write.
 */
struct ath12k_debugfs_ctx {
	struct ath12k_base *ab;
	void               *data;
};

int ath12k_debugfs_open(struct inode *inode, struct file *file);
struct dentry *ath12k_debugfs_create_file(const char *name, umode_t mode,
					  struct dentry *parent,
					  struct ath12k_base *ab, void *data,
					  const struct file_operations *fops);

#endif /* _ATH12K_DEBUGFS_H_ */
