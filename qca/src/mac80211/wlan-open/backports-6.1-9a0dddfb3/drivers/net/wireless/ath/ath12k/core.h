/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021, 2026 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_CORE_H
#define ATH12K_CORE_H

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/bitfield.h>
#include <linux/dmi.h>
#include <linux/ctype.h>
#include <linux/firmware.h>
#include <linux/of_reserved_mem.h>
#include <linux/panic_notifier.h>
#include <linux/average.h>
#include <linux/hashtable.h>
#include <linux/rhashtable.h>
#include <linux/rcupdate.h>
#include <linux/smp.h>
#include "qmi.h"
#include "htc.h"
#include "wmi.h"
#include "hal.h"
#include "ce.h"
#include "mac.h"
#include "hw.h"
#include "reg.h"
#include "thermal.h"
#include "dbring.h"
#include "fw.h"
#include "acpi.h"
#include "wow.h"
#include "debugfs_htt_stats.h"
#include "coredump.h"
#include "cmn_defs.h"
#include "spectral.h"
#include "qos.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ath12k_cmn_extn.h"
#include "qcn_extns/vendor_extn.h"
#endif /* CPTCFG_QCN_EXTN */
#include <linux/atomic.h>
#include "event.h"
#include "me.h"
#include "me_hmmc.h"

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include <ppe_ds_wlan.h>
#include <ppe_vp_public.h>
#endif
#include "cfr.h"
#include "pktlog.h"
#include "dp_stats.h"

#ifdef CPTCFG_ATHDEBUG
#include "ath_debug/athdbg_qmi.h"
#endif

#define SM(_v, _f) (((_v) << _f##_LSB) & _f##_MASK)

#define ATH12K_TX_MGMT_NUM_PENDING_MAX	512
#define ATH12K_DP_RX_FSE_FLOW_METADATA_MASK      0xFFFF

#define ATH12K_TX_MGMT_TARGET_MAX_SUPPORT_WMI 64

/* Pending management packets threshold for dropping probe responses */
#define ATH12K_PRB_RSP_DROP_THRESHOLD ((ATH12K_TX_MGMT_TARGET_MAX_SUPPORT_WMI * 3) / 4)

/* Broadcast probe request per-STA rate-limiting */
/* suppress window in ms */
#define ATH12K_BCAST_PROBE_RL_WINDOW_MS		200
/* 256 buckets */
#define ATH12K_BCAST_PROBE_RL_HASH_BITS		8
/* max table entries */
#define ATH12K_BCAST_PROBE_RL_MAX_ENTRIES	1024
/**
 * struct ath12k_bcast_probe_rl_entry - per-STA rate-limit entry
 * @addr:      STA MAC address (probe request SA / addr2)
 * @last_seen: jiffies timestamp of last forwarded probe request
 * @hnode:     hash table linkage (protected by ar->data_lock)
 */
struct ath12k_bcast_probe_rl_entry {
	u8 addr[ETH_ALEN];
	unsigned long last_seen;
	struct hlist_node hnode;
};

/* SMBIOS type containing Board Data File Name Extension */
#define ATH12K_SMBIOS_BDF_EXT_TYPE 0xF8

/* SMBIOS type structure length (excluding strings-set) */
#define ATH12K_SMBIOS_BDF_EXT_LENGTH 0x9

/* The magic used by QCA spec */
#define ATH12K_SMBIOS_BDF_EXT_MAGIC "BDF_"

/* Timeout value for the Peer cleanup during WSI bypass */
#define ATH12K_MAC_PEER_CLEANUP_TIMEOUT_MSECS 60000

#define ATH12K_INVALID_HW_MAC_ID	0xFF
#define ATH12K_CONNECTION_LOSS_HZ	(3 * HZ)
#define ATH12K_SMEM_HOST                1
#define ATH12K_Q6_CRASH_REASON          421

#define ATH12K_MON_TIMER_INTERVAL  10
#define ATH12K_RESET_TIMEOUT_HZ			(20 * HZ)
#define ATH12K_RESET_MAX_FAIL_COUNT_FIRST	3
#define ATH12K_RESET_MAX_FAIL_COUNT_FINAL	5
#define ATH12K_RESET_FAIL_TIMEOUT_HZ		(20 * HZ)
#define ATH12K_RECONFIGURE_TIMEOUT_HZ		(10 * HZ)
#define ATH12K_RECOVER_START_TIMEOUT_HZ		(20 * HZ)

#define ATH12K_INVALID_GROUP_ID  0xFF
#define ATH12K_INVALID_DEVICE_ID 0xFF

#define ATH12K_INVALID_VDEV_ID   "NA"
#define ATH12K_INVALID_RADIO_IDX "NA"

/* Max MLO clients supported in firmware * Max Radios
 */
#define ATH12K_MAX_MLO_PEERS		(512 * ATH12K_MIN_NUM_DEVICES_NLINK)
#define ATH12K_MLO_PEER_ID_INVALID      0xFFFF
#define ATH12K_STA_ID_INVALID		0xFFFF
#define ATH12K_VENDOR_VALID_INTF_BITMAP WMI_DCS_WLAN_INTF

#define ATH12K_TPC_EIRP_DBM_MIN		(-128)
#define ATH12K_TPC_EIRP_DBM_MAX		127
#define ATH12K_TPC_QDBM_PER_DBM		4

#define ATH12K_MAX_ADJACENT_CHIPS   2
#define ATH12K_WSI_MAX_ARGS 4

#define INVALID_CIPHER 0xFFFFFFFF

#define ATH12K_PHY_2GHZ "phy00"
#define ATH12K_PHY_5GHZ "phy01"
#define ATH12K_PHY_5GHZ_LOW "phy01"
#define ATH12K_PHY_5GHZ_HIGH "phy02"
#define ATH12K_PHY_6GHZ "phy03"
#define ATH12K_PHY_6GHZ_LOW "phy03"
#define ATH12K_PHY_6GHZ_HIGH "phy04"
#define ATH12K_PHY_2GHZ_5GHZ "phy05"

#ifdef CPTCFG_QCN_EXTN
/* Scan radio uses a single PHY name for all bands */
#define ATH12K_PHY_SCAN_RADIO "phy-scan-00"
#endif
#define ATH12K_Q6_POWER_UP_TIMEOUT	(20 * HZ)

#define ATH12K_MAX_TID_VALUE 8

/* Maximum number of AKM suites supported:
 * WPA2-PSK (00-0F-AC:4), SAE (00-0F-AC:8), SAE-EXT (00-0F-AC:24),
 * FT-SAE (00-0F-AC:9), FT-SAE-EXT (00-0F-AC:25)
 */
#define ATH12K_MAX_AKM_SUITES		5

#define ATH12K_FREE_MAP_ID_MASK GENMASK(31, 0)

#define ATH12K_GROUP_KEYS_NUM_MAX	128
#define ATH12K_GROUP_KEY_SLOT_INVALID	0xff
#define ATH12K_FREE_GROUP_IDX_MAP_BITS	(BITS_PER_BYTE * (sizeof(long)))
#define ATH12K_FREE_GROUP_IDX_MAP_MAX	(ATH12K_GROUP_KEYS_NUM_MAX /	\
					 ATH12K_FREE_GROUP_IDX_MAP_BITS)

/* Chip power state definitions for partner chip notification */
#define FW_ASSERTED_CHIP_PWR_DOWN 1   /* Partner chip is powering down */
#define FW_ASSERTED_CHIP_PWR_UP   2   /* Partner chip is powering up */

#define ATH12K_MAX_CORE_MASK	(0xFFFF & ((1 << NR_CPUS) - 1))
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
extern unsigned int ath12k_ppe_ds_enabled;
extern unsigned int ath12k_ppe_ds_wifi8_enabled;
#endif
extern unsigned int ath12k_rfs_core_mask[4];
extern unsigned int ath12k_frame_mode;
extern bool ath12k_fse_3_tuple_enabled;
extern bool ath12k_rx_nwifi_err_dump;
extern bool ath12k_upload_dump_before_bugon;
extern bool ath12k_carrier_vow_optimization;
extern unsigned int ath12k_reorder_VI_timeout;
extern bool ath12k_mcast_link_bmap_enable;
extern unsigned int ath12k_max_clients;

/*
 * enum ath12k_mem_profile - active memory profile
 * @ATH12K_MEM_PROFILE_DEFAULT:   1G / full ring sizes  (mem-profile=high or absent)
 * @ATH12K_MEM_PROFILE_BALANCED:  512M / reduced sizes  (mem-profile=balanced)
 * @ATH12K_MEM_PROFILE_OPTIMIZED: 256M / minimal sizes  (mem-profile=optimized)
 */
enum ath12k_mem_profile {
	ATH12K_MEM_PROFILE_DEFAULT,
	ATH12K_MEM_PROFILE_BALANCED,
	ATH12K_MEM_PROFILE_OPTIMIZED,
};

extern enum ath12k_mem_profile ath12k_active_mem_profile;

/**
 * struct ath12k_dp_params - DP-specific ring and resource sizing parameters
 *
 * This structure contains all datapath-specific ring and resource sizing
 * configurations that vary by memory profile.
 */
struct ath12k_dp_params {
	unsigned int rxdma_buf_ring_size;
	unsigned int rx_release_ring_size;
	unsigned int reo2ppe_ring;
	unsigned int ppe2tcl_ring;
	unsigned int tqm2ppe_ring_size;
	unsigned int tx_comp_ppeds_ring_size;
	unsigned int ppe_wbm2sw_ring_size;
	unsigned int rxdma_monitor_buf_ring_size;
	unsigned int rxdma_monitor_dst_ring_size;
	unsigned int smart_mon_filter_default;
	unsigned int mon_num_ppdu_desc;
	unsigned int rx_desc_count;
	unsigned int dp_max_clients;
	unsigned int num_pool_ppeds_tx_desc;
	unsigned int ppeds_hotlist_len_max;
	unsigned int dp_num_clients_max;
	unsigned int dp_mon_status_buf;
	unsigned int tx_monitor_buf_ring_size;
	unsigned int tx_monitor_buf_ring_fill_lvl;
	unsigned int tx_monitor_dst_ring_size;
	unsigned int tx_monitor_num_ppdu_desc;
	unsigned int tx_monitor_num_status_buf;
	unsigned int tx_monitor_spc_bitmap;
	unsigned int reo_dst_ring_size[5];
	unsigned int tcl_data_ring_size[5];
	unsigned int tx_compl_ring_size[5];
	unsigned int tx_desc_count[5];
};

/**
 * struct ath12k_mem_profile_based_param - Memory profile configuration per
 * memory profile.
 *
 * The global pointer ath12k_mem_profile_based_param is set once at
 * module load time by ath12k_detect_mem_profile() based on the
 * mem-profile= kernel cmdline parameter.
 */
struct ath12k_mem_profile_based_param {
	unsigned int num_vdevs;
	unsigned int num_bridge_vdevs;
	unsigned int num_max_vdevs_nlink;
	unsigned int target_mem_mode;
	unsigned int num_stations_single;
	unsigned int num_stations_dbs;
	unsigned int num_stations_dbs_sbs;
	bool monitor_support;
	bool cfr_support;
	bool spectral_support;
	bool tx_monitor_support;
	struct ath12k_dp_params dp_params;
};

extern bool ath12k_mlo_3_link_tx;
extern bool ath12k_waltest_mode;
extern bool ath12k_fw_q6_dump_collection;
extern bool ath12k_hw_group_started;
struct ath12k_dp;
struct ath12k_hp_update_timer;
struct ath12k_mgmt;
struct ath12k_mgmt_irq_grp;

/* Wifi classifier metadata
 * ----------------------------------------------------------------------------
 * | TAG    | mlo_key_valid| sawf_valid| reserved| MLO key | peer_id | MSDUQ   |
 * |(8 bits)|   (1 bit)    |  (1 bit)  | (1 bit) | (5 bits)| (10 bit)| (6 bits)|
 * ----------------------------------------------------------------------------
 */

/**
 ** MLO metadata related information.
 **/
#define ATH12K_SAWF_VALID 1
#define ATH12K_MLO_METADATA_VALID 1
#define ATH12K_MLO_METADATA_VALID_MASK BIT(23)
#define ATH12K_MLO_METADATA_TAG 0xAA
#define ATH12K_MLO_METADATA_MLO_ASSIST_TAG 0xAA800000
#define ATH12K_MLO_METADATA_MLO_ASSIST_TAG_MASK 0xFF800000
#define ATH12K_MLO_METADATA_TAG_MASK GENMASK(31, 24)
#define ATH12K_MLO_METADATA_LINKID_MASK GENMASK(20, 16)
#define ATH12K_MCAST_LINK_BMAP_MASK GENMASK(3, 0)
#define ATH12k_MLO_LINK_ID_INVALID 0xFF
#define ATH12k_DS_NODE_ID_INVALID 0xFF
#define ATH12K_SAWF_PCP_VALID  0x4
#define SAWF_PEER_MSDUQ_INVALID 0xFFFF
#define SAWF_MSDUQ_ID_INVALID   0x3F

enum ath12k_bdf_search {
	ATH12K_BDF_SEARCH_DEFAULT,
	ATH12K_BDF_SEARCH_BUS_AND_BOARD,
};

#define ATH12K_HT_MCS_MAX	7
#ifdef CPTCFG_QCN_EXTN
#define ATH12K_VHT_MCS_10_11_MAX	11
#define ATH12K_VHT_MCS_MAX	ATH12K_VHT_MCS_10_11_MAX
#else
#define ATH12K_VHT_MCS_MAX	9
#endif

#define ATH12K_HE_MCS_MAX	13
#define ATH12K_EHT_MCS_MAX	15
#define ATH12K_UHR_MCS_MAX	23

/**
 * TODO: ATH12K_VHT_MCS_MAX to support MCS-10 and 11 by default
 * when driver support for the same is added.
 */
#define ATH12K_VHT_VENDOR_MCS_MAX 2

/* EHT MCS_NSS_FOR_20_MHZ_ONLY_STA */
#define EHT_MCS_20_MHZ_ONLY_0_7_RX    GENMASK(3, 0)
#define EHT_MCS_20_MHZ_ONLY_0_7_TX    GENMASK(7, 4)
#define EHT_MCS_20_MHZ_ONLY_8_9_RX    GENMASK(11, 8)
#define EHT_MCS_20_MHZ_ONLY_8_9_TX    GENMASK(15, 12)
#define EHT_MCS_20_MHZ_ONLY_10_11_RX  GENMASK(19, 16)
#define EHT_MCS_20_MHZ_ONLY_10_11_TX  GENMASK(23, 20)
#define EHT_MCS_20_MHZ_ONLY_12_13_RX  GENMASK(27, 24)
#define EHT_MCS_20_MHZ_ONLY_12_13_TX  GENMASK(31, 28)

/* EHT MCS_NSS FOR AP MLD */
#define EHT_MCS_NSS_0_9_RX    GENMASK(3, 0)
#define EHT_MCS_NSS_0_9_TX    GENMASK(7, 4)
#define EHT_MCS_NSS_10_11_RX  GENMASK(11, 8)
#define EHT_MCS_NSS_10_11_TX  GENMASK(15, 12)
#define EHT_MCS_NSS_12_13_RX  GENMASK(19, 16)
#define EHT_MCS_NSS_12_13_TX  GENMASK(23, 20)

#define EHT_MCS_NSS_IDX_0 0
#define EHT_MCS_NSS_IDX_1 8
#define EHT_MCS_NSS_IDX_2 16
#define EHT_MCS_NSS_IDX_3 24

#define GET_RX_MCS(eht_map, idx, nss) \
		   (min(u8_get_bits((eht_map) >> (idx), \
		   IEEE80211_EHT_MCS_NSS_RX), nss))

#define GET_TX_MCS(eht_map, idx, nss) \
		   (min(u8_get_bits((eht_map) >> (idx), \
		   IEEE80211_EHT_MCS_NSS_TX), nss))

#define ATH12K_CHAN_SWITCH_RESTART_TIME_DELAY	100
#define ATH12K_CSA_FW_RESTART_TIME_DELAY	50
#define ATH12K_CSA_CALDB_UNDONE_TIME		500
#define ATH12K_CSA_DFS_GRACE_TIME		2000

#define ATH12K_CUMAC_CHIP_ID_INVALID 0xFF

enum ath12k_crypt_mode {
	/* Only use hardware crypto engine */
	ATH12K_CRYPT_MODE_HW,
	/* Only use software crypto */
	ATH12K_CRYPT_MODE_SW,
};

static inline enum wme_ac ath12k_tid_to_ac(u32 tid)
{
	return (((tid == 0) || (tid == 3)) ? WME_AC_BE :
		((tid == 1) || (tid == 2)) ? WME_AC_BK :
		((tid == 4) || (tid == 5)) ? WME_AC_VI :
		WME_AC_VO);
}

static inline u64 ath12k_le32hilo_to_u64(__le32 hi, __le32 lo)
{
	u64 hi64 = le32_to_cpu(hi);
	u64 lo64 = le32_to_cpu(lo);

	return (hi64 << 32) | lo64;
}

enum ath12k_skb_flags {
	ATH12K_SKB_HW_80211_ENCAP = BIT(0),
	ATH12K_SKB_CIPHER_SET = BIT(1),
	ATH12K_SKB_DEAUTH_DISASSOC_TRACKED = BIT(2),
	ATH12K_SKB_MGMT_LINK_AGNOSTIC = BIT(3),
	ATH12K_SKB_CUSTOM_MGMT_TX = BIT(4),
	ATH12K_SKB_CUSTOM_OFFCHAN_MGMT_TX = BIT(5),
#ifdef CPTCFG_EXT_IPA_OFFLOAD
	ATH12K_SKB_IPA_MAP_UNMAP = BIT(6),
#endif
	ATH12K_SKB_MESH_TX_INFO = BIT(7),
	ATH12K_SKB_MESH_RX_INFO = BIT(8),
	ATH12K_SKB_MGMT_MLO_PARAMS = BIT(9),
	ATH12K_SKB_MCAST_LINK_BMAP_VALID = BIT(10),
	ATH12K_SKB_MGMT_SMD_HI_PRI = BIT(11),
};

struct ath12k_skb_cb {
	dma_addr_t paddr;
	union {
		struct ath12k *ar;
		u8 eid;
	} u;
	struct ieee80211_vif *vif;
	dma_addr_t paddr_ext_desc;
	u32 cipher;
	u16 flags;
	u8 link_id;
	u8 mcast_link_bmap;
};

static_assert(sizeof(struct ath12k_skb_cb) <= 48,
	      "size of struct ath12k_skb_cb greater than 48 bytes!");

struct ath12k_skb_rxcb {
	dma_addr_t paddr;
	bool is_first_msdu;
	bool is_last_msdu;
	bool is_continuation;
	bool is_mcbc;
	bool is_eapol;
	bool is_intra_bss;
	struct hal_rx_desc *rx_desc;
	void *mhdr;
	u8 err_rel_src;
	u8 err_code;
	u8 hw_link_id;
	u8 unmapped;
	u8 is_frag;
	u8 tid;
	u16 peer_id;
	bool is_end_of_ppdu;
	u16 flow_tag;      /* fse_metadata[15:0] stamped by FSE HW on FST match */
	u16 protocol_tag;  /* cce_metadata stamped by CCE HW (requires USE_CCE2) */
};

static_assert(sizeof(struct ath12k_skb_rxcb) <= 48,
	      "size of struct ath12k_skb_rxcb greater than 48 bytes!");

enum ath12k_hw_rev {
	ATH12K_HW_QCN9274_HW10 = 0,
	ATH12K_HW_QCN9274_HW20,
	ATH12K_HW_WCN7850_HW20,
	ATH12K_HW_IPQ5332_HW10,
	ATH12K_HW_IPQ5424_HW10,
	ATH12K_HW_QCN6432_HW10,
	ATH12K_HW_QCN9625_HW10,
	ATH12K_HW_QCN9625_HW20,
	ATH12K_HW_QCN9589_HW10,

	/* Always keep extension-specific entries last. */
#ifdef CPTCFG_QCN_EXTN
	ATH12K_HW_QCN9074_HW10,
	ATH12K_HW_QCN9160_HW10,
#endif
};

#define ATH12K_DIAG_HW_ID_OFFSET	16

enum ath12k_firmware_mode {
	/* the default mode, standard 802.11 functionality */
	ATH12K_FIRMWARE_MODE_NORMAL,

	/* factory tests etc */
	ATH12K_FIRMWARE_MODE_FTM,

	/* waltest tests etc */
	ATH12K_FIRMWARE_MODE_WALTEST = 3,

	/* Cold boot calibration */
	ATH12K_FIRMWARE_MODE_COLD_BOOT = 7,
};

extern bool ath12k_cold_boot_cal;

#define ATH12K_IRQ_NUM_MAX 61
#define ATH12K_MAX_TCL_RING_NUM	3

struct ath12k_ext_irq_grp {
	struct ath12k_dp *dp;
	struct ath12k_base *ab;
	u32 irqs[ATH12K_EXT_IRQ_NUM_MAX];
	u32 num_irq;
	u32 grp_id;
	u64 timestamp;
	bool napi_enabled;
	struct napi_struct napi;
#if LINUX_VERSION_IS_GEQ(6,10,0)
	struct net_device *napi_ndev;
#else
	struct net_device napi_ndev;
#endif
	int (*irq_handler)(struct ath12k_dp *dp,
			   struct ath12k_ext_irq_grp *irq_grp, int budget);
};

enum ath12k_smbios_cc_type {
	/* disable country code setting from SMBIOS */
	ATH12K_SMBIOS_CC_DISABLE = 0,

	/* set country code by ANSI country name, based on ISO3166-1 alpha2 */
	ATH12K_SMBIOS_CC_ISO = 1,

	/* worldwide regdomain */
	ATH12K_SMBIOS_CC_WW = 2,
};

enum ath12k_msi_supported_hw {
        ATH12K_MSI_CONFIG_PCI,
        ATH12K_MSI_CONFIG_IPCI,
	/* To support 16 MSI interrupts for PCI devices */
	ATH12K_MSI_CONFIG_PCI_16,
};

struct ath12k_smbios_bdf {
	struct dmi_header hdr;
	u8 features_disabled;

	/* enum ath12k_smbios_cc_type */
	u8 country_code_flag;

	/* To set specific country, you need to set country code
	 * flag=ATH12K_SMBIOS_CC_ISO first, then if country is United
	 * States, then country code value = 0x5553 ("US",'U' = 0x55, 'S'=
	 * 0x53). To set country to INDONESIA, then country code value =
	 * 0x4944 ("IN", 'I'=0x49, 'D'=0x44). If country code flag =
	 * ATH12K_SMBIOS_CC_WW, then you can use worldwide regulatory
	 * setting.
	 */
	u16 cc_code;

	u8 bdf_enabled;
	u8 bdf_ext[];
} __packed;

#define HEHANDLE_CAP_PHYINFO_SIZE       3
#define HECAP_PHYINFO_SIZE              9
#define HECAP_MACINFO_SIZE              5
#define HECAP_TXRX_MCS_NSS_SIZE         2
#define HECAP_PPET16_PPET8_MAX_SIZE     25

#define HE_PPET16_PPET8_SIZE            8

#define ATH12K_MSI_16	16

/* 802.11ax PPE (PPDU packet Extension) threshold */
struct he_ppe_threshold {
	u32 numss_m1;
	u32 ru_mask;
	u32 ppet16_ppet8_ru3_ru0[HE_PPET16_PPET8_SIZE];
};

struct ath12k_he {
	u8 hecap_macinfo[HECAP_MACINFO_SIZE];
	u32 hecap_rxmcsnssmap;
	u32 hecap_txmcsnssmap;
	u32 hecap_phyinfo[HEHANDLE_CAP_PHYINFO_SIZE];
	struct he_ppe_threshold   hecap_ppet;
	u32 heop_param;
};

enum {
	WMI_HOST_TP_SCALE_MAX   = 0,
	WMI_HOST_TP_SCALE_50    = 1,
	WMI_HOST_TP_SCALE_25    = 2,
	WMI_HOST_TP_SCALE_12    = 3,
	WMI_HOST_TP_SCALE_MIN   = 4,
	WMI_HOST_TP_SCALE_SIZE   = 5,
};

enum ath12k_scan_state {
	ATH12K_SCAN_IDLE,
	ATH12K_SCAN_STARTING,
	ATH12K_SCAN_RUNNING,
	ATH12K_SCAN_ABORTING,
};

enum ath12k_11d_state {
	ATH12K_11D_IDLE,
	ATH12K_11D_PREPARING,
	ATH12K_11D_RUNNING,
};

enum ath12k_hw_group_flags {
	ATH12K_GROUP_FLAG_REGISTERED,
	ATH12K_GROUP_FLAG_UNREGISTER,
	ATH12K_GROUP_FLAG_RECOVERY,
	ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED,
	ATH12K_GROUP_FLAG_RAW_MODE,
	ATH12K_GROUP_FLAG_HIF_POWER_DOWN,
	ATH12K_GROUP_FLAG_SHUTDOWN,
};

enum wide_band_cap {
	ATH12K_WIDE_BAND_NONE,
	ATH12K_WIDE_BAND_5GHZ,
	ATH12K_WIDE_BAND_6GHZ,
};

enum ath12k_dev_flags {
	ATH12K_FLAG_CAC_RUNNING,
	ATH12K_FLAG_CRASH_FLUSH,
	ATH12K_FLAG_RECOVERY,
	ATH12K_FLAG_UNREGISTERING,
	ATH12K_FLAG_REGISTERED,
	ATH12K_FLAG_QMI_FAIL,
	ATH12K_FLAG_HTC_SUSPEND_COMPLETE,
	ATH12K_FLAG_CE_IRQ_ENABLED,
	ATH12K_FLAG_EXT_IRQ_ENABLED,
	ATH12K_FLAG_QMI_FW_READY_COMPLETE,
	ATH12K_FLAG_FTM_SEGMENTED,
	ATH12K_FLAG_FIXED_MEM_REGION,
	ATH12K_FLAG_BTCOEX,
	ATH12K_FLAG_WMI_INIT_DONE,
	ATH12K_FLAG_Q6_POWER_DOWN,
	ATH12K_FLAG_PPE_DS_ENABLED,
	ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS,
	ATH12K_FLAG_UMAC_RECOVERY_START,
	ATH12K_FLAG_SOC_CREATE_FAIL,
	ATH12K_FLAG_MGMT_IRQ_ENABLED,
	ATH12K_FLAG_RECOVERY_Q6_BCR,
	ATH12K_FLAG_UCAST_ENABLE_AST_OVERRIDE,
};

enum ath12k_mlo_recovery_mode {
	ATH12K_MLO_RECOVERY_MODE0 = 1,
	ATH12K_MLO_RECOVERY_MODE1 = 2,
	ATH12K_MLO_RECOVERY_MODE2 = 3,
	ATH12K_MLO_RECOVERY_MODE3 = 4,
};

#define ATH12K_STATS_MGMT_FRM_TYPE_MAX 16

struct ath12k_mgmt_frame_stats {
	u32 tx_succ_cnt[ATH12K_STATS_MGMT_FRM_TYPE_MAX];
	u32 tx_fail_cnt[ATH12K_STATS_MGMT_FRM_TYPE_MAX];
	u32 rx_cnt[ATH12K_STATS_MGMT_FRM_TYPE_MAX];
	u32 tx_compl_succ[ATH12K_STATS_MGMT_FRM_TYPE_MAX];
	u32 tx_compl_fail[ATH12K_STATS_MGMT_FRM_TYPE_MAX];
	u64 aggr_tx_mgmt_cnt;
	u64 aggr_rx_mgmt;
	u64 aggr_tx_mgmt_fail_cnt;
	u64 aggr_tx_mgmt_success_cnt;
	u64 tx_cmpl_succ_pkts;
	u64 tx_cmpl_succ_bytes;
	u64 tx_cmpl_retry_pkts;
	u64 tx_cmpl_retry_bytes;
	u64 tx_cmpl_err_pkts;
	u64 tx_cmpl_err_bytes;
	u64 rx_pn_err_cnt;
	u32 rx_assoc_no_rate_match;
	u32 rx_assoc_bad_wpaie;
	u32 rx_assoc_cap_mismatch;
};

struct ath12k_tx_conf {
	bool changed;
	u16 ac;
	struct ieee80211_tx_queue_params tx_queue_params;
};

struct ath12k_key_conf {
	enum set_key_cmd cmd;
	struct list_head list;
	struct ieee80211_sta *sta;
	struct ieee80211_key_conf *key;
};

struct ath12k_cache_qos_map {
	struct ath12k_qos_map *qos_map;
};

struct ath12k_vif_cache {
	struct ath12k_tx_conf tx_conf;
	struct ath12k_key_conf key_conf;
	u32 bss_conf_changed;
	struct ath12k_cache_qos_map cache_qos_map;
};

struct ath12k_rekey_data {
	u8 kck[NL80211_KCK_LEN];
	u8 kek[NL80211_KCK_LEN];
	u64 replay_ctr;
	bool enable_offload;
};

struct ath12k_peer_ch_width_switch_data {
	int count;
	struct wmi_chan_width_peer_arg peer_arg[];
};

struct ath12k_prb_resp_tmpl_ml_info {
	u32 hw_link_id;
	u32 cu_vdev_map_cat1_lo;
	u32 cu_vdev_map_cat1_hi;
	u32 cu_vdev_map_cat2_lo;
	u32 cu_vdev_map_cat2_hi;
};

/* ath12k only deals with 320 MHz, so 16 subchannels */
#define ATH12K_NUM_PWR_LEVELS  16

/**
 * struct chan_power_info - TPE containing power info per channel chunk
 * @chan_cfreq: channel center freq (MHz)
 * e.g.
 * channel 37/20MHz,  it is 6135
 * channel 37/40MHz,  it is 6125
 * channel 37/80MHz,  it is 6145
 * channel 37/160MHz, it is 6185
 * @tx_power: transmit power (dBm)
 */
struct chan_power_info {
	u16 chan_cfreq;
	s8 tx_power;
};

/**
 * struct reg_tpc_power_info - regulatory TPC power info
 * @is_psd_power: is PSD power or not
 * @eirp_power: Maximum EIRP power (dBm), valid only if power is PSD
 * @power_type_6g: type of power (SP/LPI/VLP)
 * @num_pwr_levels: number of power levels
 * @num_psd_pwr_levels: Number of PSD power levels configured for the current
 *                      channel context in SP power mode.
 * @num_eirp_pwr_levels: Number of EIRP levels configured for the current
 *                       channel context in SP power mode.
 * @reg_max: Array of maximum TX power (dBm) per PSD value
 * @ap_constraint_power: AP constraint power (dBm)
 * @tpe_psd: TPE PSD values processed from TPE IE
 * @tpe_eirp: TPE EIRP values processed from TPE IE
 * @num_tpe_psd: number of TPE PSD values parsed
 * @num_tpe_eirp: number of TPE EIRP values parsed
 * @chan_power_info: power info to send to FW
 * @chan_psd_power_info: Array of PSD power information per channel center
 *                       frequency for SP power mode. Each entry includes the
 *                       channel frequency and corresponding PSD power.
 * @chan_eirp_power_info: Array of EIRP power information per channel center
 *                        frequency for SP power mode. Each entry includes the
 *                        frequency and the corresponding EIRP value.
 */
struct ath12k_reg_tpc_power_info {
	bool is_psd_power;
	s8 eirp_power;
	enum wmi_reg_6g_ap_type power_type_6g;
	u8 num_pwr_levels;
	u8 num_psd_pwr_levels;
	u8 num_eirp_pwr_levels;
	u8 reg_max[ATH12K_NUM_PWR_LEVELS];
	u8 ap_constraint_power;
	s8 tpe_psd[IEEE80211_TPE_PSD_ENTRIES_320MHZ];
	s8 tpe_eirp[IEEE80211_TPE_EIRP_ENTRIES_320MHZ];
	u8 num_tpe_psd;
	u8 num_tpe_eirp;
	struct chan_power_info chan_power_info[ATH12K_NUM_PWR_LEVELS];
	struct chan_power_info chan_psd_power_info[ATH12K_NUM_PWR_LEVELS];
	struct chan_power_info chan_eirp_power_info[ATH12K_MAX_EIRP_VALS];
};

struct ath12k_dscp_range {
	u8 low;
	u8 high;
};

struct ath12k_dscp_exception {
	u8 dscp;
	u8 up;
};

struct ath12k_qos_map {
	u8 num_des;
	struct ath12k_dscp_exception dscp_exception[IEEE80211_QOS_MAP_MAX_EX];
	struct ath12k_dscp_range up[ATH12K_MAX_TID_VALUE];
};

enum ath12k_green_ap_mode {
	ATH12K_GREEN_AP_MODE_DISABLED   = 0,
	ATH12K_GREEN_AP_MODE_NO_STA     = 1,
	ATH12K_GREEN_AP_MODE_NUM_STREAM = 2,
};

struct ath12k_vap_cfg {
	u8 rc_num_retries;
	u8 ba_mode;
	u16 max_mtu_size;
	u32 dyn_bw_rts;
	u32 cwm_enable;
	u32 rate_dropdown;
	u32 cts_dtim_bcn;
	u32 cabq_maxdur;
	u32 mcast_rc_stale_period;
	u32 mcast_rc;
	u32 disable_cabq;
	u32 he_snd_mode;
	u32 gtx_enable;
	u32 hwcts2self_ofdma;
	s32 bcn_tx_power;
	u32 ampdu_aggr_size;
	u32 amsdu_aggr_size;
	u32 ba_bufsize;
	u32 protection_mode;
	u32 bw_nss_rate;
	u32 chwidth;
	u32 nss;
	u32 fixed_rate;
	u32 ul_fixed_rate;
	u32 fixed_vht_mcs;
	u32 fixed_he_mcs;
	u32 fixed_eht_mcs;
	u32 ul_fixed_he_mcs;
	u32 ul_fixed_eht_mcs;
	u32 eht_ul_ldpc;
	u32 eht_ul_ltf;
	u32 eht_ltf;
	u32 eht_ul_nss;
	u32 eht_ul_ppdu_bw;
	u32 eht_ul_shortgi;
	u32 eht_ul_stbc;
	u32 extra_eht_ltf;
	u32 he_ltf;
	u32 he_ul_ltf;
	u32 he_ar_gi_ltf;
	u32 he_ar_ldpc;
	u32 he_rtsthrshld;
	u32 enablertscts;
	u32 he_dcm;
	u32 he_extrange;
	u32 he_ul_ldpc;
	u32 he_ul_nss;
	u32 he_ul_ppdu_bw;
	u32 he_ul_shortgi;
	u32 he_ul_stbc;
	u32 rtscts_rate;
	u32 vht_sgimask;
	u32 vht80_rate;
	u32 dis_lpi_ant_optimize;
	enum ath12k_green_ap_mode ap_ps_on;
	u32 ps_timeout;
	/* Per-link EHT OFDMA/TXBF override */
	bool eht_dl_ofdma_configured;
	bool eht_dl_ofdma;
	bool eht_ul_ofdma_configured;
	bool eht_ul_ofdma;
	bool eht_dl_ofdma_txbf_configured;
	bool eht_dl_ofdma_txbf;
	/* Per-link HE OFDMA/TXBF override */
	bool he_dl_ofdma_configured;
	bool he_dl_ofdma;
	bool he_ul_ofdma_configured;
	bool he_ul_ofdma;
	bool he_dl_ofdma_txbf_configured;
	bool he_dl_ofdma_txbf;
};

struct ath12k_rssi_deauth_config {
	bool enabled;
	s8 rssi_threshold;
	u32 grace_samples;
	s8 noise_floor_offset;
} __packed;

#define ATH12K_MAX_NUM_VDEVS 17

struct ath12k_mbssid_info {
	int tx_vdev_id;
	u32 nontx_cnt;
	DECLARE_BITMAP(nontx_vdev_bmap, ATH12K_MAX_NUM_VDEVS);
};

struct ath12k_smd_params {
	bool smd_enabled;
	u8 smd_identifier[ETH_ALEN];
	u8 smd_timeout;
	bool dl_data_fwd;
	u8 max_num_of_peer_apmlds;
	bool smd_type;
	bool ptk_mode;
};

struct ath12k_uhr_cu_info {
	bool started;
	/* bitmap of enum nl80211_cu_state; sized to hold all defined values */
	DECLARE_BITMAP(pending_states, NUM_NL80211_CU_STATES);
	u32 mode_present;
	u32 npca_freq;
	u16 npca_puncture_bitmap;
};

#define ATH12K_FLAG_BEACON_RECEIVED     0

struct ath12k_rtt_pasn_peer {
	struct list_head list;
	u8 src_addr[ETH_ALEN];
	u8 peer_addr[ETH_ALEN];
	bool ltf_keyseed_required;
	u8 security_mode;
	u8 flags;
};

/**
 * struct ath12k_rtt_context - per-link RTT/PASN state.
 * Embedded in ath12k_link_vif.
 */
struct ath12k_rtt_context {
	u32 rtt_responder_role;
	struct list_head pasn_peer_list;
	spinlock_t pasn_peer_lock; /* protects pasn_peer_list */
	struct wiphy_work pasn_fw_peer_create_work;
};

/**
 * struct ath12k_rtt_capab - per-radio RTT extended capability state.
 * Embedded in struct ath12k.
 */
struct ath12k_rtt_capab {
	const struct wiphy_iftype_ext_capab *rtt_iftype_ext_capab_orig;
	unsigned int rtt_num_iftype_ext_capab_orig;
	struct wiphy_iftype_ext_capab *rtt_iftype_ext_capab;
	u8 *rtt_ap_ext_capab;
};

struct ath12k_link_vif {
	u32 vdev_id;
	u32 beacon_interval;
	u32 beacon_tx_rate;
	u32 dtim_period;

	struct ath12k *ar;

	struct wmi_wmm_params_all_arg wmm_params;
	struct wmi_wmm_params_all_arg muedca_params;
	struct list_head list;

	bool is_created;
	bool is_started;
	bool is_up;
	bool smd_prep_vdev_stopped;
	u8 bssid[ETH_ALEN];
	u8 addr[ETH_ALEN];
	struct cfg80211_bitrate_mask bitrate_mask;
	struct delayed_work connection_loss_work;
	int num_legacy_stations;
	int rtscts_prot_mode;
	int txpower;
	int tpc_ie_eirp;
	bool rsnie_present;
	bool wpaie_present;
	struct ieee80211_chanctx_conf chanctx;
	struct ath12k_reg_tpc_power_info reg_tpc_info;
	u8 vdev_stats_id;
	u32 punct_bitmap;
	u8 link_id;
	struct ath12k_vif *ahvif;
	struct ath12k_rekey_data rekey_data;
	unsigned long beacon_flags;

	struct ath12k_rssi_deauth_config rssi_deauth_cfg;

	u8 current_cntdown_counter;
	struct ath12k_link_stats link_stats;
	spinlock_t link_stats_lock; /* Protects updates to link_stats */
	bool is_scan_vif;
	bool is_mlprobe_scan_vif;
	u32 key_cipher;
	int ppe_vp_profile_idx;
	int splitphy_ds_bank_id;
	bool primary_sta_link;
	/* Add per link DS specific information here */
	bool nawds_support;
	bool spectral_enabled;
	u32 vht_cap;
	/* EHT Duplicate in 6 GHz (MCS14) per-VAP enable. Default: false.
	 * Set via bss_eht_mcs14_dup_in_6ghz in hostapd.conf or
	 * hostapd_cli set_eht_mcs14_dup_in_6ghz. Requires eht_dup_6ghz_supp
	 * on the pdev (FW hardware support).
	 */
	bool eht_mcs14_dup_in_6ghz;
	bool mvr_processing;
	enum wmi_vdev_subtype vdev_subtype;
#ifdef CPTCFG_ATH12K_DEBUGFS
	struct dentry *debugfs_twt;
#endif /* CPTCFG_ATH12K_DEBUGFS */
	struct dentry *debugfs_power_save_gtx;
	bool power_save_gtx;
	u64 obss_color_bitmap;
	struct wiphy_work update_obss_color_notify_work;
	struct wiphy_work update_bcn_template_work;
	struct ath12k_uhr_cu_info uhr_ecu;
	struct wiphy_work uhr_cu_notify_work;
	bool beacon_prot;
	bool control_frame_prot;
	u64 tbtt_offset;
	int num_stations;

	u16 last_vht_tx_mcs_map;
	u32 last_ht_tx_mcs_map;
	struct ieee80211_eht_mcs_nss_supp bcn_eht_mcs_map;
	struct ieee80211_he_mcs_nss_supp bcn_he_mcs_map;
	struct completion peer_ch_width_switch_send;
	struct wiphy_work peer_ch_width_switch_work;
	struct ath12k_peer_ch_width_switch_data *peer_ch_width_switch_data;
	bool pending_tx_vdev_up;
	u32 tx_vdev_id;
	struct ath12k_prb_resp_tmpl_ml_info ml_info;
	bool ftm_responder;
	/* will be saved to use during recovery */
	struct ieee80211_key_conf *keys[WMI_MAX_KEY_INDEX + 1];

	struct work_struct wmi_migration_cmd_work;
	struct completion wmi_migration_event_resp;
	struct list_head peer_migrate_list;
	bool is_umac_migration_in_progress;
	bool is_link_removal_in_progress;
	bool is_link_removal_update_pending;
	struct ath12k_wmi_mlo_link_removal_event_params link_removal_data;
	bool update_skip_link;
	struct ath12k_qos_map *qos_map;
	struct wiphy_work set_dscp_tid_work;
	bool set_wds_vdev_param;
	int num_peers;
	/* Number of real (non-bridge) MLO peers on this VAP.
	 * Only incremented for AP-mode vdevs; STA-mode vdevs are excluded
	 * because their peers are deleted via individual peer-delete responses,
	 * not peer_delete_all.  Used to snapshot num_ml_peers_del_all before
	 * sending peer_delete_all so mlo_peer_count is decremented correctly
	 * in the response handler.
	 */
	u32 num_ml_peers;
	/* Snapshot of num_ml_peers taken just before ath12k_wmi_peer_delete_all()
	 * is sent.  Read by the async WMI_PEER_DELETE_ALL_RESP handler to
	 * decrement mlo_peer_count by the correct count.  Cleared after use.
	 */
	u32 num_ml_peers_del_all;
	struct wiphy_work update_bcn_tx_status_work;
	struct wiphy_work tpc_ie_eirp_work;
	struct ath12k_vap_cfg vap_cfg;
	u8 gtk_pn[IEEE80211_MAX_PN_LEN];
	u8 bigtk_pn[IEEE80211_MAX_PN_LEN];
	u8 last_installed_gtk_keyix;
	u8 last_installed_bigtk_keyix;
	DECLARE_BITMAP(free_groupidx_map, ATH12K_GROUP_KEYS_NUM_MAX);
	bool bcast_rate_configured;
	u32 bcast_rate;
	u64 rx_pn_err_cnt;
	/* Flag to enable peer_del_all optimization when link is going down */
	bool peer_del_all_enable;
	bool mode3_t2lm_anchor;

	/* shared memory to firmware for critical update procedure */
	struct ath12k_cu_mem *cu_mem;

	/* arsta for the BSS self peer (vdev peer)
	 * NULL if not yet created or already freed.
	 * Protected by wiphy->mtx.
	 */
	struct ath12k_link_sta *self_arsta;

	dma_addr_t cu_mem_paddr;
	u32 nav_status;
	u32 nav_threshold;

	/* Flag to track vdev fixed rate is set or not */
	bool fixed_rate_set;

	/* Structure to store an MBSSID set info */
	struct ath12k_mbssid_info *mbssid_info;

	struct ath12k_smd_params smd_params;
	struct ath12k_rtt_context rtt_ctx;
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_link_vif_extn arvif_extn;
#endif /* CPTCFG_QCN_EXTN */
	bool secured_bss;
};

struct ath12k_dp_link_vif {
	u32 vdev_id;
	bool nawds_support;
	u8 link_id;
	u8 pdev_idx;
	u16 ast_idx;
	u16 ast_hash;
	u16 tcl_metadata;
	u8 lmac_id;
	int bank_id;
	enum wmi_phy_mode phymode;
	struct ath12k_dp_preserved_stats link_peer_delete_stats;
	struct ath12k_base *ab;
};

struct ath12k_vlan_iface {
	struct list_head list;
	struct ieee80211_vif *parent_vif;
	bool attach_link_done;
	int ppe_vp_profile_idx[ATH12K_NUM_MAX_LINKS];
	u8 grp_key_slot[ATH12K_NUM_MAX_LINKS];
	bool is_wds_4addr;
};

struct ath12k_dp_vif {
	u32 dp_features;
	u8 tx_encap_type;
	u8 rx_decap_type;
	u8 search_type;
	u8 hal_addr_search_flags;
	u8 vdev_id_check_en;
	u16 dp_vif_id;
	bool is_wds_4addr;
	u32 links_map;
	int bank_id;
	u16 ast_idx;
	u16 ast_hash;
	u32 key_cipher;
	atomic_t mcbc_gsn;
	struct ath12k_dp_link_vif dp_link_vif[ATH12K_NUM_MAX_LINKS];
	struct ath12k_dp_tx_vif_stats stats[DP_TCL_NUM_RING_MAX];
	struct ath12k_dp_rx_vif_stats rx_stats[DP_REO_DST_RING_MAX];
	struct ath12k_me_db __rcu *me_db;

	/* PPE mode independent variables */
	int ppe_vp_num;
	int ppe_core_mask;
	u8 ppe_vp_type;
	u8 mscs_hlos_tid_override;
	u32 monitor_flags;
	struct ath12k_dp_preserved_stats link_vif_delete_stats;

#ifdef CPTCFG_QCN_EXTN
	struct ath12k_dp_vif_extn dp_extn;
#endif /* CPTCFG_QCN_EXTN */
	u8 ahvif_id;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	int ppe_vp_profile_idx;
#endif
};

enum ath12k_tx_pkt_reasons {
	ATH_TX_COMPLETED_PKTS,
	ATH_TX_SFE_PKTS,
	ATH_TX_MCAST_PKTS,
	ATH_TX_EAPOL_PKTS,
	ATH_TX_NULL_PKTS,
	ATH_TX_UNICAST_PKTS,
	ATH_TX_NULL_COMPLETE_PKTS,
	ATH_TX_FAST_UNICAST,
	ATH_TX_WBM_REL_SRC,
	ATH_TX_FW_STATUS,
	ATH_TX_PPEDS_PKTS,
	ATH_TX_PKT_REASON_MAX
};

enum ath12k_tx_drop_reasons {
	ATH_TX_DUP_DESC,
	ATH_TX_BUF_ERR,
	ATH_TX_DESC_ERR,
	ATH_TX_MISC_FAIL,
	ATH_TX_DESC_NA_ERR,
	ATH_TX_TQM_REMOVE_MPDU,
	ATH_TX_TQM_THRESHOLD,
	ATH_TX_TQM_REMOVE_AGED,
	ATH_TX_TQM_REMOVE_TX,
	ATH_TX_TQM_REMOVE_DEF,
	ATH_TX_DS_TQM_REMOVE_MPDU,
	ATH_TX_DS_TQM_DROP_THRESHOLD,
	ATH_TX_DS_TQM_REMOVE_TX,
	ATH_TX_DS_TQM_REMOVE_AGED,
	ATH_TX_DS_TQM_REMOVE_DEF,
	ATH_TX_DROP_REASON_MAX
};

enum ath12k_rx_pkt_reasons {
	ATH_RX_TOTAL_OUT_PKTS,
	ATH_RX_TOTAL_PKTS,
	ATH_RX_FRAG_PKTS,
	ATH_RX_REO_PKTS,
	ATH_RX_WBM_REL_TOTAL,
	ATH_RX_REO_ERR_PKTS,
	ATH_RX_RXDMA_PKTS,
	ATH_RX_NATIVE_WIFI_PKTS,
	ATH_RX_RAW_PKTS,
	ATH_RX_ETH_PKTS,
	ATH_RX_8023_PKTS,
	ATH_RX_PPE_VP_PKTS,
	ATH_RX_HW_PKTS,
	ATH_RX_SFE_PKTS,
	ATH_RX_DS_PKTS,
	ATH_RX_PKT_REASON_MAX
};

enum ath12k_rx_drop_reasons {
	ATH_RX_DESC_ADDR_ZERO,
	ATH_RX_MSDU_BIT_MISS,
	ATH_RX_INVALID_RBM,
	ATH_RX_NULL_Q_DESC,
	ATH_RX_REO_ERR,
	ATH_RX_TKIP_MIC_ERR,
	ATH_RX_INVALID_RATE,
	ATH_RX_UNAUTH_WDS_ERR,
	ATH_RX_ECHO_ERR,
	ATH_RX_RXDMA_ERR,
	ATH_RX_RBM_ERR,
	ATH_RX_3AADR_DUP,
	ATH_RX_INV_HDR_LEN,
	ATH_RX_DESC_INVALID,
	ATH_RX_NON_BA,
	ATH_RX_NON_BA_DUP,
	ATH_RX_BA_DUP,
	ATH_RX_2K_JUMP,
	ATH_RX_ERR_OOR,
	ATH_RX_NO_BA,
	ATH_RX_EQUALS_SSN,
	ATH_RX_ERR_FLAG_SET,
	ATH_RX_DESC_BLOCKED,
	ATH_RX_PN_FAIL,
	ATH_RX_ERR_UNKNOWN,
	ATH_RX_DROP_REASON_MAX
};

struct tid_netstats {
	u64 tx_packets;
	u64 tx_bytes;
	u64 tx_pkt_stats[ATH_TX_PKT_REASON_MAX];
	u64 tx_pkt_bytes[ATH_TX_PKT_REASON_MAX];
	u64 tx_drop_stats[ATH_TX_DROP_REASON_MAX];
	u64 tx_drop_bytes[ATH_TX_DROP_REASON_MAX];
	u64 rx_packets;
	u64 rx_bytes;
	u64 rx_pkt_stats[ATH_RX_PKT_REASON_MAX];
	u64 rx_pkt_bytes[ATH_RX_PKT_REASON_MAX];
	u64 rx_drop_stats[ATH_RX_DROP_REASON_MAX];
	u64 rx_drop_bytes[ATH_RX_DROP_REASON_MAX];
};

struct pcpu_netdev_tid_stats {
	struct tid_netstats tid_stats[IEEE80211_NUM_TIDS];
	struct u64_stats_sync   syncp;
};

struct netdev_tid_stats {
	struct tid_netstats tid_stats[IEEE80211_NUM_TIDS];
};

/* Generic VIF Event Types */
enum ath12k_vif_event_type {
	ATH12K_VIF_EVENT_TYPE_PEER,
};

struct ath12k_vif {
	/* Should be the first member in the structure */
	struct ath12k_dp_vif dp_vif;
	enum wmi_vdev_type vdev_type;
	struct ieee80211_vif *vif;
	struct ath12k_hw *ah;

#ifdef CPTCFG_QCN_EXTN
	struct ath12k_vif_extn ath12k_vif_extn;
#endif /* CPTCFG_QCN_EXTN */
	u8 vap_submode;

	struct dentry *debugfs_rfs_core_mask;

	union {
		struct {
			u32 uapsd;
		} sta;
		struct {
			/* 127 stations; wmi limit */
			u8 tim_bitmap[16];
			u8 tim_len;
			u32 ssid_len;
			u8 ssid[IEEE80211_MAX_SSID_LEN];
			bool dynamic_vlan;
			bool hidden_ssid;
			/* P2P_IE with NoA attribute for P2P_GO case */
			u32 noa_len;
			u8 *noa_data;
		} ap;
	} u;

	u32 aid;
	bool ps;

	struct ath12k_link_vif deflink;
	struct ath12k_link_vif __rcu *link[ATH12K_NUM_MAX_LINKS];
	struct ath12k_vif_cache *cache[IEEE80211_MLD_MAX_NUM_LINKS];
	/* Per-link Green AP config indexed by transient IEEE link_id.
	 * Updated on every SET operation and cleared when the link is removed.
	 * Used to propagate the current configuration into a newly initialised
	 * arvif during link creation.
	 */
	enum ath12k_green_ap_mode ap_ps_on[ATH12K_NUM_MAX_LINKS];
	u32 ps_timeout[ATH12K_NUM_MAX_LINKS];
	/* Per-radio Green AP config indexed by stable hardware radio index
	 * (ar->hw_link_id).  Written on SET and on link removal so that the
	 * configuration survives ML link teardown and re-add, where the
	 * IEEE link_id assigned to a radio may change between cycles.
	 */
	enum ath12k_green_ap_mode ap_ps_on_by_hwlink[ATH12K_GROUP_MAX_RADIO];
	u32 ps_timeout_by_hwlink[ATH12K_GROUP_MAX_RADIO];
	/* indicates bitmap of link vif created in FW */
	u32 links_map;
	u8 last_scan_link;
	u8 roc_link_id;
	struct ath12k_vlan_iface *vlan_iface;
	bool mode0_recover_bridge_vdevs;
	bool mode3_t2lm_sent;
	u8 device_bitmap;
	bool chanctx_peer_del_done;
	u8 primary_link_id;
	u8 hw_link_id;
	u8 map_id;
	struct ath12k_wmm_stats wmm_stats;
	bool overide_primary_umac;
	struct work_struct disable_sg_netdev_work;
	bool disable_sg;
#ifdef CPTCFG_ATH12K_DEBUGFS
	struct dentry *debugfs_primary_link;
	struct dentry *debugfs_wmm_stats_vdev;
	struct dentry *debugfs_reset_wmm_stats;
	struct pcpu_netdev_tid_stats __percpu *tstats;
	struct dentry *debugfs_vdev_tid_stats;
	struct dentry *debugfs_reset_dp_tid_stats;
#endif /* CPTCFG_ATH12K_DEBUGFS */

	struct ath12k_mgmt_frame_stats mgmt_stats;
	u16 repurposed_links;
	/* Bitmask of link slots NULLed by ath12k_smd_remap_vif_links() because
	 * those slots were remapped to different tap indices.  Used by
	 * ath12k_mac_op_change_vif_links() to distinguish an expected NULL
	 * (remap-cleared) from an unexpected one (driver bug).
	 */
	u16 smd_remap_cleared_links;

	/* Must be last - ends in a flexible-array member.
	 *
	 * FIXME: Driver should not copy struct ieee80211_chanctx_conf,
	 * especially because it has a flexible array. Find a better way.
	 */
	struct ieee80211_chanctx_conf chanctx;
	struct ath12k_reg_tpc_power_info reg_tpc_info;
	struct ath12k_event_queue event_queue;
	/* SMD BSS Transition state */
	struct {
		bool exec_in_progress;
		u8 target_mld_addr[ETH_ALEN];
		u16 transitioning_links;
		u8 primary_link_id;
	} smd;
};

struct ath12k_vif_iter {
	u32 vdev_id;
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;
};

struct ath12k_bridge_iter {
	struct ath12k_hw *ah;
	u8 active_num_devices;
};

struct ath12k_vif_chanctx_iter {
	struct ath12k *ar;
	struct ieee80211_chanctx_conf *chanctx;
};

#define ATH12K_SCAN_TIMEOUT_HZ (20 * HZ)

enum ath12k_pkt_rx_err {
	ATH12K_PKT_RX_ERR_FCS,
	ATH12K_PKT_RX_ERR_TKIP,
	ATH12K_PKT_RX_ERR_CRYPT,
	ATH12K_PKT_RX_ERR_PEER_IDX_INVAL,
	ATH12K_PKT_RX_ERR_MAX,
};

enum ath12k_ampdu_subfrm_num {
	ATH12K_AMPDU_SUBFRM_NUM_10,
	ATH12K_AMPDU_SUBFRM_NUM_20,
	ATH12K_AMPDU_SUBFRM_NUM_30,
	ATH12K_AMPDU_SUBFRM_NUM_40,
	ATH12K_AMPDU_SUBFRM_NUM_50,
	ATH12K_AMPDU_SUBFRM_NUM_60,
	ATH12K_AMPDU_SUBFRM_NUM_MORE,
	ATH12K_AMPDU_SUBFRM_NUM_MAX,
};

enum ath12k_amsdu_subfrm_num {
	ATH12K_AMSDU_SUBFRM_NUM_1,
	ATH12K_AMSDU_SUBFRM_NUM_2,
	ATH12K_AMSDU_SUBFRM_NUM_3,
	ATH12K_AMSDU_SUBFRM_NUM_4,
	ATH12K_AMSDU_SUBFRM_NUM_MORE,
	ATH12K_AMSDU_SUBFRM_NUM_MAX,
};

struct ath12k_per_peer_cfr_capture {
	u32 cfr_enable;
	u32 cfr_period;
	u32 cfr_bandwidth;
	u32 cfr_method;
};

struct ath12k_per_ppdu_tx_stats {
	u16 succ_pkts;
	u16 failed_pkts;
	u16 retry_pkts;
	u32 succ_bytes;
	u32 failed_bytes;
	u32 retry_bytes;
};

struct ath12k_link_sta {
	struct ath12k_link_vif *arvif;
	struct ath12k_sta *ahsta;
	/* Deauth/disassoc management frames sent and awaiting tx completion */
	atomic_t pending_deauth_disassoc_tx;

	/* link address similar to ieee80211_link_sta */
	u8 addr[ETH_ALEN];

	/* the following are protected by ar->data_lock */
	u32 changed; /* IEEE80211_RC_* */
	u32 bw;
	u32 nss;
	u32 smps;

	/* Peer power-save tracking */
	enum ath12k_wmi_peer_ps_state peer_ps_state;
	u64 ps_start_time;
	u64 ps_start_jiffies;
	u64 ps_total_duration;
	u32 ps_state;
	bool peer_current_ps_valid;

	struct wiphy_work update_wk;
	u8 link_id;
	u32 bw_prev;
	u32 peer_nss;
	s8 rssi_beacon;
	s8 chain_signal[IEEE80211_MAX_CHAINS];

	/* For now the assoc link will be considered primary */
	bool is_assoc_link;

	 /* for firmware use only */
	u8 link_idx;

	/* peer addr based hash list node */
	struct hlist_node hlist_addr;

	u16 tcl_metadata;
	u16 ast_hash;
	u16 ast_idx;

	bool is_bridge_peer;
	bool smd_roam_sta;
	/* true when this arsta represents the BSS self peer (vdev peer),
	 * ahsta is NULL in this case.
	 */
	bool is_self_peer;
	/* set when this arsta was counted in num_stations_multistream;
	 * cached at add time so dec path doesn't need link_sta which may
	 * already be NULL when station_post_remove fires at teardown.
	 */
	bool is_multistream;
	/* will be saved to use during recovery */
	struct ieee80211_key_conf *keys[WMI_MAX_KEY_INDEX + 1];
#ifdef CPTCFG_ATH12K_CFR
	struct ath12k_per_peer_cfr_capture cfr_capture;
#endif
#ifdef CPTCFG_QCN_EXTN
	/* ath12k_link_sta extension structure */
	struct ath12k_link_sta_extn arsta_extn;
#endif
	s8 min_rssi;
	s8 max_rssi;

	bool is_mode3_link_recovery;
	enum wmi_phy_mode phymode;
	bool is_secured_peer;
};

struct ath12k_sta_migration_data {
	struct ath12k *ar;
	u8 link_addr[ETH_ALEN];
	u16 vdev_id;
	u16 peer_id;
	u16 ml_peer_id;
	u8 pdev_id;
	u8 chip_id;
	int ppe_vp_num;
};

enum ath12k_smd_tid {
	ATH12K_SMD_TX_MGMT_TID = 15,
	ATH12K_SMD_RX_MGMT_TID = 16,
};

struct ath12k_ba_session_params {
	u8 tid;
	u16  buf_size;
	u16  ssn;
	u16  timeout;
	bool amsdu;
	enum ieee80211_ba_policy policy;
	bool valid;
};

struct ath12k_smd_info {
	/* protects below resources */
	spinlock_t smd_lock;
	u8 st_control;
	bool torndown;
	bool ctx_inflight;
	bool latest_ctx_valid;
	ktime_t latest_ctx_ts;
	struct list_head ctx_list;
	struct ath12k_smd_ctx latest_ctx;
	struct ath12k_smd_ctx_req *current_req;

	struct work_struct ctx_wk;


	/* for WMI smd roam config cmd */
	u16 sn[IEEE80211_MAX_NUM_TIDS];
	u16 lsn_offset[IEEE80211_MAX_NUM_TIDS];
	u32 tx_ba_buf_size[IEEE80211_MAX_NUM_TIDS];
	u32 rx_ba_buf_size[IEEE80211_MAX_NUM_TIDS];
};

struct ath12k_mode3_recovery_params {
	u8   new_master_ll_id;
	u8   new_primary_hwlink_id;
	u8   assoc_link;
	u8   asserted_hw_link_id;
	bool master_crashed;
	bool asserted_peer_pending;
	bool mode3_processed;
	u16  asserted_links;
	u16  removed_links;
	u16  added_links;
};

struct ath12k_sta {
	struct ath12k_vif *ahvif;
	enum hal_pn_type pn_type;
	enum hal_encrypt_type enctype;
	/* MLD address (or link address for non-MLO); key for ag->ahsta_list */
	u8 addr[ETH_ALEN];
	/* node for ag->ahsta_list hash table */
	struct hlist_node hlist_addr;
	struct ath12k_link_sta deflink;
	struct ath12k_link_sta __rcu *link[ATH12K_NUM_MAX_LINKS];
	/* indicates bitmap of link sta created in FW */
	u32 links_map;
	u8 assoc_link_id;
	u16 ml_peer_id;
	u16 dp_peer_id;
	bool is_mlo;
	u8 num_peer;
	u8 primary_link_id;
	/* indicates bitmap of devices where peers are created */
	u8 device_bitmap;
	u32 mlo_hw_link_id_bitmap;
	/* bitmap of ar->radio_idx values where link peers exist;
	 * Protected by wiphy->mtx.
	 */
	u8 ar_bitmap;
	/* indicates bitmap of links where peer delete cmd is sent to FW */
	u32 peer_delete_cmd_sent_bitmap;
	bool peer_delete_send_mlo_hw_bitmap;

#ifdef CPTCFG_MAC80211_DEBUGFS
	/* protected by conf_mutex */
	bool aggr_mode;
#endif
	bool use_4addr_set;
	struct wiphy_work set_4addr_wk;

	enum ieee80211_sta_state state;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	int ppe_vp_num;
	struct ath12k_vlan_iface *vlan_iface;
#endif
	bool low_ack_sent;
	bool is_migration_in_progress;
	struct work_struct migration_wk;
	struct ath12k_sta_migration_data migration_data;
	struct completion dp_migration_event;
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_sta_extn ahsta_extn;
#endif
	u16 free_logical_idx_map;

	spinlock_t ba_lock; /* protects ba_params arrays */
	struct ath12k_ba_session_params rx_ba_params[IEEE80211_MAX_NUM_TIDS];
	struct ath12k_ba_session_params tx_ba_params[IEEE80211_MAX_NUM_TIDS];

	/* Opaque RCU pointer to ath12k_dp_peer */
	void __rcu *dp_peer;

	struct ath12k_mode3_recovery_params recov;
	struct ath12k_smd_info smd_info;

	/*
	 * Cached per-link sta pointers used to hand off between pre-remove
	 * handling and later state transitions. Written before clearing
	 * ahsta->link[link_id], read during teardown transitions that execute
	 * after the pre-remove step, and cleared during final unassign.
	 * Protected by wiphy->mtx (all callers hold the wiphy lock).
	 */
	struct ath12k_link_sta *saved_link_sta[ATH12K_NUM_MAX_LINKS];

	/*
	 * Set by sta_pre_rcu_remove() after clearing ahsta->link[link_id] for
	 * all active links (single-link and MLO) and before mac80211's
	 * synchronize_net(). When true, ath12k_mac_free_unassign_link_sta()
	 * skips rcu_assign_pointer + synchronize_rcu() because
	 * synchronize_net() already provided the grace period for readers of
	 * ahsta->link[]. STA-level flag is sufficient since
	 * sta_pre_rcu_remove() operates on all links of the STA. Protected by
	 * wiphy->mtx. Reset to false by memset in sta_state(NOTEXIST->NONE) at
	 * the start of the next connect cycle.
	 */
	bool pre_rcu_remove_done;
};

#define ATH12K_INVALID_RSSI_FULL -1
#define ATH12K_INVALID_RSSI_EMPTY -128

#define ATH12K_HALF_20MHZ_BW	10
#define ATH12K_20MHZ_BW		20
#define ATH12K_2GHZ_MIN_CENTER	2412
#define ATH12K_2GHZ_MAX_CENTER	2484
#define ATH12K_5GHZ_MIN_CENTER	4900
#define ATH12K_5GHZ_MAX_CENTER	5920
#define ATH12K_6GHZ_MIN_CENTER	5935
#define ATH12K_6GHZ_MAX_CENTER	7115
#define ATH12K_MIN_2GHZ_FREQ	(ATH12K_2GHZ_MIN_CENTER - ATH12K_HALF_20MHZ_BW - 1)
#define ATH12K_MAX_2GHZ_FREQ	(ATH12K_2GHZ_MAX_CENTER + ATH12K_HALF_20MHZ_BW + 1)
#define ATH12K_MIN_5GHZ_FREQ	(ATH12K_5GHZ_MIN_CENTER - ATH12K_HALF_20MHZ_BW)
#define ATH12K_MAX_5GHZ_FREQ	(ATH12K_5GHZ_MAX_CENTER + ATH12K_HALF_20MHZ_BW)
#define ATH12K_MIN_6GHZ_FREQ	(ATH12K_6GHZ_MIN_CENTER - ATH12K_HALF_20MHZ_BW)
#define ATH12K_MAX_6GHZ_FREQ	(ATH12K_6GHZ_MAX_CENTER + ATH12K_HALF_20MHZ_BW)

#define ATH12K_MAX_6G_LOW_BAND_FREQ  6425
#define ATH12K_MAX_5G_LOW_BAND_FREQ  5330
#define ATH12K_MIN_5G_HIGH_BAND_FREQ 5490

#define ATH12K_NUM_CHANS 	102
#define ATH12K_MIN_5GHZ_CHAN 	36
#define ATH12K_MAX_5GHZ_CHAN 	177
#define ATH12K_MIN_2GHZ_CHAN 	1
#define ATH12K_MAX_2GHZ_CHAN 	11

enum ath12k_hw_state {
	ATH12K_HW_STATE_OFF,
	ATH12K_HW_STATE_ON,
	ATH12K_HW_STATE_RESTARTING,
	ATH12K_HW_STATE_RESTARTED,
	ATH12K_HW_STATE_WEDGED,
	ATH12K_HW_STATE_TM,
	/* Add other states as required */
};

struct ath12k_ctrl_path_pmlo_telemetry_stats {
       u32 pdev_id;
       u8 estimated_air_time_ac_be;
       u8 estimated_air_time_ac_bk;
       u8 estimated_air_time_ac_vi;
       u8 estimated_air_time_ac_vo;
};

struct ath12k_pdev_ctrl_path_stats {
       struct ath12k_ctrl_path_pmlo_telemetry_stats telemetry_stats;
       u8 pdev_freetime_per_sec;
};

/* Antenna noise floor */
#define ATH12K_DEFAULT_NOISE_FLOOR -95

struct ath12k_ftm_event_obj {
	u32 data_pos;
	u32 expected_seq;
	u8 *eventdata;
};

struct ath12k_fw_stats {
	struct dentry *debugfs_fwstats;
	u32 pdev_id;
	u32 stats_id;
	struct list_head pdevs;
	struct list_head vdevs;
	struct list_head bcn;
	struct list_head vdev_extds;
	u32 num_vdev_recvd;
	u32 num_bcn_recvd;
	u32 num_vdev_extd_recvd;
	bool en_vdev_stats_ol;
};

struct ath12k_dbg_htt_stats {
	enum ath12k_dbg_htt_ext_stats_type type;
	u32 cfg_param[4];
	u8 reset;
	struct debug_htt_stats_req *stats_req;
};

#define ATH12K_MAX_COEX_PRIORITY_LEVEL  3

struct ath12k_debug {
	struct dentry *debugfs_pdev;
	struct dentry *debugfs_pdev_symlink;
	struct ath12k_dbg_htt_stats htt_stats;
	struct ath12k_wmi_ctrl_path_stats_list wmi_ctrl_path_stats;
	enum wmi_tlv_tag wmi_ctrl_path_stats_tagid;
	struct list_head period_wmi_list;
	struct completion wmi_ctrl_path_stats_rcvd;
	u8 wmi_ctrl_path_stats_reqid;
	/* To protect wmi_list manipulation */
	spinlock_t wmi_ctrl_path_stats_lock;
	bool wmi_ctrl_path_stats_more_enabled;
	enum wmi_halphy_ctrl_path_stats_id tpc_stats_type;
	bool tpc_request;
	struct completion tpc_complete;
	struct wmi_tpc_stats_arg *tpc_stats;
	u32 rx_filter;
	bool enable_m3_dump;
	struct dentry *debugfs_pktlog;
	struct ath12k_pktlog pktlog;
	bool is_pkt_logging;
	u32 pktlog_mode;
	u32 pktlog_filter;
	struct dentry *debugfs_nrp;
};

enum ath12k_fw_recovery_option {
	 ATH12K_FW_RECOVERY_DISABLE = 0,
	 ATH12K_FW_RECOVERY_ENABLE_AUTO, /* Automatically recover after FW assert */
	 ATH12K_FW_RECOVERY_ENABLE_MODE1,
	 ATH12K_FW_RECOVERY_ENABLE_MODE2,
	 /* Enable only recovery. Send MPD SSR WMI */
	 /* command to unlink UserPD assert from RootPD */
};

struct ath12k_chan_info {
	u32 low_freq;
	u32 high_freq;
};

#define ATH12K_MIN_ACTIVE_CHIP_FOR_BYPASS 2
enum ath12k_wsi_bypass_action {
	ATH12K_WSI_BYPASS_DEFAULT,
	ATH12K_WSI_BYPASS_REMOVE_DEVICE,
	ATH12K_WSI_BYPASS_ADD_DEVICE,
};

#define ATH12K_FLUSH_TIMEOUT (6 * HZ)
#define ATH12K_VDEV_DELETE_TIMEOUT_HZ (5 * HZ)

struct ath12k_btcoex_info {
	bool coex_support;
	u32 pta_num;
	u32 coex_mode;
	u32 bt_active_time_slot;
	u32 bt_priority_time_slot;
	u32 coex_algo_type;
	u32 pta_priority;
	u32 pta_algorithm;
	u32 wlan_prio_mask;
	u32 wlan_weight;
	u32 bt_weight;
	u32 duty_cycle;
	u32 wlan_duration;
	u32 wlan_pkt_type;
	u32 wlan_pkt_type_continued;
};

enum btcoex_algo {
	COEX_ALGO_UNCONS_FREERUN = 0,
	COEX_ALGO_FREERUN,
        COEX_ALGO_OCS,
        COEX_ALGO_MAX_SUPPORTED,
};

enum ath12k_ap_ps_state {
	ATH12K_AP_PS_STATE_OFF,
	ATH12K_AP_PS_STATE_ON,
};

#define ATH12K_ATF_MAX_GROUPS 15

struct ath12k_atf_group_info {
	u32 group_id;
	u32 group_airtime;
	u32 group_policy;
	u16 unconfigured_peers;
	u16 configured_peers;
	u32 unconfigured_peers_airtime;
	/* This is the cumulative actual airtime of all peers in the group.*/
	u8 atf_actual_airtime;
	u8 atf_ul_airtime;
	u32 atf_actual_duration;
	u32 atf_actual_ul_duration;
};

struct ath12k_atf_peer_info {
	u8 peer_macaddr[6];
	u16 percentage_peer;
	u16 group_index;
	u32 explicit_peer_flag;
};

struct ath12k_atf_peer_params {
	u32 num_peers;
	u32 pdev_id;
	u32 atf_flags;
	struct ath12k_atf_peer_info *peer_info;
};

struct ath12k_atf {
	u32 total_groups;
	struct ath12k_atf_group_info group_info[ATH12K_ATF_MAX_GROUPS];
};

struct ath12k_radio_sr_cfg {
	u32 obss_pd_thresh;
	u32 enable_per_ac;
	u8 hesiga_sr15_enable;
	u8 psr_tx_enable;
	u8 safety_margin_psr;
	u32 obss_pd_thresh_ctrl_frame;
	u8 enable_sr_truncate;
	u32 min_obss_ppdu_dur_sr;
	u32 srg_bss_color_bitmap[2];
	u32 srg_partial_bssid_bitmap[2];
	u32 srg_obss_color_enable_bitmap[2];
	u32 srg_obss_bssid_enable_bitmap[2];
	u32 non_srg_obss_color_enable_bitmap[2];
	u32 non_srg_obss_bssid_enable_bitmap[2];
};

struct ath12k_radio_cfg {
	u32 rts_cts_rate;
	u8 ps_report;
	u32 nonagg_swretry_th;
	u32 agg_swretry_th;
	u32 sta_kickout_th;
	u8 arp_override;
	bool is_ani_enable;
	u32 ani_poll_period;
	u32 ani_listen_period;
	s8 ani_ofdm_level;
	u32 ani_cck_level;
	u32 cca_threshold;
	u32 dtcs;
	u32 ltr_enable;
	u32 ac_be;
	u32 ac_bk;
	u32 ac_vi;
	u32 ac_vo;
	u32 ac_timeout;
	u32 tx_timeout;
	u32 sleep_override;
	u32 rx_override;
	u32 l1ss_enable;
	u32 dsleep_enable;
	s32 rxsop_sens_lvl;
	u8 dyngroup;
	u8 dpdenable;
	u16 burst_dur;
	u8 burst_enable;
	u32 disable_lpi_ant;
	bool en_probe_all_bw;
	u32 ul_ofdma_rtd;
	bool enable_small_mru;
	bool enable_large_mru;
	u32 pdev_reset;
	u8 current_mode;
	s32 txpowlimit2G;
	s32 txpowlimit5G;
	u32 antenna_gain_2g;
	u32 antenna_gain_5g;
	u32 ofdem_level;
	u32 txpower_scale;
	u32 rx_filter;
	u32 block_interbss;
	u32 fw_disable_reset;
	u32 ppdu_dur;
	u32 txbf_sound_period;
	u32 promisc_mode;
	u32 burst_mode;
	u8 mcast_bcast_echo;
	u32 ant_plzn;
	u16 amsdu_mask;
	u16 ampdu_mask;
	u32 mbssid_en_ctrl_frame;
	u32 resp_retry_limit;
	u32 cts_timeout;
	u32 slot_time;
	u32 ack_timeout;
	u32 cck_enable;
	u32 ru_alloc_en;
	u32 antenna_gain_half_db;
	u32 mgmt_ttl;
	u32 prb_rsp_ttl;
	u32 mu_ppdu_dur;
	u32 tbtt_ctrl;
	u32 punct_bw;
	u32 low_lat_mode;
	struct {
		u32 gpio_pin;
		u32 gpio_function;
		u32 gpio_pull_type;
		u32 gpio_dir;
		u32 gpio_intr_mode;
		u32 gpio_value;
		bool configured;
	} gpio_cfg[32];

	/* Scale the CTL power. 100%,50%, 25% etc..
	 * The acceptable scale-values are defined in "enum ath12k_tp_scale"
	 */
	u32 ctlpwrscale;

	/* Temperature monitoring */
	s32 temperature;                    /* Last temperature reading in °C */
	u8 temperature_query_pending;       /* Query sent, waiting for response */
	u32 msdu_ttl;

	/* [15:0] 2G/5G pair id, [31:16] 6G super-domain id */
	u32 regdomain;
	bool chan144_enabled;
	u32 pdev_to_reo_dest;
	u32 cal_ver_check;
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_radio_cfg_extn radio_cfg_extn;
#endif
};

/**
 * enum ath12k_rf_path_index - internal RF path operating range selection
 * @ATH12K_RF_PATH_FULL_RANGE: Primary path — full 5G operating range
 * @ATH12K_RF_PATH_HIGH_RANGE: Secondary path — high 5G operating range only
 *
 * Used as the value of ath12k_rf_path_ctx.current_index and for internal
 * comparisons in mac.c and reg.c.  Vendor-facing code in vendor.c uses the
 * parallel qca_wlan_vendor_rf_path_mode enum which has identical values.
 */
enum ath12k_rf_path_index {
	ATH12K_RF_PATH_FULL_RANGE = 0,
	ATH12K_RF_PATH_HIGH_RANGE = 1,
};

/**
 * struct ath12k_chanctx_switch_stats - Channel context switch profiling statistics
 * @total_switches: Total number of channel switches completed
 * @last_switch_time_us: Duration of the most recent channel switch in microseconds
 * @min_switch_time_us: Minimum channel switch duration observed in microseconds
 * @max_switch_time_us: Maximum channel switch duration observed in microseconds
 * @entry_time_us: Timestamp when entering switch_vif_chanctx operation
 * @mvr_posting_time_us: Time taken to post MVR (Multi-VDEV Restart) command
 * @mvr_resp_time_us: Time taken to receive MVR response from firmware
 * @mvr_timeout_count: Number of times MVR response timed out
 * @nl_notify_count: Number of cfg80211_ch_switch_notify NL events sent to applications
 */
struct ath12k_chanctx_switch_stats {
	u64 total_switches;
	u64 last_switch_time_us;
	u64 min_switch_time_us;
	u64 max_switch_time_us;
	u64 avg_switch_time_us;

	/* Timing breakdown */
	u64 entry_time_us;
	u64 mvr_posting_time_us;
	u64 mvr_resp_time_us;

	/* Error tracking */
	u64 mvr_timeout_count;

	/* NL event tracking */
	u64 nl_notify_count;
};

/* Lightweight structure to receive FW HTT map event */
struct ath12k_peer_map_pending_event {
	u16 peer_id;
	u16 ast_hash;
	u16 hw_peer_id;
	bool received;
	u8 pending_peer_addr[ETH_ALEN];
	u8 pending_peer_vdev_id;
};

struct ath12k_cu_mem {
	__le32 cu_flags;
	__le32 eht_bpcc;
	__le32 reconfig;
	__le32 ttlm_max_ch_sw_time;
	__le32 ttlm_expected_duration;
	__le32 uhr_countdown;
	__le32 uhr_ebpcc;
	__le32 max_chan_switch_time;
} __packed;

/**
 * struct ath12k_rf_path_ctx - per-radio RF path switch state
 * @current_index: active RF path index
 * @target_index: requested RF path index
 * @freq_low:  active 5G lower bound in MHz for the current RF path
 * @freq_high: active 5G upper bound in MHz for the current RF path
 * @rf_switch_done: signals FW response received
 * @is_switch_in_progress: true while RF path switch is in progress
 * @is_fw_resp_success: FW reported switch success
 * @supported: true if radio advertised valid EXT2 secondary 5G caps
 */
struct ath12k_rf_path_ctx {
	u32  current_index;
	u32  target_index;
	u32  freq_low;
	u32  freq_high;
	struct completion rf_switch_done;
	bool is_switch_in_progress;
	bool is_fw_resp_success;
	bool supported;
};

/**
 * struct ath12k_tx_mon_ssr_stats - per-radio TX monitor SSR recovery counters
 * @restart_attempts: TX monitor restart attempts during SSR recovery
 * @restart_success: successful TX monitor restarts during SSR recovery
 * @restart_no_pool: restarts skipped because the descriptor pool was not ready
 * @restart_fail: TX monitor restart failures during SSR recovery
 * @work_cancel: txmon work cancellation attempts during SSR recovery
 * @mon_desc_flushed: monitor descriptors flushed from tx_mon_desc_work_list
 * @ppdu_desc_drained: PPDU descriptors drained from used/proc lists during SSR
 * @status_desc_drained: status descriptors drained from PPDU descriptors during SSR
 */
struct ath12k_tx_mon_ssr_stats {
	u32 restart_attempts;
	u32 restart_success;
	u32 restart_no_pool;
	u32 restart_fail;
	u32 work_cancel;
	u32 mon_desc_flushed;
	u32 ppdu_desc_drained;
	u32 status_desc_drained;
};

struct ath12k {
	struct ath12k_base *ab;
	u8 pdev_idx;
	struct ath12k_pdev *pdev;
	struct ath12k_hw *ah;
	struct ath12k_wmi_pdev *wmi;
	struct ath12k_pdev_dp dp;
	u8 mac_addr[ETH_ALEN];
	struct ath12k_chan_info chan_info; /* protected by data_lock */
	u32 ht_cap_info;
	u32 vht_cap_info;
	struct ath12k_he ar_he;
	bool ofdma_txbf_conf;
	bool he_dl_enabled;
	bool he_ul_enabled;
	bool he_dlbf_enabled;
	bool eht_dl_enabled;
	bool eht_ul_enabled;
	bool eht_dlbf_enabled;
	struct {
		struct completion started;
		struct completion completed;
		struct completion on_channel;
		struct delayed_work timeout;
		struct delayed_work roc_done;
		enum ath12k_scan_state state;
		bool is_roc:1;
		bool roc_notify:1;
		int roc_freq;
		int scan_id;
		u8 parallel_scan_id; /* mac80211 parallel scan context ID; 0 = normal */
		struct wiphy_work vdev_clean_wk;
		struct ath12k_link_vif *arvif;
	} scan;

	struct {
		struct ieee80211_supported_band sbands[NUM_NL80211_BANDS];
		struct ieee80211_sband_iftype_data
			iftype[NUM_NL80211_BANDS][NUM_NL80211_IFTYPES];
	} mac;

	unsigned long dev_flags;
	unsigned int filter_flags;
	u32 min_tx_power;
	u32 max_tx_power;
	u32 txpower_limit_2g;
	u32 txpower_limit_5g;
	u32 txpower_limit_6g;
	u32 txpower_scale;
	u32 power_scale;
	u32 chan_tx_pwr;
	u32 rts_threshold;
#ifdef CPTCFG_QCN_EXTN
	u8 muedca_mode;
#endif /* CPTCFG_QCN_EXTN */
	u32 num_stations;
	u32 max_num_stations;

	/* protects the radio specific data like debug stats, ppdu_stats_info stats,
	 * vdev_stop_status info, scan data, ath12k_sta info, ath12k_link_vif info,
	 * channel context data, survey info, test mode data.
	 */
	spinlock_t data_lock;
	/* protects arsta hash table access, including BH context paths */
	spinlock_t arsta_lock;

	struct list_head arvifs;
	/* per-radio hash buckets for station address lookup */
	struct hlist_head *arsta_list;
	u8 arsta_hash_bits;
	/* should never be NULL; needed for regular htt rx */
	struct ieee80211_channel *rx_channel;

	/* valid during scan; needed for mgmt rx during scan */
	struct ieee80211_channel *scan_channel;

	struct tt_level_config tt_level_configs[ENHANCED_THERMAL_LEVELS];
	struct wmi_therm_throt_level_stats_info tt_level_stats[ENHANCED_THERMAL_LEVELS];
	struct wmi_therm_throt_stats_event tt_current_state;

	u8 cfg_tx_chainmask;
	u8 cfg_rx_chainmask;
	u8 num_rx_chains;
	u8 num_tx_chains;
	/* pdev_idx starts from 0 whereas pdev->pdev_id starts with 1 */
	u8 lmac_id;
	u8 hw_link_id;
	u8 radio_idx;

	struct completion peer_create_conf;
	struct completion peer_assoc_done;

	int install_key_status;
	struct completion install_key_done;

	int last_wmi_vdev_start_status;
	struct completion vdev_setup_done;
	struct completion vdev_delete_done;
	bool cumac_cmd_sent;
	struct completion cumac_setup_done;

	int num_peers;
	int num_ml_peers;
	int num_mapc_peers;
	int max_num_peers;
	u32 num_started_vdevs;
	u32 num_created_vdevs;
	u8 num_created_bridge_vdevs;
	unsigned long long allocated_vdev_map;

	/* bit per vdev_id, set when arvif->is_created becomes true and
	 * cleared when it becomes false; used for O(1) lookup in
	 * ath12k_wmi_vdev_is_created() instead of walking ar->arvifs
	 * Used only to debug.
	 */
	unsigned long long created_vdev_map;

	struct idr txmgmt_idr;
	/* protects txmgmt_idr data */
	spinlock_t txmgmt_idr_lock;
	atomic_t num_pending_mgmt_tx;
	wait_queue_head_t txmgmt_empty_waitq;

	/* cycle count is reported twice for each visited channel during scan.
	 * access protected by data_lock
	 */
	u32 survey_last_rx_clear_count;
	u32 survey_last_cycle_count;

	/* Channel info events are expected to come in pairs without and with
	 * COMPLETE flag set respectively for each channel visit during scan.
	 *
	 * However there are deviations from this rule. This flag is used to
	 * avoid reporting garbage data.
	 */
	struct survey_info survey[ATH12K_NUM_CHANS];
	struct completion bss_survey_done;

	struct work_struct regd_update_work;
	/* Work struct for resetting to previous country code */
	struct work_struct reg_set_previous_country;
	/* Work struct to set tx power for standlone sta after AFC done */
	struct work_struct change_6g_txpow_sta_mode_work;

	struct wiphy_work wmi_mgmt_tx_work;
	struct sk_buff_head wmi_mgmt_tx_queue;

	struct ath12k_wow wow;
	struct completion target_suspend;

	struct ath12k_per_peer_tx_stats cached_stats;
	u32 last_ppdu_id;
	u32 cached_ppdu_id;
#ifdef CPTCFG_ATH12K_DEBUGFS
	struct ath12k_debug debug;
	struct dentry *wmi_ctrl_stat;
	/* TODO: Add mac_filter, ampdu_aggr_size and wbm_tx_completion_stats stats*/
#endif
	bool supports_6ghz:1;
	bool ch_info_can_report_survey:1;
	bool target_suspend_ack:1;
	struct ath12k_pdev_ctrl_path_stats stats;
	bool dfs_block_radar_events;
	bool dfs_sub_channel_marking;
	bool monitor_vdev_created:1;
	bool monitor_started:1;
	bool nlo_enabled:1;
	bool is_tx_monitor_enabled_on_ssr:1;
	/* Add new boolean variable here. */
	struct ath12k_tx_mon_ssr_stats tx_mon_ssr_stats;

	/* Protected by wiphy::mtx lock. */
	u32 vdev_id_11d_scan;
	struct completion completed_11d_scan;
	enum ath12k_11d_state state_11d;
	u8 alpha2[REG_ALPHA2_LEN];
	u16 country_id;
	bool regdom_set_by_user;

	struct ath12k_btcoex_info coex;

	int monitor_vdev_id;

	/* freq_range changes when the regulatory domain changes. */
	struct wiphy_radio_freq_range freq_range;
	/* hw_freq_range remains constant regardless of the regulatory domain. */
	struct wiphy_radio_freq_range hw_freq_range;
	u32 num_channels;

	struct completion fw_stats_complete;
	struct completion fw_stats_done;

	bool ctrl_mem_stats;

	struct completion mlo_setup_done;
	u32 mlo_setup_status;
	u8 ftm_msgref;
	struct ath12k_fw_stats fw_stats;
	unsigned long last_signal_update;
	unsigned long last_tx_power_update;
	struct ath12k_thermal thermal;
#ifdef CPTCFG_ATH12K_SPECTRAL
	struct ath12k_spectral spectral;
#endif
	bool ap_ps_enabled;
	bool ap_ps_disabled_by_agile;
	enum ath12k_ap_ps_state ap_ps_state;
	enum ath12k_green_ap_mode ap_ps_mode;
	u16 num_stations_multistream;
	struct wiphy_delayed_work ap_ps_timer;
	u32 ap_ps_timeout;

	/* Periodic pdev stats timer: drives continuous FW counter updates
	 * (CCA/channel utilization, noise floor, tx power) at a configurable
	 * interval.  Mirrors the qca-wifi scn_stats_timer mechanism.
	 * Interval of 0 disables the timer.
	 */
	struct wiphy_delayed_work pdev_stats_timer;
	u32 pdev_stats_timer_interval;
	/* Set while a periodic fire-and-forget PDEV stats request is
	 * in-flight.  The event handler uses this to distinguish periodic
	 * requests from synchronous ones so it can update only the pdev
	 * cache without touching shared vdev/bcn lists or signalling
	 * unrelated waiters.  Protected by data_lock.
	 */
	bool pdev_stats_timer_req_pending;
	/* Cached pdev counters updated by the periodic stats event.
	 * Protected by data_lock.  Consumers read these instead of
	 * issuing a blocking WMI request.
	 */
	u32 pdev_rx_clear_count;
	u32 pdev_cycle_count;
	s32 pdev_chan_nf;
	/* Previous snapshot of the free-running HW counters, saved each time
	 * the periodic timer updates the cache.  Used to compute a per-interval
	 * delta for channel utilization instead of a lifetime average.
	 * Protected by data_lock.
	 */
	u32 pdev_prev_rx_clear_count;
	u32 pdev_prev_cycle_count;

	struct cfg80211_chan_def awgn_chandef;
	u32 chan_bw_interference_bitmap;
	bool awgn_intf_handling_in_prog;

	struct completion mvr_complete;
	bool twt_enabled;
	struct wmi_rssi_dbm_conv_offsets rssi_offsets;
	/* HTT PHY stats NF values (type 37), populated by ath12k_telemetry_get_phy_nf().
	 * bdf_nf_chains[]: static/characterized NF per chain. The firmware reports 1
	 * as a sentinel for unused/inactive chains. This is unambiguous because valid
	 * noise floor readings are always negative (in dBm, e.g. -95), so a positive
	 * value of 1 cannot be a real NF measurement.
	 * Dynamic (runtime) NF is obtained via ar->survey[idx].noise from the WMI BSS
	 * channel info path (same mechanism as iw dev survey dump).
	 */
	s32 bdf_nf_chains[ATH12K_HTT_STATS_MAX_CHAINS];
	u16 csa_active_cnt;
	s32 sensitivity_level;

	/* minimum and maximum rest time scan parameters which
	 * can be configured via debugfs and should be used
	 * only in wmi scan cmd.
	 */
	u32 scan_min_rest_time;
	u32 scan_max_rest_time;
	s8 max_allowed_tx_power;

	bool teardown_complete_event;
	u64 delta_tsf2;
	u64 delta_tqm;
	struct ath12k_afc_info afc;
#ifdef CPTCFG_ATH12K_CFR
	struct ath12k_cfr cfr;
#endif
	struct cfg80211_chan_def agile_chandef;
	bool agile_abort_pending; /* WMI abort sent; awaiting firmware ACK */
	struct wiphy_work agile_cac_abort_wq;
	struct wiphy_work ap_ps_recalc_wq;
	struct ath12k_qos_map *qos_map;

	bool erp_trigger_set;
	struct work_struct erp_handle_trigger_work;
	struct completion suspend;
	bool pdev_suspend;
	bool pdev_user_suspend;
	struct completion pdev_resume;
	struct work_struct ssr_erp_exit;

	struct timer_list atf_stats_timer;
	struct ath12k_atf atf_table;
	u8 atf_stats_enable;
	u8 atf_stats_timeout;
	bool commitatf;
	bool atf_strict_scheduling;
	u64 atf_stats_accum_start_time;
	/* ath12k extension structure */
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_extn ar_extn;
#endif /* CPTCFG_QCN_EXTN */
	struct ath12k_rtt_capab rtt_capab;
	struct completion delete_all_peer_done;
	struct wmi_vdev_host_tsf_arg tsf_report;
	struct completion tsf_report_done;
	u8 mgmt_tx_retry_limit;
	struct ath12k_radio_cfg radio_cfg;
	struct ath12k_radio_sr_cfg sr_cfg;
#ifdef CPTCFG_QCN_EXTN
	u32 vendor_mac_used_bitmap;
#endif
	u8 smart_mon_filter;
	unsigned int radar_detect_count;

	struct rhashtable *rhash_tx_skb_tbl;
	struct rhashtable_params rhash_tx_skb_param;
	/* To synchronize rhash tbl write operation */
	spinlock_t rhash_tx_lock;
	struct ath12k_chanctx_switch_stats chanctx_switch_stats;

	/*
	 * Deferred work to send NL80211_CMD_CH_SWITCH_NOTIFY for scan radio.
	 * cfg80211_ch_switch_notify() acquires wiphy mutex and cannot be called
	 * from BH/softirq context (WMI MVR event handler). Schedule work to
	 * call it from process context instead.
	 */
	struct work_struct mvr_ch_switch_notify_work;
	u32 mvr_ch_switch_notify_vdev_bm;
	struct ath12k_peer_map_pending_event peer_map_event;
	ath12k_peer_create_conf_status peer_create_status;
	/* Broadcast probe request per-STA rate-limit table.
	 * Suppresses duplicate broadcast probe requests from the same STA
	 * within ATH12K_BCAST_PROBE_RL_WINDOW_MS milliseconds.
	 */
	DECLARE_HASHTABLE(bcast_probe_rl, ATH12K_BCAST_PROBE_RL_HASH_BITS);
	bool bcast_probe_rl_enabled;
	u32 bcast_probe_rl_entries;
	u32 bcast_probe_rl_max_entries;
	u32 bcast_probe_rl_window_ms;
	struct ath12k_rf_path_ctx rf_path_ctx;

	/* Cached IGMP/MLD TID override value (0=disabled, 1-7=TID) */
	u32 igmp_tid_override;
	/* Bitmask for UNII-1/2A coex disable: bit0=UNII-1, bit1=UNII-2A */
	u8 chan_coex_disable_mask;
};

static inline bool ath12k_is_rf_path_switch_supported(struct ath12k *ar)
{
	return ar->rf_path_ctx.supported;
}

struct ath12k_6ghz_sp_reg_rule {
	int num_6ghz_sp_rule;
	struct ieee80211_reg_rule sp_reg_rule[];
};

struct ath12k_pending_primary_link {
	char ifname[IFNAMSIZ];
	u8 hw_link_id;
	bool valid;
};

struct ath12k_hw {
	struct ieee80211_hw *hw;
	struct device *dev;
	struct ath12k_hw_group *ag;
	bool queue_stop;

	/* Protect the write operation of the hardware state ath12k_hw::state
	 * between hardware start<=>reconfigure<=>stop transitions.
	 */
	struct mutex hw_mutex;
	enum ath12k_hw_state state;
	bool regd_updated;
	bool use_6ghz_regd;
	/* Protect concurrent access to the wiphy regd when processing AFC
	 * power event from multple radios.
	 */
	spinlock_t afc_lock;

	u8 num_radio;

	DECLARE_BITMAP(free_ml_peer_id_map, ATH12K_MAX_MLO_PEERS);
	unsigned long long free_ahvif_id_map;
	u32 free_map_id;

	struct ath12k_dp_hw dp_hw;
	u32 max_ml_peers_supported;
	u32 num_ml_peers;
	u32 max_ml_peer_ids;
	u16 last_ml_peer_id;

	/* Pending primary_link values written before STA vdev creation.
	 * Indexed by slot; protected by hw_mutex.
	 */
	struct ath12k_pending_primary_link pending_primary_link[ATH12K_GROUP_MAX_RADIO];

	/* Bitmap of parallel_scan_ids that aborted during a multi-radio
	 * split scan. Bit N is set when any radio with parallel_scan_id=N
	 * aborts. Cleared after the final ieee80211_scan_completed() call.
	 * Stored in ah (not ar->scan) to survive ar->scan being cleared
	 * and reused for a new scan before the last radio consolidates.
	 * 256 bits covers the full u8 parallel_scan_id range.
	 */
	DECLARE_BITMAP(scan_aborted, 256);

	/* Keep last */
	struct ath12k radio[] __aligned(sizeof(void *));
};

struct pmm_remap {
	u32 base;
	u32 size;
};

struct ath12k_band_cap {
	u32 phy_id;
	u32 max_bw_supported;
	u32 ht_cap_info;
	u32 he_cap_info[2];
	u32 he_mcs;
	u32 he_cap_phy_info[PSOC_HOST_MAX_PHY_SIZE];
	struct ath12k_wmi_ppe_threshold_arg he_ppet;
	u16 he_6ghz_capa;
	/* QCA-internal HE capability word from FW SERVICE_READY_EXT.
	 * bit 0: RX 1xLTF + 0.4us GI in HE SU PPDU
	 * bit 1: RX 2xLTF + 0.4us GI in HE SU PPDU
	 * bit 2: 2xLTF in 160/80+80 MHz HE PPDU
	 * Matches WMI_HE_CAP_1X_LTF_400NS_GI_SUPPORT etc. macros.
	 */
	u32 he_cap_info_internal;
	u32 eht_cap_mac_info[WMI_MAX_EHTCAP_MAC_SIZE];
	u32 eht_cap_phy_info[WMI_MAX_EHTCAP_PHY_SIZE];
	u32 uhr_cap_mac_info[WMI_MAX_UHRCAP_MAC_SIZE];
	u32 uhr_cap_phy_info[WMI_MAX_UHRCAP_PHY_SIZE];
	u32 uhr_param_npca_info;
	u32 eht_mcs_20_only;
	u32 eht_mcs_80;
	u32 eht_mcs_160;
	u32 eht_mcs_320;
	struct ath12k_wmi_ppe_threshold_arg eht_ppet;
	u32 eht_cap_info_internal;
};

struct ath12k_pdev_cap {
	u32 supported_bands;
	u32 ampdu_density;
	u32 vht_cap;
	u32 vht_mcs;
	/* Per-SS MCS10/11 bitmap extracted from FW vht_supp_mcs bits [23:16].
	 * Bit N set means NSS (N+1) supports VHT MCS 10 and 11.
	 * Zero means MCS10/11 is not supported by the hardware.
	 */
	u8 vht_higher_mcs_supp;
	/* EHT Duplicate in 6 GHz (MCS14): set when FW reports support in
	 * service_ready_ext eht_cap_phy_info[1] bit 23.
	 */
	bool eht_dup_6ghz_supp;
	u32 he_mcs;
	u32 tx_chain_mask;
	u32 rx_chain_mask;
	u32 tx_chain_mask_shift;
	u32 rx_chain_mask_shift;
	u32 chainmask_table_id;
	unsigned long adfs_chain_mask;
	bool agile_spectral_cap;
	bool agile_spectral_cap_160;
	bool agile_spectral_cap_80p80;
	bool agile_spectral_cap_320;
	struct ath12k_band_cap band[NUM_NL80211_BANDS];
	u32 eml_cap;
	u32 mld_cap;
	u32 ext_mld_cap;
	bool nss_ratio_enabled;
	u8 nss_ratio_info;
	/* Advertised max NSS from FW (11BN specific) */
	u8 max_tx_nss;
	u8 max_rx_nss;
	u32 scan_radio_caps;
};

struct ath12k_smd_update_work {
	struct work_struct work;
	struct ieee80211_hw *hw;
	struct ath12k_vif *ahvif;

	u16 link_bitmap;
	u32 role, type, status;
	u32 dl_sn, ul_sn, dl_drain_time;

	u8 peer_addr[ETH_ALEN];
};

#define ATH12K_SCAN_RADIO_CAP_SUPPORTED   BIT(0)
#define ATH12K_SCAN_RADIO_CAP_DFS_ENABLED BIT(1)
#define ATH12K_SCAN_RADIO_CAP_BLANKING    BIT(2)

struct mlo_timestamp {
	u32 info;
	u32 sync_timestamp_lo_us;
	u32 sync_timestamp_hi_us;
	u32 mlo_offset_lo;
	u32 mlo_offset_hi;
	u32 mlo_offset_clks;
	u32 mlo_comp_clks;
	u32 mlo_comp_timer;
};

struct ath12k_pdev {
	struct ath12k *ar;
	u32 pdev_id;
	u32 hw_link_id;
	struct ath12k_pdev_cap cap;
	u8 mac_addr[ETH_ALEN];
	struct mlo_timestamp timestamp;
	const char *phy_name;
	struct ath12k_peer_del_tracker *peer_del_tracker;
	atomic_t peer_del_tracker_entries;
};

struct ath12k_fw_pdev {
	u32 pdev_id;
	u32 phy_id;
	u32 supported_bands;
};

struct ath12k_board_data {
	const struct firmware *fw;
	const void *data;
	size_t len;
};

struct ath12k_reg_freq {
	u32 start_freq;
	u32 end_freq;
};

struct ath12k_mlo_memory {
	struct target_mem_chunk chunk[ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01];
	int mlo_mem_size;
	bool init_done;
	bool is_mlo_mem_avail;
};

/**
 * struct ath12k_stats_list_entry: Structure used to represent an entry in
 * the non-blocking stats work list
 * @node : linked list node
 * @usr_command: user inputs
 */
struct ath12k_stats_list_entry {
	struct list_head node;
	struct ath12k_telemetry_command usr_command;
};

/**
 * struct ath12k_stats_work_context: Structure representing the context of
 * stats work
 * @stats_nb_work : Instance of work
 * @list_lock : lock for the work list
 * @work_list : queue of non-blocking stats requests
 */
struct ath12k_stats_work_context {
	struct wiphy_work stats_nb_work;
	spinlock_t list_lock;
	struct list_head work_list;
};

#define ATH12K_OP_REPORT_LOW_ACK   0xC000
#define ATH12K_OP_REPORT_RSSI      0x8000
#define ATH12K_PAYLOAD_MASK        0x3FFF

#define ATH12K_REPORT_LOW_ACK_ALL  (ATH12K_OP_REPORT_LOW_ACK | ATH12K_PAYLOAD_MASK)
#define ATH12K_REPORT_RSSI_ALL     (ATH12K_OP_REPORT_RSSI | ATH12K_PAYLOAD_MASK)

/* Legacy */
#define ATH12K_REPORT_LOW_ACK_NUM_PKT   ATH12K_REPORT_LOW_ACK_ALL
#define ATH12K_IS_UMAC_RESET_IN_PROGRESS        BIT(0)
#define ATH12K_IS_UMAC_RESET_TYPE_RECOVERY	BIT(1)
#define ATH12K_IS_UMAC_RESET_FB_IN_PROGRESS	BIT(2)

/* Forward declaration for task structure */
struct ath12k_umac_reset_task;

/**
 * struct ath12k_mlo_dp_umac_reset:  mlo umac_reset context
 * @response_chip : Number of chips the event is handled for
 * @request_chip : Number of chips send the event
 * @lock : locl for prtecting the umac_reset context
 * @umac_reset_info: place holder for umac reset related falgs
 * @initiator_chip: device_id of the initiator chip
 * @task_queue: Queue of pending tasks
 * @task_queue_lock: Protects task_queue
 * @task_id: Monotonically increasing task ID
 * @tasklet: Hi-priority tasklet per CPU
 * @csd: Per-CPU call_single_data for async SMP calls
 */
struct ath12k_mlo_dp_umac_reset {
	unsigned long task_map;                /* Bitmap tracking active tasks */
	atomic_t request_chip;
	spinlock_t lock;
	u8 umac_reset_info;
	u8 initiator_chip;

	/* Multi-core task queue infrastructure */
	struct list_head task_queue;
	spinlock_t task_queue_lock;
	atomic_t task_id;
	struct tasklet_struct tasklet[NR_CPUS];
	call_single_data_t csd[NR_CPUS];
};

#define WSI_INVALID_ORDER	0xFF
#define WSI_INVALID_INDEX	0xFF

struct ath12k_mlo_wsi_device_group {
	u8 wsi_order[ATH12K_MAX_SOCS];
	u8 num_devices;
};

struct ath12k_mlo_wsi_device_load_stats {
	u32 ingress_cnt;
	u32 egress_cnt;
	bool notify;
};

struct ath12k_mlo_wsi_load_info {
	struct ath12k_mlo_wsi_device_group mlo_device_grp;
	struct ath12k_mlo_wsi_device_load_stats load_stats[ATH12K_MAX_SOCS];
};

enum ath12k_cumac_band {
	ATH12K_CUMAC_BAND_NONE = 0,
	ATH12K_CUMAC_BAND_2GHZ,
	ATH12K_CUMAC_BAND_5GHZ,
	ATH12K_CUMAC_BAND_6GHZ,
};

/* Holds info on the group of devices that are registered as a single
 * wiphy, protected with struct ath12k_hw_group::mutex.
 */
struct ath12k_hw_group {
	struct ath12k_dp_hw_group *dp_hw_grp;
	struct list_head list;
	/* per-group station (ahsta) address hash table */
	struct hlist_head *ahsta_list;
	u8 ahsta_hash_bits;
	/* protects ahsta_list; may be taken from BH context */
	spinlock_t ahsta_lock;
#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
	u8 dbs_power_reduction;
	u8 eth_power_reduction;
#endif
	u8 id;
	u8 num_devices;
	u8 num_probed;
	u8 num_started;
	unsigned long flags;
	struct ath12k_base *ab[ATH12K_MAX_SOCS];

	/* protects access to this struct */
	struct mutex mutex;

	/* Holds information of wiphy (hw) registration.
	 *
	 * In Multi/Single Link Operation case, all pdevs are registered as
	 * a single wiphy. In other (legacy/Non-MLO) cases, each pdev is
	 * registered as separate wiphys.
	 */
	struct ath12k_hw *ah[ATH12K_GROUP_MAX_RADIO];
	u8 num_hw;
	bool mlo_capable;
	struct device_node *wsi_node[ATH12K_MAX_SOCS];
	struct ath12k_mlo_memory mlo_mem;
	struct ath12k_host_mlo_mem_arena mlomem_arena;
	bool hw_link_id_init_done;
	u8 num_userpd_started;
	struct work_struct reset_group_work;
	struct ath12k_mlo_wsi_load_info *wsi_load_info;
	enum ath12k_mlo_recovery_mode recovery_mode;
	struct ath12k_mlo_dp_umac_reset mlo_umac_reset;
        struct completion umac_reset_complete;
        bool trigger_umac_reset;
	struct ath12k_qos_ctx qos;
	u64 mlo_tstamp_offset;
	struct ath12k_stats_work_context stats_work;
	u8 num_bypassed;
	bool wsi_remap_in_progress;
	/* Counts real (non-bridge) MLO peers currently alive in firmware.
	 * Incremented in ath12k_peer_create() for every MLO peer created.
	 * Decremented in the WMI peer-delete-response handlers (individual
	 * and peer_delete_all) when firmware confirms peer deletion.
	 * Protected by ahsta_lock for the WMI workqueue dec path.
	 */
	u32 mlo_peer_count;
	struct completion peer_cleanup_complete;
	u64 wsi_peer_clean_timeout;
	struct completion power_up;
	bool mlo_teardown;
	bool block_peer_create;
	u8 cumac_chip_id;
	bool cumac_selected;
	bool cumac_enabled;
	struct ath12k_base *assert_ab;
	bool recovery_t2lm_active;
};

/* Holds WSI info specific to each device, excluding WSI group info */
struct ath12k_wsi_info {
	u32 index;
	u32 hw_link_id_base;
	u32 num_adj_chips;
	u32 adj_chip_idxs[ATH12K_MAX_ADJACENT_CHIPS];
	u8 diag_device_idx_bmap;
};

enum ath12k_device_family {
	ATH12K_DEVICE_FAMILY_START,
#ifdef CPTCFG_QCN_EXTN
	ATH12K_DEVICE_FAMILY_WIFI6 = ATH12K_DEVICE_FAMILY_START,
	ATH12K_DEVICE_FAMILY_WIFI7,
#else
	ATH12K_DEVICE_FAMILY_WIFI7 = ATH12K_DEVICE_FAMILY_START,
#endif
	ATH12K_DEVICE_FAMILY_WIFI8,
	ATH12K_DEVICE_FAMILY_MAX,
};

/* Fatal error notification type based on specific platform type */
enum ath12k_core_crash_type {
	/* Fatal error notification unknown or fatal error notification
	 * is honored.
	 */
	ATH12K_NO_CRASH,

	/* Fatal error notification from remoteproc user pd for platform with
	 * ahb based internal radio and pcic based external radios
	 */
	ATH12K_RPROC_USERPD_CRASH,

	/* Fatal error notification from remoteproc root pd for platform with
	 * ahb based internal radio and pcic based external radios
	*/
	ATH12K_RPROC_ROOTPD_CRASH
};

struct ath12k_internal_pci {
        bool gic_enabled;
        wait_queue_head_t gic_msi_waitq;
        u32 dp_msi_data[ATH12K_QCN6432_EXT_IRQ_GRP_NUM_MAX];
        u32 ce_msi_data[ATH12K_QCN6432_CE_COUNT];
        u32 dp_irq_num[ATH12K_QCN6432_EXT_IRQ_GRP_NUM_MAX];
};

struct ath12k_mem_dev {
	struct device dev;
	bool dev_registered;
	bool rmem_inited;
};

/* Master structure to hold the hw data which may be used in core module */
struct ath12k_base {
	enum ath12k_hw_rev hw_rev;
	struct platform_device *pdev;
	struct device *dev;
	struct ath12k_qmi qmi;
	struct ath12k_wmi_base wmi_ab;
	struct completion fw_ready;
	u8 device_id;
	int num_radios;
	/* HW channel counters frequency value in hertz common to all MACs */
	u32 cc_freq_hz;

	struct ath12k_dump_file_data *dump_data;
	size_t ath12k_coredump_len;
	struct work_struct dump_work;

	struct ath12k_htc htc;

	struct ath12k_dp *dp;
	struct ath12k_mgmt *mgmt;

	void __iomem *mem;
	unsigned long mem_len;
	dma_addr_t mem_pa;

	void __iomem *mem_ce;
	u32 ce_remap_base_addr;
	u32 cmem_offset;
	bool ce_remap;
	void __iomem *mem_pmm;
	u32 pmm_remap_base_addr;
	bool pmm_remap;
	bool htt_flag;
	bool stats_disable;

	struct {
		enum ath12k_bus bus;
		const struct ath12k_hif_ops *ops;
	} hif;

	struct {
		struct completion wakeup_completed;
		u32 wmi_conf_rx_decap_mode;
	} wow;

	struct ath12k_ce ce;
	struct timer_list rx_replenish_retry;
	struct ath12k_hal hal;
	/* TQM command ring staging in words */
	u32 *tqm_cmd_staging;
	/* To synchronize core_start/core_stop */
	struct mutex core_lock;
	/* Protects data like peers */
	spinlock_t base_lock;

	/* Single pdev device (struct ath12k_hw_params::single_pdev_only):
	 *
	 * Firmware maintains data for all bands but advertises a single
	 * phy to the host which is stored as a single element in this
	 * array.
	 *
	 * Other devices:
	 *
	 * This array will contain as many elements as the number of
	 * radios.
	 */
	struct ath12k_pdev pdevs[MAX_RADIOS];

	/* struct ath12k_hw_params::single_pdev_only devices use this to
	 * store phy specific data
	 */
	struct ath12k_fw_pdev fw_pdev[MAX_RADIOS];
	u8 fw_pdev_count;

	struct ath12k_pdev __rcu *pdevs_active[MAX_RADIOS];

	struct ath12k_wmi_hal_reg_capabilities_ext_arg hal_reg_cap[MAX_RADIOS];
	/* Secondary RF path freq caps from SERVICE_READY_EXT2 (rf_switch_config) */
	struct ath12k_wmi_hal_reg_capabilities_ext2_arg hal_reg_cap_ext2[MAX_RADIOS];
	/* Boot-time RF path selection from rf_switch_config INI parameter. */
	enum ath12k_rf_path_index rf_switch_config;
	unsigned long long free_vdev_map;
	unsigned long long free_vdev_stats_id_map;
	wait_queue_head_t peer_mapping_wq;
	u8 mac_addr[ETH_ALEN];
	bool wmi_ready;
	u32 wlan_init_status;
	int irq_num[ATH12K_IRQ_NUM_MAX];
	struct ath12k_ext_irq_grp ext_irq_grp[ATH12K_EXT_IRQ_NUM_MAX];
	struct napi_struct *napi;
	struct ath12k_wmi_target_cap_arg target_caps;
	u32 ext_service_bitmap[WMI_SERVICE_EXT_BM_SIZE];
	bool pdevs_macaddr_valid;

	struct ath12k_hw_params *hw_params;

	const struct firmware *cal_file;

	/* Below regd's are protected by ab->data_lock */
	/* This is the regd set for every radio
	 * by the firmware during initialization
	 */
	struct ieee80211_regdomain *default_regd[MAX_RADIOS];
	/* This regd is set during dynamic country setting
	 * This may or may not be used during the runtime
	 */
	struct ieee80211_regdomain *new_regd[MAX_RADIOS];
	bool regd_freed;

	/* afc_exp_info is used to store the AFC expiry
	 * information for each radio. This is used to forward the
	 * AFC expiry information to the user space during MAC
	 * registration.
	 */
	struct ath12k_afc_expiry_info afc_exp_info[MAX_RADIOS];
	/* 6 GHz standard power rules from cc ext event are saved here
	 * as it should not be updated to cfg unless we have a AFC
	 * response
	 */
	struct ath12k_6ghz_sp_reg_rule *sp_rule;
	/* afc_dev_deployment is used to store the device deployment type
	 * as advertised by FW.
	 */
	enum ath12k_afc_dev_deploy_type afc_dev_deployment;

	/* Current DFS Regulatory */
	enum ath12k_dfs_region dfs_region;
	/* Protects reg_freq_2g/5g/6g against concurrent reset+write in
	 * ath12k_reg_build_regd() vs. read in ath12k_regd_update_freq_range(),
	 * ath12k_mac_setup_channels_rates() and
	 * ath12k_mac_setup_channels_rates_multiband().
	 */
	spinlock_t reg_freq_lock;
	struct ath12k_reg_freq reg_freq_2g;
	struct ath12k_reg_freq reg_freq_5g;
	struct ath12k_reg_freq reg_freq_6g;
	bool is_cumac_chip;
	bool cumac_configured;
#ifdef CPTCFG_ATH12K_DEBUGFS
	struct dentry *debugfs_soc;
#endif
	struct ath12k_cfg_ctx *cfg_ctx;
	unsigned long dev_flags;
	/* Skip RDDM collection of partner chips if cumac hw error causes
	 * fallback to Mode0
	 */
	bool recovery_skip_dump;
	struct completion driver_recovery;
	struct workqueue_struct *workqueue;
	struct work_struct restart_work;
	struct workqueue_struct *workqueue_aux;
	struct work_struct reset_work;
	atomic_t reset_count;
	atomic_t recovery_count;
	bool is_reset;
	struct completion reset_complete;
	/* continuous recovery fail count */
	atomic_t fail_cont_count;
	unsigned long reset_fail_timeout;
	struct work_struct update_11d_work;
	u8 new_alpha2[2];
	struct {
		/* protected by data_lock */
		u32 fw_crash_counter;
		u32 last_recovery_time;
	} stats;
	u32 pktlog_defs_checksum;

	struct ath12k_dbring_cap *db_caps;
	u32 num_db_cap;

	struct completion htc_suspend;

	enum ath12k_fw_recovery_option fw_recovery_support;
	u32 recovery_start_time;
	bool recovery_start;

	u32 *crash_info_address;
	u32 *recovery_mode_address;

	u32 fw_dbglog_param;
	u64 fw_dbglog_val;

	u64 fw_soc_drop_count;
	bool static_window_map;
	struct device_node *hremote_node;

	struct work_struct rfkill_work;
	/* true means radio is on */
	bool rfkill_radio_on;

	struct {
		enum ath12k_bdf_search bdf_search;
		u32 vendor;
		u32 device;
		u32 subsystem_vendor;
		u32 subsystem_device;
	} id;

	struct {
		u32 api_version;

		const struct firmware *fw;
		const u8 *amss_data;
		size_t amss_len;
		const u8 *amss_dualmac_data;
		size_t amss_dualmac_len;
		const u8 *m3_data;
		size_t m3_len;
		const u8 *aux_data;
		size_t aux_len;
		const u8 *mcss_data;
		size_t mcss_len;

		DECLARE_BITMAP(fw_features, ATH12K_FW_FEATURE_COUNT);
	} fw;

	struct completion restart_completed;
	struct completion rddm_reset_done;

#ifdef CONFIG_ACPI

	struct {
		bool started;
		u32 func_bit;
		bool acpi_tas_enable;
		bool acpi_bios_sar_enable;
		bool acpi_disable_11be;
		bool acpi_disable_rfkill;
		bool acpi_cca_enable;
		bool acpi_band_edge_enable;
		bool acpi_enable_bdf;
		u32 bit_flag;
		char bdf_string[ATH12K_ACPI_BDF_MAX_LEN];
		u8 tas_cfg[ATH12K_ACPI_DSM_TAS_CFG_SIZE];
		u8 tas_sar_power_table[ATH12K_ACPI_DSM_TAS_DATA_SIZE];
		u8 bios_sar_data[ATH12K_ACPI_DSM_BIOS_SAR_DATA_SIZE];
		u8 geo_offset_data[ATH12K_ACPI_DSM_GEO_OFFSET_DATA_SIZE];
		u8 cca_data[ATH12K_ACPI_DSM_CCA_DATA_SIZE];
		u8 band_edge_power[ATH12K_ACPI_DSM_BAND_EDGE_DATA_SIZE];
	} acpi;

#endif /* CONFIG_ACPI */

	struct notifier_block panic_nb;

	struct {
		struct notifier_block nb;
#define __NB_FLAGS_REGISTERED 1
		unsigned long nb_flags;
	} me;

	struct ath12k_hw_group *ag;
	struct ath12k_wsi_info wsi_info;
	enum ath12k_firmware_mode fw_mode;
	struct ath12k_ftm_event_obj ftm_event_obj;
	bool hw_group_ref;

	struct {
                const struct ath12k_msi_config *config;
                u32 ep_base_data;
                u32 irqs[32];
                u32 addr_lo;
                u32 addr_hi;
        } msi;
	bool in_panic;
	bool is_qdss_tracing;
	u32 host_ddr_fixed_mem_off;
	struct ath12k_internal_pci ipci;
	bool ce_pipe_init_done;
	bool rxgainlut_support;
	bool fw_cfg_support;
	bool is_dualmac;
	enum wide_band_cap wide_band;

	const struct ieee80211_ops *ath12k_ops;

#ifdef CPTCFG_QCN_EXTN
	struct ath12k_base_extn ath12k_base_extn;

	const struct ieee80211_ops_extn *ath12k_ops_extn;
#endif /* CPTCFG_QCN_EXTN */

	bool in_coldboot_fwreset;
	u32 chwidth_num_peer_caps;

	/* Number of ML peers supported by firmware */
	u32 max_ml_peer_supported;
	u32 max_ml_peer_ids;
	u32 fw_max_peers;
	bool mm_cal_support;

	u32 max_tid_msduq;
	u32 def_tid_msduq;

	struct work_struct recovery_work;
	struct ath12k_dp_umac_reset dp_umac_reset;
	int umcmn_irq_num;
	struct timer_list umcmn_timer;
	struct timer_list tqm_tp_timer;
	bool early_cal_support;
	bool powerup_triggered;
	struct ath12k_wsi_info bypass_wsi_info;
	bool is_bypassed;
	enum ath12k_wsi_bypass_action wsi_remap_state;
	bool is_static_bypassed;
	u32 num_max_vdev_supported;
#ifdef CPTCFG_ATHDEBUG
	struct athdbg_qmi dbg_qmi;
#endif

	u32 twt_cap_bitmap;


	u32 cu_mem_cfg_mask;
	struct ath12k_mem_dev qmi_mem_dev;
	struct ath12k_mem_dev mlo_mem_dev;

	bool map_event_required;
	bool skip_cumac_hw_reset;

	struct ath12k_mem_profile_based_param mem_params;

	u32 mapc_hw_cap_bitmap;
	u8 mapc_max_co_ap_peers;
	u8 mapc_max_ctdma_peers;
	bool umcmn_fatal_received;

	struct ath12k_wmi_enh_aoa_caps_arg enh_aoa_caps;
	bool umac_reset_init_done;

	/* must be last */
	u8 drv_priv[] __aligned(sizeof(void *));
};

struct ath12k_pdev_map {
	struct ath12k_base *ab;
	u8 pdev_idx;
};

struct ath12k_fw_stats_vdev {
	struct list_head list;

	u32 vdev_id;
	u32 beacon_snr;
	u32 data_snr;
	u32 num_tx_frames[WLAN_MAX_AC];
	u32 num_rx_frames;
	u32 num_tx_frames_retries[WLAN_MAX_AC];
	u32 num_tx_frames_failures[WLAN_MAX_AC];
	u32 num_rts_fail;
	u32 num_rts_success;
	u32 num_rx_err;
	u32 num_rx_discard;
	u32 num_tx_not_acked;
	u32 tx_rate_history[MAX_TX_RATE_VALUES];
	u32 beacon_rssi_history[MAX_TX_RATE_VALUES];
};

struct ath12k_fw_stats_vdev_extd {
	struct list_head list;

	u32 vdev_id;
	u32 fd_succ_cnt;
	u32 fd_fail_cnt;
	u32 unsolicited_prb_succ_cnt;
	u32 unsolicited_prb_fail_cnt;
	u32 flags;
	s32 vdev_tx_power;
};

struct ath12k_fw_stats_bcn {
	struct list_head list;

	u32 vdev_id;
	u32 tx_bcn_succ_cnt;
	u32 tx_bcn_outage_cnt;
};

#ifdef CPTCFG_QCN_EXTN
struct ath12k_dcs_wlan_interference {
	struct list_head list;
	struct wmi_dcs_wlan_interference_stats info;
};
#endif /* CPTCFG_QCN_EXTN */

struct ath12k_fw_stats_pdev {
	struct list_head list;

	/* PDEV stats */
	s32 ch_noise_floor;
	u32 tx_frame_count;
	u32 rx_frame_count;
	u32 rx_clear_count;
	u32 cycle_count;
	u32 phy_err_count;
	u32 chan_tx_power;
	u32 ack_rx_bad;
	u32 rts_bad;
	u32 rts_good;
	u32 fcs_bad;
	u32 no_beacons;
	u32 mib_int_count;

	/* PDEV TX stats */
	s32 comp_queued;
	s32 comp_delivered;
	s32 msdu_enqued;
	s32 mpdu_enqued;
	s32 wmm_drop;
	s32 local_enqued;
	s32 local_freed;
	s32 hw_queued;
	s32 hw_reaped;
	s32 underrun;
	s32 tx_abort;
	s32 mpdus_requed;
	u32 tx_ko;
	u32 data_rc;
	u32 self_triggers;
	u32 sw_retry_failure;
	u32 illgl_rate_phy_err;
	u32 pdev_cont_xretry;
	u32 pdev_tx_timeout;
	u32 pdev_resets;
	u32 stateless_tid_alloc_failure;
	u32 phy_underrun;
	u32 txop_ovf;

	/* PDEV RX stats */
	s32 mid_ppdu_route_change;
	s32 status_rcvd;
	s32 r0_frags;
	s32 r1_frags;
	s32 r2_frags;
	s32 r3_frags;
	s32 htt_msdus;
	s32 htt_mpdus;
	s32 loc_msdus;
	s32 loc_mpdus;
	s32 oversize_amsdu;
	s32 phy_errs;
	s32 phy_err_drop;
	s32 mpdu_errs;
};

struct ar_sta_cookie {
	u8 addr[ETH_ALEN];
	int vdev_id;
};

struct ath12k_skb_tx_info {
	struct rhash_head rhash_skb;
	struct rcu_head rcu_head;
	struct sk_buff *skb;
	struct ieee80211_tx_rate rate;
};

#define ATH12K_ERP_CUMAC_UNSET -1U
enum ath12k_core_erp_cumac_config_bitmap {
	ATH12K_ERP_CUMAC_PDEV_SUSPEND = BIT(0),
	ATH12K_ERP_CUMAC_Q6_PWR_DOWN = BIT(1),
};

void ath12k_core_panic_notifier_unregister(struct ath12k_base *ab);
int ath12k_core_qmi_firmware_ready(struct ath12k_base *ab, bool *is_ready);
int ath12k_core_init(struct ath12k_base *ath12k);
void ath12k_core_deinit(struct ath12k_base *ath12k);
struct ath12k_base *ath12k_core_alloc(struct device *dev, size_t priv_size,
				      enum ath12k_bus bus);
void ath12k_core_free(struct ath12k_base *ath12k);
int ath12k_core_fetch_board_data_api_1(struct ath12k_base *ab,
				       struct ath12k_board_data *bd,
				       char *filename);
int ath12k_core_fetch_bdf(struct ath12k_base *ath12k,
			  struct ath12k_board_data *bd);
void ath12k_core_free_bdf(struct ath12k_base *ab, struct ath12k_board_data *bd);
int ath12k_core_fetch_regdb(struct ath12k_base *ab, struct ath12k_board_data *bd);
int ath12k_core_fetch_fw_cfg(struct ath12k_base *ath12k,
			     struct ath12k_board_data *bd);
int ath12k_core_fetch_rxgainlut(struct ath12k_base *ath12k,
				struct ath12k_board_data *bd);
int ath12k_core_check_dt(struct ath12k_base *ath12k);
int ath12k_core_check_smbios(struct ath12k_base *ab);
void ath12k_core_halt(struct ath12k *ar);
int ath12k_core_resume_early(struct ath12k_base *ab);
int ath12k_core_resume(struct ath12k_base *ab);
int ath12k_core_suspend(struct ath12k_base *ab);
int ath12k_core_suspend_late(struct ath12k_base *ab);
void ath12k_core_hw_group_unassign(struct ath12k_base *ab);
u8 ath12k_get_num_partner_link(struct ath12k *ar);

int ath12k_skb_rhash_tbl_init(struct ath12k *ar);
void ath12k_skb_rhash_tbl_destroy(struct ath12k *ar);
int ath12k_skb_rhash_insert(struct ath12k *ar, struct sk_buff *skb,
			    struct ieee80211_tx_rate rate);
void ath12k_skb_rhash_remove(struct ath12k *ar, struct sk_buff *skb);
struct ath12k_skb_tx_info *
ath12k_get_skb_tx_info(struct ath12k *ar, struct sk_buff *skb);

const struct firmware *ath12k_core_firmware_request(struct ath12k_base *ab,
						    const char *filename);
void ath12k_core_issue_bug_on(struct ath12k_base *ab);
u32 ath12k_core_get_max_station_per_radio(struct ath12k_base *ab);
u32 ath12k_core_get_def_max_peers_per_radio(struct ath12k_base *ab);
u32 ath12k_core_get_supp_max_peers_per_radio(struct ath12k_base *ab);
u32 ath12k_core_get_max_num_tids(struct ath12k_base *ab);

void ath12k_core_hw_group_set_mlo_capable(struct ath12k_hw_group *ag);
void ath12k_fw_stats_init(struct ath12k *ar);
void ath12k_fw_stats_bcn_free(struct list_head *head);
void ath12k_fw_stats_free(struct ath12k_fw_stats *stats);
void ath12k_fw_stats_reset(struct ath12k *ar);
irqreturn_t ath12k_umac_reset_interrupt_handler(int irq, void *arg);
void ath12k_umac_reset_tasklet_handler(struct tasklet_struct *umac_ctxt);
void ath12k_dp_umac_reset_handle(struct ath12k_base *ab);
int ath12k_dp_umac_reset_init(struct ath12k_base *ab);
void ath12k_dp_umac_reset_deinit(struct ath12k_base *ab);
void ath12k_umac_reset_completion(struct ath12k_base *ab);
struct reserved_mem *ath12k_core_get_reserved_mem_by_name(struct ath12k_base *ab,
						  const char* name);
u8 ath12k_core_get_total_num_vdevs(struct ath12k_base *ab);
bool ath12k_core_is_vdev_limit_reached(struct ath12k *ar, bool is_bridge_vdev);
void ath12k_core_cleanup_power_down_q6(struct ath12k_hw_group *ag, bool standby_mode);
int ath12k_core_power_up(struct ath12k_hw_group *ag);

int ath12k_core_add_dl_qos(struct ath12k_base *ab,
			   struct ath12k_qos_params *params, u8 id);
int ath12k_core_del_dl_qos(struct ath12k_base *ab, u8 id);
int ath12k_core_config_ul_qos(struct ath12k *ar,
			      struct ath12k_qos_params *params,
			      u16 id, u8 *mac_addr, bool add_or_sub);
void ath12k_core_trigger_bug_on(struct ath12k_base *ab);

static inline const char *ath12k_scan_state_str(enum ath12k_scan_state state)
{
	switch (state) {
	case ATH12K_SCAN_IDLE:
		return "idle";
	case ATH12K_SCAN_STARTING:
		return "starting";
	case ATH12K_SCAN_RUNNING:
		return "running";
	case ATH12K_SCAN_ABORTING:
		return "aborting";
	}

	return "unknown";
}

static inline struct ath12k_skb_cb *ATH12K_SKB_CB(struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct ath12k_skb_cb) >
		     IEEE80211_TX_INFO_DRIVER_DATA_SIZE);
	return (struct ath12k_skb_cb *)&IEEE80211_SKB_CB(skb)->driver_data;
}

static inline struct ath12k_skb_rxcb *ATH12K_SKB_RXCB(struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct ath12k_skb_rxcb) > sizeof(skb->cb));
	return (struct ath12k_skb_rxcb *)skb->cb;
}

static inline void *ATH12K_SKB_RXCB_RAW(struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct ath12k_skb_rxcb) > sizeof(skb->cb));
	return (void *)skb->cb;
}

static inline struct ath12k_vif *ath12k_vif_to_ahvif(struct ieee80211_vif *vif)
{
	return (struct ath12k_vif *)vif->drv_priv;
}

static inline struct ath12k_sta *ath12k_sta_to_ahsta(struct ieee80211_sta *sta)
{
	return (struct ath12k_sta *)sta->drv_priv;
}

static inline struct ieee80211_sta *ath12k_ahsta_to_sta(struct ath12k_sta *ahsta)
{
	return container_of((void *)ahsta, struct ieee80211_sta, drv_priv);
}

static inline bool ath12k_is_mlo_sta(struct ieee80211_sta *sta,
				     struct ath12k_sta *ahsta)
{
	return sta && ahsta && sta->mlo && ahsta->is_mlo;
}

static inline struct ieee80211_vif *ath12k_ahvif_to_vif(struct ath12k_vif *ahvif)
{
	return container_of((void *)ahvif, struct ieee80211_vif, drv_priv);
}

static inline struct ath12k *ath12k_ab_to_ar(struct ath12k_base *ab,
					     int mac_id)
{
	return ab->pdevs[ath12k_hw_mac_id_to_pdev_id(ab->hw_params, mac_id)].ar;
}

static inline void ath12k_core_create_firmware_path(struct ath12k_base *ab,
						    const char *filename,
						    void *buf, size_t buf_len)
{
	snprintf(buf, buf_len, "%s/%s/%s", ATH12K_FW_DIR,
		 ab->hw_params->fw.dir, filename);
}

static inline const char *ath12k_bus_str(enum ath12k_bus bus)
{
	switch (bus) {
	case ATH12K_BUS_PCI:
		return "pci";
	case ATH12K_BUS_AHB:
		return "ahb";
	case ATH12K_BUS_HYBRID:
		return "ahb";
	}

	return "unknown";
}

static inline struct ath12k_link_vif *
ath12k_get_arvif_from_link_id(struct ath12k_vif *ahvif, int link_id)
{
	if (link_id >= ATH12K_NUM_MAX_LINKS)
		return NULL;

	lockdep_assert_wiphy(ahvif->ah->hw->wiphy);

	return wiphy_dereference(ahvif->ah->hw->wiphy, ahvif->link[link_id]);
}

static inline struct ath12k_hw *ath12k_hw_to_ah(struct ieee80211_hw  *hw)
{
	return hw->priv;
}

static inline struct ath12k *ath12k_ah_to_ar(struct ath12k_hw *ah, u8 radio_id)
{
	if (WARN(radio_id >= ah->num_radio,
		 "bad radio id %d, so switch to default link\n", radio_id))
		radio_id = 0;

	return &ah->radio[radio_id];
}

static inline struct ath12k_hw *ath12k_ar_to_ah(struct ath12k *ar)
{
	return ar->ah;
}

static inline struct ieee80211_hw *ath12k_ar_to_hw(struct ath12k *ar)
{
	return ar->ah->hw;
}

#define for_each_ar(ah, ar, index) \
	for ((index) = 0; ((index) < (ah)->num_radio && \
	     ((ar) = &(ah)->radio[(index)])); (index)++)

static inline struct ath12k_hw *ath12k_ag_to_ah(struct ath12k_hw_group *ag, int idx)
{
	return ag->ah[idx];
}

static inline void ath12k_ag_set_ah(struct ath12k_hw_group *ag, int idx,
				    struct ath12k_hw *ah)
{
	ag->ah[idx] = ah;
}

static inline struct ath12k_hw_group *ath12k_ab_to_ag(struct ath12k_base *ab)
{
	return ab->ag;
}

static inline struct ath12k_base *ath12k_ag_to_ab(struct ath12k_hw_group *ag,
						  u8 device_id)
{
	return ag->ab[device_id];
}

static inline struct ath12k_dp *ath12k_ab_to_dp(struct ath12k_base *ab)
{
	return ab->dp;
}

static inline bool ath12k_check_erp_power_down(struct ath12k_hw_group *ag)
{
	return test_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ag->flags);
}

static inline struct ath12k_hw_group *ath12k_ah_to_ag(struct ath12k_hw *ah)
{
	struct ath12k *ar = ah->radio;

	return ar->ab->ag;
}

static inline bool ath12k_scan_radio_supported(struct ath12k_pdev *pdev)
{
	return !!(pdev->cap.scan_radio_caps & ATH12K_SCAN_RADIO_CAP_SUPPORTED);
}

static inline bool ath12k_scan_radio_dfs_enabled(struct ath12k_pdev *pdev)
{
	return !!(pdev->cap.scan_radio_caps & ATH12K_SCAN_RADIO_CAP_DFS_ENABLED);
}

static inline bool ath12k_scan_radio_blanking_supported(struct ath12k_pdev *pdev)
{
	return !!(pdev->cap.scan_radio_caps & ATH12K_SCAN_RADIO_CAP_BLANKING);
}

int ath12k_core_config_iocoherency(struct ath12k_base *ab, bool enable);

static inline bool ath12k_hw_group_recovery_in_progress(const struct ath12k_hw_group *ag)
{
	return test_bit(ATH12K_GROUP_FLAG_RECOVERY, &ag->flags);
}

/* Mode3 (client-retaining) recovery drives its own T2LM steering
 * (one-link-per-MLD) across the asserted-chip restart.  That recovery T2LM has
 * higher precedence than any user/AP-advertised T2LM: while Mode3 recovery is
 * in progress, or specifically during the recovery T2LM window, a user-driven
 * T2LM must be aborted so it cannot clobber the recovery steering.
 */
static inline bool ath12k_mode3_t2lm_recovery_active(const struct ath12k_hw_group *ag)
{
	return ag->recovery_t2lm_active ||
	       (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE3 &&
		ath12k_hw_group_recovery_in_progress(ag));
}

/* True while Mode3 (client-retaining) recovery is running for the group.  Used
 * to reject MLO link-reconfig requests that arrive after Mode3 recovery has
 * started, since they would race the recovery's link remove/readd.
 */
static inline bool ath12k_mode3_recovery_in_progress(const struct ath12k_hw_group *ag)
{
	return ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE3 &&
	       ath12k_hw_group_recovery_in_progress(ag);
}

static inline void
ath12k_core_dma_sync_single_for_cpu(struct device *dev,
				    dma_addr_t dma_handle,
				    size_t size,
				    enum dma_data_direction direction)
{
#ifndef CONFIG_IO_COHERENCY
	dma_sync_single_for_cpu(dev, dma_handle, size, direction);
#endif
}

static inline void
ath12k_core_dma_sync_single_for_device(struct device *dev,
				       dma_addr_t dma_handle,
				       size_t size,
				       enum dma_data_direction direction)
{
#ifndef CONFIG_IO_COHERENCY
	dma_sync_single_for_device(dev, dma_handle, size, direction);
#endif
}

static inline dma_addr_t
ath12k_core_dma_map_single(struct device *dev, void *vaddr,
			   size_t size, enum dma_data_direction direction)
{
	dma_addr_t paddr;
#ifndef CONFIG_IO_COHERENCY
	int ret;

	paddr = dma_map_single(dev, vaddr, size, direction);
	ret = dma_mapping_error(dev, paddr);
	if (ret)
		return 0;
#else
	paddr = virt_to_phys(vaddr);
	if (!paddr)
		return 0;
#endif

	return paddr;
}

static inline void ath12k_core_dma_unmap_single(struct device *dev, dma_addr_t dma_handle,
						size_t size, enum dma_data_direction direction)
{
#ifndef CONFIG_IO_COHERENCY
	dma_unmap_single(dev, dma_handle, size, direction);
#endif
}

static inline void ath12k_core_dma_unmap_single_attrs(struct device *dev,
						      dma_addr_t dma_handle, size_t size,
						      enum dma_data_direction direction,
						      unsigned long attrs)
{
#ifndef CONFIG_IO_COHERENCY
	dma_unmap_single_attrs(dev, dma_handle, size, direction, attrs);
#endif
}

static inline dma_addr_t
ath12k_core_dma_map_frag(struct device *dev, const skb_frag_t *frag,
			 size_t size, int offset,
			 enum dma_data_direction direction)
{
	dma_addr_t paddr;
#ifndef CONFIG_IO_COHERENCY
	paddr = skb_frag_dma_map(dev, frag, offset, size, direction);
	if (dma_mapping_error(dev, paddr))
		return 0;
#else
	paddr = page_to_phys(skb_frag_page(frag)) + skb_frag_off(frag) + offset;
	if (!paddr)
		return 0;
#endif
	return paddr;
}

static inline dma_addr_t
ath12k_core_dma_map_page(struct device *dev, struct page *page,
			 unsigned long offset, size_t size,
			 enum dma_data_direction dir)
{
#ifndef CONFIG_IO_COHERENCY
	dma_addr_t addr;

	addr = dma_map_page(dev, page, offset, size, dir);

	return addr;
#else
	return (virt_to_phys(page_address(page)) + offset);
#endif
}

static inline void ath12k_core_dma_unmap_page(struct device *dev, dma_addr_t handle,
					      size_t size, enum dma_data_direction dir)
{
#ifndef CONFIG_IO_COHERENCY
	dma_unmap_page(dev, handle, size, dir);
#endif
}

#ifndef PLATFORM_SDX
static inline void ath12k_core_dmac_inv_range_no_dsb(const void *start, const void *end)
{
#ifndef CONFIG_IO_COHERENCY
	dmac_inv_range_no_dsb(start, end);
#endif
}
#else
static inline void ath12k_core_dmac_inv_range_no_dsb(const void *start, const void *end)
{
}
#endif


#ifndef PLATFORM_SDX
static inline void ath12k_core_dmac_inv_range(const void *start, const void *end)
{
#ifndef CONFIG_IO_COHERENCY
	dmac_inv_range(start, end);
#endif
}
#else
static inline void ath12k_core_dmac_inv_range(const void *start, const void *end)
{
}
#endif

static inline void ath12k_core_dsb(void)
{
#ifndef CONFIG_IO_COHERENCY
	dsb(st);
#endif
}

#ifndef PLATFORM_SDX
static inline void ath12k_core_dmac_clean_range(const void *start, const void *end)
{
#ifndef CONFIG_IO_COHERENCY
	dmac_clean_range(start, end);
#endif
}
#else
static inline void ath12k_core_dmac_clean_range(const void *start, const void *end)
{
}
#endif

static inline struct ath12k_base *ath12k_pdev_to_ab(struct ath12k_pdev *pdev)
{
       if (!pdev)
               return NULL;

       return pdev->ar->ab;
}

static inline int ath12k_get_ab_device_id(struct ath12k_base *ab)
{
	if (!ab)
		return -1;

	return ab->device_id;
}

static inline int ath12k_get_pdev_id(struct ath12k_pdev *pdev)
{
       if (!pdev)
               return -1;

       return pdev->pdev_id;
}

static inline int ath12k_get_peer_count(struct ath12k_base *ab, bool get_max)
{
       struct ath12k_pdev *pdev;
       int peer_count = 0;
       int i;

       if (!ab)
               return 0;

       for (i = 0; i < ab->num_radios; i++) {
               rcu_read_lock();
               pdev = rcu_dereference(ab->pdevs_active[i]);
               if (pdev && pdev->ar) {
                       if (get_max)
                               peer_count += pdev->ar->max_num_peers;
                       else
                               peer_count += pdev->ar->num_peers;
               }
               rcu_read_unlock();
       }

       return peer_count;
}

static inline void
ath12k_core_srng_get_htt_mgmt_filter(struct ath12k_base *ab,
				     struct htt_rx_ring_tlv_filter *tlv_filter)
{
	ath12k_dp_get_htt_mgmt_filter(ab, tlv_filter);
}

static inline int
ath12k_core_srng_htt_rx_filter_setup(struct ath12k_base *ab, u32 ring_id, int mac_id,
				     enum hal_ring_type ring_type, int rx_buf_size,
				     struct htt_rx_ring_tlv_filter *tlv_filter)
{
	return ath12k_dp_tx_htt_rx_filter_setup(ab, ring_id, mac_id, ring_type,
						rx_buf_size, ATH12K_PKTLOG_DISABLED,
						tlv_filter);
}

extern unsigned int ath12k_mlo_capable;

int ath12k_wsi_load_info_init(struct ath12k_base *ab);
void ath12k_wsi_load_info_deinit(struct ath12k_base *ab,
				 struct ath12k_mlo_wsi_load_info *wsi_load_info);
void ath12k_wsi_load_info_wsiorder_update(struct ath12k_base *ab);
struct ath12k_base *ath12k_core_get_ab_by_wiphy(const struct wiphy *wiphy,
					        bool no_arvifs);
u8 ath12k_core_get_ab_list_by_wiphy(const struct wiphy *wiphy,
				    struct ath12k_base **ab_list,
				    u8 ab_list_size);
int ath12k_wifi_stats_reply_setup(struct ath12k_telemetry_command *cmd);
struct ath12k_wsi_info *ath12k_core_get_current_wsi_info(struct ath12k_base *ab);
int ath12k_core_dynamic_wsi_remap(struct ath12k_base *ab);
void ath12k_core_pci_link_speed(struct ath12k_base *ab, u16 link_speed, u16 link_width);
void ath12k_update_mlo_adj_chip(struct ath12k_hw_group *ag);
void ath12k_core_wsi_remap_mlo_reconfig(struct ath12k_hw_group *ag);
int ath12k_core_wsi_remap_pdev_suspend(struct ath12k_base *ab);
void ath12k_core_cleanup(struct ath12k_base *ab);
void ath12k_core_to_group_ref_put(struct ath12k_base *ab);
void ath12k_core_radio_cleanup(struct ath12k *ar);
void ath12k_telemetry_notify_breach(u8 *mac_addr, u8 svc_id, u8 param,
				    bool set_clear, u8 tid);
void ath12k_rssi_rate_notify_breach_event(u8 *mac_addr, u8 breach_type,
					  u32 threshold_value, u32 detected_value,
					  bool set_clear);
struct ath12k_hw_group *ath12k_core_get_ag(void);
void ath12k_core_trigger_partner_device_crash(struct ath12k_base *ab);
void ath12k_core_pdev_deinit(struct ath12k_base *ab);
int ath12k_core_radio_start(struct ath12k_hw *ah);

struct ath12k *ath12k_core_ar_from_hw_link_id(struct ath12k_base *ab, u8 hw_link_id);

/* Below APIs will be cleaned up to be common utils */
int ath12k_core_crypto_param_len(struct ath12k_base *ab, enum hal_encrypt_type enctype);
int ath12k_core_crypto_icv_len(struct ath12k_base *ab, enum hal_encrypt_type enctype);
int ath12k_core_crypto_mic_len(struct ath12k_base *ab, enum hal_encrypt_type enctype);
enum ath12k_peer_metadata_version
ath12k_core_get_peer_metadata_ver(struct ath12k_base *ab);
void ath12k_mvr_ch_switch_notify_work(struct work_struct *work);

int ath12k_core_cu_mem_alloc(struct ath12k *ar, struct ath12k_link_vif *arvif);
void ath12k_core_cu_mem_free(struct ath12k *ar, struct ath12k_link_vif *arvif);
void ath12k_core_cu_mem_free_all(struct ath12k *ar);
int ath12k_core_cu_mem_pool_init(struct ath12k_hw_group *ag);
void ath12k_core_cu_mem_pool_deinit(struct ath12k_hw_group *ag);
void ath12k_core_cu_notify(struct ath12k *ar, struct ath12k_link_vif *arvif);
struct wireless_dev *ath12k_get_wdev_from_netdev(struct net_device *dev);
int ath12k_wsi_bypass_precheck(struct ath12k_base *ab, unsigned int value);
int ath12k_core_mlo_setup(struct ath12k_hw_group *ag);
int ath12k_core_wsi_mlo_teardown_umac_reset(struct ath12k_base *ab);
void ath12k_core_send_fw_hang_cmd(struct ath12k_base *ab, unsigned int value);
void ath12k_core_en_ssr(struct ath12k_base *ab, unsigned int value);
void ath12k_recovery_skip_partner_dump_collection(struct ath12k_base *ab);

#endif /* _CORE_H_ */
