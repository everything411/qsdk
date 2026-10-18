/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_H
#define ATH12K_DP_H

#include "core.h"
#include "hw.h"
#include "hal.h"
#include "dp_htt.h"
#include "dp_cmn.h"
#include "ppe.h"
#include <linux/errno.h>
#include "dp_stats.h"
#include "dp_htt_logger.h"
#include "dp_ext_desc.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ini.h"
#endif /* CPTCFG_QCN_EXTN */

#define DP_TX_MONITOR			(ab->mem_params.tx_monitor_support)
#define DP_PPE_WBM2SW_RING_SIZE		(ab->mem_params.dp_params.ppe_wbm2sw_ring_size)
#define DP_TQM2PPE_RING_SIZE		(ab->mem_params.dp_params.tqm2ppe_ring_size)
#define DP_REO2PPE_RING_SIZE		(ab->mem_params.dp_params.reo2ppe_ring)
#define DP_PPE2TCL_RING_SIZE		(ab->mem_params.dp_params.ppe2tcl_ring)
#define DP_RX_RELEASE_RING_SIZE		(ab->mem_params.dp_params.rx_release_ring_size)
#define DP_RXDMA_BUF_RING_SIZE		(ab->mem_params.dp_params.rxdma_buf_ring_size)
#define DP_TX_COMP_PPEDS_RING_SIZE	(ab->mem_params.dp_params.tx_comp_ppeds_ring_size)
#define ATH12K_RX_DESC_COUNT		(ab->mem_params.dp_params.rx_desc_count)
#define DP_NUM_CLIENTS_MAX		(ab->mem_params.dp_params.dp_num_clients_max)

#define DP_REO_DST_RING0_SIZE		(ab->mem_params.dp_params.reo_dst_ring_size[0])
#define DP_REO_DST_RING1_SIZE		(ab->mem_params.dp_params.reo_dst_ring_size[1])
#define DP_REO_DST_RING2_SIZE		(ab->mem_params.dp_params.reo_dst_ring_size[2])
#define DP_REO_DST_RING3_SIZE		(ab->mem_params.dp_params.reo_dst_ring_size[3])
#define DP_REO_DST_RING4_SIZE		(ab->mem_params.dp_params.reo_dst_ring_size[4])

#define DP_TCL_DATA_RING0_SIZE		(ab->mem_params.dp_params.tcl_data_ring_size[0])
#define DP_TCL_DATA_RING1_SIZE		(ab->mem_params.dp_params.tcl_data_ring_size[1])
#define DP_TCL_DATA_RING2_SIZE		(ab->mem_params.dp_params.tcl_data_ring_size[2])
#define DP_TCL_DATA_RING3_SIZE		(ab->mem_params.dp_params.tcl_data_ring_size[3])
#define DP_TCL_DATA_RING4_SIZE		(ab->mem_params.dp_params.tcl_data_ring_size[4])

#define DP_TX_COMP_RING0_SIZE		(ab->mem_params.dp_params.tx_compl_ring_size[0])
#define DP_TX_COMP_RING1_SIZE		(ab->mem_params.dp_params.tx_compl_ring_size[1])
#define DP_TX_COMP_RING2_SIZE		(ab->mem_params.dp_params.tx_compl_ring_size[2])
#define DP_TX_COMP_RING3_SIZE		(ab->mem_params.dp_params.tx_compl_ring_size[3])
#define DP_TX_COMP_RING4_SIZE		(ab->mem_params.dp_params.tx_compl_ring_size[4])

#define DP_TX_DESC_COUNT_POOL0		(ab->mem_params.dp_params.tx_desc_count[0])
#define DP_TX_DESC_COUNT_POOL1		(ab->mem_params.dp_params.tx_desc_count[1])
#define DP_TX_DESC_COUNT_POOL2		(ab->mem_params.dp_params.tx_desc_count[2])
#define DP_TX_DESC_COUNT_POOL3		(ab->mem_params.dp_params.tx_desc_count[3])
#define DP_TX_DESC_COUNT_POOL4		(ab->mem_params.dp_params.tx_desc_count[4])

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#define ATH12K_NUM_POOL_PPEDS_TX_DESC_DEFAULT \
	(ab->mem_params.dp_params.num_pool_ppeds_tx_desc)
#define ATH12K_PPEDS_HOTLIST_LEN_MAX_DEFAULT \
	(ab->mem_params.dp_params.ppeds_hotlist_len_max)
#endif

#define ATH12K_NUM_RX_SPT_PAGES \
	(ATH12K_RX_DESC_COUNT / ATH12K_MAX_SPT_ENTRIES)

extern u32 ath12k_dp_tx_desc_count[DP_TCL_NUM_RING_MAX];
extern u32 ath12k_dp_num_ppeds_tx_spt_pages;

/**
 * ath12k_dp_tx_spt_pages_per_pool - SPT pages needed for a given TX pool
 * @pool_id: TX pool index (0 .. ATH12K_HW_MAX_QUEUES-1)
 *
 * Returns the number of SPT pages required for @pool_id, derived from
 * ath12k_dp_tx_desc_count[pool_id] (initialized at DP setup time).
 */
static inline u32 ath12k_dp_tx_spt_pages_per_pool(u32 pool_id)
{
	return ath12k_dp_tx_desc_count[pool_id] / ATH12K_MAX_SPT_ENTRIES;
}

/**
 * ath12k_dp_tx_spt_page_offset - flat SPT page index of the first page for a pool
 * @pool_id: TX pool index
 *
 * Returns the cumulative SPT page offset for @pool_id, i.e. the sum of SPT
 * pages for all pools with index < @pool_id.
 */
static inline u32 ath12k_dp_tx_spt_page_offset(u32 pool_id)
{
	u32 offset = 0, i;

	for (i = 0; i < pool_id; i++)
		offset += ath12k_dp_tx_spt_pages_per_pool(i);

	return offset;
}

/**
 * ath12k_dp_rx_spt_page_offset - PPT index of the first RX SPT page
 *
 * The RX SPT region starts immediately after the PPEDS TX pages and the
 * per-pool TX SPT pages.  Uses the runtime total so that variable per-pool
 * tx_desc_count values are accounted for correctly.
 */
static inline u32 ath12k_dp_rx_spt_page_offset(void)
{
	return ath12k_dp_num_ppeds_tx_spt_pages +
		ath12k_dp_tx_spt_page_offset(ATH12K_HW_MAX_QUEUES);
}

/* Macros parsing INI */
#ifndef CPTCFG_QCN_EXTN
#define DP_UMAC_RESET_TIMEOUT_IN_MS	1000
#define DP_REO_EXCEPTION_RING_SIZE	128
#define DP_REO_REINJECT_RING_SIZE	32
#define DP_REO_STATUS_RING_SIZE		2048
#define DP_REO_CMD_RING_SIZE		256
#define DP_TCL_STATUS_RING_SIZE	32
#define DP_WBM_RELEASE_RING_SIZE	64
#define DP_RXDMA_ERR_DST_RING_SIZE	1024
#define DP_RXDMA_REFILL_RING_SIZE	2048
#define DP_HTT_LOGGING_ENABLE	false
#define HAL_SRNG_INT_BATCH_THRESHOLD_OTHER	1
#define HAL_SRNG_INT_BATCH_THRESHOLD_PPE2TCL 0
#define HAL_SRNG_INT_BATCH_THRESHOLD_RX 128
#define HAL_SRNG_INT_BATCH_THRESHOLD_TX 256
#define HAL_SRNG_INT_TIMER_THRESHOLD_OTHER 256
#define HAL_SRNG_INT_TIMER_THRESHOLD_PPE2TCL 3
#define HAL_SRNG_INT_TIMER_THRESHOLD_RX 200
#define HAL_SRNG_INT_TIMER_THRESHOLD_TX 1000
#define DP_UMCMN_INTR_HANDLING_DISABLE false
#define DP_STATS_MASK		0x00000001

#else /* CPTCFG_QCN_EXTN */
#define ATH12K_DP_INI_GET(__ini__)	ath12k_cfg_get(ab, ATH12K_INI_DP_##__ini__)

#define DP_UMAC_RESET_TIMEOUT_IN_MS	ATH12K_DP_INI_GET(UMAC_RESET_TIMEOUT)
#define DP_REO_EXCEPTION_RING_SIZE	ATH12K_DP_INI_GET(REO_EXCEPTION_RING_SIZE)
#define DP_REO_REINJECT_RING_SIZE	ATH12K_DP_INI_GET(REO_REINJECT_RING_SIZE)
#define DP_REO_STATUS_RING_SIZE		ATH12K_DP_INI_GET(REO_STATUS_RING_SIZE)
#define DP_REO_CMD_RING_SIZE		ATH12K_DP_INI_GET(REO_CMD_RING_SIZE)
#define DP_TCL_STATUS_RING_SIZE		ATH12K_DP_INI_GET(TCL_STATUS_RING_SIZE)
#define DP_WBM_RELEASE_RING_SIZE	ATH12K_DP_INI_GET(WBM_RELEASE_RING_SIZE)
#define DP_RXDMA_ERR_DST_RING_SIZE	ATH12K_DP_INI_GET(RXDMA_ERR_DST_RING_SIZE)
#define DP_RXDMA_REFILL_RING_SIZE	ATH12K_DP_INI_GET(RXDMA_REFILL_RING_SIZE)
#define DP_HTT_LOGGING_ENABLE		ATH12K_DP_INI_GET(HTT_LOGGING_ENABLE)

#define HAL_SRNG_INT_BATCH_THRESHOLD_OTHER \
	ATH12K_DP_INI_GET(HAL_SRNG_INT_BATCH_THRESHOLD_OTHER)
#define HAL_SRNG_INT_BATCH_THRESHOLD_PPE2TCL \
	ATH12K_DP_INI_GET(HAL_SRNG_INT_BATCH_THRESHOLD_PPE2TCL)
#define HAL_SRNG_INT_BATCH_THRESHOLD_RX \
	ATH12K_DP_INI_GET(HAL_SRNG_INT_BATCH_THRESHOLD_RX)
#define HAL_SRNG_INT_BATCH_THRESHOLD_TX \
	ATH12K_DP_INI_GET(HAL_SRNG_INT_BATCH_THRESHOLD_TX)
#define HAL_SRNG_INT_TIMER_THRESHOLD_OTHER \
	ATH12K_DP_INI_GET(HAL_SRNG_INT_TIMER_THRESHOLD_OTHER)
#define HAL_SRNG_INT_TIMER_THRESHOLD_PPE2TCL \
	ATH12K_DP_INI_GET(HAL_SRNG_INT_TIMER_THRESHOLD_PPE2TCL)
#define HAL_SRNG_INT_TIMER_THRESHOLD_RX \
	ATH12K_DP_INI_GET(HAL_SRNG_INT_TIMER_THRESHOLD_RX)
#define HAL_SRNG_INT_TIMER_THRESHOLD_TX \
	ATH12K_DP_INI_GET(HAL_SRNG_INT_TIMER_THRESHOLD_TX)

#define DP_UMCMN_INTR_HANDLING_DISABLE \
	ATH12K_DP_INI_GET(UMCMN_INTR_HANDLING_DISABLE)
#define DP_STATS_MASK		ATH12K_DP_INI_GET(STATS_MASK)

#endif /* CPTCFG_QCN_EXTN*/

#ifdef CPTCFG_EXT_IPA_OFFLOAD
#undef DP_TCL_DATA_RING0_SIZE
#undef DP_TCL_DATA_RING1_SIZE
#undef DP_TCL_DATA_RING2_SIZE
#undef DP_TCL_DATA_RING3_SIZE
#undef DP_TCL_DATA_RING4_SIZE
#define DP_TCL_DATA_RING0_SIZE		8192
#define DP_TCL_DATA_RING1_SIZE		8192
#define DP_TCL_DATA_RING2_SIZE		8192
#define DP_TCL_DATA_RING3_SIZE		8192
#define DP_TCL_DATA_RING4_SIZE		8192

#undef DP_TX_COMP_RING0_SIZE
#undef DP_TX_COMP_RING1_SIZE
#undef DP_TX_COMP_RING2_SIZE
#undef DP_TX_COMP_RING3_SIZE
#undef DP_TX_COMP_RING4_SIZE
#define DP_TX_COMP_RING0_SIZE		8192
#define DP_TX_COMP_RING1_SIZE		8192
#define DP_TX_COMP_RING2_SIZE		8192
#define DP_TX_COMP_RING3_SIZE		8192
#define DP_TX_COMP_RING4_SIZE		8192
#endif /* CPTCFG_EXT_IPA_OFFLOAD */

#define HTT_TCL_META_DATA_PEER_ID_MISSION       GENMASK(15, 3)

/* QoS meta data */
#define HTT_TCL_META_DATA_TYPE_SVC_ID_BASED	2
#define HTT_TCL_META_DATA_SAWF_SVC_ID		GENMASK(10, 3)
#define HTT_TCL_META_DATA_SAWF_TID_OVERRIDE	BIT(12)

#define HTT_TCL_META_DATA_TYPE_MISSION		GENMASK(1, 0)

#define MAX_RXDMA_PER_PDEV     2

#define MAX_NAPI_BUDGET             128
#define TX_STATUS_ENTRY_MAX_SIZE    64
#define TX_STATUS_BUFFER_SIZE       (TX_STATUS_ENTRY_MAX_SIZE * MAX_NAPI_BUDGET)

#define RX_STATUS_ENTRY_MAX_SIZE    64
#define RX_STATUS_BUFFER_SIZE       (RX_STATUS_ENTRY_MAX_SIZE * MAX_NAPI_BUDGET)

#define TX_NAPI_BUDGET             127

struct ath12k_base;
struct ath12k_hw;
struct ath12k_dp_link_peer;
struct ath12k_dp;
struct ath12k_vif;
struct ath12k_link_vif;
struct ath12k_dp_vif;
struct hal_tcl_status_ring;
struct ath12k_ext_irq_grp;
struct ath12k_dp_rx_tid;
struct ath12k_hal_reo_cmd;
struct hal_reo_dest_ring;
struct hal_reo_status;
enum hal_wbm_rel_bm_act;
struct dp_rx_fst;
struct ath12k_dp_mon;
struct ath12k_pdev_mon_dp;
struct ath12k_hp_update_timer;
struct peer_assoc_flowq_params;
struct peer_assoc_holq_params;
struct ath12k_dp_vif;
struct ath12k_dp_link_vif;

#define DP_MON_PURGE_TIMEOUT_MS     100
#define DP_MON_SERVICE_BUDGET       128
#define MAX_TCL_RING		MIN(NR_CPUS, 5)
#define MAX_TX_COMP_RING	MIN(NR_CPUS, 5)

struct dp_rxdma_ring {
	struct dp_srng refill_buf_ring;
	int bufs_max;
};

struct dp_tx_ring {
	u8 tcl_data_ring_id;
	struct dp_srng tcl_data_ring;
	struct dp_srng tcl_comp_ring;
};

struct dp_link_desc_bank {
	void *vaddr_unaligned;
	void *vaddr;
	dma_addr_t paddr_unaligned;
	dma_addr_t paddr;
	u32 size;
};

/* Size to enforce scatter idle list mode */
#define DP_LINK_DESC_ALLOC_SIZE_THRESH 0x200000
#define DP_LINK_DESC_BANKS_MAX 8

#define DP_LINK_DESC_START	0x4000
#define DP_LINK_DESC_SHIFT	3

#define DP_LINK_DESC_COOKIE_SET(id, page) \
	((((id) + DP_LINK_DESC_START) << DP_LINK_DESC_SHIFT) | (page))

#define DP_LINK_DESC_BANK_MASK	GENMASK(2, 0)

#define DP_RX_DESC_COOKIE_INDEX_MAX		0x3ffff
#define DP_RX_DESC_COOKIE_POOL_ID_MAX		0x1c0000
#define DP_RX_DESC_COOKIE_MAX	\
	(DP_RX_DESC_COOKIE_INDEX_MAX | DP_RX_DESC_COOKIE_POOL_ID_MAX)
#define DP_NOT_PPDU_ID_WRAP_AROUND 20000

enum ath12k_dp_ppdu_state {
	DP_PPDU_STATUS_START,
	DP_PPDU_STATUS_DONE,
};

enum ath12k_dp_op_type {
	ATH12K_DP_OP_INIT,
	ATH12K_DP_OP_DEINIT,
	ATH12K_DP_OP_UPDATE,
	ATH12K_DP_OP_INVALID
};

struct ath12k_wmm_stats {
       int tx_type;
       int rx_type;
       u64 total_wmm_tx_pkts[WME_NUM_AC];
       u64 total_wmm_rx_pkts[WME_NUM_AC];
       u64 total_wmm_tx_drop[WME_NUM_AC];
       u64 total_wmm_rx_drop[WME_NUM_AC];
};

/**
 * struct ath12k_pdev_telemetry_stats - Per-pdev control-path telemetry statistics
 *
 * Aggregates radio-level statistics exported via vendor telemetry events.
 * Fields cover airtime utilisation per access category, TX/RX data throughput
 * counters, association state, and RX error metrics.  The structure is
 * populated incrementally in the RX and TX data paths and is periodically
 * consumed by the telemetry agent for userspace reporting.
 *
 * @link_airtime:        Total link airtime consumed per AC (microseconds)
 * @tx_link_airtime:     TX link airtime per AC (microseconds)
 * @rx_link_airtime:     RX link airtime per AC (microseconds)
 * @tx_data_msdu_cnt:    Number of TX data MSDUs successfully transmitted
 * @total_tx_data_bytes: Total TX data payload bytes
 * @rx_data_msdu_cnt:    Number of RX data MSDUs received
 * @total_rx_data_bytes: Total RX data payload bytes
 * @time_last_assoc:     Timestamp of the most recent station association
 * @sta_vap_exist:       Non-zero if at least one STA VAP is active on this pdev
 * @rx_bar_cnt:          BAR frames received; counted before peer lookup so
 *                       frames with unknown peer_id are not missed
 * @rx_probe_req_bc:     Broadcast probe requests received; incremented once per
 *                       frame rather than once per VAP to avoid O(n) VAP
 *                       iteration in high-density (16+ VAP) deployments
 * @rx_decrypt_err:      RX decryption error count
 * @rx_mic_err:          RX MIC (TKIP) error count
 * @rx_over_run:         RX FIFO overrun error count
 * @rx_crc_err:          RX FCS/CRC error count (from WMI MIB stats)
 */
struct ath12k_pdev_telemetry_stats {
       u32 link_airtime[WLAN_MAX_AC];
       u32 tx_link_airtime[WLAN_MAX_AC];
       u32 rx_link_airtime[WLAN_MAX_AC];
	u64 tx_data_msdu_cnt;
	u64 total_tx_data_bytes;
	u64 rx_data_msdu_cnt;
	u64 total_rx_data_bytes;
	u64 time_last_assoc;
	u8 sta_vap_exist;
	/* Pdev-level BAR frame counter (counted before peer lookup so
	 * frames with peer_id=0/invalid are not missed).
	 */
	u32 rx_bar_cnt;
	/* Pdev-level broadcast probe request counter - incremented once per
	 * broadcast probe rather than once per VAP, avoiding O(n) VAP iteration
	 * on every broadcast probe in high-density (16+ VAP) environments.
	 */
	u32 rx_probe_req_bc;
	/* Broadcast probe requests suppressed by the per-STA rate-limit gate */
	u32 rx_bc_prb_req_drop;
	u32 rx_decrypt_err;
	u32 rx_mic_err;
	u32 rx_over_run;
	u32 rx_crc_err;
};

struct ath12k_atf_pdev_airtime {
	u32 tx_airtime_consumption[WME_NUM_AC];
	u32 rx_airtime_consumption[WME_NUM_AC];
};

struct ath12k_htt_ppdu_stats {
	u64 ppdu_stat_list_depth;
	u64 delayed_ba_not_recvd;
	/* ppdu_id of last received Tx HTT PPDU */
	u32 last_ppdu_id;
	bool last_ppdu_buf_drop;
	u64 ppdu_wrap_drop;
};

/* CCE protocol tag map — one entry per ath12k_routing_pkt_type (see dp_rx.h) */
struct ath12k_rx_protocol_tag_map {
	u16  tag;
	bool enabled;
};

/* FSE/CCE per-pdev statistics */
struct ath12k_fse_cce_stats {
	u64 cce_tagged_pkts[ATH12K_PKT_TYPE_MAX]; /* indexed by ath12k_routing_pkt_type */
	u64 cce_drop_pkts;
	u64 fse_tagged_pkts;
	u64 fse_drop_pkts;
	u64 fse_new_flow_pkts;
	u64 reo_err_cce_drop;
	u64 wbm_reo_cce_drop;
	u64 wbm_rxdma_fse_drop;
};

struct ath12k_pdev_dp_stats {
	struct ath12k_pdev_telemetry_stats telemetry_stats;
	struct ath12k_atf_pdev_airtime atf_airtime;
	struct ath12k_htt_ppdu_stats ppdu_list_stats;
	/* Add other new stats if required */
};

struct ath12k_pdev_dp {
	u32 mac_id;
	atomic_t num_tx_pending;
	wait_queue_head_t tx_empty_waitq;

	struct ath12k_dp *dp;
	struct ieee80211_hw *hw;

	/*Debug mask to set different level of stats*/
	u32 dp_stats_mask;

	u8 hw_link_id;
	struct ath12k *ar;
	struct ath12k_dp_hw *dp_hw;

	/* Protects ppdu stats */
	spinlock_t ppdu_list_lock;
	struct list_head ppdu_stats_info;

	bool dp_mon_pdev_configured;
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_wmm_stats wmm_stats;
	struct ath12k_dp_pdev_tid_stats tid_stats;
	u32 prev_rx_timestamp;
	/* Protected by ab: base lock
	 * determine when this stats is calculated based on peers
	 */
	struct ath12k_pdev_dp_stats stats;
	u8 qos_stats;
	/*Neighbors Peer count per pdev*/
	int num_nrps;
	u32 prev_tx_enq_tstamp;

	/* CCE protocol tag map: indexed by ath12k_routing_pkt_type */
	struct ath12k_rx_protocol_tag_map protocol_tag_map[ATH12K_PKT_TYPE_MAX];
	u8 protocol_tag_active_count;

	/* FSE/CCE statistics */
	struct ath12k_fse_cce_stats fse_cce_stats;
};

#define EAPOL_WPA_KEY_INFO_KEY_TYPE		BIT(3)
#define EAPOL_WPA_KEY_INFO_ACK			BIT(7)
#define EAPOL_WPA_KEY_INFO_MIC			BIT(8)
#define EAPOL_WPA_KEY_INFO_ENCR_KEY_DATA	BIT(12) /* IEEE 802.11i/RSN only */

#define EAPOL_PACKET_TYPE_OFFSET                1
#define EAPOL_KEY_INFO_OFFSET                   5
#define EAPOL_KEY_DATA_LENGTH_OFFSET            97
#define EAPOL_WPA_KEY_NONCE_OFFSET              17
#define EAPOL_PACKET_TYPE_KEY                   3
#define LLC_SNAP_HDR_LEN			8

enum ath12k_dp_eapol_key_type {
	DP_EAPOL_KEY_TYPE_M1 = 1,
	DP_EAPOL_KEY_TYPE_M2,
	DP_EAPOL_KEY_TYPE_M3,
	DP_EAPOL_KEY_TYPE_M4,
	DP_EAPOL_KEY_TYPE_G1,
	DP_EAPOL_KEY_TYPE_G2,

	DP_EAPOL_KEY_TYPE_MAX,
};

#define DP_AVG_TIDS_PER_CLIENT 2
#define DP_NUM_TIDS_MAX (DP_NUM_CLIENTS_MAX * DP_AVG_TIDS_PER_CLIENT)
#define DP_AVG_MSDUS_PER_FLOW 128
#define DP_AVG_FLOWS_PER_TID 2
#define DP_AVG_MPDUS_PER_TID_MAX 128
#define DP_AVG_MSDUS_PER_MPDU 4

#ifdef CPTCFG_EXT_IPA_OFFLOAD
#define DP_RX_HASH_ENABLE	0 /* Disable hash based Rx steering for IPA*/
#else
#define DP_RX_HASH_ENABLE	1 /* Enable hash based Rx steering */
#endif

#define DP_BA_WIN_SZ_MAX	1024
#define DP_IDLE_SCATTER_BUFS_MAX 16
#define ATH12K_NUM_EAPOL_RESERVE       1024
#define DP_RX_ASE_CMD_RING_SIZE		32
#define DP_RX_ASE_STATUS_RING_SIZE		256
#define DP_TCL_CMD_RING_SIZE		32
#define DP_TQM_CMD_RING_SIZE		16384
#define DP_TQM_STATUS_RING_SIZE		2048
#define DP_REO_ROAMING_RING_SIZE	2048
#ifdef CPTCFG_EXT_IPA_OFFLOAD
#define DP_RX_MAC_BUF_RING_SIZE		8192
#else
#define DP_RX_MAC_BUF_RING_SIZE		2048
#endif
#define DP_SAM_CMD_RING_SIZE		256
#define DP_SAM_STATUS_RING_SIZE		256

#define DP_RX_BUFFER_SIZE_LITE	1024
#define DP_RX_BUFFER_ALIGN_SIZE	128

#define HAL_REO2PPE_DST_IND 6

#define DP_RXDMA_BUF_COOKIE_BUF_ID	GENMASK(17, 0)
#define DP_RXDMA_BUF_COOKIE_PDEV_ID	GENMASK(19, 18)

#define DP_HW2SW_MACID(mac_id) ({ typeof(mac_id) x = (mac_id); x ? x - 1 : 0; })
#define DP_SW2HW_MACID(mac_id) ((mac_id) + 1)

#define DP_TX_DESC_ID_MAC_ID  GENMASK(1, 0)
#define DP_TX_DESC_ID_MSDU_ID GENMASK(18, 2)
#define DP_TX_DESC_ID_POOL_ID GENMASK(20, 19)

#define ATH12K_SHADOW_DP_TIMER_INTERVAL 20
#define ATH12K_SHADOW_CTRL_TIMER_INTERVAL 10

#define DP_REO_QREF_NUM		GENMASK(31, 16)
#define DP_MAX_PEER_ID		2047
#define ATH12K_PEER_ID_INVALID	0x3FFF

#define DP_TCL_ENCAP_TYPE_MAX	4

/* Invalid TX Bank ID value */
#define DP_INVALID_BANK_ID -1

/* Tx Desc Flag bit definations */
#define DP_TX_DESC_FLAG_FAST	0x1
#define DP_TX_DESC_FLAG_MCAST	0x2
#define DP_TX_DESC_FLAG_BCAST	0x4
#define DP_TX_DESC_FLAG_RECYCLE	0x8

#define MAX_TQM_RELEASE_REASON 29
#define MAX_FW_TX_STATUS 7

extern u32 ath12k_dp_tx_comp_ring_size[DP_TCL_NUM_RING_MAX];

extern u32 ath12k_dp_tcl_data_ring_size[DP_TCL_NUM_RING_MAX];

extern u32 ath12k_dp_reo_dst_ring_size[DP_REO_DST_RING_MAX];

struct ath12k_dp_tx_bank_profile {
	u8 is_configured;
	u32 num_users;
	u32 bank_config;
};


struct ath12k_rx_desc_info {
	struct list_head list;
	dma_addr_t paddr;
	u8 *vaddr;
	u32 cookie;
	u8 in_use	: 1,
	   device_id	: 3,
	   is_frag	: 1,
	   is_ppe_desc	: 1,
	   reserved	: 2;
	struct sk_buff *skb;
	u32 magic;
	u64 rsvd0;
} __packed __aligned(64);

static_assert(sizeof(struct ath12k_rx_desc_info) == 64,
	      "ath12k_rx_desc_info size != 64");

struct ath12k_tx_desc_info {
	struct list_head list;
	struct sk_buff *skb;
	union {
		struct sk_buff *skb_ext_desc;
		struct ath12k_dp_ext_desc *ext_desc;
	};
	dma_addr_t paddr;
	dma_addr_t paddr_ext_desc;
	u32 desc_id; /* Cookie */
	u16 len;
	u16 ext_desc_len;
	u32 hw_enqueue_tstamp;
	u8 hw_link_id	: 5,
	   in_use	: 1,
	   ext_kmem	: 1,
	   mmesh	: 1;
	u8 flags	: 4,
	   spl_desc	: 1,
	   is_from_sg   : 1,
	   reserved1	: 1,
	   to_fw	: 1;
	u8 pool_id;
} __packed __aligned(64);

static_assert(sizeof(struct ath12k_tx_desc_info) == 64,
	      "ath12k_tx_desc_info size != 64");

struct ath12k_ppeds_tx_desc_info {
	union {
		u8 align[64];
		struct {
			struct list_head list;
			struct sk_buff *skb;
			dma_addr_t paddr;
			u32 desc_id; /* Cookie */
			bool in_use;
			u8 mac_id;
			u8 pool_id;
			u8 flags;
			u8 device_id;
		};
	};
};

struct ath12k_dp_tx_comp_status {
	int buf_rel_source;
	struct ath12k_ppeds_tx_desc_info *tx_desc;
	u32 desc_id;
	union {
		int htt_status;
		int tqm_status;
	} u;
};

struct ath12k_spt_info {
	dma_addr_t paddr;
	u64 *vaddr;
};

struct ath12k_reo_queue_ref {
	u32 info0;
	u32 info1;
} __packed;

struct ath12k_reo_q_addr_lut {
	u32 *vaddr_unaligned;
	u32 *vaddr;
	dma_addr_t paddr_unaligned;
	dma_addr_t paddr;
	u32 size;
};

struct ath12k_link_stats {
	u32 tx_enqueued;
	u32 tx_completed;
	u32 tx_bcast_mcast;
	u32 tx_dropped;
	u32 tx_errors;
	u32 rx_errors;
	u32 rx_dropped;
	u32 tx_encap_type[DP_TCL_ENCAP_TYPE_MAX];
	u32 tx_encrypt_type[HAL_ENCRYPT_TYPE_MAX];
	u32 tx_desc_type[DP_TCL_DESC_TYPE_MAX];
};

struct rx_flow_info {
	struct hal_flow_tuple_info flow_tuple_info;
	u16 fse_metadata;
	u8 ring_id;
	u8 is_addr_ipv4	:1,
	   use_ppe	:1,
	   drop		:1;
};

struct ath12k_rx_smd_ctx_per_tid {
	u8 peer_addr[ETH_ALEN];
	u8 tid;
	u16 ssn;
	u32 ba_win_sz;
	u8 pn_len;
	u32 pn_31_0;
	u16 pn_47_32;
	u8 pn_127_48_info;
	bool to_follow_1k;
	struct hal_rx_reo_bitmap_287_0 bitmap_287_0;
	struct hal_rx_reo_bitmap_1023_288 bitmap_1023_288;
};

/* carries both input (tid bitmaps) and output (tid, status_1k, peer_addr)
 * for fetching REO SMD ctx
 */
struct ath12k_dp_smd_ctx {
	u8 peer_addr[ETH_ALEN];
	union {
		struct {
			u32 rx_tid_bitmap;
			u32 tx_tid_bitmap;
			u16 tx_tid_ba_size[8];
		} in;
		struct {
			u8 tid;
		} out;
	};
};

struct ath12k_tx_smd_ctx_per_tid {
	u8 tid;
	u16 ssn;
	u16 lsn_offset;
	u8 pn_number[16];
};

/* DP arch ops to communicate from common module
 * to arch specific module
 */
struct ath12k_dump_segment;

struct ath12k_dp_arch_ops {
	int (*dp_op_device_init)(struct ath12k_dp *dp);
	void (*dp_op_device_deinit)(struct ath12k_dp *dp);
	int (*dp_op_mlo_init)(struct ath12k_dp *dp);
	void (*dp_op_mlo_deinit)(struct ath12k_dp *dp);
	u32 (*dp_tx_get_vdev_bank_config)(struct ath12k_base *ab,
					  struct ath12k_vif *ahvif,
					  u8 link_id,
					  bool vdev_id_check_en);
	int (*dp_reo_cmd_send)(struct ath12k_base *ab,
			       void *data, size_t len,
			       enum hal_reo_cmd_type type,
			       struct ath12k_hal_reo_cmd *cmd,
			       void (*cb)(struct ath12k_dp *dp, void *ctx,
						  struct hal_reo_status *status));
	void (*setup_pn_check_reo_cmd)(struct ath12k_hal_reo_cmd *cmd,
				       struct ath12k_dp_rx_tid *rx_tid,
				       u32 cipher, enum set_key_cmd key_cmd);
	void (*rx_peer_tid_delete)(struct ath12k *ar,
				   struct ath12k_dp_link_peer *peer, u8 tid);
	bool (*dp_mlo_peer_tid_teardown_ready)(struct ath12k_dp_peer *dp_peer,
					       struct ath12k_dp_link_peer *link_peer);
	int (*reo_cache_flush)(struct ath12k_base *ab,
				struct ath12k_dp_rx_tid *rx_tid);
	int (*rx_link_desc_return)(struct ath12k_dp *dp,
				   struct ath12k_buffer_addr *buf_addr_info,
				   enum hal_wbm_rel_bm_act action);
	int (*peer_rx_tid_reo_update)(struct ath12k *ar,
				      struct ath12k_dp_link_peer *peer,
				      struct ath12k_dp_rx_tid *rx_tid,
				      u32 ba_win_sz, u16 ssn,
				      bool update_ssn);
	int (*alloc_reo_qdesc)(struct ath12k_base *ab,
			       struct ath12k_dp_rx_tid *rx_tid, u16 ssn,
			       enum hal_pn_type pn_type,
			       struct hal_rx_reo_queue **addr_aligned,
			       u16 stats_id);
	void (*peer_rx_tid_qref_setup)(struct ath12k_base *ab, u16 peer_id, u16 tid,
				       dma_addr_t paddr);
	int (*dp_pdev_alloc)(struct ath12k_base *ab);
	void (*dp_pdev_free)(struct ath12k_base *ab);
	int (*rx_fst_attach)(struct ath12k_dp *dp, struct dp_rx_fst *fst);
	void (*rx_fst_detach)(struct ath12k_dp *dp, struct dp_rx_fst *fst);
	void (*rx_flow_dump_entry)(struct ath12k_dp *dp,
				   struct rx_flow_info *flow_info);
	int (*rx_flow_add_entry)(struct ath12k_dp *dp, struct rx_flow_info *flow_info);
	int (*rx_flow_delete_entry)(struct ath12k_dp *dp, struct rx_flow_info *flow_info);
	int (*rx_flow_delete_all_entries)(struct ath12k_dp *dp);
	int (*rx_flow_fse_cache_operation)(struct ath12k_base *ab,
					   enum	dp_flow_fst_operation op_code,
					   struct hal_flow_tuple_info *tuple_info);
	ssize_t (*dump_fst_table)(struct ath12k_dp *dp, char *buf, int size);
	struct ath12k_dp_hw_group*(*dp_hw_group_alloc)(void);
	int (*peer_migrate_reo_cmd)(struct ath12k_dp *dp,
				    struct ath12k_dp_link_peer *peer,
				    u16 peer_id,
				    u8 chip_id, u8 pdev_id);
	int (*sdwf_reinject_handler)(struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_link_vif *arvif,
				     struct sk_buff *skb, struct ath12k_link_sta *arsta,
				     struct ath12k_dp_peer *dp_peer);
	int (*dp_msdu_htt_connect)(struct ath12k_dp *dp);
	int (*dp_peer_create)(struct ath12k_hw *ah, u8 *addr,
			      struct ath12k_dp_peer_create_params *params,
			      struct ieee80211_vif *vif);
	void (*dp_peer_delete)(struct ath12k_dp *dp, struct ath12k_hw *ah, u8 *addr,
			       struct ieee80211_sta *sta, u8 hw_link_id,
			       struct ieee80211_vif *vif);
	int (*dp_peer_assoc)(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			     struct ath12k_dp_vif *dp_vif, u8 *addr,
			     struct ath12k_dp_peer_create_params *params);
	void (*dp_link_peer_assign_id)(struct ath12k_dp *dp, struct ath12k *ar,
				       struct ath12k_dp_link_peer *peer);
	void (*dp_link_peer_unassign_id)(struct ath12k_dp *dp,  struct ath12k *ar,
					 struct ath12k_dp_link_peer *peer);
	void (*peer_cleanup_indication)(struct ath12k_dp *dp, struct sk_buff *skb);
	void (*peer_drv_cleanup_indication)(struct ath12k_dp *dp,
					    u16 peer_id,
					    u8 hw_link_id);
	int (*dp_ppeds_tx_completion_handler)(struct ath12k_base *ab, int budget);
	void (*dp_link_peer_assoc)(struct ath12k_dp_hw *dp_hw, struct ath12k_dp *dp,
				   u8 *addr, u32 hw_link_id, struct ieee80211_vif *vif);
	int (*dp_get_peer_mgmt_flowq)(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
				      u8 *addr,
				      struct peer_assoc_flowq_params *flowq_params,
				      struct ieee80211_vif *vif);
	int (*dp_smd_prep_transfer_ext_ctx)(struct ath12k_dp *dp,
					    struct ath12k_dp_peer *current_dp_peer,
					    const u8 *target_mld_addr,
					    u16 transitioning_links);

	void (*dp_smd_exec_activate_links)(struct ath12k_dp *dp,
					   struct ath12k_dp_hw *dp_hw,
					   struct ath12k_dp_vif *dp_vif,
					   const u8 *addr,
					   u16 active_hw_links);
	int (*dp_smd_prep_rx_tid)(struct ath12k_dp *dp,
				  struct ath12k_dp_hw *dp_hw,
				  struct ieee80211_vif *vif,
				  const u8 *addr);
	int (*dp_smd_exec_rx_tid)(struct ath12k_dp *dp,
				  struct ath12k_dp_hw *dp_hw,
				  struct ieee80211_vif *vif,
				  const u8 *addr);
	void (*dp_smd_clear_old_peer_rx_lut)(struct ath12k_dp *dp,
					     struct ath12k_dp_peer *dp_peer);
	/* Release parked ext_ctx when a PREP transition is aborted. */
	void (*dp_smd_abort_prep)(struct ath12k_dp *dp, struct ieee80211_vif *vif);
	int (*dp_get_peer_holq)(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
				u8 *addr,
				struct peer_assoc_holq_params *holq_params,
				struct ieee80211_vif *vif);
	void (*dp_vif_configure)(struct ath12k_dp *dp, struct ath12k_vif *ahvif,
				 enum ath12k_dp_op_type optype);
	void (*dp_link_vif_configure)(struct ath12k_dp *dp, struct ath12k_vif *ahvif,
				      u8 link_id, enum ath12k_dp_op_type optype);
	int (*get_peer_init_status)(struct ath12k_dp *dp,
				    struct ath12k_dp_hw *dp_hw,
				    u8 *addr);
	int (*fetch_rx_desc_replenish_ring_id)(struct ath12k_dp *dp);
	enum ath12k_dp_tx_enq_error (*dp_ext_tx)(struct ath12k_pdev_dp *dp_pdev,
						 struct ath12k_dp_vif *dp_vif,
						 struct ath12k_dp_link_vif *dp_link_vif,
						 struct ath12k_tx_desc_info *tx_desc,
						 struct ath12k_dp_ext_info *info);
	enum ath12k_dp_tx_enq_error (*dp_tx_mcast_send)
				(struct ath12k_pdev_dp *dp_pdev,
				 struct ath12k_vif *ahvif,
				 struct ath12k_dp_link_vif *dp_link_vif,
				 u8 ring_id, struct ath12k_dp_tx_msdu_info *msdu_info,
				 bool gsn_valid, u16 gsn,
				 struct sk_buff *skb, struct ath12k_link_sta *arsta,
				 struct ath12k_dp_skb_ctrl *skb_ctrl,
				 bool htt_mesh);
	void (*dp_tx_set_ast)(struct ath12k_dp_peer *dp_peer,
			      struct ath12k_dp_tx_msdu_info *msdu_info,
			      u8 hw_link_id);

	/* UMAC reset operations */
	void (*umac_reset_handle_pre_reset)(struct ath12k_base *ab);
	void (*umac_reset_handle_post_reset_start)(struct ath12k_base *ab);
	void (*umac_reset_handle_post_reset_complete)(struct ath12k_base *ab);
	void (*umac_reset_handle_init_recovery)(struct ath12k_base *ab);
	int (*umcmn_irq_config)(struct ath12k_base *ab);
	void (*umcmn_irq_free)(struct ath12k_base *ab);
	void (*umcmn_irq_enable)(struct ath12k_base *ab);
	void (*umcmn_irq_disable)(struct ath12k_base *ab);
	int (*umcmn_timer_config)(struct ath12k_base *ab);
	void (*umcmn_timer_enable)(struct ath12k_base *ab);
	void (*umcmn_timer_free)(struct ath12k_base *ab);

	ssize_t (*dump_srng_stats)(struct ath12k_dp *dp, char *buf, int size);
	ssize_t (*dump_device_dp_stats)(struct ath12k_dp *dp, char *buf, int size);
	void (*reset_device_dp_stats)(struct ath12k_dp *dp);
	void (*dp_assoc_link_update)(struct ath12k_dp *dp, struct ath12k_hw *ah,
				     struct ieee80211_sta *sta,
				     struct ieee80211_vif *vif);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	bool (*dp_ast_param_get)(struct ath12k_hw *ah, struct ieee80211_vif *vif,
				 u16 *ast_info, u16 *hw_peer_id, u8 *addr);
#endif
	int (*peer_rx_tid_reo_update_for_smd)(struct ath12k_base *ab,
					      struct ath12k_dp_hw *dp_hw,
					      struct ieee80211_vif *vif,
					      const u8 *peer_addr,
					      struct ath12k_rx_smd_ctx_per_tid
					      *rx_tid_ctx);
	int (*dp_peer_fetch_smd_ctx)(struct ath12k_base *ab,
				     struct ath12k_dp_hw *dp_hw,
				     struct ieee80211_vif *vif,
				     struct ath12k_dp_smd_ctx *ctx,
				     void (*rx_cb)(struct ath12k_dp *dp, void *ctx,
						   struct hal_reo_status *status),
				     void (*tx_cb)(struct ath12k_dp *dp, void *ctx,
						   u8 *addr, u8 tid));
	int (*dp_qos_queue_setup)(struct ath12k_dp_hw_group *dp_hw_grp,
				  struct ath12k_dp_peer *dp_peer,
				  u8 msduq, u16 qos_id,
				  u8 qm_id, bool txop_intent);
	int (*peer_tx_tid_update_for_smd)(struct ath12k_base *ab,
					  struct ath12k_dp_hw *dp_hw,
					  struct ieee80211_vif *vif,
					  const u8 *peer_addr,
					  struct ath12k_tx_smd_ctx_per_tid *tx_tid_ctx);
	ssize_t (*dump_svc_sorted_list)(struct ath12k_dp *dp, u8 ac_mask,
					char *buf, int size);
	int (*update_tx_msdu_flow)(struct ath12k_dp *dp, u32 flow_number,
				   u8 tid, u8 service_category,
				   u16 hard_drop_threshold);
	ssize_t (*dump_congestion_ctrl_stats)(struct ath12k_dp *dp,
					      char *buf, int size);
	ssize_t (*dump_congestion_recovery_hist)(struct ath12k_dp *dp,
						 char *buf, int size);
	int (*set_congestion_ctrl_param)(struct ath12k_dp *dp, u32 type, u32 value);
	int (*peer_tx_tid_sn_reset)(struct ath12k_base *ab,
				    struct ath12k_dp_hw *dp_hw,
				    struct ieee80211_vif *vif,
				    const u8 *peer_addr);
	int (*peer_rx_tid_svld_reset)(struct ath12k_base *ab,
				      struct ath12k_dp_hw *dp_hw,
				      struct ieee80211_vif *vif,
				      const u8 *peer_addr);
	int (*get_coredump_seg_count)(struct ath12k_dp *dp);
	int (*get_coredump_segs)(struct ath12k_dp *dp,
				 struct ath12k_dump_segment *seg_arr, int max_segs);
};

struct ath12k_bp_stats {
       /* Head Pointer reported by the last HTT Backpressure event for the ring */
       u16 hp;

       /* Tail Pointer reported by the last HTT Backpressure event for the ring */
       u16 tp;

       /* Number of Backpressure events received for the ring */
       u32 count;

       /* Last recorded event timestamp */
       unsigned long jiffies;
};

struct ath12k_dp_ring_bp_stats {
       struct ath12k_bp_stats umac_ring_bp_stats[HTT_SW_UMAC_RING_IDX_MAX];
       struct ath12k_bp_stats lmac_ring_bp_stats[HTT_SW_LMAC_RING_IDX_MAX][MAX_RADIOS];
};

struct ath12k_device_dp_tx_err_stats {
	/* TCL Ring Descriptor unavailable */
	u32 desc_na[DP_TCL_NUM_RING_MAX];
	/* TCL Ring Buffers unavailable */
	u32 txbuf_na[DP_TCL_NUM_RING_MAX];

	u32 threshold_limit;

	/* Other failures during dp_tx due to mem allocation failure
	 * idr unavailable etc.
	 */
	atomic_t misc_fail;
	u32 tx_comp_err[DP_TX_COMP_ERR_MAX][DP_TCL_NUM_RING_MAX];
};

struct ath12k_device_dp_rx_err_stats {
	u32 rx_err[DP_RX_ERR_MAX][DP_REO_DST_RING_MAX];
};

struct ath12k_device_dp_rx_wbm_err_stats {
	u32 rxdma_error[HAL_REO_ENTR_RING_RXDMA_ECODE_MAX];
	u32 reo_error[HAL_REO_DEST_RING_ERROR_CODE_MAX];
	u32 drop[WBM_ERR_DROP_MAX];
	u32 hal_reo_route;
};

struct ath12k_tx_comp_stats {
	u32 tx_completed;
	u32 tx_wbm_rel_source[HAL_WBM_REL_SRC_MODULE_MAX];
	u32 tqm_rel_reason[MAX_TQM_RELEASE_REASON];
	u32 fw_tx_status[MAX_FW_TX_STATUS];
};

struct ath12k_device_dp_stats {
	u32 tx_fast_unicast[MAX_TCL_RING];
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	u32 ppe_vp_mode_update_fail;
#endif
	struct ath12k_tx_comp_stats tx_comp_stats[MAX_TX_COMP_RING];
	u32 err_ring_pkts;
	u32 invalid_rbm;
	u32 reo_excep_msdu_buf_type;
	u32 free_excess_alloc_skb;
	u32 rx_wbm_rel_source[HAL_WBM_REL_SRC_MODULE_MAX] [ATH12K_MAX_SOCS];
	u32 reo_rx[DP_REO_DST_RING_MAX] [ATH12K_MAX_SOCS];
	u32 non_fast_unicast_rx[DP_REO_DST_RING_MAX][ATH12K_MAX_SOCS];
	u32 non_fast_mcast_rx[DP_REO_DST_RING_MAX][ATH12K_MAX_SOCS];
	u32 rx_eapol[ATH12K_MAX_SOCS];
	u32 rx_eapol_type[DP_EAPOL_KEY_TYPE_MAX][ATH12K_MAX_SOCS];
	u32 fast_rx[DP_REO_DST_RING_MAX] [ATH12K_MAX_SOCS];
	struct ath12k_device_dp_tx_err_stats tx_err;
	struct ath12k_device_dp_rx_err_stats rx;
	struct ath12k_dp_ring_bp_stats bp_stats;
	struct ath12k_device_dp_rx_wbm_err_stats wbm_err;
	u32 tx_mcast[MAX_TCL_RING];
	u32 tx_unicast[MAX_TCL_RING];
	u32 tx_mcuc[MAX_TCL_RING];
	u32 tx_eapol[MAX_TCL_RING];
	u32 tx_eapol_type[DP_EAPOL_KEY_TYPE_MAX][MAX_TCL_RING];
	u32 tx_null_frame[MAX_TCL_RING];
	u32 rx_pkt_null_frame_dropped;
	u32 rx_pkt_null_frame_handled;
};

#define ATH12K_DP_FST_NUM_CORES NR_CPUS
#define ATH12K_DP_MIN_FST_CORE_MASK 0x1
#define ATH12K_DP_MAX_FST_CORE_MASK ((1 << ATH12K_DP_FST_NUM_CORES) - 1)

struct dp_fst_config {
	u32 fst_core_mask;
	u8 fst_core_map[ATH12K_DP_FST_NUM_CORES];
	u8 fst_num_cores;
	u8 core_idx;
};

/**
 * struct ath12k_dp_htt_rxdma_ppe_cfg_param - Rx DMA and RxOLE PPE config
 * @override: RxDMA override to override the reo_destinatoin_indication
 * @reo_dst_ind: REO destination indication value
 * @multi_buffer_msdu_override_en: Override the indication for SG
 * @intra_bss_override: Rx OLE IntraBSS override
 * @decap_raw_override: Rx Decap Raw override
 * @decap_nwifi_override: Rx Native override
 * @ip_frag_override: IP fragments override
 */
struct ath12k_dp_htt_rxdma_ppe_cfg_param {
	u8 override;
	u8 reo_dst_ind;
	u8 multi_buffer_msdu_override_en;
	u8 intra_bss_override;
	u8 decap_raw_override;
	u8 decap_nwifi_override;
	u8 ip_frag_override;
};

struct ath12k_dbg_dp_svc_sort_stats {
	u8 ac_mask;
};

struct ath12k_dp {
	struct ath12k_base *ab;
	u8 num_bank_profiles;
	/* protects the access and update of bank_profiles */
	spinlock_t tx_bank_lock;
	struct ath12k_dp_tx_bank_profile *bank_profiles;
	enum ath12k_htc_ep_id eid;
	enum ath12k_htc_ep_id msdu_eid;
	struct completion htt_tgt_version_received;
	u8 htt_tgt_ver_major;
	u8 htt_tgt_ver_minor;
	struct dp_link_desc_bank link_desc_banks[DP_LINK_DESC_BANKS_MAX];
	u8 idle_link_rbm;
	atomic_t tqm_cmd_num;
	struct dp_srng wbm_idle_ring;
	struct dp_srng wbm_desc_rel_ring;
	struct dp_srng reo_reinject_ring;
	struct dp_srng rx_rel_ring;
	struct dp_srng reo_except_ring;
	struct dp_srng reo_cmd_ring;
	struct dp_srng reo_status_ring;
	enum ath12k_peer_metadata_version peer_metadata_ver;
	struct dp_srng reo_dst_ring[DP_REO_DST_RING_MAX];
	struct dp_tx_ring tx_ring[DP_TCL_NUM_RING_MAX];
	struct wbm_idle_scatter_list scatter_list[DP_IDLE_SCATTER_BUFS_MAX];
	u8 num_scatter_buf;
	struct list_head reo_cmd_list;
	struct list_head reo_cmd_cache_flush_list;
	struct list_head tqm_cmd_list;
	u32 reo_cmd_cache_flush_count;

	/* htt_logger_handle */
	struct htt_logger *htt_logger_handle;

	/* protects access to below fields,
	 * - reo_cmd_list
	 * - reo_cmd_cache_flush_list
	 * - reo_cmd_cache_flush_count
	 */
	spinlock_t reo_cmd_lock;
	spinlock_t tqm_cmd_lock;
	struct list_head reo_cmd_update_rx_queue_list;
	/* protects access to below field,
	 * - reo_cmd_update_rx_queue_list
	 */
	spinlock_t reo_cmd_update_rx_queue_lock;
	struct ath12k_hp_update_timer reo_cmd_timer;
	struct ath12k_hp_update_timer tx_ring_timer[DP_TCL_NUM_RING_MAX];
	struct ath12k_spt_info *spt_info;
	u32 num_spt_pages;
	u32 rx_ppt_base;
	struct ath12k_rx_desc_info **rxbaddr;
	struct ath12k_tx_desc_info **txbaddr;
	struct list_head rx_ppeds_reuse_list;
	struct list_head rx_desc_free_list;
	/* protects the free desc list */
	spinlock_t rx_desc_lock;

	struct dp_rxdma_ring rx_refill_buf_ring;
#ifdef CPTCFG_EXT_IPA_OFFLOAD
	struct dp_rxdma_ring rx_refill_buf_ring2;
#endif
	struct dp_srng rx_mac_buf_ring[MAX_RXDMA_PER_PDEV];
	struct dp_srng rxdma_err_dst_ring[MAX_RXDMA_PER_PDEV];
	struct ath12k_reo_q_addr_lut reoq_lut;
	struct ath12k_reo_q_addr_lut ml_reoq_lut;
	const struct ath12k_hw_params *hw_params;
	bool hw_peer_stats_support;
	struct ath12k_device_dp_stats device_stats;
	struct device *dev;
	struct ath12k_hal *hal;

	/* protects data fields like dp_pdevs, peers and rhead_peer_addr */
	spinlock_t dp_lock;
	struct ath12k_pdev_dp __rcu *dp_pdevs[MAX_RADIOS];
	u8 num_radios;

	struct ath12k_dp_hw_group *dp_hw_grp;
	u8 device_id;
	bool global_peer_id_supported;

	struct ath12k_dp_arch_ops *arch_ops;

	struct dp_fst_config fst_config;

	struct ath12k_dp_mon *dp_mon;

	/* Hashtable for struct ath12k_dp_link_peer keyed by mac addr */
	DECLARE_HASHTABLE(link_peer_htbl, ATH12K_DP_LINK_PEER_HASH_BITS);

	struct ath12k_ppe ppe;

	/*Neighbors Peer list for NAC RSSI*/
	struct list_head neighbor_peers;
	int num_nrps;
	bool stats_disable;

	/* Extension descriptor cache for kmem_cache allocation */
	struct kmem_cache *ext_cache;

	/* HW link ID position in PPDU_ID */
	u8 link_id_offset;
	u8 link_id_bits;
	u8 tcl_metadata_ver;
	u16 htt_tx_mon_cfg_msg_size;
	u8 htt_tx_mon_cfg_version;

	struct ath12k_dbg_dp_svc_sort_stats svc_sort_stats;

	/* Keep Last */
	u8 arch_data[] __aligned(sizeof(void *));
};
/* @brief target -> host extended statistics upload
 *
 * @details
 * The following field definitions describe the format of the HTT target
 * to host stats upload confirmation message.
 * The message contains a cookie echoed from the HTT host->target stats
 * upload request, which identifies which request the confirmation is
 * for, and a single stats can span over multiple HTT stats indication
 * due to the HTT message size limitation so every HTT ext stats indication
 * will have tag-length-value stats information elements.
 * The tag-length header for each HTT stats IND message also includes a
 * status field, to indicate whether the request for the stat type in
 * question was fully met, partially met, unable to be met, or invalid
 * (if the stat type in question is disabled in the target).
 * A Done bit 1's indicate the end of the of stats info elements.
 *
 *
 * |31                         16|15    12|11|10 8|7   5|4       0|
 * |--------------------------------------------------------------|
 * |                   reserved                   |    msg type   |
 * |--------------------------------------------------------------|
 * |                         cookie LSBs                          |
 * |--------------------------------------------------------------|
 * |                         cookie MSBs                          |
 * |--------------------------------------------------------------|
 * |      stats entry length     | rsvd   | D|  S |   stat type   |
 * |--------------------------------------------------------------|
 * |                   type-specific stats info                   |
 * |                      (see htt_stats.h)                       |
 * |--------------------------------------------------------------|
 * Header fields:
 *  - MSG_TYPE
 *    Bits 7:0
 *    Purpose: Identifies this is a extended statistics upload confirmation
 *             message.
 *    Value: 0x1c
 *  - COOKIE_LSBS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to match a target->host stats confirmation
 *        message with its preceding host->target stats request message.
 *    Value: LSBs of the opaque cookie specified by the host-side requestor
 *  - COOKIE_MSBS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to match a target->host stats confirmation
 *        message with its preceding host->target stats request message.
 *    Value: MSBs of the opaque cookie specified by the host-side requestor
 *
 * Stats Information Element tag-length header fields:
 *  - STAT_TYPE
 *    Bits 7:0
 *    Purpose: identifies the type of statistics info held in the
 *        following information element
 *    Value: htt_dbg_ext_stats_type
 *  - STATUS
 *    Bits 10:8
 *    Purpose: indicate whether the requested stats are present
 *    Value: htt_dbg_ext_stats_status
 *  - DONE
 *    Bits 11
 *    Purpose:
 *        Indicates the completion of the stats entry, this will be the last
 *        stats conf HTT segment for the requested stats type.
 *    Value:
 *        0 -> the stats retrieval is ongoing
 *        1 -> the stats retrieval is complete
 *  - LENGTH
 *    Bits 31:16
 *    Purpose: indicate the stats information size
 *    Value: This field specifies the number of bytes of stats information
 *       that follows the element tag-length header.
 *       It is expected but not required that this length is a multiple of
 *       4 bytes.
 */

 enum dp_umac_reset_recover_action {
	 ATH12K_UMAC_RESET_RX_EVENT_NONE,
	 ATH12K_UMAC_RESET_INIT_UMAC_RECOVERY,
	 ATH12K_UMAC_RESET_INIT_TARGET_RECOVERY_SYNC_USING_UMAC,
	 ATH12K_UMAC_RESET_DO_PRE_RESET,
	 ATH12K_UMAC_RESET_DO_POST_RESET_START,
	 ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE,
 };

enum dp_umac_reset_tx_cmd {
	ATH12K_UMAC_RESET_TX_CMD_NONE,
	ATH12K_UMAC_RESET_TX_CMD_TRIGGER_DONE,
	ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE,
	ATH12K_UMAC_RESET_TX_CMD_POST_RESET_START_DONE,
	ATH12K_UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE,
};

enum ath12k_umac_reset_state {
	ATH12K_UMAC_RESET_STATE_IDLE = 0,
	ATH12K_UMAC_RESET_STATE_INIT,
	ATH12K_UMAC_RESET_STATE_TRIGGER_SENT,
	ATH12K_UMAC_RESET_STATE_PRE_RESET_START,
	ATH12K_UMAC_RESET_STATE_PRE_RESET_DONE,
	ATH12K_UMAC_RESET_STATE_POST_RESET_START,
	ATH12K_UMAC_RESET_STATE_POST_RESET_DONE,
	ATH12K_UMAC_RESET_STATE_POST_RESET_COMPLETE,
	ATH12K_UMAC_RESET_STATE_ERROR,
	ATH12K_UMAC_RESET_STATE_MAX
};

struct ath12k_umac_reset_ts {
	/* Interrupt arrival timestamps for each event */
	u64 event_irq_init_umac_recovery;
	u64 event_irq_init_target_recovery;
	u64 event_irq_pre_reset;
	u64 event_irq_post_reset_start;
	u64 event_irq_post_reset_complete;
};

struct ath12k_dp_umac_reset {
	struct ath12k_base *ab;
	dma_addr_t shmem_paddr_unaligned;
	void *shmem_vaddr_unaligned;
	dma_addr_t shmem_paddr_aligned;
	struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t *shmem_vaddr_aligned;
	size_t shmem_size;
	uint32_t magic_num;
	int intr_offset;
	struct tasklet_struct intr_tq;
	int irq_num;
	struct ath12k_umac_reset_ts ts;

	/* State machine fields */
	enum ath12k_umac_reset_state current_state;
	enum ath12k_umac_reset_state prev_state;
	spinlock_t state_lock; /* Protects state transitions */

	/* State transition tracking */
	u32 state_transition_count[ATH12K_UMAC_RESET_STATE_MAX];
	u64 state_entry_time[ATH12K_UMAC_RESET_STATE_MAX];

	/* Error handling */
	u32 state_error_count;
	enum ath12k_umac_reset_state error_from_state;

	/* Post-send callback - executed after FW message send completes */
	void (*post_send_cb)(struct ath12k_base *ab);

	/* SKB queues for deferred cleanup during UMAC reset */
	struct sk_buff_head tx_skb_queue;
	struct sk_buff_head rx_skb_queue;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	struct sk_buff_head ppeds_tx_skb_queue;
#endif
};

enum ath12k_rxtid_pn_check {
	ATH12K_RXTID_PN_CHECK_DATA_TIDS,
	ATH12K_RXTID_PN_CHECK_MGMT_TIDS,
	ATH12K_RXTID_PN_CHECK_ALL_TIDS,
};

#define HTT_T2H_EXT_STATS_INFO1_DONE	BIT(11)
#define HTT_T2H_EXT_STATS_INFO1_LENGTH   GENMASK(31, 16)

#define	HTT_MAC_ADDR_L32_0	GENMASK(7, 0)
#define	HTT_MAC_ADDR_L32_1	GENMASK(15, 8)
#define	HTT_MAC_ADDR_L32_2	GENMASK(23, 16)
#define	HTT_MAC_ADDR_L32_3	GENMASK(31, 24)
#define	HTT_MAC_ADDR_H16_0	GENMASK(7, 0)
#define	HTT_MAC_ADDR_H16_1	GENMASK(15, 8)

static inline int ath12k_dp_arch_op_device_init(struct ath12k_dp *dp)
{
	return dp->arch_ops->dp_op_device_init(dp);
}

static inline void ath12k_dp_arch_op_device_deinit(struct ath12k_dp *dp)
{
	dp->arch_ops->dp_op_device_deinit(dp);
}

static inline int ath12k_dp_arch_op_mlo_init(struct ath12k_dp *dp)
{
	if (dp->arch_ops->dp_op_mlo_init)
		return dp->arch_ops->dp_op_mlo_init(dp);

	return 0;
}

static inline void ath12k_dp_arch_op_mlo_deinit(struct ath12k_dp *dp)
{
	if (dp->arch_ops->dp_op_mlo_deinit)
		dp->arch_ops->dp_op_mlo_deinit(dp);
}

static inline u32 ath12k_dp_arch_tx_get_vdev_bank_config(struct ath12k_dp *dp,
							 struct ath12k_vif *ahvif,
							 u8 link_id,
							 bool force_vdev_id_check_disable)
{
	return dp->arch_ops->dp_tx_get_vdev_bank_config(dp->ab, ahvif,
							link_id,
							force_vdev_id_check_disable);
}

static inline int ath12k_dp_arch_reo_cmd_send(struct ath12k_dp *dp,
					      void *data, size_t len,
					      enum hal_reo_cmd_type type,
					      struct ath12k_hal_reo_cmd *cmd,
					      void (*cb)(struct ath12k_dp *dp, void *ctx,
							 struct hal_reo_status *status))
{
	return dp->arch_ops->dp_reo_cmd_send(dp->ab, data, len, type, cmd, cb);
}

static inline void ath12k_dp_arch_setup_pn_check_reo_cmd(struct ath12k_dp *dp,
							 struct ath12k_hal_reo_cmd *cmd,
							 struct ath12k_dp_rx_tid *rx_tid,
							 u32 cipher,
							 enum set_key_cmd key_cmd)
{
	dp->arch_ops->setup_pn_check_reo_cmd(cmd, rx_tid, cipher, key_cmd);
}

static inline void ath12k_dp_arch_rx_peer_tid_delete(struct ath12k_dp *dp,
						     struct ath12k *ar,
						     struct ath12k_dp_link_peer *peer,
						     u8 tid)
{
	dp->arch_ops->rx_peer_tid_delete(ar, peer, tid);
}

static inline bool
ath12k_dp_arch_mlo_peer_tid_teardown_ready(struct ath12k_dp *dp,
					   struct ath12k_dp_peer *dp_peer,
					   struct ath12k_dp_link_peer *link_peer)
{
	return dp->arch_ops->dp_mlo_peer_tid_teardown_ready(dp_peer, link_peer);
}

static inline int ath12k_dp_arch_reo_cache_flush(struct ath12k_dp *dp,
						 struct ath12k_dp_rx_tid *rx_tid)
{
	return dp->arch_ops->reo_cache_flush(dp->ab, rx_tid);
}

static inline
int ath12k_dp_arch_rx_link_desc_return(struct ath12k_dp *dp,
				       struct ath12k_buffer_addr *buf_addr_info,
				       enum hal_wbm_rel_bm_act action)
{
	return dp->arch_ops->rx_link_desc_return(dp, buf_addr_info, action);
}

static inline int ath12k_dp_arch_peer_rx_tid_reo_update(struct ath12k_dp *dp,
							struct ath12k *ar,
							struct ath12k_dp_link_peer *peer,
							struct ath12k_dp_rx_tid *rx_tid,
							u32 ba_win_sz, u16 ssn,
							bool update_ssn)
{
	return dp->arch_ops->peer_rx_tid_reo_update(ar, peer, rx_tid,
						    ba_win_sz, ssn, update_ssn);
}

static inline int ath12k_dp_arch_alloc_reo_qdesc(struct ath12k_dp *dp,
						 struct ath12k_dp_rx_tid *rx_tid, u16 ssn,
						 enum hal_pn_type pn_type,
						 struct hal_rx_reo_queue **addr_aligned,
						 u16 stats_id)
{
	return dp->arch_ops->alloc_reo_qdesc(dp->ab, rx_tid, ssn, pn_type, addr_aligned,
					     stats_id);
}

static inline void ath12k_dp_arch_peer_rx_tid_qref_setup(struct ath12k_dp *dp,
							 u16 peer_id, u16 tid,
							 dma_addr_t paddr)
{
	dp->arch_ops->peer_rx_tid_qref_setup(dp->ab, peer_id, tid, paddr);
}

static inline int ath12k_dp_arch_rx_fst_attach(struct ath12k_dp *dp,
					       struct dp_rx_fst *fst)
{
	return dp->arch_ops->rx_fst_attach(dp, fst);
}

static inline void ath12k_dp_arch_rx_fst_detach(struct ath12k_dp *dp,
						struct dp_rx_fst *fst)
{
	dp->arch_ops->rx_fst_detach(dp, fst);
}

static inline void
ath12k_dp_arch_rx_flow_dump_entry(struct ath12k_dp *dp,
				  struct rx_flow_info *flow_info)
{
	return dp->arch_ops->rx_flow_dump_entry(dp, flow_info);
}

static inline int
ath12k_dp_arch_rx_flow_add_entry(struct ath12k_dp *dp,
				 struct rx_flow_info *flow_info)
{
	return dp->arch_ops->rx_flow_add_entry(dp, flow_info);
}

static inline int
ath12k_dp_arch_rx_flow_delete_entry(struct ath12k_dp *dp,
				    struct rx_flow_info *flow_info)
{
	return dp->arch_ops->rx_flow_delete_entry(dp, flow_info);
}

static inline int
ath12k_dp_arch_rx_flow_delete_all_entries(struct ath12k_dp *dp)
{
	return dp->arch_ops->rx_flow_delete_all_entries(dp);
}

static inline ssize_t
ath12k_dp_arch_dump_fst_table(struct ath12k_dp *dp, char *buf, int size)
{
	return dp->arch_ops->dump_fst_table(dp, buf, size);
}

static inline int
ath12k_dp_arch_rx_flow_fse_cache_op(struct ath12k_dp *dp,
				    enum dp_flow_fst_operation op_code,
				    struct hal_flow_tuple_info *tuple_info)
{
	return dp->arch_ops->rx_flow_fse_cache_operation(dp->ab, op_code, tuple_info);
}

static inline int ath12k_dp_arch_fetch_rx_desc_replenish_ring_id(struct ath12k_dp *dp)
{
	return dp->arch_ops->fetch_rx_desc_replenish_ring_id(dp);
}

static inline struct ath12k_dp_hw_group *
ath12k_core_dp_hw_group_alloc(struct ath12k_dp *dp)
{
	return dp->arch_ops->dp_hw_group_alloc();
}

static inline int ath12k_dp_arch_pdev_alloc(struct ath12k_dp *dp)
{
	return dp->arch_ops->dp_pdev_alloc(dp->ab);
}

static inline void ath12k_dp_arch_pdev_free(struct ath12k_dp *dp)
{
	dp->arch_ops->dp_pdev_free(dp->ab);
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static inline bool
ath12k_dp_arch_ast_param_get(struct ath12k_dp *dp,
			     struct ath12k_hw *ah, struct ieee80211_vif *vif,
			     u16 *ast_info, u16 *hw_peer_id,
			     u8 *addr)
{
	if (dp->arch_ops->dp_ast_param_get)
		return dp->arch_ops->dp_ast_param_get(ah, vif, ast_info, hw_peer_id,
						      addr);
	return false;
}
#endif

static inline int
ath12k_dp_arch_peer_create(struct ath12k_dp *dp,
			   struct ath12k_hw *ah,
			   u8 *addr,
			   struct ath12k_dp_peer_create_params *params,
			   struct ieee80211_vif *vif)
{
	if (dp->arch_ops->dp_peer_create)
		return dp->arch_ops->dp_peer_create(ah, addr, params, vif);
	return -EOPNOTSUPP;
}

static inline void
ath12k_dp_arch_peer_delete(struct ath12k_dp *dp,
			   struct ath12k_hw *ah,
			   u8 *addr,
			   struct ieee80211_sta *sta, u8 hw_link_id,
			   struct ieee80211_vif *vif)
{
	if (dp->arch_ops->dp_peer_delete)
		dp->arch_ops->dp_peer_delete(dp, ah, addr, sta, hw_link_id, vif);
}

static inline void
ath12k_dp_arch_assoc_link_update(struct ath12k_dp *dp,
				 struct ath12k_hw *ah,
				 struct ieee80211_sta *sta,
				 struct ieee80211_vif *vif)
{
	if (dp->arch_ops->dp_assoc_link_update)
		dp->arch_ops->dp_assoc_link_update(dp, ah, sta, vif);
}

static inline int ath12k_dp_arch_peer_assoc(struct ath12k_dp *dp,
					    struct ath12k_dp_hw *dp_hw,
					    struct ath12k_dp_vif *dp_vif,
					    u8 *addr,
					    struct ath12k_dp_peer_create_params *params)
{
	if (dp->arch_ops->dp_peer_assoc)
		return dp->arch_ops->dp_peer_assoc(dp, dp_hw, dp_vif, addr, params);
	return -EOPNOTSUPP;
}

static inline void ath12k_dp_arch_link_peer_assign_id(struct ath12k_dp *dp,
						      struct ath12k *ar,
						      struct ath12k_dp_link_peer *peer)
{
	if (dp->arch_ops->dp_link_peer_assign_id)
		dp->arch_ops->dp_link_peer_assign_id(dp, ar, peer);
}

static inline void ath12k_dp_arch_link_peer_unassign_id(struct ath12k_dp *dp,
							struct ath12k *ar,
							struct ath12k_dp_link_peer *peer)
{
	if (dp->arch_ops->dp_link_peer_unassign_id)
		dp->arch_ops->dp_link_peer_unassign_id(dp, ar, peer);
}

static inline void ath12k_dp_arch_peer_cleanup_indication(struct ath12k_dp *dp,
							  struct sk_buff *skb)
{
	if (dp->arch_ops->peer_cleanup_indication)
		dp->arch_ops->peer_cleanup_indication(dp, skb);
}

static inline void
ath12k_dp_arch_peer_drv_cleanup_indication(struct ath12k_dp *dp,
					   u16 peer_id,
					   u8 hw_link_id)
{
	if (dp->arch_ops->peer_drv_cleanup_indication)
		dp->arch_ops->peer_drv_cleanup_indication(dp, peer_id,
							  hw_link_id);
}

static inline void ath12k_dp_arch_link_peer_assoc(struct ath12k_dp *dp,
						  struct ath12k_dp_hw *dp_hw,
						  u8 *addr, u32 hw_link_id,
						  struct ieee80211_vif *vif)
{
	if (dp->arch_ops->dp_link_peer_assoc)
		return dp->arch_ops->dp_link_peer_assoc(dp_hw, dp, addr,
							hw_link_id, vif);
}

static inline int
ath12k_arch_dp_get_peer_mgmt_flowq(struct ath12k_dp *dp,
				   struct ath12k_dp_hw *dp_hw,
				   u8 *addr,
				   struct peer_assoc_flowq_params *flowq_params,
				   struct ieee80211_vif *vif)
{
	if (dp->arch_ops->dp_get_peer_mgmt_flowq)
		return dp->arch_ops->dp_get_peer_mgmt_flowq(dp, dp_hw, addr,
							    flowq_params, vif);
	return -EINVAL;
}

static inline int
ath12k_arch_dp_get_peer_holq(struct ath12k_dp *dp,
			     struct ath12k_dp_hw *dp_hw,
			     u8 *addr,
			     struct peer_assoc_holq_params *holq_params,
			     struct ieee80211_vif *vif)
{
	if (dp->arch_ops->dp_get_peer_holq)
		return dp->arch_ops->dp_get_peer_holq(dp, dp_hw, addr,
						      holq_params, vif);
	return -EINVAL;
}

static inline void ath12k_dp_arch_dp_vif_configure(struct ath12k_dp_hw_group *dp_hw_grp,
						   struct ath12k_vif *ahvif,
						   enum ath12k_dp_op_type optype)
{
	struct ath12k_dp *dp = dp_hw_grp->dp[0];

	if (dp->arch_ops->dp_vif_configure)
		dp->arch_ops->dp_vif_configure(dp, ahvif, optype);
}

static inline void ath12k_dp_arch_dp_link_vif_configure(struct ath12k_dp *dp,
							struct ath12k_vif *ahvif,
							u8 link_id,
							enum ath12k_dp_op_type optype)
{
	if (dp->arch_ops->dp_link_vif_configure)
		dp->arch_ops->dp_link_vif_configure(dp, ahvif, link_id, optype);
}

static inline int ath12k_dp_arch_get_peer_init_status(struct ath12k_dp *dp,
						      struct ath12k_dp_hw *dp_hw,
						      u8 *addr)
{
	if (dp->arch_ops->get_peer_init_status)
		return dp->arch_ops->get_peer_init_status(dp, dp_hw, addr);

	return 0;
}

static inline int
ath12k_dp_arch_dp_peer_fetch_smd_ctx(struct ath12k_dp *dp,
				     struct ath12k_dp_hw *dp_hw,
				     struct ieee80211_vif *vif,
				     struct ath12k_dp_smd_ctx *ctx,
				     void (*rx_cb)(struct ath12k_dp *dp, void *ctx,
						   struct hal_reo_status *reo_status),
				     void (*tx_cb)(struct ath12k_dp *dp, void *ctx,
						   u8 *addr, u8 tid))
{
	if (dp->arch_ops->dp_peer_fetch_smd_ctx)
		return dp->arch_ops->dp_peer_fetch_smd_ctx(dp->ab, dp_hw, vif, ctx,
							   rx_cb, tx_cb);
	return 0;
}

static inline int
ath12k_dp_arch_peer_tx_tid_update_for_smd(struct ath12k_dp *dp,
					  struct ath12k_dp_hw *dp_hw,
					  struct ieee80211_vif *vif,
					  const u8 *peer_addr,
					  struct ath12k_tx_smd_ctx_per_tid *tx_tid_ctx)
{
	if (dp->arch_ops->peer_tx_tid_update_for_smd)
		return dp->arch_ops->peer_tx_tid_update_for_smd(dp->ab, dp_hw, vif,
								peer_addr,
								tx_tid_ctx);

	return 0;
}

static inline int
ath12k_dp_arch_peer_rx_tid_reo_update_for_smd(struct ath12k_dp *dp,
					      struct ath12k_dp_hw *dp_hw,
					      struct ieee80211_vif *vif,
					      const u8 *peer_addr,
					      struct ath12k_rx_smd_ctx_per_tid
					      *rx_tid_ctx)
{
	if (dp->arch_ops->peer_rx_tid_reo_update_for_smd)
		return dp->arch_ops->peer_rx_tid_reo_update_for_smd(dp->ab, dp_hw,
								    vif,
								    peer_addr,
								    rx_tid_ctx);
	return 0;
}

static inline int ath12k_dp_arch_update_tx_msdu_flow(struct ath12k_dp *dp,
						     u32 flow_number,
						     u8 tid,
						     u8 svc,
						     u16 hard_drop_threshold)
{
	if (dp->arch_ops->update_tx_msdu_flow)
		return dp->arch_ops->update_tx_msdu_flow(dp,
							 flow_number,
							 tid,
							 svc,
							 hard_drop_threshold);

	return 0;
}

static inline int ath12k_dp_arch_set_congestion_ctrl_param(struct ath12k_dp *dp,
							   u32 type, u32 value)
{
	if (dp->arch_ops->set_congestion_ctrl_param)
		return dp->arch_ops->set_congestion_ctrl_param(dp, type, value);

	return 0;
}

static inline int
ath12k_dp_arch_peer_tx_tid_sn_reset(struct ath12k_dp *dp,
				    struct ath12k_dp_hw *dp_hw,
				    struct ieee80211_vif *vif,
				    const u8 *peer_addr)
{
	if (dp->arch_ops->peer_tx_tid_sn_reset)
		return dp->arch_ops->peer_tx_tid_sn_reset(dp->ab, dp_hw,
							  vif, peer_addr);
	return -EOPNOTSUPP;
}

static inline int
ath12k_dp_arch_peer_rx_tid_svld_reset(struct ath12k_dp *dp,
				      struct ath12k_dp_hw *dp_hw,
				      struct ieee80211_vif *vif,
				      const u8 *peer_addr)
{
	if (dp->arch_ops->peer_rx_tid_svld_reset)
		return dp->arch_ops->peer_rx_tid_svld_reset(dp->ab, dp_hw,
							   vif, peer_addr);
	return -EOPNOTSUPP;
}

static inline int
ath12k_dp_arch_smd_prep_transfer_ext_ctx(struct ath12k_dp *dp,
					 struct ath12k_dp_peer *current_dp_peer,
					 const u8 *target_mld_addr,
					 u16 transitioning_links)
{
	if (dp->arch_ops->dp_smd_prep_transfer_ext_ctx)
		return dp->arch_ops->dp_smd_prep_transfer_ext_ctx(dp,
								   current_dp_peer,
								   target_mld_addr,
								   transitioning_links);

	return -EOPNOTSUPP;
}

static inline void
ath12k_dp_arch_smd_exec_activate_links(struct ath12k_dp *dp,
				       struct ath12k_dp_hw *dp_hw,
				       struct ath12k_dp_vif *dp_vif,
				       const u8 *addr,
				       u16 active_hw_links)
{
	if (dp->arch_ops->dp_smd_exec_activate_links)
		dp->arch_ops->dp_smd_exec_activate_links(dp, dp_hw, dp_vif,
							  addr, active_hw_links);
}

static inline int
ath12k_dp_arch_smd_prep_rx_tid(struct ath12k_dp *dp,
			       struct ath12k_dp_hw *dp_hw,
			       struct ieee80211_vif *vif,
			       const u8 *addr)
{
	if (dp->arch_ops->dp_smd_prep_rx_tid)
		return dp->arch_ops->dp_smd_prep_rx_tid(dp, dp_hw, vif, addr);

	return -EOPNOTSUPP;
}

static inline int
ath12k_dp_arch_smd_exec_rx_tid(struct ath12k_dp *dp,
			       struct ath12k_dp_hw *dp_hw,
			       struct ieee80211_vif *vif,
			       const u8 *addr)
{
	if (dp->arch_ops->dp_smd_exec_rx_tid)
		return dp->arch_ops->dp_smd_exec_rx_tid(dp, dp_hw, vif, addr);

	return -EOPNOTSUPP;
}

static inline void
ath12k_dp_arch_smd_clear_old_peer_rx_lut(struct ath12k_dp *dp,
					 struct ath12k_dp_peer *dp_peer)
{
	if (dp->arch_ops->dp_smd_clear_old_peer_rx_lut)
		dp->arch_ops->dp_smd_clear_old_peer_rx_lut(dp, dp_peer);
}

static inline void
ath12k_dp_arch_smd_abort_prep(struct ath12k_dp *dp, struct ieee80211_vif *vif)
{
	if (dp->arch_ops->dp_smd_abort_prep)
		dp->arch_ops->dp_smd_abort_prep(dp, vif);
}

static inline void ath12k_dp_get_mac_addr(u32 addr_l32, u16 addr_h16, u8 *addr)
{
	memcpy(addr, &addr_l32, 4);
	memcpy(addr + 4, &addr_h16, ETH_ALEN - 4);
}

#define PEER_TABLE_SOC_ID_SHIFT        10
#define ATH12K_PEER_ML_ID_VALID        BIT(13)
static inline
u16 ath12k_dp_peer_get_peerid_index(struct ath12k_dp *dp, u16 peer_id)
{
	return dp->global_peer_id_supported ? peer_id
		: ((peer_id & ATH12K_PEER_ML_ID_VALID)
			? peer_id
			: ((dp->device_id << PEER_TABLE_SOC_ID_SHIFT) | peer_id));
}

static inline struct ath12k_pdev_dp *
ath12k_dp_hw_grp_to_dp_pdev(struct ath12k_dp_hw_group *dp_hw_grp, u8 hw_link_id)
{
	struct ath12k_dp_hw_link *hw_links = dp_hw_grp->hw_links;
	u8 device_id;
	struct ath12k_dp *dp;
	u8 pdev_id;

	if (hw_link_id >= ATH12K_GROUP_MAX_RADIO)
		return NULL;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(),
			 "ath12k dp to dp pdev called without rcu lock");

	device_id = hw_links[hw_link_id].device_id;
	dp = dp_hw_grp->dp[device_id];
	if (!dp)
		return NULL;

	pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params,
					      hw_links[hw_link_id].pdev_idx);
	return rcu_dereference(dp->dp_pdevs[pdev_id]);
}

static inline struct ath12k_dp *
ath12k_dp_hw_grp_to_dp(struct ath12k_dp_hw_group *dp_hw_grp, u8 device_id)
{
	return dp_hw_grp->dp[device_id];
}

static inline struct ieee80211_hw *
ath12k_dp_pdev_to_hw(struct ath12k_pdev_dp *pdev)
{
	return pdev->hw;
}

static inline struct ath12k_pdev_dp *
ath12k_dp_to_dp_pdev(struct ath12k_dp *dp, u8 pdev_id)
{
	RCU_LOCKDEP_WARN(!rcu_read_lock_held(),
                        "ath12k dp to dp pdev called without rcu lock");

	return rcu_dereference(dp->dp_pdevs[pdev_id]);
}

/**
 * ath12k_dp_pcp_tid_map() - Validate and program PCP-TID map.
 */
int ath12k_dp_pcp_tid_map(struct ath12k_dp_hw_group *dp_hw_grp);

/**
 * ath12k_dp_tid_map_precedence() - Validate and program TID precedence.
 */
int ath12k_dp_tid_map_precedence(struct ath12k_dp_hw_group *dp_hw_grp);

static inline int
ath12k_dp_arch_peer_migrate_reo_cmd(struct ath12k_dp *dp,
				    struct ath12k_dp_link_peer *peer,
				     u16 peer_id, u8 chip_id, u8 pdev_id)
{
	return dp->arch_ops->peer_migrate_reo_cmd(dp, peer, peer_id,
						  chip_id, pdev_id);
}

static inline
enum ath12k_dp_tx_enq_error
ath12k_dp_tx_mcast_send(struct ath12k_pdev_dp *dp_pdev,
			struct ath12k_vif *ahvif,
			struct ath12k_dp_link_vif *dp_link_vif,
			u8 ring_id, struct ath12k_dp_tx_msdu_info *msdu_info,
			bool gsn_valid, u16 gsn,
			struct sk_buff *skb, struct ath12k_link_sta *arsta,
			struct ath12k_dp_skb_ctrl *skb_ctrl)
{
	return dp_pdev->dp->arch_ops->dp_tx_mcast_send(dp_pdev, ahvif, dp_link_vif,
						       ring_id, msdu_info, gsn_valid,
						       gsn, skb, arsta,
						       skb_ctrl, false);
}

static inline void ath12k_dp_tx_set_ast(struct ath12k_dp *dp,
					struct ath12k_dp_peer *dp_peer,
					struct ath12k_dp_tx_msdu_info *msdu_info,
					u8 hw_link_id)
{
	dp->arch_ops->dp_tx_set_ast(dp_peer, msdu_info, hw_link_id);
}

static inline int ath12k_dp_qos_queue_setup(struct ath12k_dp *dp,
					    struct ath12k_dp_hw_group *dp_hw_grp,
					    struct ath12k_dp_peer *dp_peer,
					    u8 msduq, u16 qos_id,
					    u8 qm_id, bool txop_intent)
{
	if (dp->arch_ops->dp_qos_queue_setup)
		return dp->arch_ops->dp_qos_queue_setup(dp_hw_grp, dp_peer,
							msduq, qos_id,
							qm_id, txop_intent);
	return -EOPNOTSUPP;
}

static inline int ath12k_dp_arch_get_coredump_seg_count(struct ath12k_dp *dp)
{
	if (dp->arch_ops->get_coredump_seg_count)
		return dp->arch_ops->get_coredump_seg_count(dp);

	return 0;
}

static inline int ath12k_dp_arch_get_coredump_segs(struct ath12k_dp *dp,
						   struct ath12k_dump_segment *seg_arr,
						   int max_segs)
{
	if (dp->arch_ops->get_coredump_segs)
		return dp->arch_ops->get_coredump_segs(dp, seg_arr, max_segs);

	return 0;
}

int ath12k_dp_htt_connect(struct ath12k_dp *dp);
void ath12k_dp_partner_cc_init(struct ath12k_base *ab);
int ath12k_dp_get_pdev_telemetry_stats(struct ath12k_base *ab,
                                      int pdev_id,
                                      struct ath12k_pdev_telemetry_stats *stats);
int ath12k_dp_pdev_pre_alloc(struct ath12k *ar);
int ath12k_dp_tx_htt_srng_setup(struct ath12k_base *ab, u32 ring_id,
				int mac_id, enum hal_ring_type ring_type);
int ath12k_dp_peer_epp_setup_mgmt_tids(struct ath12k *ar, void *ptr,
				       struct ath12k_link_vif *arvif,
				       struct ath12k_link_sta *arsta);
int ath12k_dp_peer_set_default_routing(struct ath12k *ar,
				       struct ath12k_link_vif *arvif,
				       const u8 *addr);
int ath12k_dp_peer_setup(struct ath12k *ar, void *ptr, struct ath12k_link_vif *arvif,
			 const u8 *addr, u8 link_id);
void ath12k_dp_peer_cleanup(struct ath12k *ar, void *ptr, int vdev_id, const u8 *addr);
int ath12k_dp_srng_init(struct ath12k_base *ab, struct dp_srng *ring,
			enum hal_ring_type type, int ring_num, int mac_id);
int ath12k_dp_srng_alloc(struct ath12k_base *ab, struct dp_srng *ring,
			 enum hal_ring_type type, int ring_num,
			 int mac_id, int num_entries);
void ath12k_dp_srng_cleanup(struct ath12k_base *ab, struct dp_srng *ring);
int ath12k_dp_srng_setup(struct ath12k_base *ab, struct dp_srng *ring,
			 enum hal_ring_type type, int ring_num,
			 int mac_id, int num_entries);
void ath12k_dp_link_desc_cleanup(struct ath12k_base *ab,
				 struct dp_link_desc_bank *desc_bank,
				 u32 ring_type, struct dp_srng *ring);
int ath12k_dp_link_desc_alloc(struct ath12k_base *ab,
			      struct dp_link_desc_bank *link_desc_banks,
			      u32 ring_type, struct hal_srng *srng,
			      u32 n_link_desc);
int ath12k_dp_link_desc_init(struct ath12k_base *ab,
			     struct dp_link_desc_bank *link_desc_banks,
			     u32 ring_type, struct hal_srng *srng,
			     u32 n_link_desc);
int ath12k_dp_link_desc_setup(struct ath12k_base *ab,
			      struct dp_link_desc_bank *link_desc_banks,
			      u32 ring_type, struct hal_srng *srng,
			      u32 n_link_desc);
struct ath12k_rx_desc_info *ath12k_dp_get_rx_desc(struct ath12k_dp *dp,
						  u32 cookie);
struct ath12k_tx_desc_info *ath12k_dp_get_tx_desc(struct ath12k_dp *dp,
						  u32 desc_id);
bool ath12k_dp_umac_reset_in_progress(struct ath12k_base *ab);
bool ath12k_dp_wmask_compaction_rx_tlv_supported(struct ath12k_base *ab);
void ath12k_dp_reoq_lut_addr_reset(struct ath12k_dp *dp);
void ath12k_dp_srng_msi_setup(struct ath12k_base *ab,
			      struct hal_srng_params *ring_params,
			      enum hal_ring_type type, int ring_num);
void ath12k_hal_tx_config_rbm_mapping(struct ath12k_base *ab, u8 ring_num,
				      u8 rbm_id, int ring_type);
size_t ath12k_dp_get_req_entries_from_buf_ring(struct ath12k_base *ab,
					       struct hal_srng *srng,
					       struct list_head *list,
						uint8_t pool_type);
void ath12k_dp_tx_ext_desc_free(struct ath12k_dp *dp,
				struct ath12k_tx_desc_info *tx_desc);
int ath12k_dp_bank_profiles_alloc(struct ath12k_base *ab);
void ath12k_dp_bank_profiles_free(struct ath12k_base *ab);
int ath12k_dp_init_bank_profiles(struct ath12k_base *ab);
void ath12k_dp_deinit_bank_profiles(struct ath12k_base *ab);
int ath12k_dp_cc_init(struct ath12k_base *ab);
void ath12k_dp_cc_deinit(struct ath12k_base *ab);
int ath12k_dp_cc_rx_alloc(struct ath12k_base *ab);
void ath12k_dp_cc_rx_free(struct ath12k_base *ab);
int ath12k_wbm_idle_ring_alloc(struct ath12k_base *ab, u32 *n_link_desc);
int ath12k_wbm_idle_ring_init(struct ath12k_base *ab);
int ath12k_wbm_idle_ring_setup(struct ath12k_base *ab, u32 *n_link_desc);
void ath12k_wbm_idle_ring_cleanup(struct ath12k_base *ab);
int ath12k_dp_srng_common_alloc(struct ath12k_base *ab);
int ath12k_dp_srng_common_init(struct ath12k_base *ab);
int ath12k_dp_srng_common_setup(struct ath12k_base *ab);
void ath12k_dp_srng_common_cleanup(struct ath12k_base *ab);
enum ath12k_dp_eapol_key_type ath12k_dp_get_eapol_subtype(u8 *data);
ssize_t ath12k_dp_dump_device_ring_stats(struct ath12k_base *ab,
					 char *buf, int size);
void ath12k_dp_get_device_stats(struct ath12k_dp *dp,
				struct ath12k_telemetry_dp_device *telemetry_device);
int ath12k_dp_get_link_peer_stats(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_dp_peer *peer,
				  int hw_link_id,
				  struct ath12k_telemetry_dp_peer *telemetry_peer);
int ath12k_dp_get_peer_stats(struct ath12k_pdev_dp *dp_pdev,
			     struct ath12k_dp_peer *peer,
			     struct ath12k_telemetry_dp_peer *telemetry_peer,
			     u8 link_id, bool valid_link,
			     unsigned long links_map, int stats_link_id);
void ath12k_dp_get_vif_stats(struct ath12k_vif *ahvif,
			     struct ath12k_telemetry_dp_vif *telemetry_vif,
			     u8 link_id);

/* Context passed through the arsta iterator to the DP stats helper */
struct ath12k_vif_peer_iter_ctx {
	struct ath12k_link_vif          *arvif;
	struct ath12k_dp_aggr_vif_stats *aggr_vif_stats;
};
void ath12k_dp_get_pdev_stats(struct ath12k_pdev_dp *pdev,
			      struct ath12k_telemetry_dp_radio *telemetry_radio);
int ath12k_dp_alloc_proto_stats_vif(struct ath12k_dp_vif *dp_vif);
void ath12k_dp_free_proto_stats_vif(struct ath12k_dp_tx_vif_stats *vif_stats);
int ath12k_dp_alloc_proto_stats(struct ath12k *ar);
int ath12k_dp_alloc_proto_stats_peer(struct ath12k_dp_peer *dp_peer);
void ath12k_dp_free_proto_stats(struct ath12k *ar);
void ath12k_dp_free_proto_stats_peer(struct ath12k_dp_peer *dp_peer);

void ath12k_dp_update_proto_stats_vif(struct ath12k_dp_vif *dp_vif,
				      u8 link_id, struct sk_buff *skb,
				      u8 level, int ring_id);
void ath12k_dp_tx_peer_update_proto_stats(struct ath12k_dp_peer *dp_peer,
					  u8 link_id, struct sk_buff *skb,
					  u8 level, int ring_id);
void ath12k_dp_rx_update_protocol_stats(struct ath12k_dp_peer *dp_peer,
					u8 link_id, struct sk_buff *skb, u8 level,
					int ring_id);
int ath12k_dp_alloc_reoq_lut(struct ath12k_base *ab,
			     struct ath12k_reo_q_addr_lut *lut);
void ath12k_dp_update_vdev_search(struct ath12k_vif *ahvif);
int ath12k_dp_tx_get_bank_profile(struct ath12k_dp *dp, u32 bank_config);
void ath12k_dp_clear_link_desc_pool(struct ath12k_dp *dp);
void ath12k_dp_ppeds_tx_desc_cleanup(struct ath12k_base *ab);
void ath12k_dp_srng_hw_ring_disable(struct ath12k_base *ab);
void ath12k_dp_umac_tx_desc_cleanup(struct ath12k_base *ab);
void ath12k_dp_umac_rx_desc_cleanup(struct ath12k_base *ab);
void ath12k_dp_srng_hw_disable(struct ath12k_base *ab, struct dp_srng *ring);
void ath12k_dp_init_ring_size(struct ath12k_base *ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_ppeds_reinject_handler(struct ath12k_base *ab,
				   struct ath12k_ppeds_tx_desc_info *tx_desc,
				   struct htt_tx_completion *status_desc);
void ath12k_dp_ppeds_tx_comp_get_desc(struct ath12k_base *ab,
				      struct ath12k_dp_tx_comp_status *tx_comp_status,
				      struct ath12k_ppeds_tx_desc_info **tx_desc);
struct ath12k_ppeds_tx_desc_info *ath12k_dp_get_ppeds_tx_desc(struct ath12k_base *ab,
							      u32 desc_id);
int ath12k_dp_ppeds_cc_desc_cleanup(struct ath12k_base *ab);
void ath12k_dp_ppeds_tx_cmem_init(struct ath12k_base *ab);
int ath12k_dp_ppe_rxole_rxdma_cfg(struct ath12k_base *ab);
void ath12k_dp_increment_bank_num_users(struct ath12k_dp *dp,
					int bank_id);
#endif

#ifdef CPTCFG_QCN_EXTN
void ath12k_dp_srng_dst_invalidate_entries(struct ath12k_dp *dp,
					   struct hal_srng *srng,
					   int entries);

dma_addr_t ath12k_dp_rx_buffer_map(struct ath12k_dp *dp,
				   struct ath12k_rx_desc_info *rx_sw_desc);

void ath12k_dp_rx_buffer_unmap(struct ath12k_dp *dp,
			       struct ath12k_rx_desc_info *rx_sw_desc);

struct sk_buff *ath12k_dp_alloc_skb(int size);

void ath12k_dsb(void);
#else
static inline
void ath12k_dp_srng_dst_invalidate_entries(struct ath12k_dp *dp,
					   struct hal_srng *srng,
					   int entries)
{
	u32 tp, hp;
	dma_addr_t desc_paddr;

	if (!(srng->flags & HAL_SRNG_FLAGS_CACHED) || !entries)
		return;

	tp = srng->u.dst_ring.tp;
	hp = srng->u.dst_ring.cached_hp;

	desc_paddr = srng->ring_base_paddr + (tp * sizeof(u32));
	if (hp > tp) {
		dma_sync_single_for_cpu(dp->dev, desc_paddr,
					entries * srng->entry_size * sizeof(u32),
					DMA_FROM_DEVICE);
	} else {
		entries = srng->ring_size - tp;
		dma_sync_single_for_cpu(dp->dev, desc_paddr,
					entries * sizeof(u32),
					DMA_FROM_DEVICE);
		entries = hp;
		dma_sync_single_for_cpu(dp->dev,
					srng->ring_base_paddr,
					entries * sizeof(u32),
					DMA_FROM_DEVICE);
	}
}

static inline
dma_addr_t ath12k_dp_rx_buffer_map(struct ath12k_dp *dp,
				   struct ath12k_rx_desc_info *rx_sw_desc)
{
	dma_addr_t dma_addr;

	dma_addr = dma_map_single(dp->dev, (void *)rx_sw_desc->vaddr,
				  DP_RX_BUFFER_SIZE, DMA_FROM_DEVICE);

	if (dma_mapping_error(dp->dev, dma_addr)) {
		/* increment error stats */
		return DMA_MAPPING_ERROR;
	}

	return dma_addr;
}

static inline
void ath12k_dp_rx_buffer_unmap(struct ath12k_dp *dp,
			       struct ath12k_rx_desc_info *rx_sw_desc)
{
	dma_unmap_single(dp->dev, rx_sw_desc->paddr,
			 DP_RX_BUFFER_SIZE, DMA_FROM_DEVICE);
}

static inline
dma_addr_t ath12k_dp_tx_buffer_map(struct ath12k_dp *dp,
				   struct ath12k_tx_desc_info *tx_sw_desc)
{
	dma_addr_t dma_addr;

	dma_addr = dma_map_single(dp->dev, (void *)tx_sw_desc->skb->data,
				  tx_sw_desc->len, DMA_TO_DEVICE);

	if (dma_mapping_error(dp->dev, dma_addr)) {
		/* increment error stats */
		return DMA_MAPPING_ERROR;
	}

	return dma_addr;
}

#ifdef CPTCFG_QCN_EXTN
static inline
void ath12k_dp_tx_buffer_unmap(struct ath12k_dp *dp,
			       struct ath12k_tx_desc_info *tx_sw_desc)
{
	dma_unmap_single(dp->dev, tx_sw_desc->paddr,
			 tx_sw_desc->length, DMA_TO_DEVICE);
}
#endif /* CPTCFG_QCN_EXTN */

static inline void ath12k_dsb(void)
{
	/* this is empty function as data sync barrier is not needed
	 * as we use map and unmap APIs
	 */
}

static inline
struct sk_buff *ath12k_dp_alloc_skb(int size)
{
	return netdev_alloc_skb_fast(NULL, size);
}

#endif
#endif
