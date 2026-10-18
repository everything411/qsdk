/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <asm/cacheflush.h>

#ifndef __NSS_DP_ARCH_H__
#define __NSS_DP_ARCH_H__

/*
 * Set the flag only for 64-bit DMA address.
 */
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
#define NSS_DP_HIGHMEM_SUPP
#endif

#define NSS_DP_VP_HAL_MAX_PORTS		1
#define NSS_DP_HAL_MAX_PORTS		6
#define NSS_DP_MAX_PORTS		(NSS_DP_HAL_MAX_PORTS + NSS_DP_VP_HAL_MAX_PORTS)
#define NSS_DP_HAL_START_IFNUM		1

/*
 * Max FC Groups (VQs) supported for this SOC
 */
#define NSS_DP_HW_MAX_FC_GRP		32

/*
 * Maximum supported GSO segments
 */
#define NSS_DP_HAL_GSO_MAX_SEGS		GSO_MAX_SEGS

/*
 * Number of TX/RX queue supported
 */
#define NSS_DP_QUEUE_NUM		NR_CPUS

/*
 * Minimum packet size which can be transmitted by EDMA.
 */
#define NSS_DP_EDMA_TX_MIN_PKT_SZ       49

/*
 * Number of Max Tx/Rx Rings supported
 */
#define NSS_DP_EDMA_MAX_RXDESC_RINGS		24	/* Max RxDesc rings */
#define NSS_DP_EDMA_MAX_RXFILL_RINGS		20	/* Max RxFill rings */
#define NSS_DP_EDMA_MAX_TXCMPL_RINGS		20	/* Max TxCmpl rings */
#define NSS_DP_EDMA_MAX_TXDESC_RINGS		24	/* Max TxDesc rings */

#define EDMA_PPEDS_MAX_NODES	2	/* Maximum number of supported PPE-DS nodes */

/*
 * TX/RX NAPI budget
 */
#define NSS_DP_HAL_RX_NAPI_BUDGET	128
#define NSS_DP_HAL_TX_NAPI_BUDGET	512
#define NSS_DP_HAL_RXFILL_NAPI_BUDGET	512

/*
 * Timestamp information for the latency measurement
 */
#define NSS_DP_GMAC_TS_ADDR_SEC(x)	((x) + 0xD08)
#define NSS_DP_GMAC_TS_ADDR_NSEC(x)	((x) + 0xD0C)
#define NSS_DP_EDMA_DEF_TSTAMP_PORT	6

/*
 * EDMA clock's
 */
#define NSS_DP_EDMA_CSR_CLK			"nss-csr-clk"
#define NSS_DP_EDMA_NSSNOC_CSR_CLK		"nss-nssnoc-csr-clk"
#define NSS_DP_EDMA_TS_CLK			"nss-ts-clk"
#define NSS_DP_EDMA_NSSCC_CLK			"nss-nsscc-clk"
#define NSS_DP_EDMA_NSSCFG_CLK			"nss-nsscfg-clk"
#define NSS_DP_EDMA_NSSNOC_ATB_CLK		"nss-nssnoc-atb-clk"
#define NSS_DP_EDMA_NSSNOC_NSSCC_CLK		"nss-nssnoc-nsscc-clk"
#define NSS_DP_EDMA_NSSNOC_PCNOC_1_CLK		"nss-nssnoc-pcnoc-1-clk"
#define NSS_DP_EDMA_NSSNOC_QOSGEN_REF_CLK	"nss-nssnoc-qosgen-ref-clk"
#define NSS_DP_EDMA_NSSNOC_SNOC_1_CLK		"nss-nssnoc-snoc-1-clk"
#define NSS_DP_EDMA_NSSNOC_SNOC_CLK		"nss-nssnoc-snoc-clk"
#define NSS_DP_EDMA_NSSNOC_TIMEOUT_REF_CLK	"nss-nssnoc-timeout-ref-clk"
#define NSS_DP_EDMA_NSSNOC_XO_DCD_CLK		"nss-nssnoc-xo-dcd-clk"
#define NSS_DP_EDMA_NSSNOC_MEM_NOC_1_CLK	"nss-nssnoc-mem-noc-1-clk"
#define NSS_DP_EDMA_NSSNOC_MEMNOC_CLK		"nss-nssnoc-memnoc-clk"
#define NSS_DP_EDMA_CLK				"nss-edma-clk"

/*
 * EDMA clock's frequencies
 */
#define NSS_DP_EDMA_CSR_CLK_FREQ			100000000
#define NSS_DP_EDMA_NSSNOC_CSR_CLK_FREQ			100000000
#define NSS_DP_EDMA_TS_CLK_FREQ				24000000
#define NSS_DP_EDMA_NSSCC_CLK_FREQ			100000000
#define NSS_DP_EDMA_NSSCFG_CLK_FREQ			100000000
#define NSS_DP_EDMA_NSSNOC_ATB_CLK_FREQ			240000000
#define NSS_DP_EDMA_NSSNOC_NSSCC_CLK_FREQ		100000000
#define NSS_DP_EDMA_NSSNOC_PCNOC_1_CLK_FREQ		100000000
#define NSS_DP_EDMA_NSSNOC_QOSGEN_REF_CLK_FREQ		12000000
#define NSS_DP_EDMA_NSSNOC_SNOC_1_CLK_FREQ		266666666
#define NSS_DP_EDMA_NSSNOC_SNOC_CLK_FREQ		266666666
#define NSS_DP_EDMA_NSSNOC_TIMEOUT_REF_CLK_FREQ		6000000
#define NSS_DP_EDMA_NSSNOC_XO_DCD_CLK_FREQ		24000000
#define NSS_DP_EDMA_NSSNOC_MEM_NOC_1_CLK_FREQ		462000000
#define NSS_DP_EDMA_NSSNOC_MEMNOC_CLK_FREQ		462000000

#define NSS_DP_EDMA_DDRQ_BLK_NUM_DEF	2
#define NSS_DP_EDMA_DDRQ_BLK_SIZE_DEF	0

#define NSS_DP_EDMA_DDRQ_EN_PORT_BM	0x3f

#define EDMA_MAX_DMA_MASK_BIT_HI 32

#define EDMA_MAX_TXDESC_TO_CORE_MAP_PER_TYPE	(NR_CPUS * 2)
#define EDMA_MAX_TXDESC_RING_PER_TYPE	(NR_CPUS * 2)
#define EDMA_MAX_TXCMPL_RING_PER_TYPE	(NR_CPUS * 2)
#define EDMA_MAX_RXDESC_RING_PER_TYPE	(NR_CPUS * 2)
#define EDMA_MAX_RXFILL_RING_PER_TYPE	(NR_CPUS * 2)

#define EDMA_MAX_TXDESC_RING_PPEVP	NR_CPUS
#define EDMA_MAX_TXCMPL_RING_PPEVP	NR_CPUS

extern int edma_dp_host_rx_rings[EDMA_MAX_RXDESC_RING_PER_TYPE];
extern int edma_dp_host_rx_queue_map[EDMA_MAX_RXDESC_RING_PER_TYPE];
extern int edma_dp_host_rxfill_map[EDMA_MAX_RXFILL_RING_PER_TYPE];
extern int edma_dp_host_tx_rings[EDMA_MAX_TXDESC_RING_PER_TYPE];
extern int edma_dp_host_txcmpl_rings[EDMA_MAX_TXCMPL_RING_PER_TYPE];
extern int edma_dp_host_txcmpl_map[EDMA_MAX_TXDESC_RING_PER_TYPE];
extern int edma_dp_host_tx_ring_to_core_map[EDMA_MAX_TXDESC_TO_CORE_MAP_PER_TYPE];
extern int edma_dp_boot_rx_fill_cnt;
extern uint32_t edma_dp_host_rx_ring_sz;
extern uint32_t edma_dp_host_rxfill_ring_sz;
extern uint32_t edma_dp_host_tx_ring_sz;
extern uint32_t edma_dp_host_txcmpl_ring_sz;

extern int edma_dp_ppe_vp_num_tx_rings;
extern int edma_dp_ppe_vp_tx_rings[EDMA_MAX_TXDESC_RING_PPEVP];
extern int edma_dp_ppe_vp_txcmpl_map[EDMA_MAX_TXCMPL_RING_PPEVP];
extern int edma_dp_ppe_vp_num_tx_rings_per_core;
extern int edma_dp_ppe_vp_tx_ring_to_core_map[EDMA_MAX_TXDESC_TO_CORE_MAP_PER_TYPE];
extern uint32_t edma_dp_ppe_vp_tx_ring_sz;

#ifdef NSS_DP_HW_GRO
extern uint32_t edma_dp_gro_rx_ring_sz;
extern uint32_t edma_dp_gro_rxfill_ring_sz;
#define EDMA_GRO_PPE_QUEUE_BASE			176
extern int edma_dp_gro_ppe_queue_base;
#endif

extern int edma_dp_ppe_ds_rx_rings[EDMA_PPEDS_MAX_NODES];
extern int edma_dp_ppe_ds_rx_queue_map[EDMA_PPEDS_MAX_NODES];
extern int edma_dp_ppe_ds_num_rx_queue[EDMA_PPEDS_MAX_NODES];
extern int edma_dp_ppe_ds_num_rxdesc_per_node[EDMA_PPEDS_MAX_NODES];
extern int edma_dp_ppe_ds_rxfill_rings[EDMA_PPEDS_MAX_NODES];
extern int edma_dp_ppe_ds_tx_rings[EDMA_PPEDS_MAX_NODES];
extern int edma_dp_ppe_ds_num_txdesc_per_node[EDMA_PPEDS_MAX_NODES];
extern int edma_dp_ppe_ds_txcmpl_rings[EDMA_PPEDS_MAX_NODES];

/*
 * PPE VP feature ring arrays
 */
extern int edma_dp_ppe_vp_feat_num_rings;
extern int edma_dp_ppe_vp_feat_rx_rings[EDMA_MAX_RXDESC_RING_PER_TYPE];
extern int edma_dp_ppe_vp_feat_rx_queue_map[EDMA_MAX_RXDESC_RING_PER_TYPE];
extern int edma_dp_ppe_vp_feat_rxfill_map[EDMA_MAX_RXFILL_RING_PER_TYPE];
extern int edma_dp_ppe_vp_feat_type_map[EDMA_MAX_RXDESC_RING_PER_TYPE];
extern uint32_t edma_dp_ppe_vp_feat_rx_ring_sz;
extern uint32_t edma_dp_ppe_vp_feat_rxfill_ring_sz;

#define NSS_DP_VP_FEAT_NUM_RINGS	1

/**
 * nss_dp_hal_gmac_stats
 *	The per-GMAC statistics structure.
 */
struct nss_dp_hal_gmac_stats {
	uint64_t rx_packets;		/**< Number of RX packets */
	uint64_t rx_bytes;		/**< Number of RX bytes */
	uint64_t rx_dropped;		/**< Number of RX dropped packets */
	uint64_t rx_fraglist_packets;	/**< Number of RX fraglist packets */
	uint64_t rx_nr_frag_packets;	/**< Number of RX nr fragment packets */
	uint64_t rx_nr_frag_headroom_err;
			/**< Number of RX nr fragment packets with headroom error */
	uint64_t rx_get_host_qdisc_dev_fail;	/**< Number of RX packets failed to get qdisc netdev */
	uint64_t rx_host_qdisc_xmit_fail;	/**< Number of RX packet failed for qdisc transmit */
	uint64_t tx_packets;		/**< Number of TX packets */
	uint64_t tx_bytes;		/**< Number of TX bytes */
	uint64_t tx_dropped;		/**< Number of TX dropped packets */
	uint64_t tx_nr_frag_packets;	/**< Number of TX nr fragment packets */
	uint64_t tx_fraglist_packets;	/**< Number of TX fraglist packets */
	uint64_t tx_fraglist_with_nr_frags_packets;	/**< Number of TX fraglist packets with nr fragments */
	uint64_t tx_tso_packets;	/**< Number of TX TCP segmentation offload packets */
	uint64_t tx_tso_drop_packets;	/**< Number of TX TCP segmentation dropped packets */
	uint64_t tx_gso_packets;	/**< Number of TX SW GSO packets */
	uint64_t tx_gso_drop_packets;	/**< Number of TX SW GSO dropped packets */
	uint64_t tx_queue_stopped[NR_CPUS];
			/**< Number of times Queue got stopped */
};

/**
 * nss_dp_hal_nsm_sawf_sc_stats
 *	Per-service code stats to be send to NSM.
 */
struct nss_dp_hal_nsm_sawf_sc_stats {
	uint64_t rx_packets;	/**< Packets received for a service code on the PPE queues. */
	uint64_t rx_bytes;	/**< Bytes received for a service code on the PPE queues. */
};

extern int edma_init(void);
extern void edma_cleanup(bool is_dp_override);
extern bool edma_nsm_sawf_sc_stats_read(struct nss_dp_hal_nsm_sawf_sc_stats *nsm_stats, uint8_t service_class);
extern bool nss_dp_hal_nsm_sawf_sc_stats_read(struct nss_dp_hal_nsm_sawf_sc_stats *nsm_stats, uint8_t service_class);
extern int32_t nss_dp_hal_clock_set_and_enable(struct device *dev, const char *id, unsigned long rate);
extern struct nss_dp_data_plane_ops nss_dp_edma_ops;
extern int32_t nss_dp_hal_configure_clocks(void *ctx);
extern int nss_dp_hal_cache_info_setup(void *ctx);
extern int32_t nss_dp_hal_hw_reset(void *ctx);
extern void nss_dp_hal_configure_ring_size(uint32_t mem_profile);
#ifdef NSS_DP_PPEDS_SUPPORT
extern struct nss_dp_ppeds_ops edma_ppeds_ops_wifi7;
extern struct nss_dp_ppeds_ops edma_ppeds_ops_wifi8;
#endif

static inline void edma_dmac_inv_range(const void *start, const void *end){
#ifndef CONFIG_IO_COHERENCY
        dmac_inv_range(start, end);
#endif
}

static inline void edma_dmac_inv_range_no_dsb(const void *start, const void *end){
#ifndef CONFIG_IO_COHERENCY
        dmac_inv_range_no_dsb(start, end);
#endif
}

static inline void edma_dmac_clean_range_no_dsb(const void *start, const void *end){
#ifndef CONFIG_IO_COHERENCY
        dmac_clean_range_no_dsb(start, end);
#endif
}

static inline void edma_dsb(void){
#ifndef CONFIG_IO_COHERENCY
        dsb(st);
#endif
}

#endif /* __NSS_DP_ARCH_H__ */
