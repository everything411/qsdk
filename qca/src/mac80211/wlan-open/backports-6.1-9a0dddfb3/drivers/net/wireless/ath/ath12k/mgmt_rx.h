/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_MGMT_RX_H
#define ATH12K_MGMT_RX_H

#include "hw.h"
#include "hal.h"
#include "debug.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ini.h"
#endif
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ath12k_cmn_extn.h"
#endif /* CPTCFG_QCN_EXTN */

struct ath12k_mgmt;

#define MGMT_RX_BUFFER_SIZE                2048

#define MGMT_REO_DST_RING_SIZE             1024
#define MGMT_REO_EXCEPTION_RING_SIZE       1024
#define MGMT_WBM_IDLE_BUF_RING_SIZE        2048
#define MGMT_REFILL_RING_SIZE              MGMT_WBM_IDLE_BUF_RING_SIZE

#define MGMT_RX_DESC_COUNT                 2048
#define MGMT_RX_DESC_BLOCK_SIZE            512
#define NUM_MGMT_RX_DESC_BLOCKS            (MGMT_RX_DESC_COUNT / MGMT_RX_DESC_BLOCK_SIZE)

/* Descriptor hierarchy:
 * +--------+        +-------+-------+-----+-------+
 * | Block0 | -----> | Slot0 | Slot1 | ... | SlotM |
 * +--------+        +-------+-------+-----+-------+
 * |  ....  |
 * +--------+        +-------+-------+-----+-------+
 * | BlockN | -----> | Slot0 | Slot1 | ... | SlotM |
 * +--------+        +-------+-------+-----+-------+
 *
 * Of the available 20-bit cookie, b0-b8 are used to identify the slot and b9-b19 are used
 * to identify the block.
 */
#define MGMT_RX_DESC_SLOT_MASK             GENMASK(8, 0)
#define MGMT_RX_DESC_BLOCK_MASK            GENMASK(19, 9)
#define MGMT_RX_DESC_COOKIE_SHIFT          9

#define ATH12K_MGMT_RX_DESC_MAGIC          0xABBAABBA

enum ath12k_mgmt_srng_pkt_type {
	ATH12K_MGMT_SRNG_PKT_TYPE_RX,
	ATH12K_MGMT_SRNG_PKT_TYPE_RX_ERR,

	/* Keep last */
	ATH12K_MGMT_SRNG_PKT_TYPE_MAX,
};

struct mgmt_srng {
	u32 *vaddr_unaligned;
	u32 *vaddr;
	dma_addr_t paddr_unaligned;
	dma_addr_t paddr;
	int size;
	u32 ring_id;
	u8 cached;
};

#define ATH12K_MGMT_IRQ_GRP_NUM_MAX 2
#define ATH12K_MGMT_IRQ_PER_GRP_NUM_MAX 2
#define ATH12K_MGMT_IRQ_GRP_ID_INVALID -1
#define MGMT_IRQ_NAME_LEN 20

struct ath12k_mgmt_arch_ops {
	int (*mgmt_op_device_init)(struct ath12k_mgmt *mgmt);
	void (*mgmt_op_device_deinit)(struct ath12k_mgmt *mgmt);
	int (*mgmt_op_htt_setup)(struct ath12k_mgmt *mgmt);
	int (*mgmt_op_dump_ring_stats)(struct ath12k_mgmt *mgmt, char *buf, int size);
	void (*mgmt_rx_replenish_buffs)(struct ath12k_mgmt *mgmt,
					struct list_head *used_list,
					bool in_use);
	bool (*mgmt_op_override_mld_tx)(struct ath12k_mgmt *mgmt);
};

struct ath12k_mgmt_irq_grp {
	int grp_id;
	struct ath12k_base *ab;
	char irq_name[MGMT_IRQ_NAME_LEN];
#if LINUX_VERSION_IS_GEQ(6, 13, 0)
	struct work_struct intr_wq;
	void (*irq_grp_handler)(struct work_struct *w);
#else
	struct tasklet_struct intr_tq;
	void (*irq_grp_handler)(struct tasklet_struct *t);
#endif
	int num_irq;
	int irqs[ATH12K_MGMT_IRQ_PER_GRP_NUM_MAX];
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_mgmt_irq_grp_extn irq_grp_extn;
#endif
};

#define ATH12K_SRNG_STATS_MGMT_FRM_STYPE_MAX 16
struct ath12k_device_mgmt_srng_stats {
	u32 invalid_pkts; /* non-mgmt pkts invalidly routed to mgmt srng */
	u32 invalid_push_pkts; /* pkts with invalid push reason */
	u32 rx_pkts[ATH12K_SRNG_STATS_MGMT_FRM_STYPE_MAX];
	u32 bar_pkts[ATH12K_MGMT_SRNG_PKT_TYPE_MAX]; /* BAR ctrl frames in mgmt srng */
	u32 err_ring_pkts;
	/* subset of err_ring_pkts */
	u32 rxdma_err[HAL_REO_ENTR_RING_RXDMA_ECODE_MAX];
	u32 reo_err[HAL_REO_DEST_RING_ERROR_CODE_MAX];
	u32 reo_err_rx[ATH12K_SRNG_STATS_MGMT_FRM_STYPE_MAX];
	u32 frag_pkts;

#ifdef CPTCFG_QCN_EXTN
	struct ath12k_device_mgmt_srng_stats_extn stats_extn;
#endif
};

struct ath12k_mgmt {
	struct ath12k_base *ab;
	struct device *dev;
	struct ath12k_hal *hal;
	struct ath12k_mgmt_arch_ops *arch_ops;
	const struct ath12k_hw_params *hw_params;
	struct ath12k_mgmt_irq_grp *irq_grp;
	u8 num_irq_grp;
	bool init_done;

	/* Rx buffer descriptors for mgmt */
	struct ath12k_rx_desc_info *rx_desc_baddr[NUM_MGMT_RX_DESC_BLOCKS];

	/* protects rx descriptors for arch-specific rx_refill_ring */
	spinlock_t rx_desc_lock;
	struct list_head rx_desc_free_list;

	struct ath12k_device_mgmt_srng_stats srng_stats;

	/* must be last */
	u8 arch_priv[] __aligned(sizeof(void *));
};

int ath12k_mgmt_device_init(struct ath12k_mgmt *mgmt);
void ath12k_mgmt_device_deinit(struct ath12k_mgmt *mgmt);

void ath12k_mgmt_irq_grp_setup(struct ath12k_mgmt *mgmt);
void ath12k_mgmt_irq_grp_cleanup(struct ath12k_mgmt *mgmt);
void ath12k_mgmt_irq_grp_enable(struct ath12k_mgmt_irq_grp *irq_grp);
void ath12k_mgmt_irq_grp_disable(struct ath12k_mgmt_irq_grp *irq_grp);

int ath12k_mgmt_srng_setup(struct ath12k_base *ab, struct mgmt_srng *ring,
			   enum hal_ring_type type, int ring_num,
			   int mac_id, int grp_id, int num_entries);
void ath12k_mgmt_srng_cleanup(struct ath12k_base *ab, struct mgmt_srng *ring);

static inline u32 ath12k_mgmt_gen_rx_desc_cookie(u16 block, u16 slot)
{
	return (u32)block << MGMT_RX_DESC_COOKIE_SHIFT | slot;
}

struct ath12k_rx_desc_info *ath12k_mgmt_get_rx_desc_from_cookie(struct ath12k_mgmt *mgmt,
								u32 cookie);
int ath12k_mgmt_rx_desc_init(struct ath12k_base *ab);
void ath12k_mgmt_rx_desc_cleanup(struct ath12k_base *ab);
size_t ath12k_mgmt_rx_desc_list_cut_nodes(struct list_head *used_list,
					  struct list_head *rx_desc_list,
					  size_t count);
size_t ath12k_mgmt_get_req_entries_from_refill_ring(struct ath12k_base *ab,
						    struct mgmt_srng *rx_refill_ring,
						    struct list_head *list);
struct sk_buff *ath12k_mgmt_rx_get_mmpdu_last_buf(struct sk_buff_head *mmpdu_list,
						  struct sk_buff *first);

int ath12k_mgmt_htt_setup(struct ath12k_hw_group *ag);

bool ath12k_mgmt_override_mld_tx(struct ath12k_base *ab);

static inline int ath12k_mgmt_arch_op_device_init(struct ath12k_mgmt *mgmt)
{
	if (!mgmt->arch_ops->mgmt_op_device_init)
		return -EOPNOTSUPP;

	return mgmt->arch_ops->mgmt_op_device_init(mgmt);
}

static inline void ath12k_mgmt_arch_op_device_deinit(struct ath12k_mgmt *mgmt)
{
	if (!mgmt->arch_ops->mgmt_op_device_deinit)
		return;

	mgmt->arch_ops->mgmt_op_device_deinit(mgmt);
}

static inline int ath12k_mgmt_arch_op_htt_setup(struct ath12k_mgmt *mgmt)
{
	if (!mgmt->arch_ops->mgmt_op_htt_setup)
		return -EOPNOTSUPP;

#ifdef CPTCFG_QCN_EXTN
	if (ath12k_cfg_get(mgmt->ab, ATH12K_CFG_REO_MGMT_PATH_DISABLE)) {
		ath12k_info(mgmt->ab,
			    "REO2SW management path is disabled, not configuring RDIs");
		return 0;
	}
#endif /* CPTCFG_QCN_EXTN */

	return mgmt->arch_ops->mgmt_op_htt_setup(mgmt);
}

static inline void ath12k_mgmt_rx_replenish_buffs(struct ath12k_mgmt *mgmt,
						  struct list_head *used_list,
						  bool reuse)
{
	if (mgmt->arch_ops->mgmt_rx_replenish_buffs)
		mgmt->arch_ops->mgmt_rx_replenish_buffs(mgmt, used_list, reuse);
}

static inline bool ath12k_mgmt_arch_op_override_mld_tx(struct ath12k_mgmt *mgmt)
{
	if (mgmt->arch_ops->mgmt_op_override_mld_tx)
		return mgmt->arch_ops->mgmt_op_override_mld_tx(mgmt);

	return true;
}

#endif
