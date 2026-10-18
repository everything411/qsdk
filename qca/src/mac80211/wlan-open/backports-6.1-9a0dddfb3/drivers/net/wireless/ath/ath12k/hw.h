/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_HW_H
#define ATH12K_HW_H

#include <linux/mhi.h>
#include <linux/uuid.h>

#include "wmi.h"
#include "hal.h"

struct ath12k_csi_cfr_header;
struct ath12k_cfr_peer_tx_param;
struct ath12k_cfr_look_up_table;
struct ath12k_mac_pri_link_migr_peer_node;
struct ath12k_hw_group;
struct ath12k_base;
struct ath12k;
struct ath12k_link_vif;

/* Target configuration defines */


/* Num VDEVS per radio */
#define TARGET_NUM_VDEVS		(ab->mem_params.num_vdevs)
/* Maximum number of AP interfaces allowed per radio for MBSSID.
 * This excludes the monitor vdev included in TARGET_NUM_VDEVS.
 */
#define ATH12K_MBSSID_MAX_INTERFACES	((ab->mem_params.num_vdevs) - 1)

/* Num of Bridge vdevs per radio */
#define ATH12K_MAX_NUM_VDEVS_NLINK	(ab->mem_params.num_max_vdevs_nlink)
#define TARGET_NUM_BRIDGE_VDEVS		(ab->mem_params.num_bridge_vdevs)
#define ATH12K_QMI_TARGET_MEM_MODE      (ab->mem_params.target_mem_mode)

#define TARGET_NUM_STATIONS_SINGLE	\
	((ab->mem_params.num_stations_single) ? \
	 (ab->mem_params.num_stations_single) : \
	 ((ath12k_max_clients > ab->hw_params->max_clients_supported) ? \
	  ab->hw_params->max_clients_supported : ath12k_max_clients))

/* Max num of stations for DBS */
#define TARGET_NUM_STATIONS_DBS		\
	((ab->mem_params.num_stations_dbs) ? \
	 (ab->mem_params.num_stations_dbs) : \
	 (((int)(ath12k_max_clients / 2) > ab->hw_params->max_clients_dbs) ? \
	  ab->hw_params->max_clients_dbs : (int)ath12k_max_clients / 2))

/* Max num of stations for DBS_SBS */
#define TARGET_NUM_STATIONS_DBS_SBS	\
	((ab->mem_params.num_stations_dbs_sbs) ? \
	 (ab->mem_params.num_stations_dbs_sbs) : \
	 (((int)(ath12k_max_clients / 3) > ab->hw_params->max_clients_dbs_sbs) ? \
	  ab->hw_params->max_clients_dbs_sbs : (int)ath12k_max_clients / 3))

#if defined(CONFIG_ATH12K_MEM_PROFILE_256M) || defined(CPTCFG_ATH12K_MEM_PROFILE_256M)
#define ATH12K_COLD_BOOT_CALIB		false
#else
#define ATH12K_COLD_BOOT_CALIB		true
#endif

#define TARGET_NUM_PEERS_PDEV_SINGLE	(TARGET_NUM_STATIONS_SINGLE + \
					 TARGET_NUM_VDEVS)
#define TARGET_NUM_PEERS_PDEV_DBS	(TARGET_NUM_STATIONS_DBS + \
					 TARGET_NUM_VDEVS)
#define TARGET_NUM_PEERS_PDEV_DBS_SBS	(TARGET_NUM_STATIONS_DBS_SBS + \
					 TARGET_NUM_VDEVS)

#define TARGET_NUM_BRIDGE_SELF_PEER	TARGET_NUM_BRIDGE_VDEVS

/* Num of peers for Single Radio mode */
#define TARGET_NUM_PEERS_SINGLE		(TARGET_NUM_PEERS_PDEV_SINGLE)

/* Num of peers for DBS */
#define TARGET_NUM_PEERS_DBS		(2 * TARGET_NUM_PEERS_PDEV_DBS)

/* Num of peers for DBS_SBS */
#define TARGET_NUM_PEERS_DBS_SBS	(3 * TARGET_NUM_PEERS_PDEV_DBS_SBS)

#define TARGET_NUM_PEERS(x)	TARGET_NUM_PEERS_##x
#define TARGET_NUM_PEER_KEYS	2
/* Do we need to change the below */
#define TARGET_NUM_TIDS(x)	(2 * TARGET_NUM_PEERS(x) + \
				 4 * TARGET_NUM_VDEVS + 8)

#define TARGET_AST_SKID_LIMIT	16
#define TARGET_NUM_OFFLD_PEERS	4
#define TARGET_NUM_OFFLD_REORDER_BUFFS 4

#define TARGET_TX_CHAIN_MASK	(BIT(0) | BIT(1) | BIT(2) | BIT(4))
#define TARGET_RX_CHAIN_MASK	(BIT(0) | BIT(1) | BIT(2) | BIT(4))
#define TARGET_RX_TIMEOUT_LO_PRI	100
#define TARGET_RX_TIMEOUT_HI_PRI	40

#define TARGET_DECAP_MODE_RAW		0
#define TARGET_DECAP_MODE_NATIVE_WIFI	1
#define TARGET_DECAP_MODE_ETH		2

#define TARGET_SCAN_MAX_PENDING_REQS	4
#define TARGET_BMISS_OFFLOAD_MAX_VDEV	3
#define TARGET_ROAM_OFFLOAD_MAX_VDEV	3
#define TARGET_ROAM_OFFLOAD_MAX_AP_PROFILES	8
#define TARGET_GTK_OFFLOAD_MAX_VDEV	3
#define TARGET_NUM_MCAST_GROUPS		12
#define TARGET_NUM_MCAST_TABLE_ELEMS	64
#define TARGET_MCAST2UCAST_MODE		2
#define TARGET_TX_DBG_LOG_SIZE		1024
#define TARGET_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK 1
#define TARGET_VOW_CONFIG		0
#define TARGET_NUM_MSDU_DESC		(2500)
#define TARGET_MAX_FRAG_ENTRIES		6
#define TARGET_MAX_BCN_OFFLD		16
#define TARGET_NUM_WDS_ENTRIES		32
#define TARGET_DMA_BURST_SIZE		1
#define TARGET_RX_BATCHMODE		1
#define TARGET_EMA_MAX_PROFILE_PERIOD	8
#define TARGET_MIN_MBSSID_GROUP_SIZE	2
#define TARGET_MAX_MBSSID_GROUPS	(TARGET_MAX_BCN_OFFLD / \
					 TARGET_MIN_MBSSID_GROUP_SIZE)

#define ATH12K_HW_DEFAULT_QUEUE		0
#define ATH12K_HW_MAX_QUEUES		MIN(NR_CPUS, 5)
#define ATH12K_QUEUE_LEN		4096

#define ATH12K_HW_RATECODE_CCK_SHORT_PREAM_MASK  0x4

#define ATH12K_FW_DIR			"ath12k"

#define ATH12K_BOARD_MAGIC		"QCA-ATH12K-BOARD"
#define ATH12K_SCAN_RADIO		"QCA-ATH11K-BOARD"
#define ATH12K_BOARD_API2_FILE		"board-2.bin"
#define ATH12K_DEFAULT_BOARD_FILE	"board.bin"
#define ATH12K_DEFAULT_CAL_FILE		"caldata.bin"
#define ATH12K_QMI_DEF_CAL_FILE_PREFIX  "caldata_"
#define ATH12K_QMI_DEF_CAL_FILE_SUFFIX  ".bin"
#define ATH12K_AMSS_FILE		"amss.bin"
#define ATH12K_AMSS_DUALMAC_FILE	"amss_dualmac.bin"
#define ATH12K_M3_FILE			"m3.bin"
#define ATH12K_REGDB_FILE_NAME		"regdb.bin"
#define ATH12K_RXGAINLUT_FILE_PREFIX	"rxgainlut.b"
#define ATH12K_RXGAINLUT_FILE		"rxgainlut.bin"
#define ATH12K_DEFAULT_ID		255
#define ATH12K_FW_CFG_FILE             "fw_ini_cfg.bin"

#define ATH12K_PCIE_MAX_PAYLOAD_SIZE	128
#define ATH12K_IPQ5332_USERPD_ID	1
#define ATH12K_QCN6432_USERPD_ID_1	2
#define ATH12K_QCN6432_USERPD_ID_2	3
#define ATH12K_QCN9160_USERPD_ID_3	4

#define ATH12K_HOST_AFC_QCN6432_MEM_OFFSET 0xD8000
#define ATH12K_MIN_NUM_DEVICES_NLINK 4

#define ATH12K_UMAC_RESET_IPC_IPQ5332   451
#define ATH12K_UMAC_RESET_IPC_QCN6432   7

/* Max Number of Non-Tx AP interfaces allowed per radio for MBSSID */
#define ATH12K_MAX_MBSSID_NONTX_INTERFACES   (ATH12K_MBSSID_MAX_INTERFACES - 1)

enum ath12k_hw_rate_cck {
	ATH12K_HW_RATE_CCK_LP_11M = 0,
	ATH12K_HW_RATE_CCK_LP_5_5M,
	ATH12K_HW_RATE_CCK_LP_2M,
	ATH12K_HW_RATE_CCK_LP_1M,
	ATH12K_HW_RATE_CCK_SP_11M,
	ATH12K_HW_RATE_CCK_SP_5_5M,
	ATH12K_HW_RATE_CCK_SP_2M,
};

enum ath12k_hw_rate_ofdm {
	ATH12K_HW_RATE_OFDM_48M = 0,
	ATH12K_HW_RATE_OFDM_24M,
	ATH12K_HW_RATE_OFDM_12M,
	ATH12K_HW_RATE_OFDM_6M,
	ATH12K_HW_RATE_OFDM_54M,
	ATH12K_HW_RATE_OFDM_36M,
	ATH12K_HW_RATE_OFDM_18M,
	ATH12K_HW_RATE_OFDM_9M,
};

enum ath12k_bus {
	ATH12K_BUS_PCI,
	ATH12K_BUS_AHB,
	ATH12K_BUS_HYBRID,
};

/* nl80211_chan_width slots we validate. */
enum ath12k_spectral_bw_slot {
	ATH12K_SPECTRAL_BW_20MHZ,
	ATH12K_SPECTRAL_BW_40MHZ,
	ATH12K_SPECTRAL_BW_80MHZ,
	ATH12K_SPECTRAL_BW_160MHZ,
	ATH12K_SPECTRAL_BW_320MHZ,
	ATH12K_SPECTRAL_NUM_BW_SLOTS,
};

#define ATH12K_EXT_IRQ_NUM_MAX	22

struct hal_rx_desc;
struct hal_tcl_data_cmd;
struct htt_rx_ring_tlv_filter;
enum hal_encrypt_type;

struct ath12k_hw_ring_mask {
	u8 tx[ATH12K_EXT_IRQ_NUM_MAX];
	u8 rx_mon_dest[ATH12K_EXT_IRQ_NUM_MAX];
	u8 rx_mon_status[ATH12K_EXT_IRQ_NUM_MAX];
	u8 rx[ATH12K_EXT_IRQ_NUM_MAX];
	u8 rx_err[ATH12K_EXT_IRQ_NUM_MAX];
	u8 rx_wbm_rel[ATH12K_EXT_IRQ_NUM_MAX];
	u8 reo_status[ATH12K_EXT_IRQ_NUM_MAX];
	u8 host2rxdma[ATH12K_EXT_IRQ_NUM_MAX];
	u8 tx_mon_dest[ATH12K_EXT_IRQ_NUM_MAX];
	u8 host2txmon[ATH12K_EXT_IRQ_NUM_MAX]; /*host2txmon buff ring*/
	u8 host2rxmon[ATH12K_EXT_IRQ_NUM_MAX];
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	u8 ppe2tcl[ATH12K_EXT_IRQ_NUM_MAX];
	u8 reo2ppe[ATH12K_EXT_IRQ_NUM_MAX];
	u8 ppeds_tx_cmpln[ATH12K_EXT_IRQ_NUM_MAX];
#endif
	u8 umac_dp_reset[ATH12K_EXT_IRQ_NUM_MAX];
	u8 umcmn_interrupts[ATH12K_EXT_IRQ_NUM_MAX];
	u8 tx_exception[ATH12K_EXT_IRQ_NUM_MAX];
	u8 tcl_status[ATH12K_EXT_IRQ_NUM_MAX];
	u8 tqm_status[ATH12K_EXT_IRQ_NUM_MAX];
	u8 sam_status[ATH12K_EXT_IRQ_NUM_MAX];
	u8 ase_status[ATH12K_EXT_IRQ_NUM_MAX];
	u8 tx_peer_telemetry[ATH12K_EXT_IRQ_NUM_MAX];
	u8 rx_peer_telemetry[ATH12K_EXT_IRQ_NUM_MAX];
	u8 reo_flush[ATH12K_EXT_IRQ_NUM_MAX];
	/* Dedicated ring for FW-owned buffer completions (fw_tqm2sw WAR) */
	u8 tqm2sw_fw[ATH12K_EXT_IRQ_NUM_MAX];
};

enum ath12k_m3_fw_loaders {
	ath12k_m3_fw_loader_driver,
	ath12k_m3_fw_loader_remoteproc,
};

struct ath12k_cp_arch_ops {
	int (*cu_mem_pool_init)(struct ath12k_hw_group *ag);
	void (*cu_mem_pool_deinit)(void);
	int (*cu_mem_alloc)(struct ath12k *ar, struct ath12k_link_vif *arvif);
	void (*cu_mem_free)(struct ath12k *ar, struct ath12k_link_vif *arvif);
	void (*cu_notify)(struct ath12k *ar, struct ath12k_link_vif *arvif);
	int (*wsi_bypass_remove)(struct ath12k_base *ab);
	int (*wsi_bypass_add)(struct ath12k_base *ab);
	void (*en_ssr)(struct ath12k_base *ab, unsigned int value);
};


enum ath12k_umcmn_interrupt_handling {
	UMCMN_INTERRUPT_DISABLE,
	UMCMN_INTERRUPT_ENABLE,
	UMCMN_INTERRUPT_POLL,
};

struct ath12k_hw_params {
	const char *name;
	u16 hw_rev;

	struct {
		const char *dir;
		size_t board_size;
		size_t cal_offset;
		enum ath12k_m3_fw_loaders m3_loader;
		bool unified_fw_image:1;
	} fw;

	u16 max_clients_supported;
	u16 max_clients_dbs;
	u16 max_clients_dbs_sbs;

	u8 max_radios;
	bool single_pdev_only:1;
	u32 qmi_service_ins_id;
	bool internal_sleep_clock:1;

	const struct ath12k_hw_ops *hw_ops;
	struct ath12k_hw_ring_mask *ring_mask;

	const struct ce_attr *host_ce_config;
	u32 ce_count;
	const struct ce_pipe_config *target_ce_config;
	u32 target_ce_count;
	const struct service_to_pipe *svc_to_ce_map;
	u32 svc_to_ce_map_len;

	bool rxdma1_enable:1;
	int num_rxdma_per_pdev;
	int num_rxdma_dst_ring;
	bool rx_mac_buf_ring:1;
	bool vdev_start_delay:1;

	struct {
		u8 fft_sz;
		u8 fft_bin_sz;
		u8 fft_pad_sz;
		u8 summary_pad_sz;
		u8 fft_hdr_len;
		u16 max_fft_bins;
		u8 fft_size_min;
		u16 fft_size_max[ATH12K_SPECTRAL_NUM_BW_SLOTS];
		bool fragment_160mhz;
		bool supports_320mhz;
	} spectral;

	u16 interface_modes;
	bool supports_monitor:1;

	bool idle_ps:1;
	bool cold_boot_calib:1;
	bool download_calib:1;
	bool supports_suspend:1;
	bool reoq_lut_support:1;
	bool supports_shadow_regs:1;
	bool supports_aspm:1;
	bool current_cc_support:1;
	bool supports_tx_monitor:1;

	u32 num_tcl_banks;
	u32 max_tx_ring;

	const struct mhi_controller_config *mhi_config;

	void (*wmi_init)(struct ath12k_base *ab,
			 struct ath12k_wmi_resource_config_arg *config);

	const struct hal_ops *hal_ops;
	const struct ath12k_cp_arch_ops *cp_arch_ops;

	u64 qmi_cnss_feature_bitmap;

	u32 rfkill_pin;
	u32 rfkill_cfg;
	u32 rfkill_on_level;

	u32 rddm_size;

	u8 def_num_link;
	u16 max_mlo_peer;

	u32 otp_board_id_register;

	bool supports_sta_ps;

	const guid_t *acpi_guid;
	bool supports_dynamic_smps_6ghz;

	u32 iova_mask;

	const struct ce_ie_addr *ce_ie_addr;
	const struct ce_remap *ce_remap;
	const struct pmm_remap *pmm_remap;
	u32 afc_mem_offset;
	bool send_platform_model;
	bool handle_beacon_miss;
	bool en_qdsslog;
	bool support_fse;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	bool ds_support;
	bool ds_txrx_hw_auto_idx; /* HW index movement of TxRx rings */
	bool ds_hw_buff_mgmt;	/* HW buffer management of Rxfill/Tx completion rings */
#endif

	u8 ext_irq_grp_num_max;
	u8 route_wbm_release;
	bool supports_ap_ps;
	bool support_ce_manual_poll;
	bool ftm_responder;
	bool alloc_cacheable_memory;
	bool credit_flow;
	bool support_umac_reset;
	u16 umac_reset_ipc;
	bool umac_irq_line_reset;
	bool is_plink_preferable;
	bool cfr_support;
	bool cumac_support;
	u8 cumac_chip_priority;
	bool tqm_status_war;
	enum ath12k_umcmn_interrupt_handling support_umcmn_interrupts;
	u32 cfr_dma_hdr_size;
	u32 cfr_num_stream_bufs;
	u32 cfr_stream_buf_size;
	bool mlo_3_link_tx_support;
	bool quad_ring_monitor_support;
	const char *board_magic;
	bool peer_del_all_support;
	u8 tlv_logger_support;
	/*
	 * fw_tqm2sw_war: enable the dedicated fw_tqm2sw ring WAR for
	 * FW-owned buffer completions.  This is a hardware bug present on
	 * Trestles V1 (QCN9625/QCN9589 HW1.0) that is fixed in V2; the WAR
	 * must not be enabled on V2 or later silicon.
	 */
	bool tqm2sw_fw_war;
};

struct ath12k_hw_ops {
	u8 (*get_hw_mac_from_pdev_id)(int pdev_id);
	int (*mac_id_to_pdev_id)(const struct ath12k_hw_params *hw, int mac_id);
	int (*mac_id_to_srng_id)(const struct ath12k_hw_params *hw, int mac_id);
	int (*rxdma_ring_sel_config)(struct ath12k_base *ab);
	u8 (*get_ring_selector)(struct sk_buff *skb);
	bool (*dp_srng_is_tx_comp_ring)(int ring_num);
	void (*fill_cfr_hdr_info)(struct ath12k *ar,
				  struct ath12k_csi_cfr_header *header,
				  struct ath12k_cfr_peer_tx_param *params);
	int (*parse_cfr_enh_dma_hdr)(struct ath12k *ar, u8 *data,
				     struct ath12k_cfr_look_up_table *lut,
				     u32 *length);
	bool (*hw_link_id_required_in_mgmt_send)(struct ath12k_base *ab);
	int (*mgmt_rxdma_ring_sel_config)(struct ath12k_base *ab);
	void (*rx_peer_ba_config)(struct ath12k_base *ab, u8 tid, u32 *ba_win_size,
				  u16 *ssn);
	bool (*rx_peer_tid_skip_pn_replay)(struct ath12k_base *ab, u8 tid);
	void (*dp_peer_migration)(struct ath12k_link_vif *arvif,
				   struct ath12k_mac_pri_link_migr_peer_node *peer_node);
	bool (*is_mgmt_reoq_tid)(u8 tid);
	void (*update_tqm_status_ring_tp)(struct ath12k_base *ab, bool enable);
};

static inline
int ath12k_hw_get_mac_from_pdev_id(const struct ath12k_hw_params *hw,
				   int pdev_idx)
{
	if (hw->hw_ops && hw->hw_ops->get_hw_mac_from_pdev_id)
		return hw->hw_ops->get_hw_mac_from_pdev_id(pdev_idx);

	return 0;
}

static inline int ath12k_hw_mac_id_to_pdev_id(const struct ath12k_hw_params *hw,
					      int mac_id)
{
	if (hw->hw_ops->mac_id_to_pdev_id)
		return hw->hw_ops->mac_id_to_pdev_id(hw, mac_id);

	return 0;
}

static inline int ath12k_hw_mac_id_to_srng_id(const struct ath12k_hw_params *hw,
					      int mac_id)
{
	if (hw->hw_ops->mac_id_to_srng_id)
		return hw->hw_ops->mac_id_to_srng_id(hw, mac_id);

	return 0;
}

static inline bool ath12k_hw_is_mgmt_reoq_tid(const struct ath12k_hw_params *hw,
					      u8 tid)
{
	if (hw->hw_ops->is_mgmt_reoq_tid)
		return hw->hw_ops->is_mgmt_reoq_tid(tid);

	return false;
}

struct ath12k_fw_ie {
	__le32 id;
	__le32 len;
	u8 data[];
};

enum ath12k_bd_ie_board_type {
	ATH12K_BD_IE_BOARD_NAME = 0,
	ATH12K_BD_IE_BOARD_DATA = 1,
};

enum ath12k_bd_ie_regdb_type {
	ATH12K_BD_IE_REGDB_NAME = 0,
	ATH12K_BD_IE_REGDB_DATA = 1,
};

enum ath12k_bd_ie_rxgainlut_type {
	 ATH12K_BD_IE_RXGAINLUT_NAME = 0,
	 ATH12K_BD_IE_RXGAINLUT_DATA = 1,
};

enum ath12k_bd_ie_type {
	/* contains sub IEs of enum ath12k_bd_ie_board_type */
	ATH12K_BD_IE_BOARD = 0,
	/* contains sub IEs of enum ath12k_bd_ie_regdb_type */
	ATH12K_BD_IE_REGDB = 1,
	/* contains sub IEs of enum ath12k_bd_ie_rxgainlut_type */
	ATH12K_BD_IE_RXGAINLUT = 2,
};

enum ath12k_dp_mon_tlv_logger_support {
	ATH12K_TLV_LOGGER_DISABLED = 0,
	ATH12K_TLV_LOGGER_RX_ENABLED = 1,
	ATH12K_TLV_LOGGER_TX_ENABLED = 2,
	ATH12K_TLV_LOGGER_RX_TX_ENABLED = 3,
};

static inline const char *ath12k_bd_ie_type_str(enum ath12k_bd_ie_type type)
{
	switch (type) {
	case ATH12K_BD_IE_BOARD:
		return "board data";
	case ATH12K_BD_IE_REGDB:
		return "regdb data";
	case ATH12K_BD_IE_RXGAINLUT:
		return "rxgainlut data";
	}

	return "unknown";
}

#endif
