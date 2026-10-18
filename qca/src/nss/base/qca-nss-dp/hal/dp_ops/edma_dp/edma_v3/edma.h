/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __EDMA_H__
#define __EDMA_H__

#include <fal/fal_qm.h>
#include <fal/fal_qos.h>
#include <linux/netdevice.h>
#include <linux/reset.h>
#include <linux/of_platform.h>
#include <linux/version.h>
#include <linux/timer.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 6, 0))
#include <net/gso.h>
#endif
#include <nss_dp_arch.h>
#include <nss_dp_api_if.h>
#include <nss_dp_hal_if.h>
#include <ppe_drv.h>
#include "edma_rx.h"
#include "edma_tx.h"
#ifdef NSS_DP_PPEDS_SUPPORT
#include "edma_ppeds_priv.h"
#endif
#ifdef NSS_DP_DDRQ_SUPPORT
#include "edma_ddrq.h"
#endif
#ifdef NSS_DP_UDP_ST_SUPPORT
#include "edma_cfg_tx_udp_st.h"
#endif

/*
 * The driver uses kernel DMA constructs that assume an architecture
 * where the view of physical addresses is consistent between SoC and
 * IO device(EDMA).
 * Note that this may not be compatible for platforms where this
 * assumption is not true, for example IO devices with IOMMU support.
 */
#if !defined(NSS_DP_IPQ96XX) && \
	(defined(CONFIG_ARM_SMMU) || defined(CONFIG_IOMMU_SUPPORT))
#error "Build Error: Platform is enabled with IOMMU/SMMU support."
#endif

#define EDMA_HW_RESET_ID		"edma_rst"
#define EDMA_CFG_RESET_ID		"edma_cfg_rst"
#define EDMA_DEVICE_NODE_NAME		"edma"
#define EDMA_START_GMACS		NSS_DP_HAL_START_IFNUM
#define EDMA_MAX_GMACS			NSS_DP_HAL_MAX_PORTS
#define EDMA_MAX_PORTS			NSS_DP_MAX_PORTS

#ifdef NSS_DP_MHT_SW_PORT_MAP
#define EDMA_MAX_TX_PORTS		NSS_DP_HAL_MAX_TX_PORTS
#define EDMA_MAC_TX_MAP			EDMA_MAX_TX_PORTS

/*
 * If enabled, separate rings is allocated for VP port.
 * Else, VP port shares it's rings with MHT.
 */
#ifdef NSS_DP_EDMA_MHT_SW_WITH_VP_RING
#define EDMA_MAX_FC_GRP			EDMA_MAX_TX_PORTS - 1
#else
#define EDMA_MAX_FC_GRP			EDMA_MAX_TX_PORTS
#endif

/*
 * If enabled, We need to skip the 12 interrupts
 * that belongs to 4 PPEDS nodes. Else, skip 6
 * interrupts that belong to 2 PPEDS nodes.
 */
#ifdef NSS_DP_EDMA_SKIP_FOUR_PPEDS_NODES
#define EDMA_PPEDS_IRQS			12
#else
#define EDMA_PPEDS_IRQS			6
#endif

#else
#define EDMA_MAX_TX_PORTS		EDMA_TX_RING_PER_CORE_MAX
#define EDMA_MAX_FC_GRP			EDMA_MAX_GMACS
#define EDMA_MAC_TX_MAP			EDMA_MAX_PORTS
#endif


#define EDMA_IRQ_NAME_SIZE		32

#ifdef NSS_DP_HW_GRO
#define EDMA_NETDEV_FEATURES           NETIF_F_FRAGLIST \
                                       | NETIF_F_SG \
                                       | NETIF_F_RXCSUM \
                                       | NETIF_F_HW_CSUM \
                                       | NETIF_F_TSO \
                                       | NETIF_F_TSO6 \
                                       | NETIF_F_GRO_HW
#else
#define EDMA_NETDEV_FEATURES		NETIF_F_FRAGLIST \
					| NETIF_F_SG \
					| NETIF_F_RXCSUM \
					| NETIF_F_HW_CSUM \
					| NETIF_F_TSO \
					| NETIF_F_TSO6
#endif

#define EDMA_SWITCH_DEV_ID	0
#define EDMA_PPE_QUEUE_LEVEL	0
#define EDMA_BITS_IN_WORD	32
#define EDMA_PORT_SRC_PROFILE	0

#ifdef NSS_DP_HW_GRO
#define EDMA_RX_RING_GRO_NUM_MAX 4

#define EDMA_RX_GRO_TIMEOUT_MAX 0xFFFF
#define EDMA_RX_GRO_BUFFER_LEN_MAX 0x1FFFF
#define EDMA_RX_GRO_DESC_COUNT_MAX 0x3F

#define EDMA_RX_GRO_TIMEOUT_DEFAULT 0xFFF
#define EDMA_RX_GRO_BUFFER_LEN_DEFAULT 0x1FFFF
#define EDMA_RX_GRO_DESC_COUNT_DEFAULT (MAX_SKB_FRAGS + 1)
#endif

/*
 * Maximum queue priority
 */
#define EDMA_PRI_MAX		16

/*
 * Maximum queue priority supported per core
 */
#define EDMA_MAX_PRI_PER_CORE	8

/*
 * Bitmap for ring to PPE queue's mapping.
 *
 * A bitmap for 300 PPE queues requires 10 32bit integers
 */
#define EDMA_RING_MAPPED_QUEUE_BM_WORD_COUNT	10

#define EDMA_RX_RING_MODE_BITMASK_DEF		0xF00000
#define EDMA_TX_RING_MODE_BITMASK_DEF		0x0

#define EDMA_RING_MODE_NOT_SET			-1

/*
 * QID to RID Table
 */
#define EDMA_QID2RID_TABLE_MEM(q)	(0xb9000 + (0x4 * (q)))

#define EDMA_TIMESTAMP_SEC_MASK		EDMA_RX_SDESC_TSTAMP_HI_MASK
#define EDMA_TIMESTAMP_NSEC_TO_USEC(x)	((x) / 1000)
#define EDMA_TIMESTAMP_TO_USEC(x, y)	(((uint64_t)(x) * 1000000) + (EDMA_TIMESTAMP_NSEC_TO_USEC(y)))

/*
 * Indicate the maximum 4MB byte of data which is expected to be used
 * So array of EDMA_MAX_LOOPBACK_BUF will have pointer to memory of max 4MB
 * With maximum 32 index, we can store more DDR buffers for loopback ring.
 *
 * So for a 64K descriptor, each occupying 1536 bytes, total memory to be used is ~96MB for each
 * we need an array of 32 to maintain these pages.
 */
#define EDMA_MAX_LOOPBACK_BUF 32

#define EDMA_PASSTHROUGH_VAL_INVALID	-1

/*
 * EDMA ring subtypes. Each type can have subtype which can inherit the
 * data from its parent type. For example, EDMA_RING_TYPE_HOST can have
 * 2 subtypes - HOST_SFE and HOST_VP. This can be extended further as
 * required.
 */
#define EDMA_RING_TYPE_FLAGS_HOST_COMMON	0x1
#define EDMA_RING_TYPE_FLAGS_HOST_VP		0x2	/* General PPE VP TX/RX data rings */
#define EDMA_RING_TYPE_FLAGS_HOST_GRO		0x4
#define EDMA_RING_TYPE_FLAGS_DS			0x8
#define EDMA_RING_TYPE_FLAGS_PAGE_POOL		0x10	/* Page pool alloc mode */
#define EDMA_RING_TYPE_FLAGS_HOST_VP_FEAT	0x20	/* VP feature-classified RX rings (e.g. CAPWAP) */

/*
 * EDMA ring status flags
 */
#define EDMA_RING_STATUS_FLAGS_IN_USE		0x1
#define EDMA_RING_STATUS_FLAGS_IS_CONFIGURED	0x2

/*
 * EDMA ring flags
*/
#define EDMA_RING_FLAGS_SEC_RING_VALID	(1UL << 0)

/*
 * EDMA invalid ring id
 */
#define EDMA_RING_FLAGS_INVALID_ID	-1

/*
 * edma_port_ucast_queues
 * 	EDMA unicast queue number
 * To-do: read queue start from dtsi
 */
enum edma_port_ucast_queues {
	EDMA_CPU_PORT_QUEUE_START = 0,
	EDMA_CPU_PORT_QUEUE_MAX = 31,
};

/*
 * edma_cpu_port_mcast_queues
 *	EDMA multicast queue number
 */
enum edma_cpu_port_mcast_queues {
	EDMA_CPU_PORT_MCAST_QUEUE_START = 256,
	EDMA_CPU_PORT_MCAST_QUEUE_END = 271,
};

/*
 * edma_ring_types_t
 *	EDMA ring types
 */
typedef enum {
	EDMA_RING_TYPE_HOST = 1,	/* Host specific rings */
	EDMA_RING_TYPE_DS		/* Direct Switch specific rings */
} edma_ring_types_t;

/*
 * EDMA profile ID
 *
 * To-do: Use enum once ppe-drv introduces profile id enum.
 */
#define EDMA_CPU_PORT_PROFILE_ID	0

/*
 * EDMA maximum RSS hash
 */
#define EDMA_RSS_HASH_MAX	256

/*
 * EDMA QID2RID configuration
 */
#define EDMA_QID2RID_NUM_PER_REG	4

/*
 * One clock cycle = 1/(EDMA clock frequency in Mhz) micro seconds
 *
 * One timer unit is 128 clock cycles.
 *
 * So, therefore the microsecond to timer unit calculation is:
 * Timer unit	= time in microseconds / (one clock cycle in microsecond * cycles in 1 timer unit)
 * 		= ('x' microsecond * EDMA clock frequency in MHz ('y') / 128)
 */
#define CYCLE_PER_TIMER_UNIT	128
#define MICROSEC_TO_TIMER_UNIT(x, y)	((x) * (y) / CYCLE_PER_TIMER_UNIT)
#define MHZ			1000000UL

#define EDMA_DESC_AVAIL_COUNT(head, tail, max) (((head) - (tail)) + (max)) & ((max) - 1)

/*
 * EDMA MISC status get macros
 */
#define EDMA_MISC_AXI_RD_ERR_STATUS_GET(x)	((x) & EDMA_MISC_AXI_RD_ERR_MASK)
#define EDMA_MISC_AXI_WR_ERR_STATUS_GET(x)	(((x) & EDMA_MISC_AXI_WR_ERR_MASK) >> 1)
#define EDMA_MISC_RX_DESC_FIFO_FULL_STATUS_GET(x)	(((x) & EDMA_MISC_RX_DESC_FIFO_FULL_MASK) >> 2)
#define EDMA_MISC_RX_ERR_BUF_SIZE_STATUS_GET(x)		(((x) & EDMA_MISC_RX_ERR_BUF_SIZE_MASK) >> 3)
#define EDMA_MISC_TX_SRAM_FULL_STATUS_GET(x)		(((x) & EDMA_MISC_TX_SRAM_FULL_MASK) >> 4)
#define EDMA_MISC_TX_CMPL_BUF_FULL_STATUS_GET(x)		(((x) & EDMA_MISC_TX_CMPL_BUF_FULL_MASK) >> 5)
#define EDMA_MISC_DATA_LEN_ERR_STATUS_GET(x)		(((x) & EDMA_MISC_DATA_LEN_ERR_MASK) >> 6)
#define EDMA_MISC_TX_TIMEOUT_STATUS_GET(x)		(((x) & EDMA_MISC_TX_TIMEOUT_MASK) >> 7)
#define EDMA_MISC_PASS_THR_ERR_FWD_STATUS_GET(x)	(((x) & EDMA_MISC_PASS_THR_ERR_FWD_MASK) >> 8)
#define EDMA_MISC_TXQ_PASSTHR_OFFSET_MIS_STATUS_GET(x)	(((x) & EDMA_MISC_TXQ_PASSTHR_OFFSET_MIS_MASK) >> 9)
#define EDMA_MISC_TXQ_DS_CMPL_ERR_STATUS_GET(x)		(((x) & EDMA_MISC_TXQ_DS_CMPL_ERR_MASK) >> 10)

#define __DDR_SIZE_KBYTES(x) ((x) * 1024)
#define __DDR_SIZE_MBYTES(x) (__DDR_SIZE_KBYTES(x) * 1024)
#define __DDR_SIZE_GBYTES(x) (__DDR_SIZE_MBYTES(x) * 1024)

#define EDMA_DEFAULT_DDR_SIZE __DDR_SIZE_GBYTES(3UL) /* 3GB */
#define EDMA_DEFAULT_DMA_MASK_BIT_HI 32

#define EDMA_TXRX_RING_PH_EN_MASK(ring_id)			(0x1 << (ring_id))
#define EDMA_RING_MODE_GET(idx, mode_bm)		(((mode_bm) & EDMA_TXRX_RING_PH_EN_MASK(idx)) >> (idx))

/*
 * Validate txcompl, rxdesc, and rxfill ring id params as integers
 */
#define param_check_bp_stats_en_txcmpl_ring_id(name, p) \
        __param_check(name, p, int)

#define param_check_bp_stats_en_rxdesc_ring_id(name, p) \
        __param_check(name, p, int)

#define param_check_bp_stats_en_rxfill_ring_id(name, p) \
        __param_check(name, p, int)

/*
 * EDMA Ring usage stats macro
 */
enum edma_ring_usage_percentage {
	EDMA_RING_USAGE_50_PERCENTAGE = 50,
	EDMA_RING_USAGE_70_PERCENTAGE = 70,
	EDMA_RING_USAGE_90_PERCENTAGE = 90,
	EDMA_RING_USAGE_100_PERCENTAGE = 100,
};

/*
 * edma_misc_stats
 *	EDMA miscellaneous stats
 */
struct edma_misc_stats {
	uint64_t edma_misc_axi_read_err;		/* AXI read error */
	uint64_t edma_misc_axi_write_err;		/* AXI write error */
	uint64_t edma_misc_rx_desc_fifo_full;		/* Rx descriptor FIFO full error */
	uint64_t edma_misc_rx_buf_size_err;		/* Rx buffer size too small error */
	uint64_t edma_misc_tx_sram_full;		/* Tx packet SRAM buffer full error */
	uint64_t edma_misc_tx_data_len_err;		/* Tx data length error */
	uint64_t edma_misc_tx_timeout;			/* Tx timeout error */
	uint64_t edma_misc_tx_cmpl_buf_full;		/* Tx completion buffer full error */
	uint64_t edma_misc_pass_thr_err_fwd;		/* Pass through packet forward error */
	uint64_t edma_misc_txq_passthr_offset_miss;	/* TXQ pass through offset miss error */
	uint64_t edma_misc_txq_ds_cmpl_err;		/* TXQ DS CMPL error */
	struct u64_stats_sync syncp;			/* Synchronization pointer */
};

/*
 * edma_sawf_sc_stats
 *	EDMA per-service code stats
 */
struct edma_sawf_sc_stats {
	uint64_t rx_packets;		/* Per service code counter for packets recieved on queues from PPE */
	uint64_t rx_bytes;		/* Per service code counter for bytes recieved on queues from PPE */
	struct u64_stats_sync syncp;	/* Synchronization pointer */
};

/*
 * edma_pcpu_stats
 *	EDMA per cpu stats data structure
 */
struct edma_pcpu_stats {
	struct edma_rx_stats __percpu *rx_stats;
			/* Per CPU Rx statistics */
	struct edma_tx_stats __percpu *tx_stats;
			/* Per CPU Tx statistics */
};

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
/*
 * edma_dp_loopback_buf_info
 *	Loopback buffer info
 */
struct edma_dp_loopback_buf_info {
	unsigned long loopback_buf;
			/* Array to hold loopback rxfill ring buffer address */
	int loopback_order;
			/* Order for loopback rxfill page allocation */
};
#endif

/*
 * edma_txdesc_ring_info
 *	TX descriptor ring information
 */
struct edma_txdesc_ring_info {
	struct edma_txdesc_ring *txdesc_ring;	/* TX ring pointer */
	edma_ring_types_t ring_type;		/* Ring type */
	uint32_t desc_count;			/* Number of Descriptors */
	uint32_t type_flags;			/* Ring type flags */
	uint32_t status_flags;			/* Ring status flags */
	uint32_t txcmpl_ring_id;		/* TX completion ring ID for this ring */
	uint32_t fc_grp_id;			/* Flow Control group ID */
};

/*
 * edma_txcmpl_ring_info
 *	TX completion ring information
 */
struct edma_txcmpl_ring_info {
	struct edma_txcmpl_ring *txcmpl_ring;	/* TX completion ring pointer */
	edma_ring_types_t ring_type;		/* Ring type */
	uint32_t desc_count;			/* Number of Descriptors */
	uint32_t type_flags;			/* Ring type flags */
	uint32_t status_flags;			/* Ring status flags */
	uint32_t intr_num;			/* Interrupt number */
};

/*
 * edma_rxdesc_ring_info
 *	RX desc ring information
 */
struct edma_rxdesc_ring_info {
	struct edma_rxdesc_ring *rxdesc_ring;		/* Rx ring pointer */
	edma_ring_types_t ring_type;			/* Ring type */
	uint32_t desc_count;				/* Number of Descriptors */
	uint32_t rxfill_ring_id;			/* RX fill ring ID for this ring */
	uint32_t ppe_queue_base;			/* Queue base */
	uint32_t ppe_num_queues;			/* Number of queues for this ring */
	uint32_t type_flags;				/* Ring type flags */
	uint32_t status_flags;				/* Ring status flags */
	uint32_t intr_num;				/* Interrupt number */
	edma_vp_feat_type_t vp_feat_type;		/* VP feature sub-type */
};

/*
 * edma_rxfill_ring_info
 *	RX Fill ring information
 */
struct edma_rxfill_ring_info {
	struct edma_rxfill_ring *rxfill_ring;		/* Fill ring pointer */
	edma_ring_types_t ring_type;			/* Ring type */
	uint32_t type_flags;				/* Ring type flags */
	uint32_t status_flags;				/* Ring status flags */
	uint32_t desc_count;				/* Number of Descriptors */
	uint32_t alloc_size;				/* Alloc size (Total buffer size) */
	uint32_t buffer_len;				/* buffer length (Max packet length supported per buffer) */
	bool page_mode;					/* Page mode */
	uint32_t intr_num;				/* Interrupt number */
	uint32_t flags;					/* flags */
};

/*
 * edma_rx_per_ring_map
 *	One RX ring mapping.
 */
struct edma_rx_per_ring_map {
	uint32_t rx_ring_id;		/* RX ring ID */
	uint32_t rx_fill_ring_id;	/* RX fill ring ID */
	uint32_t ppe_queue_base;	/* PPE Queue base */
};

/*
 * edma_tx_per_ring_map
 *	One TX ring mapping.
 */
struct edma_tx_per_ring_map {
	uint32_t tx_ring_id;		/* TX ring ID */
	uint32_t tx_cmpl_ring_id;	/* TX completion ring ID */
	uint32_t fc_grp_id;		/* Flow Control group ID */
};

/*
 * edma_rx_rings_info
 *	RX rings information.
 */
struct edma_rx_rings_info {
	struct edma_rx_per_ring_map rx_map[EDMA_MAX_RXDESC_RING_PER_TYPE];	/* RX per ring map */
	uint32_t num_rx_rings;							/* Number of rings to be configured */
	uint32_t num_queues_per_ring;						/* Queue set for each ring */
};

/*
 * edma_tx_rings_info
 *	TX rings information.
 */
struct edma_tx_rings_info {
	struct edma_tx_per_ring_map tx_map[EDMA_MAX_TXDESC_RING_PER_TYPE];	/* TX per ring map */
	uint32_t tx_ring_per_core_map[NR_CPUS][EDMA_MAX_TX_RINGS_PER_CORE];	/* Max rings per core */
	uint32_t num_tx_rings;							/* Number of rings to be configured */
	uint32_t max_rings_per_core;						/* Max rings per core */
};

/*
 * edma_rings_common_info
 *	Common information per section.
 */
struct edma_rings_common_info {
	uint32_t edma_rxfill_ring_map[EDMA_MAX_RXFILL_RING_PER_TYPE];		/* RXFILL ring map */
	uint32_t edma_txcmpl_ring_map[EDMA_MAX_TXCMPL_RING_PER_TYPE];		/* TXCMPL ring map */
	uint32_t edma_num_rxfill_rings;						/* Number of RXFILL rings. */
	uint32_t edma_num_txcmpl_rings;						/* Number of TXCMPL rings. */
};

#ifdef NSS_DP_PPEDS_SUPPORT
/*
 * edma_ppeds_node_info
 *	PPE DS node information.
 */
struct edma_ppeds_node_info {
	struct edma_rx_per_ring_map rx_map[EDMA_PPEDS_MAX_RINGS_PER_NODE];	/* RX ring map */
	struct edma_tx_per_ring_map tx_map[EDMA_PPEDS_MAX_RINGS_PER_NODE];	/* TX ring map */
	uint32_t num_rx_rings;							/* Number of RX rings */
	uint32_t num_tx_rings;							/* Number of TX rings */
	uint32_t num_queues_per_ring;						/* Number of queues per ring. */
};

/*
 * edma_ppeds_info
 *	PPE DS information.
 */
struct edma_ppeds_info {
	struct edma_ppeds_node_info node_info[EDMA_PPEDS_MAX_NODES];	/* PPEDS node information. */
	uint32_t num_nodes;						/* Number of DS nodes. */
};
#endif

/*
 * edma_ds_info
 *	DS mode configuration information.
 */
struct edma_ds_info {
#ifdef NSS_DP_PPEDS_SUPPORT
	struct edma_ppeds_info ppeds_info;		/* PPE-DS config information. */
#endif
};

/*
 * edma_host_gro_info
 *	GRO mode configuration information.
 */
struct edma_host_gro_info {
	struct edma_rx_rings_info rx_info;	/* RX rings information */
};

/*
 * edma_host_sfe_info
 *	SFE mode configuration information.
 */
struct edma_host_sfe_info {
	struct edma_rx_rings_info rx_info;	/* RX rings information */
	struct edma_tx_rings_info tx_info;	/* TX rings information */
};

/*
 * edma_host_vp_info
 *	VP mode configuration information.
 */
struct edma_host_vp_info {
	struct edma_rx_rings_info rx_info;		/* RX rings information */
	struct edma_tx_rings_info tx_info;		/* TX rings information */
};

/*
 * edma_host_vp_feat_info
 *	VP feature rings configuration information (e.g. CAPWAP).
 */
struct edma_host_vp_feat_info {
	struct edma_rx_rings_info rx_info;		/* RX rings information */
};

/*
 * edma_host_info
 *	HOST mode configuration information.
 */
struct edma_host_info {
	struct edma_rings_common_info common_info;
	struct edma_host_sfe_info sfe_info;		/* Host SFE specific information. */
	struct edma_host_vp_info vp_info;		/* Host VP specific information. */
	struct edma_host_gro_info gro_info;		/* Host GRO specific information. */
	struct edma_host_vp_feat_info vp_feat_info;	/* Host VP feature specific information. */
};

/*
 * edma_init_info
 *	EDMA configuration information (RX / TX rings, queues)
 */
struct edma_init_info {
	struct edma_host_info host_info;
						/* Host config info. */
	struct edma_ds_info ds_info;
						/* DS config info. */
	uint32_t valid_flags;
						/* Valid flags indicating the VP,
						 * HOST, DS context information is valid or not
						 */
};

/*
 * edma_hw_gro_ctx
 *	HW gro context structure
 */
struct edma_hw_gro_ctx {
	int gro_timeout_usecs;
		/* GRO default timeout value */
	int gro_buffer_len;
		/* GRO default buffer lenght */
	int gro_desc_count;
		/* GRO default descriptor count */
	bool hw_gro_en;
		/* HW GRO enable */
	uint8_t rx_gro_queue_start;
		/* RX GRO queue start */
	uint8_t rx_gro_ring_start;
		/* RX GRO ring start */
};

/*
 * edma_init_stage - EDMA initialization stage tracking
 *
 * This enum defines bits for tracking completion of each initialization stage.
 * Each bit represents successful completion of a specific initialization point.
 * Used for crash dump analysis to determine where initialization failed.
 */
enum edma_init_stage {
	EDMA_INIT_STAGE_CTX_ALLOC = 0,           /* edma_gbl_ctx allocated */
	EDMA_INIT_STAGE_RING_MAPS_INIT,          /* Ring maps initialized */
	EDMA_INIT_STAGE_DTS_PARSED,              /* Device tree parsed */
	EDMA_INIT_STAGE_DESC_MAP_VALID,          /* Descriptor map validated */
	EDMA_INIT_STAGE_SYSCTL_REG,              /* Sysctl registered */
	EDMA_INIT_STAGE_MEM_REGION_REQ,          /* Memory region requested */
	EDMA_INIT_STAGE_IOREMAP_DONE,            /* IO remap completed */
	EDMA_INIT_STAGE_DEBUGFS_INIT,            /* Debugfs initialized */
	EDMA_INIT_STAGE_PPEDS_INIT,              /* PPE-DS initialized */
	EDMA_INIT_STAGE_CLOCKS_CONFIGURED,       /* All clocks configured */
	EDMA_INIT_STAGE_HW_RESET_DONE,           /* Hardware reset completed */
	EDMA_INIT_STAGE_PAGE_MODE_SET,           /* Page mode configured */
	EDMA_INIT_STAGE_RINGS_ALLOCATED,         /* Rings allocated */
	EDMA_INIT_STAGE_TX_MAPPING_DONE,         /* Tx mapping configured */
	EDMA_INIT_STAGE_RX_MAPPING_DONE,         /* Rx mapping configured */
	EDMA_INIT_STAGE_TX_RINGS_CFG,            /* Tx rings configured */
	EDMA_INIT_STAGE_RX_RINGS_CFG,            /* Rx rings configured */
	EDMA_INIT_STAGE_DMA_CTRL_CFG,            /* DMA control configured */
	EDMA_INIT_STAGE_PRIO_MAP_CFG,            /* Priority map configured */
	EDMA_INIT_STAGE_RPS_HASH_CFG,            /* RPS hash configured */
	EDMA_INIT_STAGE_LOOPBACK_CFG,            /* Loopback configured */
	EDMA_INIT_STAGE_PORT_ENABLED,            /* EDMA port enabled */
	EDMA_INIT_STAGE_PROCFS_INIT,             /* Procfs initialized */
	EDMA_INIT_STAGE_MINIDUMP_REG,            /* Minidump registered */

	EDMA_INIT_STAGE_MAX                      /* Maximum stages */
};

/*
 * edma_clock_init_stage - Clock initialization stage tracking
 *
 * These bits track individual clock configuration stages.
 * Different SoCs will use different subsets of these bits.
 */
enum edma_clock_init_stage {
	EDMA_CLK_STAGE_CSR = 0,		/* NSS_DP_EDMA_CSR_CLK */
	EDMA_CLK_STAGE_NSSNOC_CSR,	/* NSS_DP_EDMA_NSSNOC_CSR_CLK */
	EDMA_CLK_STAGE_TS,		/* NSS_DP_EDMA_TS_CLK */
	EDMA_CLK_STAGE_NSSCC,		/* NSS_DP_EDMA_NSCC_CLK */
	EDMA_CLK_STAGE_NSSCFG,		/* NSS_DP_EDMA_NSSCFG_CLK */
	EDMA_CLK_STAGE_NSSNOC_ATB,	/* NSS_DP_EDMA_NSSNOC_ATB_CLK */
	EDMA_CLK_STAGE_NSSNOC_NSSCC,	/* NSS_DP_EDMA_NSSNOC_NSSCC_CLK */
	EDMA_CLK_STAGE_NSSNOC_PCNOC_1,	/* NSS_DP_EDMA_NSSNOC_PCNOC_1_CLK */
	EDMA_CLK_STAGE_NSSNOC_QOSGEN_REF,	/* NSS_DP_EDMA_NSSNOC_QOSGEN_REF_CLK */
	EDMA_CLK_STAGE_NSS_NOC_REG,	/* NSS NOC register update */
	EDMA_CLK_STAGE_NSSNOC_SNOC_1,		/* NSS_DP_EDMA_NSSNOC_SNOC_1_CLK */
	EDMA_CLK_STAGE_NSSNOC_SNOC,		/* NSS_DP_EDMA_NSSNOC_SNOC_CLK */
	EDMA_CLK_STAGE_NSSNOC_TIMEOUT_REF,	/* NSS_DP_EDMA_NSSNOC_TIMEOUT_REF_CLK */
	EDMA_CLK_STAGE_NSSNOC_XO_DCD,		/* NSS_DP_EDMA_NSSNOC_XO_DCD_CLK */
	EDMA_CLK_STAGE_NSSNOC_MEMNOC,		/* NSS_DP_EDMA_NSSNOC_MEMNOC_CLK */
	EDMA_CLK_STAGE_NSSNOC_MEM_NOC_1,	/* NSS_DP_EDMA_NSSNOC_MEM_NOC_1_CLK */
	EDMA_CLK_STAGE_MEM_NOC_NSSNOC,		/* NSS_DP_EDMA_MEM_NOC_NSSNOC_CLK */
	EDMA_CLK_STAGE_SNOC_NSSNOC,	/* NSS_DP_EDMA_SNOC_NSSNOC_CLK */
	EDMA_CLK_STAGE_SNOC_NSSNOC_1,	/* NSS_DP_EDMA_SNOC_NSSNOC_1_CLK */
	EDMA_CLK_STAGE_MAX
};

/*
 * edma_gbl_ctx
 *	EDMA private data structure
 */
struct edma_gbl_ctx {
	struct net_device *netdev_arr[EDMA_MAX_PORTS];
			/* Net device for each GMAC port */
	struct device_node *device_node;
			/* Device tree node */
	struct reset_control *hw_rst;
			/* Hardware reset
 			 * TODO - Revisit if this hardware reset is actually required.
			 */
	struct reset_control *cfg_rst;
			/* EDMA configuration reset */
	struct platform_device *pdev;
			/* Platform device */
	void __iomem *reg_base;
			/* EDMA base register mapped address */
	struct resource *reg_resource;
			/* Memory resource */
	atomic_t active_port_count;
			/* Count of active number of ports */
	bool napi_added;
			/* NAPI flag */

	struct ctl_table_header *ctl_table_hdr;
			/* sysctl table entry */

	struct edma_rxfill_ring_info rxfill_info[EDMA_MAX_RXFILL_RINGS];
			/* RX fill ring information */
	struct edma_rxdesc_ring_info rxdesc_info[EDMA_MAX_RXDESC_RINGS];
			/* RX desc ring information */
	struct edma_txdesc_ring_info txdesc_info[EDMA_MAX_TXDESC_RINGS];
			/* TX desc ring information */
	struct edma_txcmpl_ring_info txcmpl_info[EDMA_MAX_TXCMPL_RINGS];
			/* TX cmpl ring information */

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
	struct edma_rxdesc_ring *rxdesc_loopback_rings;
			/* Rx Descriptor loopback ring */
	struct edma_rxfill_ring *rxfill_loopback_rings;
			/* Rx Fill loopback ring */
	struct edma_txdesc_ring *txdesc_loopback_rings;
			/* TX descriptor loopback ring */
	struct edma_txcmpl_ring *txcmpl_loopback_rings;
			/* TX completion loopback ring */
#endif
	uint32_t (*rxdesc_ring_to_queue_bm)[EDMA_RING_MAPPED_QUEUE_BM_WORD_COUNT];
			/* Bitmap of mapped PPE queue ids of the Rx descriptor rings */
	uint32_t *cache_data;
			/* pointer to EDMA descriptor rings cache register data */
	int32_t tx_fc_grp_map[NSS_DP_HW_MAX_FC_GRP];
			/* Per GMAC TxDesc ring to flow control group mapping */

	struct dentry *root_dentry;	/* Root debugfs entry */
	struct dentry *stats_dentry;	/* Statistics debugfs entry */
	struct dentry *bp_stats_dentry;	/* Back pressure statistics debugfs entry */
#ifdef NSS_DP_DDRQ_SUPPORT
	struct dentry *ddrq_info_dentry;	/* DDRQ information debugfs entry */
#endif

	struct edma_misc_stats __percpu *misc_stats;
			/* Per CPU miscellaneous statistics */
	struct edma_sawf_sc_stats sawf_sc_stats[PPE_DRV_SAWF_SC_MAX];
			/* Per service class stats */

	uint64_t mem_size;
			/* DDR size on Board */
	uint32_t tx_priority_level;
			/* Tx priority level per port */
	uint32_t rx_priority_level;
			/* Rx priority level per core */
	uint32_t rxdesc_ring_max;
			/* Max RX desc rings */
	uint32_t txdesc_ring_max;
			/* Max TX desc rings */
	uint32_t rxfill_ring_max;
			/* Max RX fill rings */
	uint32_t txcmpl_ring_max;
			/* Max TX comp rings */
	uint32_t misc_intr;
			/* Misc IRQ number */

	uint32_t rxfill_intr_mask;
			/* Rx fill ring interrupt mask */
	uint32_t rxdesc_intr_mask;
			/* Rx Desc ring interrupt mask */
	uint32_t txcmpl_intr_mask;
			/* Tx Cmpl ring interrupt mask */
	uint32_t misc_intr_mask;
			/* Misc interrupt interrupt mask */
	uint32_t dp_override_cnt;
			/* Number of interfaces overriden */
	uint32_t rx_page_mode;
			/* Page mode enabled or disabled */
	uint32_t rx_jumbo_mru;
			/* Jumbo MRU value */
#if defined(NSS_DP_POINT_OFFLOAD)
	uint32_t txdesc_point_offload_ring;
			/* TX desc ring for point offlaod */
	uint32_t txcmpl_point_offload_ring;
			/* TX completion ring for point offlaod */
	uint32_t rxfill_point_offload_ring;
			/* RX fill ring for point offload */
	uint32_t rxdesc_point_offload_ring;
			/* RX desc ring for point offload */
	uint32_t rxdesc_point_offload_ring_to_queue_bm[EDMA_RING_MAPPED_QUEUE_BM_WORD_COUNT];
			/* PPE queue ids of the Rx descriptor point offload rings */
	uint16_t point_offload_queue;
			/* Point offload base queue id */
#endif
#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
	uint32_t loopback_ring_size;
			/* Loopback ring size */
	uint32_t loopback_buf_size;
			/* Loopback buffer size */
	uint32_t num_loopback_rings;
			/* Number of loopback rings */
	uint32_t rxdesc_loopback_ring_to_queue_bm[EDMA_RING_MAPPED_QUEUE_BM_WORD_COUNT];
			/* PPE queue ids of the Rx descriptor loopback rings */
	uint8_t *txdesc_loopback_ring_id_arr;
			/* Array of TX desc Ring IDs */
	uint8_t *txcmpl_loopback_ring_id_arr;
			/* Array of TX completion Ring IDs */
	uint8_t *rxdesc_loopback_ring_id_arr;
			/* Array of RX desc Ring IDs */
	uint8_t *rxfill_loopback_ring_id_arr;
			/* Array of RX fill Ring IDs */
	uint32_t loopback_queue_base;
			/* Loopback Base Queue ID */
	uint32_t loopback_num_queues;
			/* Number of loopback queues */
	bool loopback_en;
			/* Loopback enabled */
	uint32_t loopback_feature_type; /* Loopback ring feature type */
	struct edma_dp_loopback_buf_info buf_info[EDMA_MAX_LOOPBACK_BUF];
#endif
	bool edma_initialized;
			/* Flag to check initialization status */
	struct edma_hw_gro_ctx hw_gro_ctx;
			/* HW GRO context */
#ifdef NSS_DP_PPEDS_SUPPORT
	struct edma_ppeds_drv ppeds_drv;
			/* PPE-DS nodes information */
#endif
	uint8_t rx_queue_start;
			/* Rx queue start */
	bool enable_ring_util_stats;
			/* Flag for tracking ring utilization */
#ifdef NSS_DP_MHT_SW_PORT_MAP
	uint8_t max_tx_ports;
			/* Max Tx ports */
	uint32_t mht_tx_ports;
			/* Max MHT Tx ports */
	uint32_t mht_txcmpl_ports;
			/* Max MHT Txcmpl ports */
#endif
	struct work_struct work;
                        /* Creating work struct */
	uint32_t edma_timer_rate;
			/* EDMA clock's timer rate in Mhz */

#ifdef NSS_DP_DDRQ_SUPPORT
	edma_ddrq_cfg_t ddrq_def_cfg;
#endif

#ifdef CONFIG_SKB_TIMESTAMP
	void __iomem *tstamp_sec;
			/* EDMA timestamp value in second */
	void __iomem *tstamp_nsec;
			/* EDMA timestamp value in nano-second */
#endif

	/* Initialization tracking bitmaps for crash dump analysis */
	uint32_t hw_init_bitmap;
			/* Tracks main initialization stages */
	uint32_t clk_init_bitmap;
			/* Tracks clock initialization stages */
#ifdef NSS_DP_UDP_ST_SUPPORT
	struct edma_udp_st_ctx udp_st_ctx;
			/* UDP-ST TX ring context */
#endif
	uint32_t mem_profile;
	uint32_t rx_ring_sz;
	uint32_t tx_ring_sz;
	uint32_t umac_reset_delay_war_inc_cnt;
};

typedef void (*edma_rx_wifi_qos_handler_t) (struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
					    struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
					    struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec);

extern struct edma_gbl_ctx *edma_gbl_ctx;
extern struct edma_init_info init_info;
extern uint32_t edma_hang_recover;
extern int edma_dp_extension_en;
extern int edma_ddrq_sp_cloned_pt_mode;

extern int edma_rx_ring_mode_bitmask;
extern int edma_tx_ring_mode_bitmask;

#ifdef NSS_DP_UDP_ST_SUPPORT
/*
 * UDP-ST module parameters
 */
extern int edma_udp_st_tx_ring;
extern int edma_udp_st_tx_cmpl_ring;
extern int edma_udp_st_fc_grp_id;
extern int edma_udp_st_ring_size;
extern int edma_udp_st_pass_through_mode;
#endif

#ifdef NSS_DP_DDRQ_SUPPORT
extern int32_t edma_passthrough_val;
extern int edma_ddrq_gbl_en_sw;
extern int edma_ddrq_gbl_en_hw;
extern int edma_ddrq_gbl_data_offset0;
extern int edma_ddrq_desc_pf_thres;
extern int edma_ddrq_data_offset;
extern int edma_ddrq_blk_num;
extern int edma_ddrq_blk_size;
extern int edma_ddrq_desc_wb_thres;
extern int edma_ddrq_en_port_bm;;
extern int edma_ddrq_blk_num_map[EDMA_DDRQ_BLK_NUM_MAX_ARR_IDX];
extern int edma_ddrq_blk_size_map[EDMA_DDRQ_BLK_SIZE_MAX_ARR_IDX];
extern int edma_ddrq_vp_port_map[NSS_DP_MAX_PORTS];
extern int edma_ddrq_ac_queue_ac_en;
extern int edma_ddrq_ac_queue_color_aware;
extern int edma_ddrq_ac_queue_wred_en;
extern int edma_ddrq_ac_queue_shared_ceiling;
extern int edma_ddrq_ac_queue_grp_id;
extern int edma_ddrq_ac_queue_pre_alloc;
extern int edma_ddrq_ac_queue_shared_dynamic;
extern int edma_ddrq_ac_queue_shared_weight;
extern int edma_ddrq_grp_ac_en;
extern int edma_ddrq_grp_color_aware;
extern int edma_ddrq_grp_shrd_limit;
extern int edma_ddrq_grp_dp_thrd;
extern int edma_ddrq_grp_id_bm;
extern int edma_ddrq_isq_base;
extern int edma_ddrq_lp_queue_base;
extern int edma_ddrq_lp_num_queues;
extern int edma_ddrq_lp_id;
extern int edma_ddrq_lp_fc_grp_id;
#endif

int edma_irq_init(void);
irqreturn_t edma_misc_handle_irq(int irq, void *ctx);
int32_t edma_misc_stats_alloc(void);
void edma_misc_stats_free(void);
void edma_enable_interrupts(struct edma_gbl_ctx *egc);
void edma_enable_rx_interrupts(struct edma_gbl_ctx *egc);
void edma_disable_interrupts(struct edma_gbl_ctx *egc);
void edma_disable_rx_interrupts(struct edma_gbl_ctx *egc);
void edma_configure_rps_hash_map(struct edma_gbl_ctx *egc);
int edma_hang_recovery_handler(struct ctl_table *table, int write, void __user *buffer, size_t *lenp, loff_t *ppos);
int edma_vlan_append_handler(struct ctl_table *table, int write, void __user *buffer, size_t *lenp, loff_t *ppos);
int edma_force_crash_handler(struct ctl_table *table, int write, void __user *buffer, size_t *lenp, loff_t *ppos);

/*
 * Forward declarations for custom param ops used in module_param_array
 */
static const struct kernel_param_ops param_ops_bp_stats_en_rxfill_ring_id;
static const struct kernel_param_ops param_ops_bp_stats_en_rxdesc_ring_id;
static const struct kernel_param_ops param_ops_bp_stats_en_txcmpl_ring_id;

/*
 * edma_reg_read()
 *	Read EDMA register
 */
static inline uint32_t edma_reg_read(uint32_t reg_off)
{
	return hal_read_reg(edma_gbl_ctx->reg_base, reg_off);
}

/*
 * edma_reg_write()
 *	Write EDMA register
 */
static inline void edma_reg_write(uint32_t reg_off, uint32_t val)
{
	hal_write_reg(edma_gbl_ctx->reg_base, reg_off, val);
}

/*
 * edma_update_ring_stats
 *	Update the ring util stats
 */
static inline int edma_update_ring_stats(uint32_t work_to_do, uint32_t max_desc,
					 struct edma_ring_util_stats *ring_util)
{
	int ring_usage;

	ring_usage = (100 * work_to_do)/max_desc;

	if (ring_usage == EDMA_RING_USAGE_100_PERCENTAGE) {
		ring_util->util[EDMA_RING_USAGE_100_FULL]++;
	} else if (ring_usage > EDMA_RING_USAGE_90_PERCENTAGE) {
		ring_util->util[EDMA_RING_USAGE_90_TO_100_FULL]++;
	} else if ((ring_usage > EDMA_RING_USAGE_70_PERCENTAGE) &&
		  (ring_usage <= EDMA_RING_USAGE_90_PERCENTAGE)) {
		ring_util->util[EDMA_RING_USAGE_70_TO_90_FULL]++;
	} else if ((ring_usage > EDMA_RING_USAGE_50_PERCENTAGE) &&
		  (ring_usage <= EDMA_RING_USAGE_70_PERCENTAGE)) {
		ring_util->util[EDMA_RING_USAGE_50_TO_70_FULL]++;
	} else {
		ring_util->util[EDMA_RING_USAGE_LESS_50_FULL]++;
	}

	return 0;
}

/*
 * edma_dp_stats_fetch_begin
 *	fetch dp 64-bit statistics begin
 */
static inline unsigned int edma_dp_stats_fetch_begin(const struct u64_stats_sync *syncp)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	return u64_stats_fetch_begin_irq(syncp);
#else
	return u64_stats_fetch_begin(syncp);
#endif
}

/*
 * edma_dp_stats_fetch_retry
 *	retry dp 64-bit statistics fetch
 */
static inline bool edma_dp_stats_fetch_retry(const struct u64_stats_sync *syncp,
					     unsigned int start)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	return u64_stats_fetch_retry_irq(syncp, start);
#else
	return u64_stats_fetch_retry(syncp, start);
#endif
}

/*
 * edma_dp_per_ring_reset_support
 *	Check if EDMA ring reset is supported or not.
 *	For now it is added for target IPQ54XX - this can further be
 *	used for other platforms in future.
 */
static inline bool edma_dp_per_ring_reset_support(void)
{
	bool ring_reset_en = false;
#if defined(NSS_DP_EDMA_RING_RESET)
	ring_reset_en = true;
#endif
	return ring_reset_en;
}

/*
 * edma_set_init_stage()
 *	Mark an initialization stage as complete
 */
static inline void edma_set_init_stage(enum edma_init_stage stage)
{
	if (edma_gbl_ctx && (stage < EDMA_INIT_STAGE_MAX)) {
		edma_gbl_ctx->hw_init_bitmap |= BIT(stage);
	}
}

/*
 * edma_set_clk_stage()
 *	Mark a clock initialization stage as complete
 */
static inline void edma_set_clk_stage(enum edma_clock_init_stage stage)
{
	if (edma_gbl_ctx && (stage < EDMA_CLK_STAGE_MAX)) {
		edma_gbl_ctx->clk_init_bitmap |= BIT(stage);
	}
}
#endif	/* __EDMA_H__ */
