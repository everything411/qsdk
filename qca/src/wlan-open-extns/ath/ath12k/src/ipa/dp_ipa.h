/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_IPA_OFFLOAD_H
#define ATH12K_IPA_OFFLOAD_H
#ifndef CONFIG_IPA3
#define CONFIG_IPA3 1
#endif

#include "../../core.h"
#include "../../pci.h"
#include "../../debug.h"
#include "../../hif.h"
#include "../../dp_rx.h" //Require to allocate RX buffers and use RX alloc/free api
#include "../../dp_peer.h"
#include "../../wmi.h"

//TODO: Enable below inclusion with IPA driver
#include <linux/ipa.h>
#include <linux/ipa_wdi3.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>

struct frag_header {
	struct {
		struct {} dummy_struct;
		u8 reserved[];
	};
};

struct ipa_header {
	struct {
		struct {} dummy_struct;
		u8 reserved[];
	};
};

#define ath12k_ipa_info(__ab, __log, ...) ath12k_info(__ab, "%s[%d]: " __log, __func__, __LINE__, ##__VA_ARGS__)
#define ath12k_ipa_err(__ab, __log, ...) ath12k_err(__ab, "%s[%d]: " __log, __func__, __LINE__, ##__VA_ARGS__)
#define ath12k_ipa_dbg(__ab, __log, ...) ath12k_dbg(__ab, ATH12K_DBG_IPA, "%s[%d]: " __log, __func__, __LINE__, ##__VA_ARGS__)

#define IPA_CTX(ab) ab->ath12k_base_extn.ipa_ctx
#define ATH12K_IPA_RX_BUF_DMA_BITS		32

#define NL80211_BAND_UNSPECIFIED ((enum nl80211_band)-1)

#define ATH12K_IPA_TCL_SW_RING 0

#define MAX_IPA_IFACE                       16
#define ATH12K_IPA_MAX_SYSBAM_PIPE          4
#define WLAN_IPA_WLAN_HDR_DES_MAC_OFFSET    0
#define WLAN_IPA_UC_WLAN_HDR_DES_MAC_OFFSET \
	(sizeof(struct frag_header) + sizeof(struct ipa_header))

#define ATH12K_IPA_MAX_SESSION              MAX_IPA_IFACE

#define ATH12K_IPA_RX_PIPE                  (ATH12K_IPA_MAX_SYSBAM_PIPE - 1)
#define ATH12K_IPA_ENABLE_MASK              BIT(0)
#define ATH12K_IPA_PRE_FILTER_ENABLE_MASK   BIT(1)
#define ATH12K_IPA_IPV6_ENABLE_MASK         BIT(2)
#define ATH12K_IPA_RM_ENABLE_MASK           BIT(3)
#define ATH12K_IPA_CLK_SCALING_ENABLE_MASK  BIT(4)
#define ATH12K_IPA_UC_ENABLE_MASK           BIT(5)
#define ATH12K_IPA_UC_STA_ENABLE_MASK       BIT(6)
#define ATH12K_IPA_REAL_TIME_DEBUGGING      BIT(8)
/* With CONFIG_IPA_WDI3_TX_TWO_PIPES=y, this bitmask is added to support
 * runtime IPA two tx pipes feature enablement.
 */
#define ATH12K_IPA_TWO_TX_PIPES_ENABLE_MASK     BIT(9)
#define ATH12K_IPA_SET_PORT_IN_CCE_CONFIG_MASK  BIT(10)
#define ATH12K_IPA_LOW_POWER_MODE_ENABLE_MASK   BIT(11)

#define ATH12K_IPA_MAX_BANDWIDTH              4800
#define ATH12K_IPA_MAX_BANDWIDTH_2G           1400

#define ATH12K_IPA_UC_BW_MONITOR_LEVEL        3

#define ATH12K_IPA_HDL_INVALID		0xFF
#define ATH12K_IPA_HDL_FIRST		0x0
#define ATH12K_IPA_HDL_SECOND		0x1
#define ATH12K_IPA_HDL_THIRD		0x2

#define L3_HEADER_PADDING		2

/* IPA RING defines */
#define ATH12K_IPA_TCL_RING				3
#define ATH12K_IPA_TX_COMP_RING				ATH12K_IPA_TCL_RING
#define ATH12K_IPA_REO_DST_RING				3
#define ATH12K_IPA_RX_REFILL_RING			ATH12K_IPA_REO_DST_RING

/* IPA buffer defines */
#define ATH12K_IPA_TX_BUF_CNT				8192
#define ATH12K_IPA_RX_REFILL_BUF_RING			8192
#define ATH12K_IPA_UC_TX_BUF_SIZE_DEFAULT		2048
#define ATH12K_IPA_WAR_WBM2SW_REL_RING_NO_BUF_ENTRIES	17
#define ATH12K_IPA_TX_BUFFER_ALIGN_SIZE			256
#define ATH12K_IPA_MAC_ADDR_SIZE			6
#define ATH12K_IPA_SESSION_ID_SHIFT			1
#define MAX_IPA_RX_FREE_DESC				64
#define ATH12K_IPA_WDI3					3
#define ATH12K_IPA_WDI_VER_INVALID			0xFF

#define ATH12K_IPA_IS_CONFIG_ENABLED(ipa_cfg, mask) \
	(((ipa_cfg)->ipa_config & (mask)) == (mask))

#define WLAN_IPA_AST_META_DATA_MASK htonl(0x000000FF)
#define DP_IPA_UC_WLAN_HDR_DES_MAC_OFFSET	0

#define ATH12K_IPA_DMA_MAP_SINGLE(ab, defrag_skb, buf_paddr) do { \
	buf_paddr = dma_map_single((ab)->dev, \
				   (defrag_skb)->data, \
				   DP_RX_BUFFER_SIZE, \
				   DMA_FROM_DEVICE); \
	ATH12K_SKB_CB(defrag_skb)->paddr = buf_paddr; \
} while (0)

#define ATH12K_CORE_DMAC_INV_RANGE(...) ((void)0)
#define VIRT_TO_PHYS(...) ((void)0)

#define ATH12K_DMA_UNMAP_DEFRAG_SKB(ab, skb) \
	ath12k_core_dma_unmap_single((ab)->dev, \
				     ATH12K_SKB_CB(skb)->paddr, \
				     DP_RX_BUFFER_SIZE, \
				     DMA_FROM_DEVICE) \

#define ATH12K_DMA_UNMAP_WITH_FREE_SKB(ab, skb) \
	do { \
		ATH12K_DMA_UNMAP_DEFRAG_SKB(ab, skb); \
		dev_kfree_skb_any(skb); \
	} while (0)

#define IPA_SET_RX_BUF_SMMU_MAP(ab, skb) \
	do { \
		ath12k_dp_ipa_handle_buf_smmu_map_unmap(ab, skb, DP_RX_BUFFER_SIZE, 1, \
				IPA_CTX(ab)->hdl); \
	} while (0)

#define IPA_SET_RX_BUF_SMMU_UNMAP(ab, skb, unmap) \
	do { \
		ath12k_dp_ipa_handle_buf_smmu_map_unmap(ab, skb, DP_RX_BUFFER_SIZE, 0, \
				IPA_CTX(ab)->hdl); \
		if (unmap) \
			ATH12K_DMA_UNMAP_DEFRAG_SKB(ab, skb); \
	} while (0)

#define ATH12K_HAL_SRNG_ACCESS_BEGIN(ab, tcl_ring) \
	ath12k_hal_srng_access_begin(ab, tcl_ring);

#define ATH12K_HAL_SRNG_ACCESS_END(ab, tcl_ring) \
	ath12k_hal_srng_access_end(ab, tcl_ring);

#define IPA_TX_BUFFER_FREE(ab) \
	ath12k_dp_ipa_uc_detach(ab);

#define IPA_TX_BUFFER_ALLOC(ab) \
	dp_ipa_uc_attach(ab);

#define RETURN_IPA_CODE(val) return val

#define ATH12K_DP_RXDMA_RING_CONFIG(ring_id, dp) \
	ring_id = dp->rx_mac_buf_ring[0].ring_id;

#define ATH12K_SKB_COPY_PADDR(skb, ti) \
	ATH12K_SKB_CB(skb)->paddr = ti.paddr;

# define ATH12K_SET_IPA_TCL_RING(ti) \
	if (ti.ring_id == ATH12K_IPA_TCL_RING) \
		ti.ring_id = ATH12K_IPA_TCL_SW_RING;

#define ATH12K_CORE_DMA_UNMAP_SINGLE(...) ((void)0)

int ath12k_dp_ipa_plugin_register_ops_extn(struct ath12k_base *ab);
void ath12k_dp_ipa_plugin_deregister_ops_extn(struct ath12k_base *ab);

int ath12k_dp_ipa_pci_smmu_fault_handler(struct iommu_domain *domain,
					 struct device *dev, unsigned long iova,
					 int flags, void *handler_token);
int ath12k_pci_init_smmu_extn(struct ath12k_pci *pci_priv);
void ath12k_pci_deinit_smmu_extn(struct ath12k_pci *pci_priv);
struct iommu_domain *ath12k_dp_ipa_smmu_get_domain(struct ath12k_base *ab);
int ath12k_dp_ipa_smmu_map(struct ath12k_base *ab, phys_addr_t paddr,
			   u32 *iova_addr, size_t size);
int ath12k_dp_ipa_smmu_unmap(struct ath12k_base *ab, u32 iova_addr, size_t size);

/**
 * enum wlan_ipa_uc_op_code - IPA UC operation message
 *
 * @WLAN_IPA_UC_OPCODE_UC_READY: IPA UC ready indication
 * @WLAN_IPA_SMMU_MAP: IPA SMMU map call
 * @WLAN_IPA_SMMU_UNMAP: IPA SMMU unmap call
 * @WLAN_IPA_UC_OPCODE_MAX: IPA UC max operation code
 */
enum ath12k_ipa_uc_op_code {
	ATH12K_IPA_UC_OPCODE_UC_READY = 0,
//	WLAN_IPA_SMMU_MAP = 1,
//	WLAN_IPA_SMMU_UNMAP = 2,
	/* keep this last */
	ATH12K_IPA_UC_OPCODE_MAX
};

/**
 * enum wlan_ipa_init_state: ipa init state
 * @WLAN_IPA_STATE_DEINIT: ipa deinit inprogress
 * @WLAN_IPA_STATE_INIT: ipa init inprogress
 * @WLAN_IPA_STATE_SETUP_DONE: ipa pipe setup done
 * @WLAN_IPA_STATE_PIPE_CONNECTION_DONE: ipa pipe connection done
 * @WLAN_IPA_STATE_PIPE_ENABLED: ipa pipe enabled
 */
enum ath12k_ipa_init_state {
	ATH12K_IPA_STATE_DEINIT = 0,
	ATH12K_IPA_STATE_INIT = 1,
	ATH12K_IPA_STATE_SETUP_DONE = 2,
	ATH12K_IPA_STATE_PIPE_CONNECTION_DONE = 3,
	ATH12K_IPA_STATE_PIPE_ENABLED = 4
};


enum ath12k_ipa_opmode {
	ATH12K_IPA_STA,
	ATH12K_IPA_SAP,
	ATH12K_IPA_MAX_NO_OF_MODE,
};

/**
 * enum wlan_ipa_wlan_event - WLAN IPA events
 * @WLAN_IPA_CLIENT_CONNECT: Client Connects
 * @WLAN_IPA_CLIENT_DISCONNECT: Client Disconnects
 * @WLAN_IPA_AP_CONNECT: SoftAP is started
 * @WLAN_IPA_AP_DISCONNECT: SoftAP is stopped
 * @WLAN_IPA_STA_CONNECT: STA associates to AP
 * @WLAN_IPA_STA_DISCONNECT: STA dissociates from AP
 * @WLAN_IPA_CLIENT_CONNECT_EX: Peer associates/re-associates to softap
 * @WLAN_IPA_MLO_CLIENT_CONNECT_EX: MLO Peer Connect
 * @WLAN_IPA_MLO_CLIENT_DISCONNECT: MLO Peer Disconnect
 * @WLAN_IPA_WLAN_EVENT_MAX: Max value for the enum
 */
enum ath12k_ipa_wlan_event {
	ATH12K_IPA_CLIENT_CONNECT,
	ATH12K_IPA_CLIENT_DISCONNECT,
	ATH12K_IPA_AP_CONNECT,
	ATH12K_IPA_AP_DISCONNECT,
	ATH12K_IPA_STA_CONNECT,
	ATH12K_IPA_STA_DISCONNECT,
	ATH12K_IPA_CLIENT_CONNECT_EX,
	ATH12K_IPA_MLO_CLIENT_CONNECT_EX,
	ATH12K_IPA_MLO_CLIENT_DISCONNECT,
	ATH12K_IPA_WLAN_EVENT_MAX
};

/**
 * enum wlan_ipa_bw_level - IPA bandwidth level
 * @WLAN_IPA_BW_LEVEL_LOW: vote for low bandwidth
 * @WLAN_IPA_BW_LEVEL_MEDIUM: vote for medium bandwidth
 * @WLAN_IPA_BW_LEVEL_HIGH: vote for high bandwidth
 * @WLAN_IPA_BW_LEVEL_MAX: Max value for the enum
 */
enum ath12k_ipa_bw_level {
	ATH12K_IPA_BW_LEVEL_LOW,
	ATH12K_IPA_BW_LEVEL_MEDIUM,
	ATH12K_IPA_BW_LEVEL_HIGH,
	ATH12K_IPA_BW_LEVEL_MAX
};

enum iommu_attr {
	ATH12K_IOMMU_MAPPING_CONF_S1_BYPASS,
};

struct dp_ipa_net_device_priv {
	char name[IFNAMSIZ];
	u8 dev_addr[ETH_ALEN];
	int ifindex;
};

/**
 * struct ath12k_ipa_arch_ops - Generation-specific IPA operation callbacks.
 *
 * @dp_srng_cleanup:          Clean up IPA-specific datapath rings.
 * @dp_srng_setup:            Set up IPA-specific datapath rings.
 * @dp_srng_get_rsrc:         Populate TX/RX ring resource descriptors for IPA.
 * @dp_rx_default_routing:    Return the REO destination ring index for IPA RX.
 * @dp_rxdma_buf_setup:       Set up the RXDMA buffer ring for IPA.
 * @dp_tx_set_bank_config:    Configure the TCL bank register for IPA TX.
 * @dp_rx_buf_smmu_map_unmap: Map or unmap RX buffers in the SMMU for IPA.
 * @dp_rx_pipe_set_rdi:       Set the RX pipe RDI (ring doorbell info) for IPA.
 * @ipa_get_rbm_id:           Return the Return Buffer Manager ID for IPA TX.
 * @ipa_get_ring_id:          Return the hardware ring ID for a given ring type.
 * @dp_srng_rx_init:          Initialize the IPA-specific RX ring (WiFi8 only).
 * @ipa_set_rx_doorbell_paddr: Program the RX ready doorbell physical address.
 * @ipa_set_tx_doorbell_paddr: Program the TX completion doorbell physical address.
 * @ipa_tx_comp_ring_init_hp: Initialize the TX completion ring head pointer.
 * @ipa_get_wdi_version:      Return the WDI protocol version (e.g. IPA_WDI_4).
 * @ipa_iface_setup_set_vp_num: Set the VP (virtual port) number in the IPA
 *                             interface registration parameters (WiFi8 only).
 * @ipa_iface_cleanup_vp_num: Release the VP number on interface teardown (WiFi8).
 * @ipa_iface_setup_set_intf_idx: Set the interface index in the IPA registration
 *                             parameters (WiFi8 only).
 * @ipa_client_connect_ex:    Handle a WLAN_CLIENT_CONNECT_EX event; called when
 *                             a peer associates to a SAP interface.
 * @ipa_client_disconnect:    Handle a WLAN_CLIENT_DISCONNECT event; called when
 *                             a peer disassociates from a SAP interface.
 * @ipa_iface_context_init:   Allocate and initialize the iface_context array and
 *                             all session-indexed arrays (iface_context, vdev_to_iface,
 *                             vdev_offload_enabled, disable_intrabss_fwd).
 *                             Must also set max_iface, max_session_id,
 *                             invalid_session_id, and wdi_version.
 *                             WiFi7: max_iface=16, max_session_id=16, invalid=16.
 *                             WiFi8: max_iface=48, max_session_id=256, invalid=0.
 *                             Returns 0 on success, negative errno on failure.
 * @ipa_iface_context_deinit: Free all arrays allocated by @ipa_iface_context_init:
 *                             iface_context, vdev_to_iface, vdev_offload_enabled,
 *                             and disable_intrabss_fwd.
 * @ipa_get_session_id:       Return the session identifier for a link VIF.
 *                             WiFi7: returns arvif->vdev_id (per-SoC, 0..6).
 *                             WiFi8: returns arvif->ahvif->dp_vif.ahvif_id
 *                             (group-wide unique, 1..63; 0 = invalid sentinel).
 *                             Called from ath12k_ipa_enqueue_evt().
 */
struct ath12k_ipa_arch_ops {
	int (*dp_srng_cleanup)(struct ath12k_base *ab);
	int (*dp_srng_setup)(struct ath12k_base *ab);
	void (*dp_srng_get_rsrc)(struct ath12k_base *ab);
	u32 (*dp_rx_default_routing)(u32 reo_dest, struct ath12k_base *ab);
	int (*dp_rxdma_buf_setup)(struct ath12k_base *ab);
	int (*dp_get_tx_bank_id)(struct ath12k_base *ab, u8 *bank_id);
	int (*dp_rx_buf_smmu_map_unmap)(struct ath12k_base *ab, bool create);
	int (*dp_srng_rx_init)(struct ath12k_base *ab);
	void (*ipa_set_rx_doorbell_paddr)(struct ath12k_ipa *ipa_ctx);
	void (*ipa_set_tx_doorbell_paddr)(struct ath12k_ipa *ipa_ctx);
	void (*ipa_tx_comp_ring_init_hp)(struct ath12k_ipa *ipa_ctx);
	int (*ipa_tx_buffer_attach)(struct ath12k_base *ab);
	void (*dp_rx_pipe_set_rdi)(struct ath12k_ipa *ipa_ctx,
				   struct ipa_wdi_pipe_setup_info_smmu *rx_smmu);
	u8 (*ipa_get_wdi_version)(void);
	void (*ipa_iface_setup_set_metadata)(struct ipa_wdi_reg_intf_in_params *in);
	void (*ipa_iface_setup_set_vp_num)(u8 link_id,
					   struct ipa_wdi_reg_intf_in_params *in);
	void (*ipa_iface_cleanup_vp_num)(const char *dev_name, u8 link_id);
	void (*ipa_iface_setup_set_intf_idx)(struct ipa_wdi_reg_intf_in_params *in,
					     int intf_idx);
	int (*ipa_client_connect_ex)(struct ath12k_ipa *ipa_ctx,
				     enum ipa_wlan_event event,
				     u32 vdev_id,
				     const u8 mac[ETH_ALEN],
				     struct dp_ipa_net_device_priv *dev,
				     bool is_mlo);
	int (*ipa_client_disconnect)(struct ath12k_ipa *ipa_ctx,
				     enum ipa_wlan_event event,
				     u32 vdev_id,
				     const u8 mac[ETH_ALEN],
				     struct dp_ipa_net_device_priv *dev,
				     bool is_mlo);
	int (*ipa_iface_context_init)(struct ath12k_ipa *ipa_ctx);
	void (*ipa_iface_context_deinit)(struct ath12k_ipa *ipa_ctx);
	u16 (*ipa_get_session_id)(struct ath12k_link_vif *arvif);
	void (*ipa_initialize_default)(struct ath12k_ipa *ipa_ctx, u16 i);
	void (*ipa_uc_offload_enable)(struct ath12k_ipa *ipa_ctx,
					       u32 offload_type, u32 vdev_id,
					       bool enable);
	void (*dp_ipa_w2i)(void *priv,
			   enum ipa_dp_evt_type evt,
			   unsigned long data);
};

struct ath12k_ipa_mac_addr {
	u8 bytes[ATH12K_IPA_MAC_ADDR_SIZE];
};

/**
 * struct ipa_uc_offload_control_params - ipa offload control params
 * @offload_type: ipa offload type
 * @vdev_id: vdev id
 * @enable: ipa offload enable/disable
 */
struct ipa_uc_offload_control_params {
	u32 offload_type;
	u32 vdev_id;
	u32 enable;
};

/**
 * struct ipa_intrabss_control_params - ipa intrabss control params
 * @vdev_id: vdev id
 * @enable: ipa intrabss enable/disable
 */
struct ipa_intrabss_control_params {
	u32 vdev_id;
	u32 enable;
};

/*
 * struct shared_mem - Shared memory resource
 * @mem_info: memory info struct
 * @vaddr: virtual address
 * @sgtable: scatter-gather table
 * @unsigned long memctx: dma address
 *
 */
typedef struct shared_mem {
	struct ipa_wdi_buffer_info mem_info;
	void *vaddr;
	struct sg_table sgtable;
	unsigned long memctx;
} shared_mem_t;

/* IPA uC datapath offload Wlan Tx resources */
struct ath12k_ipa_dp_tx_rsc {
	/* Resource info to be passed to IPA */
	dma_addr_t ipa_tcl_ring_base_paddr;
	void *ipa_tcl_ring_base_vaddr;
	u32 ipa_tcl_ring_size;
	dma_addr_t ipa_tcl_hp_paddr;
	u32 alloc_tx_buf_cnt;
	struct sg_table sgtable_tcl;
	phys_addr_t tcl_pa;

	dma_addr_t ipa_wbm_ring_base_paddr;
	void *ipa_wbm_ring_base_vaddr;
	u32 ipa_wbm_ring_size;
	dma_addr_t ipa_wbm_tp_paddr;
	/* WBM2SW HP shadow paddr */
	dma_addr_t ipa_wbm_hp_shadow_paddr;
	struct sg_table sgtable_wbm;
	phys_addr_t wbm_pa;

	/* TX buffers populated into the WBM ring */
	void **tx_buf_pool_vaddr_unaligned;
	unsigned long **tx_buf_pool_paddr_unaligned;

	/* IPA UC doorbell registers paddr */
	unsigned long tx_comp_doorbell_paddr;
	u32 *tx_comp_doorbell_vaddr;
};

/* IPA uC datapath offload Wlan Rx resources */
struct ath12k_ipa_dp_rx_rsc {
	/* Resource info to be passed to IPA */
	dma_addr_t ipa_reo_ring_base_paddr;
	void *ipa_reo_ring_base_vaddr;
	u32 ipa_reo_ring_size;
	dma_addr_t ipa_reo_tp_paddr;
	struct sg_table sgtable_reo;
	phys_addr_t reo_pa;

	/* Resource info to be passed to firmware and IPA */
	dma_addr_t ipa_rx_refill_buf_ring_base_paddr;
	void *ipa_rx_refill_buf_ring_base_vaddr;
	u32 ipa_rx_refill_buf_ring_size;
	dma_addr_t ipa_rx_refill_buf_hp_paddr;
	//struct sg_table sgtable_refill;
	shared_mem_t sgtable_refill;
	phys_addr_t refill_pa;

	/* RX buffers populated into the RXDMA BUF ring */
	void **rx_buf_pool_vaddr_unaligned;
	u32 alloc_rx_buf_cnt;

	/* IPA UC doorbell registers paddr */
	unsigned long rx_ready_doorbell_paddr;
};

/**
 * struct wlan_ipa_sys_pipe - IPA system pipe
 * @conn_hdl: IPA system pipe connection handle
 * @conn_hdl_valid: IPA system pipe valid flag
 * @ipa_sys_params: IPA system pipe params
 */
struct ath12k_ipa_sys_pipe {
	u32 conn_hdl;
	u8 conn_hdl_valid;
	struct ipa_sys_connect_params ipa_sys_params;
};

/**
 * struct wlan_ipa_iface_stats - IPA system pipe
 * @num_tx: Number of TX packets
 * @num_tx_drop: Number of TX packet drops
 * @num_tx_err: Number of TX packet errors
 * @num_tx_cac_drop: Number of TX packet drop due to CAC
 * @num_rx_ipa_excep: Number of RX IPA exception packets
 */
struct ath12k_ipa_iface_stats {
	u64 num_tx;
	u64 num_tx_drop;
	u64 num_tx_err;
	u64 num_tx_cac_drop;
	u64 num_rx_ipa_excep;
};

/**
 * struct ath12k_ipa_stats - IPA system stats
 * @event: WLAN IPA event record
 * @send_msg: Number of sent IPA messages
 * @failed_msg: Number of failed IPA messages
 * @cons_perf_req: Number of CONS pipe perf request
 * @prod_perf_req: Number of PROD pipe perf request
 * @rx_drop_excep: Number of RX packet drops
 * @tx_desc_q_cnt: Number of TX descriptor queue count
 * @tx_desc_error: Number of TX descriptor error
 * @rx_excep: Number of RX IPA exception packets
 * @rx_no_iface: No of pkts before iface setup
 * @sent_to_mac: Number of TX forward to MAC layer
 * @rx_eapol: Number of RX EAPOL packets
 */
struct ath12k_ipa_stats {
	u32 event[ATH12K_IPA_WLAN_EVENT_MAX];
	u64 send_msg;
	u64 failed_msg;
	u64 cons_perf_req;
	u64 prod_perf_req;
	u64 rx_drop_excep;
	u64 tx_desc_q_cnt;
	u64 tx_desc_error;
	u64 rx_excep;
	u64 rx_no_iface;
	u64 sent_to_mac;
	u64 rx_eapol;
};

/**
 * struct ipa_uc_fw_stats - IPA FW stats
 * @tx_comp_ring_size: TX completion ring size
 * @txcomp_ring_dbell_addr: TX comp ring door bell address
 * @txcomp_ring_dbell_ind_val: TX cop ring door bell indication
 * @txcomp_ring_dbell_cached_val: TX cop ring cached value
 * @txpkts_enqueued: TX packets enqueued
 * @txpkts_completed: TX packets completed
 * @tx_is_suspend: TX suspend flag
 * @tx_reserved: Reserved for TX stat
 * @rx_ind_ring_base: RX indication ring base addess
 * @rx_ind_ring_size: RX indication ring size
 * @rx_ind_ring_dbell_addr: RX indication ring doorbell address
 * @rx_ind_ring_dbell_ind_val: RX indication ring doorbell indication
 * @rx_ind_ring_dbell_ind_cached_val: RX indication ring doorbell cached value
 * @rx_ind_ring_rdidx_addr: RX indication ring read index address
 * @rx_ind_ring_rd_idx_cached_val: RX indication ring read index cached value
 * @rx_refill_idx: RX ring refill index
 * @rx_num_pkts_indicated: Number of RX packets indicated
 * @rx_buf_refilled: Number of RX buffer refilled
 * @rx_num_ind_drop_no_space: Number of RX indication drops due to no space
 * @rx_num_ind_drop_no_buf: Number of RX indication drops due to no buffer
 * @rx_is_suspend: RX suspend flag
 * @rx_reserved: Reserved for RX stat
 */
struct ath12k_ipa_uc_fw_stats {
	u32 tx_comp_ring_base;
	u32 tx_comp_ring_size;
	u32 tx_comp_ring_dbell_addr;
	u32 tx_comp_ring_dbell_ind_val;
	u32 tx_comp_ring_dbell_cached_val;
	u32 tx_pkts_enqueued;
	u32 tx_pkts_completed;
	u32 tx_is_suspend;
	u32 tx_reserved;
	u32 rx_ind_ring_base;
	u32 rx_ind_ring_size;
	u32 rx_ind_ring_dbell_addr;
	u32 rx_ind_ring_dbell_ind_val;
	u32 rx_ind_ring_dbell_ind_cached_val;
	u32 rx_ind_ring_rdidx_addr;
	u32 rx_ind_ring_rd_idx_cached_val;
	u32 rx_refill_idx;
	u32 rx_num_pkts_indicated;
	u32 rx_buf_refilled;
	u32 rx_num_ind_drop_no_space;
	u32 rx_num_ind_drop_no_buf;
	u32 rx_is_suspend;
	u32 rx_resered;
};

struct ipa_wifi_ops {
	bool (*tx)(struct ath12k_base *ab, struct sk_buff *skb, bool is_mcbc);
};

struct ath12k_ipa_iface_context {
	struct ath12k_ipa *ipa_ctx;

	enum ipa_client_type cons_client;
	enum ipa_client_type prod_client;

	u8 iface_id;       /* This iface ID */
	struct net_device *dev;
	enum ath12k_ipa_opmode device_mode;
	u8 mac_addr[ATH12K_IPA_MAC_ADDR_SIZE];
	atomic_t conn_count;
	atomic_t disconn_count;
	u16 session_id;
	spinlock_t interface_lock;
	u32 ifa_address;
	struct ath12k_ipa_iface_stats stats;
	struct ath12k_ipa_mac_addr bssid;
	u8 is_authenticated;
	bool alt_pipe;
	struct dp_ipa_net_device_priv net_dev;
	bool is_mlo_vdev;
};

/**
 * struct op_msg_type - IPA operation message type
 * @msg_t: Message type
 * @rsvd: Reserved
 * @op_code: IPA Operation type
 * @len: IPA message length
 * @rsvd_snd: Reserved
 * @vdev_id: vdev id
 * @nbuf: tx nbuf
 */
struct op_msg_type {
	u8 msg_t;
	u8 rsvd;
	u16 op_code;
	u16 len;
	u16 rsvd_snd;
	u8 vdev_id;
	struct sk_buff *nbuf;
};

/**
 * struct msg_elem
 * @vdev_id: vdev id
 * @nbuf: nbuf
 * @op_code: IPA Operation type
 * @hdl: handle of filter deleted
 * @result: result of deletion
 */
struct msg_elem {
	u8 vdev_id;
	struct sk_buff *nbuf;
	u16 op_code;
	u32 hdl;
	u16 result;
};

/**
 * struct op_msg_list
 * @hp: hp of list
 * @tp: tp of list
 * @entries: list of messages
 * @list_size: max list size
 * @lock: spin lock for list
 */
struct op_msg_list {
	u16 hp;
	u16 tp;
	struct msg_elem *entries;
	u16 list_size;
	spinlock_t lock;
};

/**
 * struct uc_op_work_struct
 * @work: uC OP work
 * @msg: OP message
 * @dev: pointer to net device, used by osif_psoc_sync_trans_start_wait
 * @ipa_priv_bp: back pointer to ipa_obj
 * @msg_list: list of messages, to be used in case of parallel msgs
 * @flag: flag to be set when msg list is required
 */
struct uc_op_work_struct {
	struct work_struct work;
	struct op_msg_type *msg;
	struct device *dev;
	struct ath12k_ipa *ipa_priv_bp;
	struct op_msg_list *msg_list;
	u16 flag;
};

/**
 * struct dp_ipa_resources - Resources needed for IPA
 *
 * @tx_ring: Tx data ring info
 * @tx_num_alloc_buffer: Number of Tx buffer
 * @tx_comp_ring: Tx comp ring info
 * @rx_rdy_ring: Rx dst ring info
 * @rx_refill_ring: Rx refill ring info
 * @tx_comp_doorbell_paddr: Physical doorbell address for Tx compl ring
 * @tx_comp_doorbell_vaddr: Virtual doorbell address for Tx compl ring
 * @rx_ready_doorbell_paddr: Physical doorbell address for Rx dst ring
 * @is_db_ddr_mapped: Doorbell is DDR mapped
 * @tx_alt_ring: Tx Alt data ring info
 * @tx_alt_ring_num_alloc_buffer: Number of Tx buffer Alt ring
 * @tx_alt_comp_ring: Tx Alr comp ring info
 * @tx_alt_comp_doorbell_paddr: Physical doorbell address for Tx Alt compl ring
 * @tx_alt_comp_doorbell_vaddr: Virtual doorbell address for Tx Alt compl ring
 * @rx_alt_rdy_ring: Rx Alt dst ring info
 * @rx_alt_refill_ring: Rx Alt refill ring info
 * @rx_alt_ready_doorbell_paddr: Physical doorbell address for Rx Alt dst ring
 */
struct ath12k_ipa_resources {
	shared_mem_t tx_ring;
	u32 tx_num_alloc_buffer;

	shared_mem_t tx_comp_ring;
	shared_mem_t rx_rdy_ring;
	shared_mem_t rx_refill_ring;

	/* IPA UC doorbell registers paddr */
	unsigned long tx_comp_doorbell_paddr;
	u32 *tx_comp_doorbell_vaddr;
	unsigned long rx_ready_doorbell_paddr;

	bool is_db_ddr_mapped;

	shared_mem_t tx_alt_ring;
	u32 tx_alt_ring_num_alloc_buffer;
	shared_mem_t tx_alt_comp_ring;

	/* IPA UC doorbell registers paddr */
	unsigned long tx_alt_comp_doorbell_paddr;
	u32 *tx_alt_comp_doorbell_vaddr;
};

/**
 * struct wlan_ipa_config
 * @ipa_config: IPA config
 * @desc_size: IPA descriptor size
 * @txbuf_count: TX buffer count
 * @bus_bw_high: Bus bandwidth high threshold
 * @bus_bw_medium: Bus bandwidth medium threshold
 * @bus_bw_low: Bus bandwidth low threshold
 * @ipa_bw_high: IPA bandwidth high threshold
 * @ipa_bw_medium: IPA bandwidth medium threshold
 * @ipa_bw_low: IPA bandwidth low threshold
 * @ipa_force_voting: support force bw voting
 * @ipa_wds: WDS support for IPA
 * @ipa_vlan_support: support got vlan with IPA
 */
struct ath12k_ipa_config {
	u32 ipa_config;
	u32 desc_size;
	u32 txbuf_count;
	u32 bus_bw_high;
	u32 bus_bw_medium;
	u32 bus_bw_low;
	u32 ipa_bw_high;
	u32 ipa_bw_medium;
	u32 ipa_bw_low;
	bool ipa_force_voting;
	bool ipa_wds;
	bool ipa_vlan_support;
};


/**
 * struct ath12k_ipa - IPA offload context for a single ath12k device (WiFi7)
 *                     or the entire cumac group (WiFi8).
 * @ab:                    Back-pointer to the owning ath12k_base.
 * @ipa_uc_tx_rsc:         Primary TX ring resources shared with IPA.
 * @ipa_uc_rx_rsc:         Primary RX ring resources shared with IPA.
 * @ipa_uc_tx_alt_rsc:     Alternate TX ring resources (two-pipe mode).
 * @ipa_uc_rx_alt_rsc:     Alternate RX ring resources.
 * @stats:                 Aggregate IPA statistics counters.
 * @enable_disable_lock:   Spinlock protecting pipe enable/disable state.
 * @ipa_pipes_down:        True when IPA pipes are currently down.
 * @pipes_down_in_progress: Mutual exclusion flag for IPA disable-pipes path.
 * @pipes_enable_in_progress: Mutual exclusion flag for IPA enable-pipes path.
 * @pipes_disabled:        Atomic flag set when cdp_disable_ipa_pipes is called.
 * @ipa_pipes_enabled:     Atomic flag tracking pipe-enabled state.
 * @pending_tx_start_ticks: Timestamp (ticks) after which idle pipes are disabled.
 * @iface_context:         Dynamically allocated array of per-interface state.
 *                         NULL until arch_ops->ipa_iface_context_init() is called.
 *                         WiFi7: ATH12K_IPA_MAX_IFACE (7) entries.
 *                         WiFi8: ATH12K_IPA_MAX_IFACE_WIFI8 (21) entries (3 SoCs × 7).
 * @max_iface:             Capacity of @iface_context[]; set by ipa_iface_context_init.
 * @sys_pipe:              IPA system pipe array.
 * @num_iface:             Number of currently active interfaces.
 * @config:                Pointer to IPA configuration parameters.
 * @wdi_version:           WDI protocol version (IPA_WDI_4 for WiFi7,
 *                         IPA_WDI_6 for WiFi8).
 * @pending_event:         List of pending WLAN events awaiting processing.
 * @event_lock:            Mutex serializing WLAN event processing.
 * @ipa_lock:              Mutex protecting IPA UC loaded handler.
 * @rt_debug_lock:         Mutex for real-time debug operations.
 * @g_init_deinit_lock:    Mutex serializing init/deinit sequences.
 * @wdi_enabled:           True when WDI interface is enabled.
 * @autonomy_disabled:     Atomic flag set when cdp_ipa_disable_autonomy is called.
 * @ipa_resource_comp:     Completion used to synchronize resource loading.
 * @ipa_init_state:        Current IPA initialization state machine value.
 * @deinit_in_prog:        Atomic flag indicating deinit is in progress.
 * @num_sap_connected:     Number of SAP interfaces currently connected to IPA.
 * @sap_num_connected_sta: Number of non-MLO STA clients connected to SAP.
 * @sap_num_mlo_connected_sta: Number of MLO STA clients connected to SAP.
 * @sta_connected:         Number of STA interfaces currently connected to IPA.
 * @tx_pipe_handle:        TX pipe handle returned by IPA.
 * @rx_pipe_handle:        RX pipe handle returned by IPA.
 * @resource_loading:      True while IPA resources are being loaded.
 * @resource_unloading:    True while IPA resources are being unloaded.
 * @ipa_tx_packets_diff:   TX packet count delta for bandwidth monitoring.
 * @ipa_rx_packets_diff:   RX packet count delta for bandwidth monitoring.
 * @ipa_p_tx_packets:      Previous TX packet count snapshot.
 * @ipa_p_rx_packets:      Previous RX packet count snapshot.
 * @ipa_map_allowed:       Atomic flag permitting SMMU buffer mapping.
 * @uc_loaded:             True when IPA uC firmware is loaded and ready.
 * @over_gsi:              True when IPA is operating over GSI transport.
 * @is_smmu_enabled:       True when SMMU S1 stage is enabled.
 * @instance_id:           IPA instance identifier for this device.
 * @handle_initialized:    True after ipa_wdi_init_per_inst() succeeds.
 * @hdl:                   WDI handle returned by ipa_wdi_init_per_inst().
 * @curr_bw_level:         Current IPA bandwidth vote level.
 * @uc_op_work:            Work structs for deferred uC opcode processing.
 * @vdev_to_iface:         Maps session_id to @iface_context[] index.
 *                         Dynamically allocated with @max_session_id entries.
 *                         Initialized to @invalid_session_id ("no iface mapped").
 *                         WiFi7: 7 entries (session_id = vdev_id, 0..6).
 *                         WiFi8: 256 entries (session_id = ahvif_id, 0..255).
 * @vdev_offload_enabled:  Per-session IPA UC offload enable flag.
 *                         Dynamically allocated with @max_session_id entries.
 * @disable_intrabss_fwd:  Per-session intrabss forwarding disable flag.
 *                         Dynamically allocated with @max_session_id entries.
 * @max_session_id:        Size of @vdev_to_iface[], @vdev_offload_enabled[],
 *                         and @disable_intrabss_fwd[]; set by ipa_iface_context_init.
 *                         WiFi7: ATH12K_IPA_MAX_SESSION (7).
 *                         WiFi8: ATH12K_MAX_AHVIF_ID + 1 (256).
 * @invalid_session_id:    Sentinel stored in @iface_context[].session_id and
 *                         @vdev_to_iface[] to indicate a free/unassigned slot.
 *                         WiFi7: ATH12K_IPA_MAX_SESSION (7).
 *                         WiFi8: ATH12K_INVALID_AHVIF_ID (0).
 * @session_link_mask:     Session bit mask for MLO links.
 * @curr_prod_bw:          Current producer (RX) bandwidth vote.
 * @curr_cons_bw:          Current consumer (TX) bandwidth vote.
 * @is_db_ddr_mapped:      True when doorbell registers are DDR-mapped.
 * @ipa_first_tx_db_access: True before the first TX doorbell access after connect.
 * @arch_ops:              Pointer to generation-specific arch operations.
 */
struct ath12k_ipa {
	struct ath12k_base *ab;
	struct ath12k_ipa_dp_tx_rsc ipa_uc_tx_rsc;
	struct ath12k_ipa_dp_rx_rsc ipa_uc_rx_rsc;
	struct ath12k_ipa_dp_tx_rsc ipa_uc_tx_alt_rsc;
	struct ath12k_ipa_dp_rx_rsc ipa_uc_rx_alt_rsc;
	struct ath12k_ipa_stats stats;
	spinlock_t enable_disable_lock;
	bool ipa_pipes_down;
	bool pipes_down_in_progress;
	bool pipes_enable_in_progress;
	atomic_t pipes_disabled;
	atomic_t ipa_pipes_enabled;
	u64 pending_tx_start_ticks;
	struct ath12k_ipa_iface_context *iface_context;
	u8 max_iface;
	struct ath12k_ipa_sys_pipe sys_pipe[ATH12K_IPA_MAX_SYSBAM_PIPE];
	u8 num_iface;
	struct ath12k_ipa_config *config;
	u32 wdi_version;
	struct list_head pending_event;
	struct mutex event_lock;
	struct mutex ipa_lock;
	struct mutex rt_debug_lock;
	struct mutex g_init_deinit_lock;
	bool wdi_enabled;
	atomic_t autonomy_disabled;
	struct completion ipa_resource_comp;
	u8 ipa_init_state;
	atomic_t deinit_in_prog;
	u8 num_sap_connected;
	u16 sap_num_connected_sta;
	u16 sap_num_mlo_connected_sta;
	u8 sta_connected;
	u32 tx_pipe_handle;
	u32 rx_pipe_handle;
	bool resource_loading;
	bool resource_unloading;
	u32 ipa_tx_packets_diff;
	u32 ipa_rx_packets_diff;
	u32 ipa_p_tx_packets;
	u32 ipa_p_rx_packets;
	atomic_t ipa_map_allowed;
	bool uc_loaded;
	bool over_gsi;
	bool is_smmu_enabled;
	u8 instance_id;
	bool handle_initialized;
	ipa_wdi_hdl_t hdl;
	/* IPA offload TX capabilities reported by ipa_wdi_init_per_inst().
	 * offload_supported: true when IPA HW supports ULSO/segmentation offload
	 *   and ipa_wdi_offload_tx_dp_per_inst() may be used.
	 * offload_headroom:  bytes to reserve in dev->needed_headroom for the
	 *   QMAP extension header prepended by ipa_wdi_offload_tx_dp_per_inst().
	 * offload_features:  netdev HW feature flags to OR into hw_features.
	 * offload_gso_max_size: value to assign to netdev->gso_max_size.
	 * offload_max_pkts:  high watermark for in-flight offload TX packets;
	 *   stop the TX queue at this level, restart at offload_max_pkts/2.
	 */
	bool offload_supported;
	u32  offload_headroom;
	netdev_features_t offload_features;
	u32  offload_gso_max_size;
	u32  offload_max_pkts;
	atomic_t tx_outstanding;
	atomic_t queues_stopped; /* 1 when netdev TX queues are stopped by IPA */
	u8 curr_bw_level;
	struct uc_op_work_struct uc_op_work[ATH12K_IPA_UC_OPCODE_MAX];
	u8 *vdev_to_iface;
	bool *vdev_offload_enabled;
	bool *disable_intrabss_fwd;
	u16 max_session_id;
	u16 invalid_session_id;
	u8 *session_link_mask;
	u32 curr_prod_bw;
	u32 curr_cons_bw;
	bool is_db_ddr_mapped;
	bool ipa_first_tx_db_access;
	struct ath12k_ipa_arch_ops *arch_ops;
};

/**
 * struct dp_ipa_uc_tx_hdr - full tx header registered to IPA hardware
 * @eth:     ether II header
 */
struct ath12k_ipa_uc_tx_hdr {
	struct ethhdr eth;
} __packed;

/**
 * struct dp_ipa_uc_tx_vlan_hdr - full tx header registered to IPA hardware
 * @eth:     ether II header
 */
struct ath12k_ipa_uc_tx_vlan_hdr {
	struct vlan_ethhdr eth;
} __packed;

/**
 * struct dp_ipa_uc_rx_hdr - full rx header registered to IPA hardware
 * @eth:     ether II header
 */
struct ath12k_ipa_uc_rx_hdr {
	struct ethhdr eth;
} __packed;

#define ATH12K_IPA_UC_WLAN_TX_HDR_LEN      sizeof(struct ath12k_ipa_uc_tx_hdr)
#define ATH12K_IPA_UC_WLAN_TX_VLAN_HDR_LEN sizeof(struct ath12k_ipa_uc_tx_vlan_hdr)
#define ATH12K_IPA_UC_WLAN_RX_HDR_LEN      sizeof(struct ath12k_ipa_uc_rx_hdr)


int ath12k_dp_ipa_setup(struct ath12k_ipa *ipa_ctx);
int ath12k_dp_ipa_perf_init_perf_level(struct ath12k_ipa *ipa_ctx);
bool ath12k_dp_ipa_perf_set_perf_level_bw_enabled(struct ath12k_ipa *ipa_ctx);

static inline bool dp_ipa_enable(void)
{
	return true;
}

struct wlan_ipa_evt_wq_args {
	u8 mac_addr[ETH_ALEN];
	enum ipa_wlan_event event;
	u16 session_id;
	u8 pdev_idx;
	u8 device_id;
	u8 link_id;
	bool is_mlo;
	struct dp_ipa_net_device_priv dev;
	enum nl80211_iftype mode;
	enum nl80211_band band;
	struct list_head list_elem;
};

struct ath12k_ipa_event_entry {
	u8 mac_addr[ETH_ALEN];
	u32 vdev_id;
	u8 device_id;
	u8 pdev_idx;
	bool is_mlo;
	struct dp_ipa_net_device_priv dev;
	struct rhash_head node;
};

struct ath12k_dp_hw_group_ipa {
	struct work_struct work;
	struct ath12k_dp_hw_group *dp_hw_grp;

	spinlock_t list_lock; /* Protects access to the IPA event list */
	struct list_head list;

	struct rhashtable *mac_table;
	struct rhashtable_params mac_table_params;
};

enum {
	WMI_AP_RX_DATA_OFFLOAD             = 0x00,
	WMI_STA_RX_DATA_OFFLOAD            = 0x01,
};

void ath12k_wlan_ipa_evt_handler (struct work_struct *work);

int ath12k_ipa_wlan_evt(struct ath12k_ipa *ipa_ctx,
			enum ipa_wlan_event event,
			u32 vdev_id, u8 pdev_idx, const u8 mac[ETH_ALEN],
			struct dp_ipa_net_device_priv *dev, enum nl80211_iftype mode,
			enum nl80211_band band, bool is_mlo, u8 link_id);

int ath12k_wlan_ipa_send_msg(struct ath12k_ipa *ipa_ctx,
			     enum ipa_wlan_event event, const u8 mac[ETH_ALEN],
			     struct dp_ipa_net_device_priv *dev, bool mlo, u8 id);

int ath12k_wlan_ipa_send_msg_ex(struct ath12k_ipa *ipa_ctx,
				enum ipa_wlan_event event, const u8 mac[ETH_ALEN],
				struct dp_ipa_net_device_priv *dev, bool mlo, u8 id);

bool ath12k_ipa_get_is_tx1_used(struct ath12k_base *ab, u8 pdev_idx);

void ath12k_wlan_ipa_msg_free_fn(void *buff, u32 len, u32 type);

void ath12k_ipa_enqueue_evt(enum ipa_wlan_event event, struct ath12k_link_vif *arvif,
			    const u8 mac[ETH_ALEN], bool is_mlo);

bool ath12k_wlan_ipa_is_sta_only_offload_enabled(void);

bool ath12k_ipa_config_is_enabled(void);

int ath12k_ipa_send_intrabss_enable_disable(struct ath12k_base *ab,
					    struct ipa_intrabss_control_params *params);

int
ath12k_ipa_send_uc_offload_enable_disable(struct ath12k_base *ab,
					  struct ipa_uc_offload_control_params *params);

bool ath12k_wlan_ipa_is_enabled(struct ath12k_ipa_config *ipa_cfg);

bool ath12k_wlan_ipa_uc_is_enabled(struct ath12k_ipa_config *ipa_cfg);

bool ath12k_wlan_ipa_uc_sta_is_enabled(struct ath12k_ipa_config *ipa_cfg);

int ath12k_wlan_ipa_get_ifaceid(struct ath12k_ipa *ipa_ctx, u16 session_id);

struct ath12k_ipa_iface_context *
ath12k_wlan_ipa_get_iface(struct ath12k_ipa *ipa_ctx,
			  enum nl80211_iftype mode);

struct ath12k_ipa_iface_context *
ath12k_wlan_ipa_get_iface_by_mode_netdev(struct ath12k_ipa *ipa_ctx,
					 enum nl80211_iftype mode,
					 struct dp_ipa_net_device_priv *net_dev_priv,
					 u16 session_id);

enum ath12k_ipa_opmode ath12k_ipa_mode(enum nl80211_iftype mode);

void wlan_ipa_cleanup_iface(struct ath12k_ipa_iface_context *iface_context,
			    const u8 *mac_addr, ipa_wdi_hdl_t hdl, u16 session_id,
			    bool mld_enabled, u8 link_id);

int wlan_ipa_setup_iface(struct ath12k_ipa *ipa_ctx,
			 struct dp_ipa_net_device_priv *net_dev_priv,
			 enum ath12k_ipa_opmode device_mode, u16 session_id,
			 const u8 *mac_addr,
			 bool is_2g_iface, bool is_mlo_vdev,
			 bool is_tx1_used, u8 link_id);

int wlan_ipa_check_iface_netdev_sessid(struct ath12k_ipa_iface_context *iface_ctx,
				       struct dp_ipa_net_device_priv *net_dev_priv,
				       u16 session_id);

void ath12k_dp_ipa_peer_unmap_event_wds(struct ath12k_base *ab, u8 vdev_id, u16 peer_id,
					u8 *mac_addr);

void ath12k_dp_ipa_peer_map_event_wds(struct ath12k_base *ab, u8 vdev_id, u16 peer_id,
				      u8 *mac_addr);

void ath12k_debugfs_register_ipa_extn(struct ath12k *ar);

void ath12k_wmi_copy_resource_config_ipa(struct ath12k_base *ab,
					 struct ath12k_wmi_resource_config_params *wmi_cfg,
					 struct ath12k_wmi_resource_config_arg *tg_cfg);

int ath12k_dp_ipa_get_shared_mem_info(struct ath12k_base *ab,
				      shared_mem_t *shared_mem, void *cpu_addr,
				      dma_addr_t dma_addr, u32 size);
int ath12k_dp_ipa_handle_buf_smmu_map_unmap(struct ath12k_base *ab,
						   struct sk_buff *skb, u32 size,
						   bool create, ipa_wdi_hdl_t hdl);
int ath12k_dp_ipa_uc_detach(struct ath12k_base *ab);
int dp_ipa_uc_attach(struct ath12k_base *ab);
int ath12k_dp_ipa_register_is_ipa_ready(struct ath12k_base *ab);
int ath12k_dp_ipa_uc_ol_deinit(struct ath12k_base *ab);
int ath12k_dp_ipa_sg_tbl_setup(struct device *dev, struct sg_table *sgt,
			       void *cpu_addr, dma_addr_t dma_addr, size_t size);
void ath12k_dp_ipa_mac80211_rx(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
			       struct sk_buff *skb);

static inline void
ath12k_dp_ipa_arch_op_ipa_iface_setup_set_metadata(struct ath12k_ipa *ipa_ctx,
						   struct ipa_wdi_reg_intf_in_params *in)
{
	if (ipa_ctx && ipa_ctx->arch_ops &&
	    ipa_ctx->arch_ops->ipa_iface_setup_set_metadata)
		ipa_ctx->arch_ops->ipa_iface_setup_set_metadata(in);
}

static inline void
ath12k_dp_ipa_arch_op_ipa_iface_setup_set_vp_num(struct ath12k_ipa *ipa_ctx,
						 u8 link_id,
						 struct ipa_wdi_reg_intf_in_params *in)
{
	if (ipa_ctx && ipa_ctx->arch_ops &&
	    ipa_ctx->arch_ops->ipa_iface_setup_set_vp_num)
		ipa_ctx->arch_ops->ipa_iface_setup_set_vp_num(link_id, in);
}

static inline void
ath12k_dp_ipa_arch_op_ipa_iface_cleanup_vp_num(struct ath12k_ipa *ipa_ctx,
					       const char *dev_name,
					       u8 link_id)
{
	if (ipa_ctx && ipa_ctx->arch_ops &&
	    ipa_ctx->arch_ops->ipa_iface_cleanup_vp_num)
		ipa_ctx->arch_ops->ipa_iface_cleanup_vp_num(dev_name, link_id);
}

static inline void
ath12k_dp_ipa_arch_op_ipa_iface_setup_set_intf_idx(struct ath12k_ipa *ipa_ctx,
						   struct ipa_wdi_reg_intf_in_params *in,
						   int intf_idx)
{
	if (ipa_ctx && ipa_ctx->arch_ops &&
	    ipa_ctx->arch_ops->ipa_iface_setup_set_intf_idx)
		ipa_ctx->arch_ops->ipa_iface_setup_set_intf_idx(in, intf_idx);
}

static inline int
ath12k_dp_ipa_arch_op_ipa_client_connect_ex(struct ath12k_ipa *ipa_ctx,
					    enum ipa_wlan_event event,
					    u32 vdev_id,
					    const u8 mac[ETH_ALEN],
					    struct dp_ipa_net_device_priv *dev,
					    bool is_mlo)
{
	int ret = 0;

	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->ipa_client_connect_ex) {
		ret = ipa_ctx->arch_ops->ipa_client_connect_ex(ipa_ctx, event, vdev_id,
							       mac, dev, is_mlo);
	}

	return ret;
}

static inline int
ath12k_dp_ipa_arch_op_ipa_client_disconnect(struct ath12k_ipa *ipa_ctx,
					    enum ipa_wlan_event event,
					    u32 vdev_id,
					    const u8 mac[ETH_ALEN],
					    struct dp_ipa_net_device_priv *dev,
					    bool is_mlo)
{
	int ret = 0;

	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->ipa_client_disconnect) {
		ret = ipa_ctx->arch_ops->ipa_client_disconnect(ipa_ctx, event, vdev_id,
							       mac, dev, is_mlo);
	}

	return ret;
}

static inline int ath12k_dp_ipa_arch_op_srng_setup(struct ath12k_base *ab)
{
	int ret = 0;
	struct ath12k_ipa *ipa_ctx = IPA_CTX(ab);

	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->dp_srng_setup) {
		ret = ipa_ctx->arch_ops->dp_srng_setup(ab);
	}

	return ret;
}

static inline int ath12k_dp_ipa_arch_op_srng_cleanup(struct ath12k_base *ab)
{
	int ret = 0;
	struct ath12k_ipa *ipa_ctx = IPA_CTX(ab);

	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->dp_srng_cleanup) {
		ret = ipa_ctx->arch_ops->dp_srng_cleanup(ab);
	}

	return ret;
}

static inline void ath12k_dp_ipa_arch_op_get_srng_rsrc(struct ath12k_base *ab)
{
	struct ath12k_ipa *ipa_ctx = IPA_CTX(ab);

	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->dp_srng_get_rsrc)
		ipa_ctx->arch_ops->dp_srng_get_rsrc(ab);
}

static inline int
ath12k_dp_ipa_arch_op_rxdma_buf_setup(struct ath12k_base *ab)
{
	struct ath12k_ipa *ipa_ctx = IPA_CTX(ab);

	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->dp_rxdma_buf_setup)
		return ipa_ctx->arch_ops->dp_rxdma_buf_setup(ab);

	return 0;
}

static inline int
ath12k_dp_ipa_arch_op_get_tx_bank_id(struct ath12k_base *ab, u8 *bank_id)
{
	struct ath12k_ipa *ipa_ctx = IPA_CTX(ab);

	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->dp_get_tx_bank_id)
		return ipa_ctx->arch_ops->dp_get_tx_bank_id(ab, bank_id);

	return -EINVAL;
}

static inline int
ath12k_dp_ipa_arch_op_rx_buf_smmu_map_unmap(struct ath12k_base *ab, bool create)
{
	struct ath12k_ipa *ipa_ctx = IPA_CTX(ab);

	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->dp_rx_buf_smmu_map_unmap)
		return ipa_ctx->arch_ops->dp_rx_buf_smmu_map_unmap(ab, create);

	return 0;
}

static inline void
ath12k_dp_ipa_arch_op_set_tx_doorbell_paddr(struct ath12k_ipa *ipa_ctx)
{
	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->ipa_set_tx_doorbell_paddr)
		ipa_ctx->arch_ops->ipa_set_tx_doorbell_paddr(ipa_ctx);
}

static inline void
ath12k_dp_ipa_arch_op_set_rx_doorbell_paddr(struct ath12k_ipa *ipa_ctx)
{
	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->ipa_set_rx_doorbell_paddr)
		ipa_ctx->arch_ops->ipa_set_rx_doorbell_paddr(ipa_ctx);
}

static inline void
ath12k_dp_ipa_arch_op_tx_comp_ring_init_hp(struct ath12k_ipa *ipa_ctx)
{
	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->ipa_tx_comp_ring_init_hp)
		ipa_ctx->arch_ops->ipa_tx_comp_ring_init_hp(ipa_ctx);
}

static inline int
ath12k_dp_ipa_arch_op_tx_buffer_attach(struct ath12k_base *ab)
{
	struct ath12k_ipa *ipa_ctx = IPA_CTX(ab);

	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->ipa_tx_buffer_attach)
		return ipa_ctx->arch_ops->ipa_tx_buffer_attach(ab);

	return 0;
}

static inline void
ath12k_dp_ipa_arch_op_rx_pipe_set_rdi(struct ath12k_ipa *ipa_ctx,
				      struct ipa_wdi_pipe_setup_info_smmu *rx_smmu)
{
	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->dp_rx_pipe_set_rdi)
		ipa_ctx->arch_ops->dp_rx_pipe_set_rdi(ipa_ctx, rx_smmu);
}

static inline u8
ath12k_dp_ipa_arch_op_get_wdi_version(struct ath12k_ipa *ipa_ctx)
{
	if (ipa_ctx && ipa_ctx->arch_ops && ipa_ctx->arch_ops->ipa_get_wdi_version)
		return ipa_ctx->arch_ops->ipa_get_wdi_version();

	return ATH12K_IPA_WDI_VER_INVALID;
}

static inline int
ath12k_dp_ipa_arch_op_iface_context_init(struct ath12k_ipa *ipa_ctx)
{
	if (ipa_ctx->arch_ops && ipa_ctx->arch_ops->ipa_iface_context_init)
		return ipa_ctx->arch_ops->ipa_iface_context_init(ipa_ctx);

	return -EINVAL;
}

static inline void
ath12k_dp_ipa_arch_op_iface_context_deinit(struct ath12k_ipa *ipa_ctx)
{
	/* Warn if arch_ops or the deinit callback is missing to catch memory leaks */
	if (WARN_ON_ONCE(!ipa_ctx || !ipa_ctx->arch_ops ||
			 !ipa_ctx->arch_ops->ipa_iface_context_deinit))
		return;

	ipa_ctx->arch_ops->ipa_iface_context_deinit(ipa_ctx);
}

static inline u16
ath12k_dp_ipa_arch_op_get_session_id(struct ath12k_ipa *ipa_ctx,
			     struct ath12k_link_vif *arvif)
{
	if (ipa_ctx->arch_ops && ipa_ctx->arch_ops->ipa_get_session_id)
		return ipa_ctx->arch_ops->ipa_get_session_id(arvif);

	return arvif->vdev_id;
}

static inline void
ath12k_dp_ipa_arch_op_initialize_default(struct ath12k_ipa *ipa_ctx, u16 i)
{
	if (ipa_ctx->arch_ops && ipa_ctx->arch_ops->ipa_initialize_default)
		return ipa_ctx->arch_ops->ipa_initialize_default(ipa_ctx, i);
}

static inline void
ath12k_dp_ipa_arch_op_uc_offload_enable(struct ath12k_ipa *ipa_ctx,
					u32 offload_type, u32 vdev_id,
					bool enable)
{
	if (ipa_ctx->arch_ops && ipa_ctx->arch_ops->ipa_uc_offload_enable)
		return ipa_ctx->arch_ops->ipa_uc_offload_enable(ipa_ctx, offload_type, vdev_id, enable);
}

void ath12k_dp_ipa_peer_notify(struct ath12k *ar,
			       struct ath12k_dp_link_peer *peer,
			       struct ath12k_link_vif *arvif,
			       u32 vdev_id, bool connect);
void ath12k_dp_ipa_vif_notify(struct ath12k_link_vif *arvif,
			      bool restart, bool connect);
int ath12k_dp_ipa_hw_group_init(struct ath12k_base *ab,
				struct ath12k_dp_hw_group *dp_hw_grp);
void ath12k_dp_ipa_hw_group_deinit(struct ath12k_dp_hw_group *dp_hw_grp);

#endif
