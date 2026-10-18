/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _NSS_PLUGINS_H_
#define _NSS_PLUGINS_H_

#include <linux/netdevice.h>
#include <linux/module.h>
#include <ppe_vp_public.h>

#define PPE_DS_MAX_RING_PER_NODE	2
#define PPE_DS_WIFI_ARCH_MODE_WIFI7	7
#define PPE_DS_WIFI_ARCH_MODE_WIFI8	8

/**
 * ppe_ds_wlan_node_freq
 *	Radio channel frequency range for a PPE-DS node.
 */
struct ppe_ds_wlan_node_freq {
	uint32_t low_freq;	/**< Radio channel low frequency in MHz */
	uint32_t high_freq;	/**< Radio channel high frequency in MHz */
};

/**
 * ppe_ds_wlan_node_type_t
 *	PPE-DS node type
 */
typedef enum {
	PPE_DS_NODE_TYPE_2G,		/**< radio 2G */
	PPE_DS_NODE_TYPE_5G,		/**< radio 5G */
	PPE_DS_NODE_TYPE_6G,		/**< radio 6G */
	PPE_DS_NODE_TYPE_MAX,
} ppe_ds_wlan_node_type_t;

/**
 * ppe_ds_wlan_txdesc_elem
 *	PPEDS WLAN Tx descriptor element information
 */
struct ppe_ds_wlan_txdesc_elem {
	uint32_t opaque_lo;		/**< Low 32-bit opaque field content */
	uint32_t opaque_hi;		/**< High 32-bit opaque field content */
	dma_addr_t buff_addr;		/**< Buffer address */
};

/**
 * ppe_ds_wlan_rxdesc_elem
 *	PPEDS WLAN Rx descriptor element information
 */
struct ppe_ds_wlan_rxdesc_elem {
	unsigned long cookie;		/**< cookie information */
};

struct ppe_wlan_plugin_stats;

/**
 * ppe_ds_wlan_handle
 *	PPE-DS WLAN handle
 */
typedef struct ppe_ds_wlan_handle {
	uint32_t reserved;			/**< Reserved */
	char priv[] __aligned(NETDEV_ALIGN);	/**< contains the address of WLAN SoC base address */
} ppe_ds_wlan_handle_t;

/**
 * ppe_ds_wlan_ctx_info_handle
 *	PPE-DS wlan umac reset handle
 */
struct ppe_ds_wlan_ctx_info_handle {
	uint32_t umac_reset_inprogress;		/**< umac reset in progress information */
};

/**
 * ppe_ds_wlan_reg_info
 *	PPE-DS WLAN rings information
 */
struct ppe_ds_wlan_reg_info {
	dma_addr_t ppe2tcl_ba;		/**< PPE2TCL ring base address */
	dma_addr_t reo2ppe_ba;		/**< REO2PPE ring base address */
	uint32_t ppe2tcl_num_desc;	/**< PPE2TCL ring descriptor count */
	uint32_t reo2ppe_num_desc;	/**< REO2PPE ring descriptor count */
	ppe_ds_wlan_node_type_t node_type;	/**< PPE-DS node type */
	uint32_t ppe2tcl_start_idx;		/**< PPE2TCL ring index */
	uint32_t reo2ppe_start_idx;		/**< REO2PPE ring index */
	bool ppe_ds_int_mode_enabled;	/**< Interrupt mode to process PPE2TCL */
	uint8_t dp_ppeds_node_id;	/**< Node id of ds node */
	struct ppe_ds_wlan_node_freq freq;	/**< Radio channel frequency range in MHz */
};

struct ppe_ds_reg_addr_info {
	void __iomem *vaddr; /**< Virtual address */
	dma_addr_t paddr; /**< Physical address */
};

/**
 * ppe_ds_wlan_reg_data_ring_cfg
 *	Data ring information
 */
struct ppe_ds_wlan_reg_data_ring_cfg {
	uint8_t num_reo2ppe;	/**< Number of REO2PPE ring*/
	uint8_t num_ppe2tcl;	/**< Number of PPE2TCL ring*/
	dma_addr_t ppe2tcl_ba[PPE_DS_MAX_RING_PER_NODE];	/**< PPE2TCL ring base address */
	dma_addr_t reo2ppe_ba[PPE_DS_MAX_RING_PER_NODE];	/**< REO2PPE ring base address */
	uint32_t ppe2tcl_num_desc[PPE_DS_MAX_RING_PER_NODE];	/**< PPE2TCL ring descriptor count */
	uint32_t reo2ppe_num_desc[PPE_DS_MAX_RING_PER_NODE];	/**< REO2PPE ring descriptor count */
};

/**
 * ppe_ds_wlan_reg_data_ring_hptp_cfg
 *	Data ring TX RX information.
 */
struct ppe_ds_wlan_reg_data_ring_hptp_cfg {
	struct ppe_ds_reg_addr_info wlan_ppe2tcl_hp_addr[PPE_DS_MAX_RING_PER_NODE];	/**< edma PPE2TCL Producer register address */
	struct ppe_ds_reg_addr_info wlan_reo2ppe_tp_addr[PPE_DS_MAX_RING_PER_NODE];	/**< edma REO2PPE consumer register address */
	struct ppe_ds_reg_addr_info edma_txdesc_prod_addr[PPE_DS_MAX_RING_PER_NODE];	/**< edma TXDESC producer register address */
	struct ppe_ds_reg_addr_info edma_rxdesc_cons_addr[PPE_DS_MAX_RING_PER_NODE];	/**< edma RXDESC consumer register address */
};

/**
 * ppe_ds_wlan_reg_hbm_ring_cfg
 *	HW buffer ring information
 */
struct ppe_ds_wlan_reg_hbm_ring_cfg {
	dma_addr_t tqm2ppe_ba;	/**< TQM2PPE ring's base address */
	dma_addr_t ppe2wbm_ba;	/**< PPE2WBM ring's base address */
	uint32_t tqm2ppe_num_desc;	/**< TQM2PPE descriptor count */
	uint32_t ppe2wbm_num_desc;	/**< PPE2WBM descriptor count */
};

/**
 * ppe_ds_wlan_reg_hbm_ring_hptp_cfg
 *	HW buffer ring TX RX information.
 */
struct ppe_ds_wlan_reg_hbm_ring_hptp_cfg {
	struct ppe_ds_reg_addr_info edma_txcmpl_cons_addr;	/**< edma TXCMPL consumer register address */
	struct ppe_ds_reg_addr_info edma_rxfill_prod_addr;	/**< edma RXFILL Producer register address */
	struct ppe_ds_reg_addr_info wlan_ppe2wbm_hp_addr;	/**< edma PPE2WBM Producer register address */
	struct ppe_ds_reg_addr_info wlan_tqm2ppe_tp_addr;	/**< edma TQM2PPE tail pointer register address */
};

/**
 * ppe_ds_wlan_arch_reg_info
 *	PPE-DS WLAN rings information for wifi7/wifi8
 */
struct ppe_ds_wlan_arch_reg_info {
	ppe_ds_wlan_node_type_t node_type;	/**< PPE-DS node type */
	uint8_t dp_ppeds_node_id;	/**< Node id of ds node */
	uint8_t wifi_arch_mode;	/**< 7 = wifi 7, 8 = wifi8 */

	union {
		struct {
			dma_addr_t ppe2tcl_ba;	/**< PPE2TCL ring base address */
			dma_addr_t reo2ppe_ba;	/**< REO2PPE ring base address */
			uint32_t ppe2tcl_num_desc;	/**< PPE2TCL ring descriptor count */
			uint32_t reo2ppe_num_desc;	/**< REO2PPE ring descriptor count */
			uint32_t ppe2tcl_start_idx;	/**< PPE2TCL ring index */
			uint32_t reo2ppe_start_idx;	/**< REO2PPE ring index */
			bool ppe_ds_int_mode_enabled;	/**< Interrupt mode to process PPE2TCL */
		} wifi7_cfg;

		struct {
			uint8_t data_ring_auto_index_en;	/**< Auto index Enabled / Disabled by EDMA*/
			uint8_t hw_buff_mgmt_en;	/**< HW buffer manager Enabled / Disabled by EDMA */
			struct {
				struct ppe_ds_wlan_reg_data_ring_cfg ring_info;
				struct ppe_ds_wlan_reg_data_ring_hptp_cfg txrx_info;
			} data_ring;

			struct {
				struct ppe_ds_wlan_reg_hbm_ring_cfg ring_info;
				struct ppe_ds_wlan_reg_hbm_ring_hptp_cfg txrx_info;
			} hw_buf_mgmt;
		} wifi8_cfg;
	};

	struct ppe_ds_wlan_node_freq freq;	/**< Radio channel frequency range in MHz */
};

struct ppe_vp_ui;

/**
 * ppe_ds_wlan_rxfill_ring_info_wifi7
 *	WiFi7 PPE-DS RxFill ring filled-count information (single SW ring, no HW buffer management)
 */
struct ppe_ds_wlan_rxfill_ring_info_wifi7 {
	uint16_t prim_prod_idx;		/**< RxFill ring producer index */
	uint16_t prim_cons_idx;		/**< RxFill ring consumer index (read from HW) */
	uint16_t prim_active_cnt;	/**< RxFill ring active count */
};

/**
 * ppe_ds_wlan_rxfill_ring_info_wifi8
 *	WiFi8 PPE-DS SW and HW RxFill ring filled-count information
 */
struct ppe_ds_wlan_rxfill_ring_info_wifi8 {
	uint16_t prim_prod_idx;		/**< SW RxFill ring producer index */
	uint16_t prim_cons_idx;		/**< SW RxFill ring consumer index (read from HW) */
	uint16_t prim_active_cnt;	/**< SW RxFill ring active count */
	uint16_t secd_prod_idx;		/**< HW RxFill ring producer index (read from HW) */
	uint16_t secd_cons_idx;		/**< HW RxFill ring consumer index */
	uint16_t secd_active_cnt;	/**< HW RxFill ring active count */
	bool hw_buff_mgmt_en;		/**< HW buffer manager enabled flag */
};

/**
 * ppe_ds_wlan_rxfill_ring_info
 *	Generic PPE-DS RxFill ring filled-count information, arch-specific via union
 */
struct ppe_ds_wlan_rxfill_ring_info {
	uint8_t arch_mode;			/**< WiFi architecture mode (PPE_DS_WIFI_ARCH_MODE_WIFI7/8) */
	union {
		struct ppe_ds_wlan_rxfill_ring_info_wifi7 wifi7;	/**< WiFi7 RxFill ring info */
		struct ppe_ds_wlan_rxfill_ring_info_wifi8 wifi8;	/**< WiFi8 RxFill ring info */
	};
};

#define ppe_ds_wlan_ops_v2 ppe_ds_wlan_ops

/**
 * ppe_ds_wlan_ops
 *	PPE-DS WLAN operations
 */
struct ppe_ds_wlan_ops {
	uint32_t (*get_tx_desc_many)(int ppeds_node_id, struct ppe_ds_wlan_txdesc_elem *,
			uint32_t num_buff_req, uint32_t buff_size, uint32_t headroom);
	/**< Callback to get WLAN Tx descriptors and buffers */
	void (*release_tx_desc_single)(int ppeds_node_id, uint32_t cookie);
	/**< Callback to release WLAN Tx descriptor and buffer */
	void (*set_tcl_prod_idx)(int ppeds_node_id, uint16_t tcl_prod_idx);
	/**< Callback to set PPE2TCL ring's producer index */
	void (*set_reo_cons_idx)(int ppeds_node_id, uint16_t reo_cons_idx);
	/**< Callback to set REO2PPE ring's consumer index */
	uint16_t (*get_tcl_cons_idx)(int ppeds_node_id);
	/**< Callback to get PPE2TCL ring's consumer index */
	uint16_t (*get_reo_prod_idx)(int ppeds_node_id);
	/**< Callback to get REO2PPE ring's producer index */
	void (*release_rx_desc)(int ppeds_node_id,
			struct ppe_ds_wlan_rxdesc_elem *arr, uint16_t count);
	/**< Callback to release WLAN Rx descriptors and buffers */
	void (*enable_tx_consume_intr)(int ppeds_node_id,
			bool enable);
	/**< Callback to toggle wlan interrupt */
	void (*notify_napi_done)(int ppeds_node_id);
	/**< Callback to trigger after ppeds ring process completes */
	uint8_t (*get_wlan_arch_mode)(void);
	/**< Returns whether WLAN is wifi7 or wifi8 */
};

/*
 * ds_inst_alloc_func_t
 * 	Callback for PPE_DS WLAN instance allocation
 *
 * @param[in] ppe_ds_wlan_ops PPE-DS WLAN operations
 * @param[in] priv_size sizeof dp_soc_be
 *
 * @return
 * PPE-DS node id
 *
 */
typedef int (*ds_inst_alloc_func_t)(struct ppe_ds_wlan_ops *ops, size_t priv_size);

/*
 * ds_inst_start_func_t
 * 	Callback for ppe_ds_wlan_instance_start
 *
 * @param[in] ppe_ds_wlan_ctx_info_handle PPE-DS wlan umac reset handle
 * @param[in] ppeds_node_id Index of PPE-DS node configuration
 *
 * @return
 * Status of PPE-DS instance registration operation
 *
 */
typedef int (*ds_inst_start_func_t)(struct ppe_ds_wlan_ctx_info_handle *wlan_info_hd, int ppeds_node_id);

/*
 * ds_inst_register_func_t
 * 	Callback for ppe_ds_wlan_inst_register
 *
 * @param[in] ppe_ds_wlan_reg_info PPE-DS WLAN rings information
 * @param[in] ppeds_node_id Index of PPE-DS node configuration
 *
 * @return
 * True/False
 *
 */
typedef bool (*ds_inst_register_func_t)(struct ppe_ds_wlan_reg_info *reg_info, int ppeds_node_id);

/*
 * ds_inst_register_func_wifi_arch_mode_t
 * 	Callback for ppe_ds_wlan_inst_register_arch_mode_wifi7/wifi8
 *
 * @param[in] ppe_ds_wlan_arch_reg_info PPE-DS WLAN rings information
 * @param[in] ppeds_node_id Index of PPE-DS node configuration
 *
 * @return
 * True/False
 *
 */
typedef bool (*ds_inst_register_func_wifi_arch_mode_t)(struct ppe_ds_wlan_arch_reg_info *reg_info, int ppeds_node_id);

/*
 * ds_inst_stop_func_t
 * 	Callback for ppe_ds_wlan_instance_stop
 *
 * @param[in] ppeds_node_id Index of PPE-DS node configuration
 *
 * @return
 * None
 *
 */
typedef void (*ds_inst_stop_func_t)(struct ppe_ds_wlan_ctx_info_handle *wlan_info_hd, int ppeds_node_id);

/*
 * ds_inst_free_func_t
 * 	Callback for ppe_ds_wlan_inst_free
 *
 * @param[in] ppe_ds_wlan_ctx_info_handle PPE-DS umac reset Information handle
 * @param[in] ppeds_node_id Index of PPE-DS node configuration
 *
 * @return
 * None
 *
 */
typedef void (*ds_inst_free_func_t)(int ppeds_node_id);

/*
 * ds_inst_get_ctx_func_t
 * 	Callback for ppe_ds_wlan_get_intr_ctxt
 *
 * @param[in] ppeds_node_id Index of PPE-DS node configuration
 *
 * @return
 * Context from ppe
 *
 */
typedef void* (*ds_inst_get_ctx_func_t)(int ppeds_node_id);

/*
 * ds_inst_ppe2tcl_intr_func_t
 * 	Callback for ppe_ds_ppe2tcl_wlan_handle_intr
 *
 * @param[in] ppeds_node_id Index of PPE-DS node configuration
 *
 * @return
 * 0
 *
 */
typedef int (*ds_inst_ppe2tcl_intr_func_t)(int ppeds_node_id);

/*
 * ds_inst_reo2ppe_intr_func_t
 * 	Callback for ppe_ds_reo2ppe_wlan_handle_intr
 *
 * @param[in] ppeds_node_id Index of PPE-DS node configuration
 *
 * @return
 * 0
 *
 */
typedef int (*ds_inst_reo2ppe_intr_func_t)(int ppeds_node_id);

/*
 * ds_inst_stats_update_func_t
 * 	Callback for stat update
 * @param[in] net_device ath dev
 * @param[in] ppe_wlan_plugin_stats Plugin stat
 *
 * @return
 * None
 */
typedef void (*ds_inst_stats_update_func_t)(struct net_device *ath_dev, struct ppe_wlan_plugin_stats *stats);

/*
 * get_vp_num_func_t
 * 	Callback for ppe_vp_get_vp
 *
 * @param[in] net_device ath dev
 *
 * @return
 * VP number for the netdev
 */
typedef int16_t (*get_vp_num_func_t)(struct net_device *ath_dev);

/*
 * service_status_update_func_t
 * 	Callback for ppe_ds_wlan_service_status_update
 *
 * @param[in] ppeds_node_id Index of PPE-DS node configuration
 * @param[in] enable enable/disable bit
 *
 * @return
 * None
 */
typedef void (*service_status_update_func_t)(int ppeds_node_id, bool enable);

/*
 * update_vp_config_func_t
 * 	Callback for ppe_vp_status_update
 *
 * @param[in] net_device ath dev
 * @param[in] ppe_vp_ui VP update info
 *
 * @return
 * ppe_vp_status_t
 */
typedef ppe_vp_status_t (*update_vp_config_func_t)(struct net_device *ath_dev, struct ppe_vp_ui *vpui);

/*
 * get_rxfill_ring_info_func_t
 * 	Callback for ppe_ds_wlan_get_rxfill_ring_info
 *
 * @param[in] ppeds_node_id Index of PPE-DS node configuration
 * @param[out] info RxFill ring filled-count info
 *
 * @return
 * None
 */
typedef void (*get_rxfill_ring_info_func_t)(int ppeds_node_id,
		struct ppe_ds_wlan_rxfill_ring_info *info);

/*
 * ds_node_freq_get_func_t
 *	Callback to get the radio channel frequency range for a PPE-DS node.
 *
 * @param[in] node_id    PPE-DS node index
 * @param[in] freq       Radio channel frequency range in MHz
 */
typedef void (*ds_node_freq_get_func_t)(int node_id, struct ppe_ds_wlan_node_freq *freq);

/**
 * nss_plugins_ops
 * 	Data structure for NSS Plugin ops
 */
struct nss_plugins_ops {
	ds_inst_alloc_func_t ds_inst_alloc;		/**< Callback for PPE-DS WLAN instance allocation API */
	ds_inst_start_func_t ds_inst_start;		/**< Callback for PPE-DS WLAN instance start API */
	ds_inst_register_func_t ds_inst_register;		/**< Callback for PPE-DS WLAN instance registration API */
	ds_inst_register_func_wifi_arch_mode_t ds_inst_register_wifi_arch_mode;		/**< Callback for PPE-DS WLAN instance registration API */
	ds_inst_stop_func_t ds_inst_stop;		/**< Callback for PPE-DS WLAN instance stop API */
	ds_inst_free_func_t ds_inst_free;		/**< Callback for PPE-DS WLAN instance free API */
	ds_inst_get_ctx_func_t ds_inst_get_ctx;		/**< Callback for ppe_ds_wlan_get_intr_ctxt API */
	ds_inst_ppe2tcl_intr_func_t ds_inst_ppe2tcl_intr;		/**< Callback for PPE-DS PPE2TCL IRQ Tx processing API */
	ds_inst_reo2ppe_intr_func_t ds_inst_reo2ppe_intr;		/**< Callback for PPE-DS REO2PPE IRQ Rx processing API */
	ds_inst_stats_update_func_t ds_inst_stats_update;		/**< Callback for stat update */
	get_vp_num_func_t get_vp_num;		/**< Callback to get vp number */
	service_status_update_func_t service_status_update;		/**< Callback to get status update */
	update_vp_config_func_t vp_cfg_update; 		/**< Callback to update VP config */
	get_rxfill_ring_info_func_t get_rxfill_ring_info;	/**< Callback to get RxFill ring filled-count info */
	ds_node_freq_get_func_t get_ds_node_freq;	/**< Callback to get WiFi SoC node radio channel frequency range */
};

/*
 * qca_nss_wifi_plugins_get_ops
 *	API to get the nss plugin operations
 *
 * @datatypes
 * nss_plugins_ops
 *
 * @return
 * nss_plugins_ops operation structure, else NULL
 */
struct nss_plugins_ops* qca_nss_wifi_plugins_get_ops(void);

#endif
