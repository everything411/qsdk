/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __EDMA_DDRQ_H__
#define __EDMA_DDRQ_H__

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/irq.h>
#include <linux/reset.h>
#include <linux/skbuff.h>

#include "edma_regs.h"
#include "edma_debug.h"
#include "edma_debugfs.h"
#include "edma_procfs.h"
#include "nss_dp_ddrq.h"

/*
 * edma_reg_read()/edma_reg_write() are defined in edma.h.
 * Keep forward declarations here because edma.h includes this header
 * before those inline definitions.
 */
static inline uint32_t edma_reg_read(uint32_t reg_off);
static inline void edma_reg_write(uint32_t reg_off, uint32_t val);

#define EDMA_DDRQ_NO_OP_DEF_VAL		-1
#define EDMA_DDRQ_DATA_OFFSET_DEF	0
#define EDMA_DDRQ_GBL_EN_HW_DEF		1
#define EDMA_DDRQ_GBL_EN_SW_DEF		1
#define EDMA_DDRQ_GBL_PF_THRES		4
#define EDMA_DDRQ_GBL_WB_THRES		8
#define EDMA_DDRQ_GBL_DATA_OFFSET_REG0_VAL	0x20
#define EDMA_DDRQ_GBL_DATA_OFFSET_REG1_VAL	0
#define EDMA_DDRQ_GBL_DATA_OFFSET_REG2_VAL	0x82
#define EDMA_DDRQ_GBL_DATA_OFFSET_REG3_VAL	0x9a

#define EDMA_DDRQ_AC_Q_AC_EN_DEF	1
#define EDMA_DDRQ_AC_Q_COLOR_AWARE_DEF	0
#define EDMA_DDRQ_AC_Q_WRED_EN_DEF	0
#define EDMA_DDRQ_AC_Q_GRP_ID_DEF	0
#define EDMA_DDRQ_AC_Q_PRE_ALLOC_DEF	2
#define EDMA_DDRQ_AC_Q_SHRD_DYNAMIC	1
#define EDMA_DDRQ_AC_Q_SHRD_WEIGHT	7

#define EDMA_DDRQ_AC_GRP_AC_EN_DEF	0
#define EDMA_DDRQ_AC_GRP_COLOR_AWARE_DEF	0
#define EDMA_DDRQ_AC_GRP_ID_BM_DEF	1

#define EDMA_DDRQ_ISQ_BASE		238
#define EDMA_DDRQ_ISQ_NUM		8

#define EDMA_DDRQ_LP_QUEUE_BASE_DEF	246
#define EDMA_DDRQ_LP_NUM_QUEUES_DEF	8
#define EDMA_DDRQ_LP_ID			1
#define EDMA_DDRQ_LP_FC_GRP_ID		16

#define EDMA_DDRQ_BLK_NUM_MAX_ARR_IDX	3
#define EDMA_DDRQ_BLK_SIZE_MAX_ARR_IDX	4

#define EDMA_DDRQ_ESRAMQ_CNT_PER_PORT		8
#define EDMA_DDRQ_MAX_CNT_WORD		(NSS_DP_DDRQ_MAX_CNT / 32)
#define EDMA_DDRQ_DATA_REGION		"ddrq_data_region"
#define EDMA_DDRQ_DESC_REGION		"ddrq_desc_region"

#define OFFSET_IN_WORD(n)	(32 - ((n) % 32))
#define OFFSET_IN_DOUBLE_WORD(n)	(64 - ((n) % 64))

#define EDMA_DDRQ_CFG_BASE_INDEX		16

#define EDMA_DDRQ_AC_QUEUE_STATE_ENABLED	1

#define EDMA_DDRQ_RING_BASE_INDEX		48
#define EDMA_DDRQ_RESET_RING_INDEX		0

#define EDMA_DDRQ_AC_CFG_GAP_GRN_RED_MIN_OFFSET	94
#define EDMA_DDRQ_AC_CFG_GAP_GRN_YEL_MAX_OFFSET	58
#define EDMA_DDRQ_AC_CFG_SHARED_CEILING_OFFSET	21
#define EDMA_DDRQ_AC_CFG_YEL_RESUME_OFFSET	118

#define EDMA_DDRQ_AC_GRP_GRN_RESUME_OFFSET	25
#define EDMA_DDRQ_AC_GRP_RED_RESUME_OFFSET	62

#define EDMA_DDRQ_AC_GRP_GAP_SHRD_LMT_DRP_THRES_VAL	256

#define EDMA_DDRQ_DBG_CNT_OCC_THRESHOLD_OFFSET	1

#define EDMA_DDRQ_DBG_CNT_OCC_STATS_PEAK_BYTES_OFFSET	32
#define EDMA_DDRQ_DBG_CNT_OCC_STATS_BYTES_OFFSET_1	5
#define EDMA_DDRQ_DBG_CNT_OCC_STATS_BYTES_OFFSET_2	37

#define EDMA_LP_RING_ID_BASE			32

#define EDMA_DDRQ_GRP_SHARED_LIMIT_MIN		32
#define EDMA_DDRQ_QUEUE_PRE_ALLOC_LIMIT_MIN	2
#define EDMA_DDRQ_GET_SHARED_LIMIT(blk_num, prealloc_limit) \
	((blk_num)-(EDMA_DDRQ_GRP_SHARED_LIMIT_MIN + (prealloc_limit * NSS_DP_DDRQ_MAX_CNT)))
#define EDMA_DDRQ_GET_SHARED_CEILING(blk_num)	(((blk_num) * 80) / 100)
#define EDMA_DDRQ_ENQ_DIS_REG_OFFSET_LIST \
	EDMA_REG_DDRQ_ENQ_DIS_REG0_OFFSET, \
	EDMA_REG_DDRQ_ENQ_DIS_REG1_OFFSET, \
	EDMA_REG_DDRQ_ENQ_DIS_REG2_OFFSET, \
	EDMA_REG_DDRQ_ENQ_DIS_REG3_OFFSET, \
	EDMA_REG_DDRQ_ENQ_DIS_REG4_OFFSET, \
	EDMA_REG_DDRQ_ENQ_DIS_REG5_OFFSET

#define EDMA_DDRQ_DEQ_DROP_REG_OFFSET_LIST \
	EDMA_REG_DDRQ_DEQ_DROP_REG0_OFFSET, \
	EDMA_REG_DDRQ_DEQ_DROP_REG1_OFFSET, \
	EDMA_REG_DDRQ_DEQ_DROP_REG2_OFFSET, \
	EDMA_REG_DDRQ_DEQ_DROP_REG3_OFFSET, \
	EDMA_REG_DDRQ_DEQ_DROP_REG4_OFFSET, \
	EDMA_REG_DDRQ_DEQ_DROP_REG5_OFFSET

/*
 * The loop count maximum value, which the software uses to wait for the
 * EDMA HW to consume all the packets in the DDRQs for the particular Tx ring.
 */
#define EDMA_DDRQ_HW_CONSUME_LOOP_CNT		100

/*
 * edma_ddrq_ac_queue_cfg_tbl_t
 *	DDRQ AC queue configuration structure
 */
typedef struct {
	uint32_t  ac_cfg_ac_en:1;			/* Admission control knob */
	uint32_t  ac_cfg_wred_en:1;			/* WRED enable knob */
	uint32_t  ac_cfg_bp_en:1;			/* Backpressure knob */
	uint32_t  ac_cfg_grp_id:2;			/* Group id */
	uint32_t  ac_cfg_pre_alloc_limit:12;		/* Pre-alloc limit */
	uint32_t  ac_cfg_shared_dynamic:1;		/* Shared dynamic */
	uint32_t  ac_cfg_shared_weight:3;		/* Shared weight */
	uint32_t  ac_cfg_shared_ceiling_0:11;		/* Shared ceiling */
	uint32_t  ac_cfg_shared_ceiling_1:1;		/* Shared ceiling */
	uint32_t  ac_cfg_gap_grn_grn_min:12;		/* Gap between green max and green min */
	uint32_t  ac_cfg_grn_resume_offset:12;		/* Green resume offset */
	uint32_t  ac_cfg_color_aware:1;			/* Color aware knob */
	uint32_t  ac_cfg_gap_grn_yel_max_0:6;		/* Gap between green max and yellow max */
	uint32_t  ac_cfg_gap_grn_yel_max_1:6;		/* Gap between green max and yellow max */
	uint32_t  ac_cfg_gap_grn_yel_min:12;		/* Gap between green max and yellow min */
	uint32_t  ac_cfg_gap_grn_red_max:12;		/* Gap between green max and red max */
	uint32_t  ac_cfg_gap_grn_red_min_0:2;		/* Gap between green max and red min */
	uint32_t  ac_cfg_gap_grn_red_min_1:10;		/* Gap between green max and red min */
	uint32_t  ac_cfg_red_resume_offset:12;		/* Red resume offset */
	uint32_t  ac_cfg_yel_resume_offset_0:10;	/* Yellow resume offset */
	uint32_t  ac_cfg_yel_resume_offset_1:2;		/* Yellow resume offset */
	uint32_t  ac_cfg_ecn_mark_en:1;			/* ECN mark enable knob */
	uint32_t  _reserved:29;				/* Reserved */
} edma_ddrq_ac_queue_cfg_tbl_t;

/*
 * edma_ddrq_ac_queue_cfg_tbl_u
 *	DDRQ AC queue configuration union
 */
typedef union {
	edma_ddrq_ac_queue_cfg_tbl_t ddrq_cfg;		/* DDRQ AC queue configuration */
	uint32_t val[5];				/* Helper for hardware read/write of configuration */
} edma_ddrq_ac_queue_cfg_tbl_u;


/*
 * edma_ddrq_ac_grp_cfg_tbl_t
 *	DDRQ group configuration structure
 */
typedef struct {
	uint32_t  ac_cfg_ac_en:1;			/* Admission control knob */
	uint32_t  ac_grp_dp_thrd:12;			/* Drop threshold */
	uint32_t  ac_grp_gap_shrd_limit:12;		/* Shared limit */
	uint32_t  ac_grp_grn_resume_offset_0:7;		/* Green resume offset */
	uint32_t  ac_grp_grn_resume_offset_1:5;		/* Green resume offset */
	uint32_t  ac_cfg_color_aware:1;			/* Color aware knob */
	uint32_t  ac_grp_gap_grn_red:12;		/* Gap between green and red */
	uint32_t  ac_grp_gap_grn_yel:12;		/* Gap between green and yellow */
	uint32_t  ac_grp_red_resume_offset_0:2;		/* Red resume offset */
	uint32_t  ac_grp_red_resume_offset_1:10;	/* Red resume offset */
	uint32_t  ac_grp_yel_resume_offset:12;		/* Yellow resume offset */
	uint32_t  _reserved:10;				/* Reserved */
} edma_ddrq_ac_grp_cfg_tbl_t;

/*
 * edma_ddrq_ac_grp_cfg_tbl_u
 *	DDRQ group configuration union
 */
typedef union {
	uint32_t val[3];				/* Helper for hardware read/write of configuration */
	edma_ddrq_ac_grp_cfg_tbl_t ddrq_grp_cfg;	/* DDRQ group configuration */
} edma_ddrq_ac_grp_cfg_tbl_u;

/*
 * edma_ddrq_gbl_cfg_t
 *	DDRQ default global configuration structure
 */
typedef struct {
	int32_t ddrq_desc_pf_thres;			/* Prefetch threshold */
	int32_t ddrq_pkt_data_align;			/* Data align */
	int32_t ddrq_data_offset;			/* Offset for data write */
	int32_t ddrq_blk_num_cfg;			/* DDRQ block number */
	int32_t ddrq_blk_size_cfg;			/* Size of one DDRQ block */
	int32_t ddrq_desc_wb_thres;			/* Writeback threshold */
	int32_t ddrq_data_offset0;			/* Data offset to be used in Tx descriptor */
	int8_t ddrq_en_hw;				/* Hardware DDRQ feature enable/disable knob */
	int8_t ddrq_en_sw;				/* Software DDRQ feature enable/disable knob */
} edma_ddrq_gbl_cfg_t;

/*
 * edma_ddrq_idv_cfg_t
 *	DDRQ default AC queue configuration structure
 */
typedef struct {
	nss_dp_ddrq_ac_queue_cfg_tbl_t ddrq_ac_cfg;	/* DDRQ default individual configuration */
	uint32_t ddrq_en_port_bm;			/* Port based DDRQ state bitmask */
} edma_ddrq_idv_cfg_t;

/*
 * edma_ddrq_grp_cfg_t
 *	DDRQ default group configuration structure
 */
typedef struct {
	nss_dp_ddrq_ac_grp_cfg_tbl_t ddrq_grp_cfg;	/* DDRQ default group configuration */
	uint32_t ddrq_grp_en_bm;			/* DDRQ group enable/disable bitmask */
} edma_ddrq_grp_cfg_t;

/*
 * edma_ddrq_mem_reg_t
 *	DDRQ memory region structure
 */
typedef struct {
	phys_addr_t phy_addr;				/* Region base address */
	size_t size;					/* Region size */
} edma_ddrq_mem_reg_t;

/*
 *edma_ddrq_lp_cfg_t
 *	DDRQ loopback configuration structure
 */
typedef struct {
	uint32_t lp_id;					/* Loopback ring id */
	uint32_t queue_base;				/* loopback ring base queue */
	uint32_t num_queues;				/* Number of queues to be mapped */
	uint16_t lp_fc_grp_id;				/* FC group id */
} edma_ddrq_lp_cfg_t;

/*
 * edma_ddrq_cfg_t
 *	DDRQ default configuration's parent structure
 */
typedef struct {
	edma_ddrq_mem_reg_t ddrq_desc_mem_reg;		/* Default DDRQ descriptor region configuration */
	edma_ddrq_mem_reg_t ddrq_data_mem_reg;		/* Default DDRQ data region configuration */
	edma_ddrq_gbl_cfg_t ddrq_gbl_cfg;		/* Default global DDRQ configuration */
	edma_ddrq_idv_cfg_t ddrq_idv_cfg;		/* Default individual DDRQ configuration */
	edma_ddrq_grp_cfg_t ddrq_grp_cfg;		/* Default DDRQ group configuration */
	edma_ddrq_lp_cfg_t ddrq_lp_cfg;			/* Default Loopback ring configuration */
} edma_ddrq_cfg_t;

/*
 * edma_ddrq_occupancy_threshold_t
 *	DDRQ occupancy threshold configuration structure
 */
typedef struct {
	uint32_t  threshold_type:1;			/* Threshold type */
	uint32_t  threshold_val_0:31;			/* Threshold value */
	uint32_t  threshold_val_1:7;			/* Threshold value */
	uint32_t _reserved:26;				/* Reserved */
} edma_ddrq_occupancy_threshold_t;

/*
 * edma_ddrq_occupancy_threshold_u
 *	DDRQ occupancy threshold configuration union
 */
typedef union {
	edma_ddrq_occupancy_threshold_t ddrq_occ_thres_cfg;	/* DDRQ occupancy threshold configuration */
	uint32_t val[2];					/* Helper for hardware read/write of configuration */
} edma_ddrq_occupancy_threshold_u;

/*
 * edma_ddrq_occupancy_stats_t
 *	DDRQ occupancy statistics structure
 */
typedef struct {
	uint32_t  peak_bytes_0:32;				/* Peak bytes count */
	uint32_t  peak_bytes_1:6;				/* Peak byte count */
	uint32_t  peak_pkts:21;					/* Peak packet count */
	uint32_t  bytes_0:5;					/* Byte count */
	uint32_t  bytes_1:32;					/* Byte count */
	uint32_t  bytes_2:1;					/* Byte count */
	uint32_t  pkts:21;					/* Packet count */
	uint32_t _reserved:10;					/* Reserved */
} edma_ddrq_occupancy_stats_t;

/*
 * edma_ddrq_occupancy_stats_u
 *	DDRQ occupancy statistics union
 */
typedef union {
	edma_ddrq_occupancy_stats_t ddrq_occ_stats;		/* Occupancy stat configuration */
	uint32_t val[4];					/* Helper for hardware read/write of configuration */
} edma_ddrq_occupancy_stats_u;

/*
 * edma_ddrq_regs_set_reset()
 *	Set or reset all registers in a register-set table.
 */
static inline void edma_ddrq_regs_set_reset(const uint32_t *reg_offsets, uint32_t val)
{
	uint32_t i;

	for (i = 0; i < EDMA_REG_DDRQ_ENQ_DIS_AND_DEQ_DRP_OFF_CNT; i++) {
		edma_reg_write(reg_offsets[i], val);
		edma_debug("reg offset: %d, data: 0x%0x\n",
				reg_offsets[i], edma_reg_read(reg_offsets[i]));
	}
}

/*
 * edma_ddrq_enq_dis_reg_offsets_get()
 *	Get EDMA enqueue-disable register offsets.
 */
static inline const uint32_t *edma_ddrq_enq_dis_reg_offsets_get(void)
{
	static const uint32_t reg_offsets[] = {EDMA_DDRQ_ENQ_DIS_REG_OFFSET_LIST};

	return reg_offsets;
}

/*
 * edma_ddrq_deq_drop_reg_offsets_get()
 *	Get EDMA dequeue-drop register offsets.
 */
static inline const uint32_t *edma_ddrq_deq_drop_reg_offsets_get(void)
{
	static const uint32_t reg_offsets[] = {EDMA_DDRQ_DEQ_DROP_REG_OFFSET_LIST};

	return reg_offsets;
}

/*
 * edma_ddrq_enqueue_disable_all()
 *	Disable the enqueue of all the DDRQs.
 */
static inline void edma_ddrq_enqueue_disable_all(void)
{
	edma_ddrq_regs_set_reset(edma_ddrq_enq_dis_reg_offsets_get(), EDMA_REG_DDRQ_ENQ_DIS_SET_ALL);
}

/*
 * edma_ddrq_enqueue_enable_all()
 *	Enable the enqueue of all the DDRQs
 */
static inline void edma_ddrq_enqueue_enable_all(void)
{
	edma_ddrq_regs_set_reset(edma_ddrq_enq_dis_reg_offsets_get(), EDMA_REG_DDRQ_ENQ_DIS_RESET_ALL);
}

/*
 * edma_ddrq_dequeue_drop_enable_all()
 *	Enable dequeue drop on all the DDRQs.
 */
static inline void edma_ddrq_dequeue_drop_enable_all(void)
{
	edma_ddrq_regs_set_reset(edma_ddrq_deq_drop_reg_offsets_get(), EDMA_REG_DDRQ_DEQ_DROP_SET_ALL);
}

/*
 * edma_ddrq_dequeue_drop_disable_all()
 *	Disable dequeue drop on all the DDRQs.
 */
static inline void edma_ddrq_dequeue_drop_disable_all(void)
{
	edma_ddrq_regs_set_reset(edma_ddrq_deq_drop_reg_offsets_get(), EDMA_REG_DDRQ_DEQ_DROP_RESET_ALL);
}

nss_dp_ddrq_ret_t edma_ddrq_ppe_ports_queue_profile_set(void);
nss_dp_ddrq_ret_t edma_ddrq_cfg_get(nss_dp_ddrq_obj_id_t *obj, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg, uint32_t count);
nss_dp_ddrq_ret_t edma_ddrq_cfg_set(nss_dp_ddrq_obj_id_t *obj, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg);
nss_dp_ddrq_ret_t edma_ddrq_grp_cfg_get(uint32_t ddrq_grp_id, nss_dp_ddrq_ac_grp_cfg_tbl_t *ddrq_grp_cfg);
nss_dp_ddrq_ret_t edma_ddrq_grp_cfg_set(uint32_t ddrq_grp_id, nss_dp_ddrq_ac_grp_cfg_tbl_t *ddrq_grp_cfg);
nss_dp_ddrq_ret_t edma_ddrq_enqueue_disable(nss_dp_ddrq_obj_id_t *obj, bool disable);
nss_dp_ddrq_ret_t edma_ddrq_dequeue_drop(nss_dp_ddrq_obj_id_t *obj, bool drop);
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_reset(void);
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_start(void);
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_stop(void);
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_restart(void);
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_threshold_get(uint32_t ddrq_id, nss_dp_ddrq_occupancy_threshold_t *threshold);
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_threshold_set(uint32_t ddrq_id, nss_dp_ddrq_occupancy_threshold_t *threshold);
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_get(uint32_t ddrq_id, nss_dp_ddrq_occupancy_stats_t *ddrq_stats);
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_status_get(uint32_t ddrq_id, bool *status);
int edma_ddrq_init(edma_ddrq_cfg_t *ddrq_cfg);
int32_t edma_ddrq_dp_dev_set(struct net_device *dev);
#endif		/* __EDMA_DDRQ_H__ */
