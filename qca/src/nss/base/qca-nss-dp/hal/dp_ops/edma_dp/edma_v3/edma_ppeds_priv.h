/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __EDMA_PPEDS_PRIV__
#define __EDMA_PPEDS_PRIV__

#include "nss_dp_ppeds.h"
#include <linux/atomic.h>
#include <linux/bitops.h>

#define EDMA_PPEDS_RX_WEIGHT	1	/* PPE-DS Rx processing budget */
#define EDMA_PPEDS_SERVICE_STOP_BIT 0
#define EDMA_PPEDS_TXCOMP_NAPI_BIT 1
#define EDMA_PPEDS_MAX_RINGS_PER_NODE	2

#define EDMA_PPEDS_MAX_RX_RINGS	2
#define EDMA_PPEDS_MAX_TX_RINGS	2

#define EDMA_IRQ_NAME_SIZE		32

/*
 * Rx rings flow control threshold values
 *
 * The Rx flow control has the X-OFF and the X-ON threshold values.
 * Whenever the free Rx ring descriptor count falls below the X-OFF value, the
 * ring level flow control will kick in and the mapped PPE queues will be backpressured.
 * Similarly, whenever the free Rx ring descriptor count crosses the X-ON value,
 * the ring level flow control will be disabled.
 */
#define EDMA_PPEDS_RX_FC_XOFF_DEF	32
#define EDMA_PPEDS_RX_FC_XON_DEF	64

/*
 * EDMA PPE-DS node entry index definitions
 */
enum {
	EDMA_PPEDS_ENTRY_RXFILL_IDX,
	EDMA_PPEDS_ENTRY_TXCMPL_IDX,
	EDMA_PPEDS_ENTRY_RX_IDX,
	EDMA_PPEDS_ENTRY_TX_IDX,
	EDMA_PPEDS_ENTRY_QID_START_IDX,
	EDMA_PPEDS_ENTRY_QID_COUNT_IDX,
	EDMA_PPEDS_NUM_ENTRY
};

/*
 * edma_ppeds_node_state_t
 *	PPE-DS node state
 */
typedef enum {
	EDMA_PPEDS_NODE_STATE_NOT_AVAIL,
	EDMA_PPEDS_NODE_STATE_AVAIL,
	EDMA_PPEDS_NODE_STATE_ALLOC,
	EDMA_PPEDS_NODE_STATE_REG_IN_PROG,
	EDMA_PPEDS_NODE_STATE_REG_DONE,
	EDMA_PPEDS_NODE_STATE_START_IN_PROG,
	EDMA_PPEDS_NODE_STATE_START_DONE,
	EDMA_PPEDS_NODE_STATE_STOP_IN_PROG,
	EDMA_PPEDS_NODE_STATE_STOP_DONE,
	EDMA_PPEDS_NODE_STATE_FREE_IN_PROG,
} edma_ppeds_node_state_t;

/*
 * PPE-DS Interrupt's entry index in the node configuration
 */
enum {
	EDMA_PPEDS_TXCOMP_IRQ_IDX,
	EDMA_PPEDS_RXDESC_IRQ_IDX,
	EDMA_PPEDS_RXFILL_IRQ_IDX,
	EDMA_PPEDS_IRQ_NUM
};

struct edma_ppeds_hw_buf_mgmt {
	struct edma_rxfill_ring rxfill_ring;	/* PPE-DS EDMA Rxfill ring */
	struct edma_txcmpl_ring txcmpl_ring;	/* PPE-DS EDMA Tx complete ring */
};

/*
 * edma_ppeds_node_wifi8
 *	ppeds wifi8 node information.
 */
struct edma_ppeds_node_wifi8 {
	struct edma_rxfill_ring rxfill_ring;	/* PPE-DS EDMA Rxfill ring */
	struct edma_txcmpl_ring txcmpl_ring;	/* PPE-DS EDMA Tx complete ring */
	struct edma_ppeds_hw_buf_mgmt hw_buf_mgmt;
	struct edma_rxdesc_ring rx_ring[EDMA_PPEDS_MAX_RINGS_PER_NODE];	/* PPE-DS EDMA Rx ring */
	struct edma_txdesc_ring tx_ring[EDMA_PPEDS_MAX_RINGS_PER_NODE];	/* PPE-DS EDMA Tx ring */
	uint32_t ppe_qid[EDMA_PPEDS_MAX_RINGS_PER_NODE];			/* PPE-DS node start queue id */
	uint32_t ppe_num_queues[EDMA_PPEDS_MAX_RINGS_PER_NODE];		/* PPE-DS node queue count */
	uint32_t txcmpl_intr;			/* PPE-DS EDMA Tx complete IRQ */
	uint32_t rxfill_intr;			/* PPE-DS EDMA Rxfill IRQ */
	uint32_t rxdesc_intr[EDMA_PPEDS_MAX_RINGS_PER_NODE];			/* PPE-DS EDMA Rx IRQ */
	char txcmpl_irq_name[EDMA_IRQ_NAME_SIZE];
	char rxfill_irq_name[EDMA_IRQ_NAME_SIZE];
	char rxdesc_irq_name[EDMA_IRQ_NAME_SIZE];
};

/*
 * edma_ppeds_node_wifi7
 *	ppeds wifi7 node information.
 */
struct edma_ppeds_node_wifi7 {
	struct edma_rxfill_ring rxfill_ring;	/* PPE-DS EDMA Rxfill ring */
	struct edma_txcmpl_ring txcmpl_ring;	/* PPE-DS EDMA Tx complete ring */
	struct edma_rxdesc_ring rx_ring;	/* PPE-DS EDMA Rx ring */
	struct edma_txdesc_ring tx_ring;	/* PPE-DS EDMA Tx ring */
	uint32_t ppe_qid;			/* PPE-DS node start queue id */
	uint32_t ppe_num_queues;		/* PPE-DS node queue count */
	uint32_t txcmpl_intr;			/* PPE-DS EDMA Tx complete IRQ */
	uint32_t rxfill_intr;			/* PPE-DS EDMA Rxfill IRQ */
	uint32_t rxdesc_intr;			/* PPE-DS EDMA Rx IRQ */
	char txcmpl_irq_name[EDMA_IRQ_NAME_SIZE];
	char rxfill_irq_name[EDMA_IRQ_NAME_SIZE];
	char rxdesc_irq_name[EDMA_IRQ_NAME_SIZE];
};

/*
 * PPE-DS EDMA node descriptor
 */
struct edma_ppeds {
	const struct nss_dp_ppeds_cb *ops;	/* PPE-DS EDMA callback pointer */
	struct net_device napi_ndev;		/* Dummy net_device for NAPI */
	enum edma_ppeds_wifi_arch_mode wifi_arch_mode;	/* WiFi architecture mode */
	union {
		struct edma_ppeds_node_wifi7 wifi7_cfg;
		struct edma_ppeds_node_wifi8 wifi8_cfg;
	};
	uint32_t umac_reset_inprogress;		/* Umac reset progress status */
	unsigned long service_running;		/* PPE-DS ring usage service status */
	uint8_t db_idx;				/* PPE-DS node index */
	nss_dp_ppeds_handle_t ppeds_handle;	/* PPE-DS handle */
};

/*
 * edma_ppeds_node_cfg
 *	EDMA PPE-DS node configuration structure
 */
struct edma_ppeds_node_cfg {
	uint32_t node_map[EDMA_PPEDS_NUM_ENTRY];
					/* PPE-DS node configuration mapping */
	uint32_t irq_map[EDMA_PPEDS_IRQ_NUM];
					/* PPE-DS node IRQ mapping */
	edma_ppeds_node_state_t node_state;
					/* PPE-DS node state information */
	struct edma_ppeds *ppeds_db;	/* PPE-DS EDMA node pointer */
};

/*
 * edma_ppeds_drv
 *	PPE-DS EDMA configuration descriptor
 */
struct edma_ppeds_drv {
	uint32_t num_nodes;			/* Number of PPE-DS nodes */
	struct edma_ppeds_node_cfg ppeds_node_cfg[EDMA_PPEDS_MAX_NODES];
						/* PPE-DS node configuration information */
	rwlock_t lock;			/* Lock for accessing the PPE-DS node information */
};

int edma_ppeds_init(struct edma_ppeds_drv* drv);
void edma_ppeds_deinit(struct edma_ppeds_drv *drv);
int edma_ppeds_rx_fill_ring_alloc(struct edma_rxfill_ring *rxfill_ring, bool hw_buf_mgmt);
void edma_ppeds_rx_fill_ring_free(struct edma_rxfill_ring *rxfill_ring);
int edma_ppeds_tx_cmpl_ring_alloc(struct edma_txcmpl_ring *txcmpl_ring, bool hw_buf_mgmt);
void edma_ppeds_tx_cmpl_ring_free(struct edma_txcmpl_ring *txcmpl_ring);
int edma_ppeds_tx_secondary_alloc(struct edma_txdesc_ring *txdesc_ring);
void edma_ppeds_service_status_update(nss_dp_ppeds_handle_t *ppeds_handle, bool enable);
void edma_ppeds_reset_gbl_ds_ctx(void);

#ifdef NSS_DP_PON_SUPPORT
void edma_ppeds_pon_pq_disable(void);
void edma_ppeds_pon_pq_enable(void);
#endif /* NSS_DP_PON_SUPPORT */

extern void *edma_ppeds_tx_ring_sec_mem;
extern int edma_ppeds_tx_ring_entries;
#endif	/* __EDMA_PPEDS_PRIV__ */
