/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/irq.h>
#include <linux/reset.h>
#include <linux/skbuff.h>
#include <ppe_drv_cc.h>
#include <fal/fal_qm.h>
#include <fal/fal_type.h>
#include <fal/fal_servcode.h>
#include <fal/fal_pon.h>
#include <ppe_drv.h>
#include "edma.h"
#include "edma_regs.h"
#include "edma_debug.h"
#include "nss_dp_dev.h"
#include "edma_debugfs.h"
#include "edma_procfs.h"
#include "edma_ddrq.h"
#include "nss_dp_dev.h"

/*
 * DDRQ memory region's block wise size map
 */
int edma_ddrq_blk_num_map[EDMA_DDRQ_BLK_NUM_MAX_ARR_IDX] = {1024, 2048, 4096};
int edma_ddrq_blk_size_map[EDMA_DDRQ_BLK_SIZE_MAX_ARR_IDX] = {4096, 8192, 16384, 32768};

/*
 * edma_ddrq_reg_tbl_get()
 *	API to get DDRQ AC queue configuration table's value from the hardware
 */
static int32_t edma_ddrq_reg_tbl_get(uint32_t reg_addr_idx, uint32_t *val, uint32_t num)
{
	uint32_t i = 0;

	for(i = 0; i < num; i++) {
		val[i] = edma_reg_read((EDMA_REG_DDRQ_AC_QUEUE_CFG_TBL(reg_addr_idx) + i * 4));
	}

	return 0;
}

/*
 * edma_ddrq_reg_tbl_set()
 *	API to set DDRQ AC queue configuration table's value to the hardware
 */
static int32_t edma_ddrq_reg_tbl_set(uint32_t reg_addr_idx, uint32_t *val, uint32_t num)
{
	uint32_t i = 0;
	for(i = 0; i < num; i++) {
		edma_reg_write((EDMA_REG_DDRQ_AC_QUEUE_CFG_TBL(reg_addr_idx) + i * 4), val[i]);
	}

	return 0;
}

/*
 * edma_ddrq_reg_grp_tbl_get()
 *	API to get DDRQ AC table configuration values from the hardware
 */
static int32_t edma_ddrq_reg_grp_tbl_get(uint32_t reg_addr_idx, uint32_t *val, uint32_t num)
{
	uint32_t i = 0;

	if (num > 3) {
		edma_err("invalid num (%d) for ddrq grp tbl get cfg\n", num);
		return -1;
	}
	for(i = 0; i < num; i++) {
		val[i] = edma_reg_read((EDMA_REG_DDRQ_AC_GRP_CFG_TBL(reg_addr_idx) + i * 4));
	}

	return 0;
}

/*
 * edma_ddrq_reg_grp_tbl_set()
 *	API to set DDRQ AC table configuration values to the hardware
 */
static int32_t edma_ddrq_reg_grp_tbl_set(uint32_t reg_addr_idx, uint32_t *val, uint32_t num)
{
	uint32_t i = 0;

	if (num > 3) {
		edma_err("invalid num (%d) for ddrq grp tbl set cfg\n", num);
		return -1;
	}
	for(i = 0; i < num; i++) {
		edma_reg_write((EDMA_REG_DDRQ_AC_GRP_CFG_TBL(reg_addr_idx) + i * 4), val[i]);
	}

	return 0;
}

/*
 * edma_ddrq_reg_dbg_cnt_threshold_get()
 *	API to get DDRQ threshold configuration from hardware
 */
static int32_t edma_ddrq_reg_dbg_cnt_threshold_get(uint32_t reg_addr_idx, uint32_t *val, uint32_t num)
{
	uint32_t i = 0;

	for(i = 0; i < num; i++) {
		val[i] = edma_reg_read((EDMA_REG_DDRQ_DBG_CNT_THRESHOLD_OFFSET(reg_addr_idx) + i * 4));
	}

	return 0;
}

/*
 * edma_ddrq_reg_dbg_cnt_threshold_set()
 *	API to set DDRQ threshold configuration from hardware
 */
static int32_t edma_ddrq_reg_dbg_cnt_threshold_set(uint32_t reg_addr_idx, uint32_t *val, uint32_t num)
{
	uint32_t i = 0;
	for(i = 0; i < num; i++) {
		edma_reg_write((EDMA_REG_DDRQ_DBG_CNT_THRESHOLD_OFFSET(reg_addr_idx) + i * 4), val[i]);
	}

	return 0;
}

/*
 * edma_ddrq_reg_dbg_cnt_occ_stats_get()
 *	API to get DDRQ occupancy statistics from the hardware
 */
static int32_t edma_ddrq_reg_dbg_cnt_occ_stats_get(uint32_t reg_addr_idx, uint32_t *val, uint32_t num)
{
	uint32_t i = 0;

	for(i = 0; i < num; i++) {
		val[i] = edma_reg_read((EDMA_REG_DDRQ_DBG_CNT_STATS_OFFSET(reg_addr_idx) + i * 4));
	}

	return 0;
}

/*
 * edma_ddrq_ac_queue_cfg_dump_local()
 *	API to dump local DDRQ AC queue configurations
 */
static void edma_ddrq_ac_queue_cfg_dump_local(edma_ddrq_ac_queue_cfg_tbl_t *cfg)
{
	printk(KERN_DEBUG"%s: DDRQ AC queue configuration get local values:\n", __func__);
	printk(KERN_DEBUG"\t\t ac_en: 0x%0x, wred_en: 0x%0x, bp_en: 0x%0x, grp_id: 0x%0x, pre_alloc_limit: 0x%0x"
			" shared dynamic: 0x%0x, shared weight: 0x%0x, shared ceiling_0: 0x%0x"
			" shared_ceiling_1: 0x%0x, grn_min: 0x%0x, grn res off: 0x%0x, color aware: 0x%0x"
			" yel_max_0: 0x%0x, yel_max_1: 0x%0x, yel_min: 0x%0x, red_max: 0x%0x, red_min_0: 0x%0x,"
			" red_min_1: 0x%0x, red res off: 0x%0x, yel res off_0: 0x%0x, yel res off_1: 0x%0x"
			" ecn mark: 0x%0x\n", cfg->ac_cfg_ac_en, cfg->ac_cfg_wred_en,
			cfg->ac_cfg_bp_en, cfg->ac_cfg_grp_id, cfg->ac_cfg_pre_alloc_limit,
			cfg->ac_cfg_shared_dynamic, cfg->ac_cfg_shared_weight,
			cfg->ac_cfg_shared_ceiling_0, cfg->ac_cfg_shared_ceiling_1,
			cfg->ac_cfg_gap_grn_grn_min, cfg->ac_cfg_grn_resume_offset,
			cfg->ac_cfg_color_aware, cfg->ac_cfg_gap_grn_yel_max_0,
			cfg->ac_cfg_gap_grn_yel_max_1, cfg->ac_cfg_gap_grn_yel_min,
			cfg->ac_cfg_gap_grn_red_max, cfg->ac_cfg_gap_grn_red_min_0,
			cfg->ac_cfg_gap_grn_red_min_1, cfg->ac_cfg_red_resume_offset,
			cfg->ac_cfg_yel_resume_offset_0, cfg->ac_cfg_yel_resume_offset_1,
			cfg->ac_cfg_ecn_mark_en);
}

/*
 * edma_ddrq_grp_cfg_dump()
 *	API to dump DDRQ group configurations
 */
static void edma_ddrq_grp_cfg_dump(nss_dp_ddrq_ac_grp_cfg_tbl_t *cfg)
{
	printk(KERN_DEBUG"%s: DDRQ AC group configuration get values:\n", __func__);
	printk(KERN_DEBUG"\t\t ac_en: 0x%0x, dp_thrd: 0x%0x, gap_shrd_limit: 0x%0x, ac_grp_grn_resume_offset: 0x%0x,"
			" color_aware: 0x%0x, gap_grn_red: 0x%0x"
			" gap_grn_yel: 0x%0x, red_resume_offset: 0x%0x,"
			" yel_resume_offset: 0x%0x\n", cfg->ac_cfg_ac_en, cfg->ac_grp_dp_thrd,
			cfg->ac_grp_gap_shrd_limit, cfg->ac_grp_grn_resume_offset,
			cfg->ac_cfg_color_aware,
			cfg->ac_grp_gap_grn_red, cfg->ac_grp_gap_grn_yel,
			cfg->ac_grp_red_resume_offset, cfg->ac_grp_yel_resume_offset);
}

/*
 * edma_ddrq_ac_queue_cfg_tbl_get()
 *	API to get DDRQ AC queue configuration values
 */
static int32_t edma_ddrq_ac_queue_cfg_tbl_get(uint32_t index, edma_ddrq_ac_queue_cfg_tbl_u *ddrq_cfg)
{
	int32_t ret;

	ret = edma_ddrq_reg_tbl_get(index, ddrq_cfg->val, sizeof(edma_ddrq_ac_queue_cfg_tbl_u)/sizeof(uint32_t));
	if (ret) {
		edma_err("%s: failed to get ddrq queue cfg table for %d index\n", __func__, index);
		return ret;
	}

	return ret;
}

/*
 * edma_ddrq_ac_queue_cfg_tbl_set()
 *	API to set DDRQ AC queue configuration values
 */
static int32_t edma_ddrq_ac_queue_cfg_tbl_set(uint32_t index, edma_ddrq_ac_queue_cfg_tbl_u *ddrq_cfg)
{
	if ((index == 0) || (index == 16) || (index == 24) || (index == 32) || (index == 40) ||
			 (index == 48) || (index == 56)) {
		edma_ddrq_ac_queue_cfg_dump_local(&ddrq_cfg->ddrq_cfg);
	}
	return edma_ddrq_reg_tbl_set(index, ddrq_cfg->val, sizeof(edma_ddrq_ac_queue_cfg_tbl_u)/sizeof(uint32_t));
}

/*
 * edma_ddrq_grp_cfg_tbl_get()
 *	API to get DDRQ group configuration values
 */
static int32_t edma_ddrq_grp_cfg_tbl_get(uint32_t index, edma_ddrq_ac_grp_cfg_tbl_u *ddrq_cfg)
{
	int32_t ret;

	ret = edma_ddrq_reg_grp_tbl_get(index, ddrq_cfg->val, sizeof(edma_ddrq_ac_grp_cfg_tbl_u)/sizeof(uint32_t));
	if (ret) {
		edma_err("Error in getting DDRQ group table get config for %d index\n", index);
		return ret;
	}

	return ret;
}

/*
 * edma_ddrq_grp_cfg_tbl_set()
 *	API to set DDRQ group configuration values
 */
static int32_t edma_ddrq_grp_cfg_tbl_set(uint32_t index, edma_ddrq_ac_grp_cfg_tbl_u *ddrq_cfg)
{
	return edma_ddrq_reg_grp_tbl_set(index, ddrq_cfg->val, sizeof(edma_ddrq_ac_grp_cfg_tbl_u)/sizeof(uint32_t));
}

/*
 * edma_ddrq_enq_disable()
 *	API for per DDRQ enqueue disable operation
 */
static int32_t edma_ddrq_enq_disable(uint32_t ddrq_id, bool disable)
{
	uint32_t data, reg_offset;

	switch (ddrq_id >> EDMA_DDRQ_MAX_CNT_WORD) {
	case 0:
		reg_offset = EDMA_REG_DDRQ_ENQ_DIS_REG0_OFFSET;
		break;
	case 1:
		reg_offset = EDMA_REG_DDRQ_ENQ_DIS_REG1_OFFSET;
		break;
	case 2:
		reg_offset = EDMA_REG_DDRQ_ENQ_DIS_REG2_OFFSET;
		break;
	case 3:
		reg_offset = EDMA_REG_DDRQ_ENQ_DIS_REG3_OFFSET;
		break;
	case 4:
		reg_offset = EDMA_REG_DDRQ_ENQ_DIS_REG4_OFFSET;
		break;
	case 5:
		reg_offset = EDMA_REG_DDRQ_ENQ_DIS_REG5_OFFSET;
		break;
	default:
			edma_err("Invalid ddrq id %d\n", (ddrq_id >> EDMA_DDRQ_MAX_CNT_WORD));
			return -EINVAL;
	}

	data =  edma_reg_read(reg_offset);
	if (disable) {
		data |= (1 << (ddrq_id % EDMA_BITS_IN_WORD));
	} else {
		data &= ~(1 << (ddrq_id % EDMA_BITS_IN_WORD));
	}
	edma_reg_write(reg_offset, data);

	return 0;
}

/*
 * edma_ddrq_deq_drop()
 *	API for per DDRQ dequeue drop
 */
static int32_t edma_ddrq_deq_drop(uint32_t ddrq_id, bool drop)
{
	uint32_t data, reg_offset;

	switch (ddrq_id >> EDMA_DDRQ_MAX_CNT_WORD) {
	case 0:
		reg_offset = EDMA_REG_DDRQ_DEQ_DROP_REG0_OFFSET;
		break;
	case 1:
		reg_offset = EDMA_REG_DDRQ_DEQ_DROP_REG1_OFFSET;
		break;
	case 2:
		reg_offset = EDMA_REG_DDRQ_DEQ_DROP_REG2_OFFSET;
		break;
	case 3:
		reg_offset = EDMA_REG_DDRQ_DEQ_DROP_REG3_OFFSET;
		break;
	case 4:
		reg_offset = EDMA_REG_DDRQ_DEQ_DROP_REG4_OFFSET;
		break;
	case 5:
		reg_offset = EDMA_REG_DDRQ_DEQ_DROP_REG5_OFFSET;
		break;
	default:
			edma_err("Invalid ddrq id %d\n", (ddrq_id >> EDMA_DDRQ_MAX_CNT_WORD));
			return -EINVAL;
	}

	data =  edma_reg_read(reg_offset);
	if (drop) {
		data |= (1 << (ddrq_id % EDMA_BITS_IN_WORD));
	} else {
		data &= ~(1 << (ddrq_id % EDMA_BITS_IN_WORD));
	}
	edma_reg_write(reg_offset, data);

	return 0;
}

/*
 * edma_ddrq_dbg_cnt_threshold_get()
 *	API to get DDRQ occupancy threshold configuration
 */
static int32_t edma_ddrq_dbg_cnt_threshold_get(uint32_t index, edma_ddrq_occupancy_threshold_u *thres_cfg)
{
	return edma_ddrq_reg_dbg_cnt_threshold_get(index, thres_cfg->val, sizeof(edma_ddrq_occupancy_threshold_u)/sizeof(uint32_t));
}

/*
 * edma_ddrq_dbg_cnt_threshold_set()
 *	API to set DDRQ occupancy threshold configuration
 */
static int32_t edma_ddrq_dbg_cnt_threshold_set(uint32_t index, edma_ddrq_occupancy_threshold_u *thres_cfg)
{
	return edma_ddrq_reg_dbg_cnt_threshold_set(index, thres_cfg->val, sizeof(edma_ddrq_occupancy_threshold_u)/sizeof(uint32_t));
}

/*
 * edma_ddrq_dbg_cnt_occ_stats_get()
 *	API to get DDRQ occupancy statistics
 */
static int32_t edma_ddrq_dbg_cnt_occ_stats_get(uint32_t index, edma_ddrq_occupancy_stats_u *occ_stats)
{
	return edma_ddrq_reg_dbg_cnt_occ_stats_get(index, occ_stats->val, sizeof(edma_ddrq_occupancy_stats_u)/sizeof(uint32_t));
}

#ifdef NSS_DP_PON_SUPPORT
/*
 * edma_ddrq_pon_dp_dev_set()
 *	API to set PON related DDRQ passthrough and SC related information in the dp_dev
 */
static void edma_ddrq_pon_dp_dev_set(struct nss_dp_dev *dp_dev)
{
	/*
	 * Check whether DDRQs on the PON port are set
	 */
	if (edma_ddrq_en_port_bm & (1 << (dp_dev->macid - 1))) {
		if (edma_passthrough_val == EDMA_PASSTHROUGH_VAL_INVALID) {
			dp_dev->pt_info.dst_pt_mode_val = EDMA_TXDESC_PASS_THROUGH_MODE_128B;
		} else {
			dp_dev->pt_info.dst_pt_mode_val = edma_passthrough_val;
		}
		dp_dev->pt_info.sc = PPE_DRV_SC_DDRQ_PON_PT_MODE;
	} else {
		/*
		 * DDRQs on the PON port are not set
		 */
		dp_dev->pt_info.dst_pt_mode_val = EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA;
		dp_dev->pt_info.sc = PPE_DRV_SC_GEM_LOOKUP;
	}
}
#endif

/*
 * edma_ddrq_dp_dev_set()
 *	API to set DDRQ passthrough and SC related information in the dp_dev
 */
int32_t edma_ddrq_dp_dev_set(struct net_device *dev)
{
	struct nss_dp_dev *dp_dev;
	uint32_t mac_id;

	if (!dev) {
		edma_err("Invalid netdevice passed for DDRQ DP dev information set\n");
		return -EINVAL;
	}

	dp_dev = netdev_priv(dev);
	mac_id = dp_dev->macid;
	if (mac_id > (NSS_DP_HAL_MAX_PORTS + 2)) {
		edma_err("Invalid mac_id (%d) passed for DDRQ dp dev set operation\n", mac_id);
		return -EINVAL;
	}

	/*
	 * Set DDRQ related datapath informations in the NSS-DP ETH port's DP DEV
	 */
	if (mac_id <= NSS_DP_HAL_MAX_PORTS) {
#ifdef NSS_DP_PON_SUPPORT
		/*
		 * Check for the PON device
		 */
		if (dp_dev->gem_port) {
			edma_ddrq_pon_dp_dev_set(dp_dev);
			goto done;
		}
#endif
		/*
		 * ETH device.
		 *
		 * Check whether DDRQs on the particular port is set
		 */
		if (edma_ddrq_en_port_bm & (1 << (mac_id - 1))) {
			if (edma_passthrough_val == EDMA_PASSTHROUGH_VAL_INVALID) {
				dp_dev->pt_info.dst_pt_mode_val = EDMA_TXDESC_PASS_THROUGH_MODE_0B;
			} else {
				dp_dev->pt_info.dst_pt_mode_val = edma_passthrough_val;
			}
			dp_dev->pt_info.sc = PPE_DRV_SC_DDRQ_ETH_PT_MODE;
		} else {
			/*
			 * DDRQs on the particular port is not set
			 */
			dp_dev->pt_info.dst_pt_mode_val = EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA;
			dp_dev->pt_info.sc = PPE_DRV_SC_BYPASS_ALL;
		}
	} else {
		/*
		 * VP device.
		 *
		 */

		dp_dev->pt_info.src_pt_mode_val = EDMA_TXDESC_PASS_THROUGH_MODE_128B;
		/*
		 * TODO:
		 * Currently assigning full packet passthrough mode to known DST DP VP devices
		 */
		dp_dev->pt_info.dst_pt_mode_val = EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA;
	}

#ifdef NSS_DP_PON_SUPPORT
done:
#endif
	edma_warn("Updated PT info for port: %d, src_pt_val: %d, dst_pt_val: %d, sc: %d\n", mac_id,
						 dp_dev->pt_info.src_pt_mode_val,
						 dp_dev->pt_info.dst_pt_mode_val,
						 dp_dev->pt_info.sc);

	return 0;
}

/*
 * edma_ddrq_cfg_set_inval()
 *	API to set DDRQ configuration values to invalid
 */
static void edma_ddrq_cfg_set_inval(nss_dp_ddrq_ac_queue_cfg_tbl_t *cfg)
{
	cfg->ac_cfg_gap_grn_grn_min = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_gap_grn_red_max = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_gap_grn_red_min = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_gap_grn_yel_max = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_gap_grn_yel_min = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_grn_resume_offset = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_red_resume_offset = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_yel_resume_offset = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_pre_alloc_limit = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_shared_ceiling = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_ac_en = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_bp_en = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_color_aware = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_ecn_mark_en = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_grp_id = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_shared_dynamic = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_shared_weight = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_wred_en = NSS_DP_DDRQ_INV_VAL;
	cfg->ddrq_state = NSS_DP_DDRQ_INV_VAL;
}

/*
 * edma_ddrq_grp_cfg_set_inval()
 *	API to set DDRQ group configuration values to invalid
 */
static void edma_ddrq_grp_cfg_set_inval(nss_dp_ddrq_ac_grp_cfg_tbl_t *cfg)
{
	cfg->ac_grp_dp_thrd = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_grp_gap_grn_red = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_grp_gap_grn_yel = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_grp_grn_resume_offset = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_grp_red_resume_offset = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_grp_gap_shrd_limit = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_grp_yel_resume_offset = NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_ac_en= NSS_DP_DDRQ_INV_VAL;
	cfg->ac_cfg_color_aware= NSS_DP_DDRQ_INV_VAL;
}

/*
 * edma_ddrq_dbg_cnt_occ_stats_copy_from_local()
 *	API to copy DDRQ occupancy statistics from hardware format to the software format
 */
static void edma_ddrq_dbg_cnt_occ_stats_copy_from_local(nss_dp_ddrq_occupancy_stats_t *tgt, edma_ddrq_occupancy_stats_t *src)
{
	if (!tgt || !src) {
		return;
	}

	tgt->peak_pkts = src->peak_pkts;
	tgt->pkts = src->pkts;
	tgt->peak_bytes = src->peak_bytes_0 | \
				      (((uint64_t)src->peak_bytes_1) << OFFSET_IN_DOUBLE_WORD(EDMA_DDRQ_DBG_CNT_OCC_STATS_PEAK_BYTES_OFFSET));
	tgt->bytes = src->bytes_0 | \
				      (((uint64_t)src->bytes_1) << OFFSET_IN_DOUBLE_WORD(EDMA_DDRQ_DBG_CNT_OCC_STATS_BYTES_OFFSET_1)) | \
				      (((uint64_t)src->bytes_2) << OFFSET_IN_DOUBLE_WORD(EDMA_DDRQ_DBG_CNT_OCC_STATS_BYTES_OFFSET_2));
}

/*
 * edma_ddrq_dbg_cnt_threshold_copy_from_local()
 *	API to copy DDRQ threshold configuration from hardware format to the software format
 */
static void edma_ddrq_dbg_cnt_threshold_copy_from_local(nss_dp_ddrq_occupancy_threshold_t *tgt, edma_ddrq_occupancy_threshold_t *src)
{
	if (!tgt || !src) {
		return;
	}

	tgt->threshold_type = src->threshold_type;
	tgt->threshold_val = src->threshold_val_0 | \
				      (((int64_t)src->threshold_val_1) << OFFSET_IN_WORD(EDMA_DDRQ_DBG_CNT_OCC_THRESHOLD_OFFSET));
}

/*
 * edma_ddrq_dbg_cnt_threshold_copy_new_cfg()
 *	API to copy DDRQ threshold configuration from software format to hardware format
 */
static void edma_ddrq_dbg_cnt_threshold_copy_new_cfg(edma_ddrq_occupancy_threshold_t *tgt, nss_dp_ddrq_occupancy_threshold_t *src)
{
	if (!tgt || !src) {
		return;
	}

	if (src->threshold_type != NSS_DP_DDRQ_INV_VAL) {
		tgt->threshold_type = src->threshold_type;
	}

	if (src->threshold_val != NSS_DP_DDRQ_INV_VAL) {
		tgt->threshold_val_0 = src->threshold_val;
		tgt->threshold_val_1 = src->threshold_val >>
						 OFFSET_IN_WORD(EDMA_DDRQ_DBG_CNT_OCC_THRESHOLD_OFFSET);
	}

}

/*
 * edma_ddrq_ac_grp_cfg_tbl_copy_from_local()
 *	API to copy DDRQ group configurations from hardare format to the software format
 */
static void edma_ddrq_ac_grp_cfg_tbl_copy_from_local(nss_dp_ddrq_ac_grp_cfg_tbl_t *tgt, edma_ddrq_ac_grp_cfg_tbl_t *src)
{
	if (!tgt || !src) {
		return;
	}

	tgt->ac_cfg_ac_en = src->ac_cfg_ac_en;
	tgt->ac_cfg_color_aware = src->ac_cfg_color_aware;
	tgt->ac_grp_dp_thrd = src->ac_grp_dp_thrd;
	tgt->ac_grp_gap_grn_red = src->ac_grp_gap_grn_red;
	tgt->ac_grp_gap_grn_yel = src->ac_grp_gap_grn_yel;
	tgt->ac_grp_gap_shrd_limit = src->ac_grp_gap_shrd_limit;
	tgt->ac_grp_yel_resume_offset = src->ac_grp_yel_resume_offset;
	tgt->ac_grp_grn_resume_offset = src->ac_grp_grn_resume_offset_0 | \
				      (src->ac_grp_grn_resume_offset_1 << OFFSET_IN_WORD(EDMA_DDRQ_AC_GRP_GRN_RESUME_OFFSET));
	tgt->ac_grp_red_resume_offset = src->ac_grp_red_resume_offset_0 | \
				      (src->ac_grp_red_resume_offset_1 << OFFSET_IN_WORD(EDMA_DDRQ_AC_GRP_RED_RESUME_OFFSET));
}

/*
 * edma_ddrq_ac_grp_cfg_tbl_copy_new_cfg()
 *	API to copy DDRQ group configurations from software format to the hardware format
 */
static void edma_ddrq_ac_grp_cfg_tbl_copy_new_cfg(edma_ddrq_ac_grp_cfg_tbl_t *tgt, nss_dp_ddrq_ac_grp_cfg_tbl_t *src)
{

	if (!tgt || !src) {
		return;
	}

	if (src->ac_cfg_ac_en != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_ac_en = src->ac_cfg_ac_en;
	}

	if (src->ac_cfg_color_aware != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_color_aware = src->ac_cfg_color_aware;
	}

	if (src->ac_grp_dp_thrd != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_grp_dp_thrd = src->ac_grp_dp_thrd;
	}

	if (src->ac_grp_gap_grn_red != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_grp_gap_grn_red = src->ac_grp_gap_grn_red;
	}

	if (src->ac_grp_gap_grn_yel != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_grp_gap_grn_yel = src->ac_grp_gap_grn_yel;
	}

	if (src->ac_grp_gap_shrd_limit != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_grp_gap_shrd_limit = src->ac_grp_gap_shrd_limit;
	}

	if (src->ac_grp_yel_resume_offset != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_grp_yel_resume_offset = src->ac_grp_yel_resume_offset;
	}

	if (src->ac_grp_grn_resume_offset != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_grp_grn_resume_offset_0 = src->ac_grp_grn_resume_offset;
		tgt->ac_grp_grn_resume_offset_1 = src->ac_grp_grn_resume_offset >>
						 OFFSET_IN_WORD(EDMA_DDRQ_AC_GRP_GRN_RESUME_OFFSET);
	}

	if (src->ac_grp_red_resume_offset != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_grp_red_resume_offset_0 = src->ac_grp_red_resume_offset;
		tgt->ac_grp_red_resume_offset_1 = src->ac_grp_red_resume_offset >>
						 OFFSET_IN_WORD(EDMA_DDRQ_AC_GRP_RED_RESUME_OFFSET);
	}
}

/*
 * edma_ddrq_ac_queue_cfg_tbl_copy_from_local()
 *	API to copy DDRQ AC queue configurations from hardware format to software format
 */
static void edma_ddrq_ac_queue_cfg_tbl_copy_from_local(nss_dp_ddrq_ac_queue_cfg_tbl_t *tgt, edma_ddrq_ac_queue_cfg_tbl_t *src)
{
	if (!tgt || !src) {
		return;
	}

	tgt->ac_cfg_ac_en = src->ac_cfg_ac_en;
	tgt->ac_cfg_bp_en = src->ac_cfg_bp_en;
	tgt->ac_cfg_color_aware = src->ac_cfg_color_aware;
	tgt->ac_cfg_ecn_mark_en = src->ac_cfg_ecn_mark_en;
	tgt->ac_cfg_gap_grn_grn_min = src->ac_cfg_gap_grn_grn_min;
	tgt->ac_cfg_gap_grn_red_max = src->ac_cfg_gap_grn_red_max;
	tgt->ac_cfg_shared_dynamic = src->ac_cfg_shared_dynamic;
	tgt->ac_cfg_shared_weight = src->ac_cfg_shared_weight;
	tgt->ac_cfg_wred_en = src->ac_cfg_wred_en;
	tgt->ac_cfg_gap_grn_yel_min = src->ac_cfg_gap_grn_yel_min;
	tgt->ac_cfg_grn_resume_offset = src->ac_cfg_grn_resume_offset;
	tgt->ac_cfg_grp_id = src->ac_cfg_grp_id;
	tgt->ac_cfg_pre_alloc_limit = src->ac_cfg_pre_alloc_limit;
	tgt->ac_cfg_red_resume_offset = src->ac_cfg_red_resume_offset;
	tgt->ac_cfg_gap_grn_red_min = src->ac_cfg_gap_grn_red_min_0 | \
				      (src->ac_cfg_gap_grn_red_min_1 << OFFSET_IN_WORD(EDMA_DDRQ_AC_CFG_GAP_GRN_RED_MIN_OFFSET));
	tgt->ac_cfg_gap_grn_yel_max = src->ac_cfg_gap_grn_yel_max_0 | \
				      (src->ac_cfg_gap_grn_yel_max_1 << OFFSET_IN_WORD(EDMA_DDRQ_AC_CFG_GAP_GRN_YEL_MAX_OFFSET));
	tgt->ac_cfg_shared_ceiling = src->ac_cfg_shared_ceiling_0 | \
				     (src->ac_cfg_shared_ceiling_1 << OFFSET_IN_WORD(EDMA_DDRQ_AC_CFG_SHARED_CEILING_OFFSET));
	tgt->ac_cfg_yel_resume_offset = src->ac_cfg_yel_resume_offset_0 | \
					(src->ac_cfg_yel_resume_offset_1 << OFFSET_IN_WORD(EDMA_DDRQ_AC_CFG_YEL_RESUME_OFFSET));
}

/*
 * edma_ddrq_ac_queue_cfg_tbl_copy_new_cfg()
 *	API to copy DDRQ AC queue configurations from software format to hardware format
 */
static void edma_ddrq_ac_queue_cfg_tbl_copy_new_cfg(edma_ddrq_ac_queue_cfg_tbl_t *tgt, nss_dp_ddrq_ac_queue_cfg_tbl_t *src)
{
	if (!tgt || !src) {
		return;
	}

	if (src->ac_cfg_ac_en != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_ac_en = src->ac_cfg_ac_en;
	}

	if (src->ac_cfg_bp_en != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_bp_en = src->ac_cfg_bp_en;
	}

	if (src->ac_cfg_color_aware != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_color_aware = src->ac_cfg_color_aware;
	}

	if (src->ac_cfg_ecn_mark_en != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_ecn_mark_en = src->ac_cfg_ecn_mark_en;
	}

	if (src->ac_cfg_gap_grn_grn_min != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_gap_grn_grn_min = src->ac_cfg_gap_grn_grn_min;
	}

	if (src->ac_cfg_gap_grn_red_max != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_gap_grn_red_max = src->ac_cfg_gap_grn_red_max;
	}

	if (src->ac_cfg_gap_grn_red_min != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_gap_grn_red_min_0 = src->ac_cfg_gap_grn_red_min;
		tgt->ac_cfg_gap_grn_red_min_1 = src->ac_cfg_gap_grn_red_min >>
						 OFFSET_IN_WORD(EDMA_DDRQ_AC_CFG_GAP_GRN_RED_MIN_OFFSET);
	}

	if (src->ac_cfg_gap_grn_yel_max != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_gap_grn_yel_max_0 = src->ac_cfg_gap_grn_yel_max;
		tgt->ac_cfg_gap_grn_yel_max_1 = src->ac_cfg_gap_grn_yel_max >>
						 OFFSET_IN_WORD(EDMA_DDRQ_AC_CFG_GAP_GRN_YEL_MAX_OFFSET);;
	}

	if (src->ac_cfg_gap_grn_yel_min != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_gap_grn_yel_min = src->ac_cfg_gap_grn_yel_min;
	}

	if (src->ac_cfg_grn_resume_offset != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_grn_resume_offset = src->ac_cfg_grn_resume_offset;
	}

	if (src->ac_cfg_grp_id != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_grp_id = src->ac_cfg_grp_id;
	}

	if (src->ac_cfg_pre_alloc_limit != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_pre_alloc_limit = src->ac_cfg_pre_alloc_limit;
	}

	if (src->ac_cfg_red_resume_offset != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_red_resume_offset = src->ac_cfg_red_resume_offset;
	}

	if (src->ac_cfg_shared_ceiling != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_shared_ceiling_0 = src->ac_cfg_shared_ceiling;
		tgt->ac_cfg_shared_ceiling_1 = src->ac_cfg_shared_ceiling >>
						 OFFSET_IN_WORD(EDMA_DDRQ_AC_CFG_SHARED_CEILING_OFFSET);;
	}

	if (src->ac_cfg_shared_dynamic != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_shared_dynamic = src->ac_cfg_shared_dynamic;
	}

	if (src->ac_cfg_shared_weight != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_shared_weight = src->ac_cfg_shared_weight;
	}

	if (src->ac_cfg_wred_en != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_wred_en = src->ac_cfg_wred_en;
	}

	if (src->ac_cfg_yel_resume_offset != NSS_DP_DDRQ_INV_VAL) {
		tgt->ac_cfg_yel_resume_offset_0 = src->ac_cfg_yel_resume_offset;
		tgt->ac_cfg_yel_resume_offset_1 = src->ac_cfg_yel_resume_offset >>
						 OFFSET_IN_WORD(EDMA_DDRQ_AC_CFG_YEL_RESUME_OFFSET);
	}
}

/*
 * edma_ddrq_qid_to_ring_mapping()
 *	API to configure DDRQ queue to ring mapping
 */
static int edma_ddrq_qid_to_ring_mapping(uint32_t qid, bool cfg_state)
{
	uint32_t reg_index = qid / EDMA_QID2RID_NUM_PER_REG;
	uint32_t data, cur_data, ring_id;

	if (cfg_state) {
		ring_id = qid + EDMA_DDRQ_RING_BASE_INDEX;
	} else {
		ring_id = EDMA_DDRQ_RESET_RING_INDEX;
	}

	cur_data = edma_reg_read(EDMA_QID2RID_TABLE_MEM(reg_index));
	edma_info("queue_id: %d, ring_id: %d, cur_data: 0x%0x, cfg_state: %d\n", qid, ring_id,
				cur_data, cfg_state);

	if ((qid % EDMA_QID2RID_NUM_PER_REG) == 0) {
		data = EDMA_RX_RING_ID_QUEUE0_SET(ring_id);
		cur_data = EDMA_RX_RING_ID_QUEUE0_RESET(cur_data);
	} else if ((qid % EDMA_QID2RID_NUM_PER_REG) == 1) {
		data = EDMA_RX_RING_ID_QUEUE1_SET(ring_id);
		cur_data = EDMA_RX_RING_ID_QUEUE1_RESET(cur_data);
	} else if ((qid % EDMA_QID2RID_NUM_PER_REG) == 2) {
		data = EDMA_RX_RING_ID_QUEUE2_SET(ring_id);
		cur_data = EDMA_RX_RING_ID_QUEUE2_RESET(cur_data);
	} else if ((qid % EDMA_QID2RID_NUM_PER_REG) == 3) {
		data = EDMA_RX_RING_ID_QUEUE3_SET(ring_id);
		cur_data = EDMA_RX_RING_ID_QUEUE3_RESET(cur_data);
	}

	data |= cur_data;
	edma_reg_write(EDMA_QID2RID_TABLE_MEM(reg_index), data);

	edma_info("DDRQ QID2RID(%d) reg: 0x%0x, data: 0x%0x, cur_data: 0x%0x\n", qid,
			EDMA_QID2RID_TABLE_MEM(reg_index), data, cur_data);

	return 0;
}

/*
 * edma_ddrq_vp_tbl_cfg()
 *	API for configuring DDRQ VP table
 */
static int edma_ddrq_vp_tbl_cfg(uint32_t id, uint32_t port)
{
	uint32_t data;

	/*
	 * DDRQ TODO : Add index validation for edma_ddrq_vp_port_map array
	 */
	data = (edma_ddrq_vp_port_map[port - 1] & EDMA_REG_DDRQ_VIRTUAL_PORT_ID_MASK);
	edma_reg_write(EDMA_REG_DDRQ_VIRTUAL_PORT_TBL_OFFSET(id), data);

	edma_info("ddrq id : %d, vp_id: %d, port: %d, reg_off: 0x%0x\n", id, data,
				port, EDMA_REG_DDRQ_VIRTUAL_PORT_TBL_OFFSET(id));

	return 0;
}

/*
 * edma_ddrq_ac_queue_cfg_state_set()
 *	API to set DDRQ state
 */
static int edma_ddrq_ac_queue_cfg_state_set(uint32_t id, bool ddrq_state)
{
	sw_error_t err;
	fal_ucast_queue_ddrq_en_t ddrq_en;

	fal_ucast_queue_ddrq_en_get(EDMA_SWITCH_DEV_ID, id, &ddrq_en);
	ddrq_en.ddrq_en = ddrq_state;
	err = fal_ucast_queue_ddrq_en_set(EDMA_SWITCH_DEV_ID, id, &ddrq_en);
	if (err != SW_OK) {
		edma_err("DDRQ enabled failed (%d) for %d DDRQ\n", err, id);
		return -EINVAL;
	}
	edma_info("DDRQ state config success for %d ddrq, state: %d, qid_mismatch check_en: %d\n",
			 id, ddrq_en.ddrq_en, ddrq_en.qid_mismatch_check_en);
	return 0;
}

/*
 * edma_ddrq_ac_queue_cfg_state_get()
 *	API to get DDRQ state
 */
static int32_t edma_ddrq_ac_queue_cfg_state_get(uint32_t id, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg)
{
	fal_ucast_queue_ddrq_en_t ddrq_en;

	fal_ucast_queue_ddrq_en_get(EDMA_SWITCH_DEV_ID, id, &ddrq_en);
	ddrq_cfg->ddrq_state = ((ddrq_en.ddrq_en == A_TRUE) ? true: false);
	edma_warn("DDRQ state is %d\n", ddrq_cfg->ddrq_state);
	return 0;
}

/*
 * edma_ddrq_cfg_get()
 *	API to get DDRQ AC queue configurations
 */
nss_dp_ddrq_ret_t edma_ddrq_cfg_get(nss_dp_ddrq_obj_id_t *obj, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg, uint32_t count)
{
	edma_ddrq_ac_queue_cfg_tbl_u ddrq_cfg_l = {0};
	int32_t queue_id;

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	if (!obj || !ddrq_cfg) {
		edma_err("Invalid input parameters in DDRQ cfg get API\n");
		return DDRQ_RET_INVAL;
	}

	if (!count || (count > EDMA_DDRQ_ESRAMQ_CNT_PER_PORT)) {
		edma_err("Invalid count value: %d. Maximum allowed count is %d\n", count, EDMA_DDRQ_ESRAMQ_CNT_PER_PORT);
		return DDRQ_RET_INVAL;
	}

	if (obj->cfg_type == NSS_DP_DDRQ_CFG_TYPE_QUEUE) {
		if (edma_ddrq_ac_queue_cfg_state_get(obj->cfg_id , ddrq_cfg)) {
			edma_err("Error in getting the DDRQ state for %d idx\n", obj->cfg_id);
		}
		if (edma_ddrq_ac_queue_cfg_tbl_get(obj->cfg_id + EDMA_DDRQ_CFG_BASE_INDEX, &ddrq_cfg_l)) {
			edma_err("Error in getting ddrq cfg tbl cfg for %d idx\n", obj->cfg_id);
			return DDRQ_RET_ERR;
		}

		edma_ddrq_ac_queue_cfg_tbl_copy_from_local(ddrq_cfg, &ddrq_cfg_l.ddrq_cfg);
	} else if (obj->cfg_type == NSS_DP_DDRQ_CFG_TYPE_PORT) {
		nss_dp_ddrq_ac_queue_cfg_tbl_t *cfg = ddrq_cfg;
		int i;

		if ((queue_id = ppe_drv_port_ucast_queue_get_by_port(obj->cfg_id)) < 0) {
			edma_err("Error in getting queue_id for %d PPE port\n", obj->cfg_id);
			return DDRQ_RET_INVAL;
		}

		for (i = 0; i < count; i++) {
			if (edma_ddrq_ac_queue_cfg_state_get(queue_id + i, cfg)) {
				edma_err("Error in getting the DDRQ state for %d idx\n", queue_id + i);
			}

			if (edma_ddrq_ac_queue_cfg_tbl_get(queue_id + i + EDMA_DDRQ_CFG_BASE_INDEX, &ddrq_cfg_l)) {
				edma_err("Error in getting ddrq cfg tbl cfg for %d idx\n", queue_id + i);
				return DDRQ_RET_ERR;
			}

			edma_ddrq_ac_queue_cfg_tbl_copy_from_local(cfg, &ddrq_cfg_l.ddrq_cfg);
			cfg++;
		}
	} else {
		edma_err("Invalid input DDRQ obj type: %d\n", obj->cfg_type);
		return DDRQ_RET_INVAL;
	}

	edma_warn("DDRQ AC queue cfg get successful\n");
	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_cfg_set()
 *	API to set DDRQ AC queue configurations values
 */
nss_dp_ddrq_ret_t edma_ddrq_cfg_set(nss_dp_ddrq_obj_id_t *obj, nss_dp_ddrq_ac_queue_cfg_tbl_t *ddrq_cfg)
{
	edma_ddrq_ac_queue_cfg_tbl_u ddrq_cfg_l = {0};
	bool ac_en;
	bool ac_fc_en;
	int32_t queue_id;

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	if (!obj || !ddrq_cfg) {
		edma_err("Invalid input parameters in DDRQ cfg set API\n");
		return DDRQ_RET_INVAL;
	}

	if (obj->cfg_type == NSS_DP_DDRQ_CFG_TYPE_QUEUE) {

		if (obj->cfg_id >= NSS_DP_DDRQ_MAX_CNT) {
			edma_err("Invalid queue id (%d) passed for DDRQ configuration\n",
						obj->cfg_id);
			return DDRQ_RET_INVAL;
		}

		if (edma_ddrq_ac_queue_cfg_tbl_get(obj->cfg_id + EDMA_DDRQ_CFG_BASE_INDEX, &ddrq_cfg_l)) {
			edma_err("Error in getting ddrq ac queue cfg tbl get for %d idx\n", obj->cfg_id);
			return DDRQ_RET_ERR;
		}

		edma_ddrq_ac_queue_cfg_tbl_copy_new_cfg(&ddrq_cfg_l.ddrq_cfg, ddrq_cfg);
		if (edma_ddrq_ac_queue_cfg_tbl_set(obj->cfg_id + EDMA_DDRQ_CFG_BASE_INDEX, &ddrq_cfg_l)) {
			edma_err("Error in setting DDRQ AC queue cfg tbl set for %d idx\n", obj->cfg_id);
			return DDRQ_RET_ERR;
		}

		if (ddrq_cfg->ddrq_state != NSS_DP_DDRQ_INV_VAL) {
			if (edma_ddrq_ac_queue_cfg_state_set(obj->cfg_id , (ddrq_cfg->ddrq_state ? true: false))) {
				edma_err("Error in setting the state of %d ddrq id\n", obj->cfg_id);
				return DDRQ_RET_ERR;
			}

			if (ddrq_cfg->ddrq_state) {
				edma_ddrq_qid_to_ring_mapping(obj->cfg_id , true);

				/*
				 * When ac_en/ac_fc_en are set to 1, the flow control will be overriden
				 * for that queue and packets drop gets enabled.
				 */
				ac_en = true;
				ac_fc_en = true;
				if (!ppe_drv_qos_ac_ctrl_cfg(obj->cfg_id, ac_en, ac_fc_en)) {
					edma_err("Error in configuring qid:%d ac_en:%d ac_fc_en:%d configuration\n",
							obj->cfg_id, ac_en, ac_fc_en);
					return DDRQ_RET_ERR;
				}
			} else {
				edma_ddrq_qid_to_ring_mapping(obj->cfg_id, false);

				/*
				 * Reset ac_en/ac_fc_en to its default value
				 */
				ac_en = true;
				ac_fc_en = false;
				if (!ppe_drv_qos_ac_ctrl_cfg(obj->cfg_id, ac_en, ac_fc_en)) {
					edma_err("Error in configuring qid:%d ac_en:%d ac_fc_en:%d configuration\n",
							obj->cfg_id, ac_en, ac_fc_en);
					return DDRQ_RET_ERR;
				}
			}
		}
	} else if (obj->cfg_type == NSS_DP_DDRQ_CFG_TYPE_PORT) {
		int i;
		uint32_t queue_cnt;
		fal_portscheduler_resource_t cfg = {0};

		if (fal_port_scheduler_resource_get(EDMA_SWITCH_DEV_ID, obj->cfg_id, &cfg)) {
			edma_err("Error in getting PPE PORT (%d) queue base information\n", obj->cfg_id);
			return DDRQ_RET_INVAL;
		}

		queue_id = cfg.ucastq_start;
		if ((queue_id + cfg.ucastq_num) <= NSS_DP_DDRQ_MAX_CNT) {
			queue_cnt = cfg.ucastq_num;
		} else {
			/*
			 * Assumption :
			 * PPE ETH port queues are being allocated from the end of the DDRQs range
			 */
			queue_cnt = NSS_DP_DDRQ_MAX_CNT - cfg.ucastq_start;
		}

		if ((queue_id >= NSS_DP_DDRQ_MAX_CNT) ||
				((queue_id + queue_cnt) > NSS_DP_DDRQ_MAX_CNT)) {
			edma_err("Invalid queue information for %d port. qbase: %d, qcnt: %d\n",
						obj->cfg_id, queue_id, queue_cnt);
			return DDRQ_RET_INVAL;
		}
		edma_debug("port: %d, queue base: %d, queue cnt: %d\n", obj->cfg_id, queue_id, queue_cnt);

		for (i = 0; i < queue_cnt; i++) {
			if (edma_ddrq_ac_queue_cfg_tbl_get(queue_id + i + EDMA_DDRQ_CFG_BASE_INDEX, &ddrq_cfg_l)) {
				edma_err("Error in getting ddrq ac queue cfg tbl get for %d idx\n", queue_id + i);
				return DDRQ_RET_ERR;
			}
			edma_ddrq_ac_queue_cfg_tbl_copy_new_cfg(&ddrq_cfg_l.ddrq_cfg, ddrq_cfg);
			if (edma_ddrq_ac_queue_cfg_tbl_set(queue_id + i + EDMA_DDRQ_CFG_BASE_INDEX, &ddrq_cfg_l)) {
				edma_err("Error in setting DDRQ AC queue cfg tbl set for %d idx\n", queue_id + i);
				return DDRQ_RET_ERR;
			}

			if (ddrq_cfg->ddrq_state != NSS_DP_DDRQ_INV_VAL) {
				if (edma_ddrq_ac_queue_cfg_state_set(queue_id + i, (ddrq_cfg->ddrq_state ? true: false))) {
					edma_err("Error in setting the state of %d port id\n", obj->cfg_id);
					return DDRQ_RET_ERR;
				}

				if (ddrq_cfg->ddrq_state) {
					edma_ddrq_qid_to_ring_mapping(queue_id + i, true);
					edma_ddrq_vp_tbl_cfg(queue_id + i, obj->cfg_id);

					/*
					 * When ac_en/ac_fc_en are set to 1, the flow control will be overriden
					 * for that queue and packets drop gets enabled.
					 */
					ac_en = true;
					ac_fc_en = true;
					if (!ppe_drv_qos_ac_ctrl_cfg(queue_id + i, ac_en, ac_fc_en)) {
						edma_warn("Error in configuring qid:%d ac_en:%d ac_fc_en:%d configuration\n",
								queue_id + i, ac_en, ac_fc_en);
					}
				} else {
					edma_ddrq_qid_to_ring_mapping(queue_id + i, false);

					/*
					 * Reset ac_en/ac_fc_en to its default value
					 */
					ac_en = true;
					ac_fc_en = false;
					if (!ppe_drv_qos_ac_ctrl_cfg(queue_id + i, ac_en, ac_fc_en)) {
						edma_warn("Error in configuring qid:%d ac_en:%d ac_fc_en:%d configuration\n",
								queue_id + i, ac_en, ac_fc_en);
					}
				}
			}
		}

		if (ddrq_cfg->ddrq_state != NSS_DP_DDRQ_INV_VAL) {
			struct net_device *dev;

			/*
			 * Update the global DDRQ port bitmap
			 */
			edma_ddrq_en_port_bm = ((edma_ddrq_en_port_bm & ~(1 << (obj->cfg_id - 1))) |
					 (ddrq_cfg->ddrq_state << (obj->cfg_id - 1)));

			/*
			 * Update the PPE global DDRQ enable bitmask with the DDRQ state change
			 */
			if (ppe_drv_set_ddrq_en_bitmask(queue_id, queue_cnt, (ddrq_cfg->ddrq_state ? true: false))) {
				edma_err("Error in setting DDRQ enable bitmask for port %d\n", obj->cfg_id);
				return DDRQ_RET_ERR;
			}

			/*
			 * Update the port's dp dev information as per the updated DDRQ state change
			 */
			dev = edma_gbl_ctx->netdev_arr[obj->cfg_id - 1];
			if (!dev) {
				return DDRQ_RET_ERR;
			}
			if (edma_ddrq_dp_dev_set(dev)) {
				edma_err("Error in setting DP DEV information for %d port\n", obj->cfg_id);
				return DDRQ_RET_ERR;
			}
		}
	} else if (obj->cfg_type == NSS_DP_DDRQ_CFG_TYPE_LP) {
		if (edma_ddrq_ac_queue_cfg_tbl_get(obj->cfg_id, &ddrq_cfg_l)) {
			edma_err("Error in getting loopback ac queue cfg tbl get for %d idx\n", obj->cfg_id);
			return DDRQ_RET_ERR;
		}

		edma_ddrq_ac_queue_cfg_tbl_copy_new_cfg(&ddrq_cfg_l.ddrq_cfg, ddrq_cfg);
		if (edma_ddrq_ac_queue_cfg_tbl_set(obj->cfg_id, &ddrq_cfg_l)) {
			edma_err("Error in setting loopback AC queue cfg tbl set for %d idx\n", obj->cfg_id);
			return DDRQ_RET_ERR;
		}
	} else {
		edma_err("Invalid input DDRQ obj type: %d\n", obj->cfg_type);
		return DDRQ_RET_INVAL;
	}

	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_grp_cfg_get()
 *	API to get DDRQ group configurations
 */
nss_dp_ddrq_ret_t edma_ddrq_grp_cfg_get(uint32_t ddrq_grp_id, nss_dp_ddrq_ac_grp_cfg_tbl_t *ddrq_grp_cfg)
{
	edma_ddrq_ac_grp_cfg_tbl_u ddrq_grp_cfg_l = {0};

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	if (edma_ddrq_grp_cfg_tbl_get(ddrq_grp_id, &ddrq_grp_cfg_l)) {
		edma_err("Error in getting DDRQ group cfg table configuration for %d group\n", ddrq_grp_id);
		return DDRQ_RET_ERR;
	}
	edma_ddrq_ac_grp_cfg_tbl_copy_from_local(ddrq_grp_cfg, &ddrq_grp_cfg_l.ddrq_grp_cfg);
	edma_ddrq_grp_cfg_dump(ddrq_grp_cfg);

	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_grp_cfg_set()
 *	API to set DDRQ group configurations
 */
nss_dp_ddrq_ret_t edma_ddrq_grp_cfg_set(uint32_t ddrq_grp_id, nss_dp_ddrq_ac_grp_cfg_tbl_t *ddrq_grp_cfg)
{
	edma_ddrq_ac_grp_cfg_tbl_u ddrq_grp_cfg_l = {0};

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	if (edma_ddrq_grp_cfg_tbl_get(ddrq_grp_id, &ddrq_grp_cfg_l)) {
		edma_err("Error in getting DDRQ group cfg table configuration for %d group\n", ddrq_grp_id);
		return DDRQ_RET_ERR;
	}
	edma_ddrq_ac_grp_cfg_tbl_copy_new_cfg(&ddrq_grp_cfg_l.ddrq_grp_cfg, ddrq_grp_cfg);
	if (edma_ddrq_grp_cfg_tbl_set(ddrq_grp_id, &ddrq_grp_cfg_l)) {
		edma_err("Error in setting DDRQ group config table for %d group\n", ddrq_grp_id);
		return DDRQ_RET_ERR;
	}

	edma_debug("ddrq grp cfg tbl set done for %d grp id\n", ddrq_grp_id);
	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_enqueue_disable()
 *	API for DDRQ enqueue disable
 */
nss_dp_ddrq_ret_t edma_ddrq_enqueue_disable(nss_dp_ddrq_obj_id_t *obj, bool disable)
{
	int32_t queue_id, i;

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	if (!obj) {
		edma_err("Invalid input parameters in DDRQ enqueue disable set API\n");
		return DDRQ_RET_INVAL;
	}

	if (obj->cfg_type == NSS_DP_DDRQ_CFG_TYPE_QUEUE) {
		if (edma_ddrq_enq_disable(obj->cfg_id, disable)) {
			edma_err("Error in EDMA ddrq enqueue disable\n");
			return DDRQ_RET_INVAL;
		}
	} else if (obj->cfg_type == NSS_DP_DDRQ_CFG_TYPE_PORT) {
		if ((queue_id = ppe_drv_port_ucast_queue_get_by_port(obj->cfg_id)) < 0) {
			edma_err("Error in getting queue_id for %d PPE port\n", obj->cfg_id);
			return DDRQ_RET_INVAL;
		}
		for (i = 0; i < EDMA_DDRQ_ESRAMQ_CNT_PER_PORT; i++) {
			if (edma_ddrq_enq_disable(queue_id + i, disable)) {
				edma_err("Error in EDMA ddrq enqueue disable\n");
				return DDRQ_RET_INVAL;
			}
		}
	}

	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_dequeue_drop()
 *	API for DDRQ dequeue drop
 */
nss_dp_ddrq_ret_t edma_ddrq_dequeue_drop(nss_dp_ddrq_obj_id_t *obj, bool drop)
{
	int32_t queue_id, i;

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	if (!obj) {
		edma_err("Invalid input parameters in DDRQ dequeue drop set API\n");
		return DDRQ_RET_INVAL;
	}

	if (obj->cfg_type == NSS_DP_DDRQ_CFG_TYPE_QUEUE) {
		if (edma_ddrq_deq_drop(obj->cfg_id, drop)) {
			edma_err("Error in EDMA ddrq dequeue drop\n");
			return DDRQ_RET_INVAL;
		}
	} else if (obj->cfg_type == NSS_DP_DDRQ_CFG_TYPE_PORT) {
		if ((queue_id = ppe_drv_port_ucast_queue_get_by_port(obj->cfg_id)) < 0) {
			edma_err("Error in getting queue_id for %d PPE port\n", obj->cfg_id);
			return DDRQ_RET_INVAL;
		}
		for (i = 0; i < EDMA_DDRQ_ESRAMQ_CNT_PER_PORT; i++) {
			if (edma_ddrq_deq_drop(queue_id + i, drop)) {
				edma_err("Error in EDMA ddrq dequeue drop\n");
				return DDRQ_RET_INVAL;
			}
		}
	}

	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_enq_ctrl_cfg()
 *	API to configure DDRQ ENQ_CTRL configurations
 */
static int edma_ddrq_enq_ctrl_cfg(fal_passthrough_mode_t pt_mode,
				fal_passthrough_src_profile_t *pt_src_profile, bool enq_enable)
{
	sw_error_t err;

	err = fal_qm_passthrough_source_profile_set(EDMA_SWITCH_DEV_ID, pt_mode, pt_src_profile);
	if (err != SW_OK) {
		edma_err("Error in %d PT src profile configuration\n", pt_mode);
		return -EINVAL;
	}

	err = fal_qm_passthrough_direct_enqueue_set(EDMA_SWITCH_DEV_ID, pt_mode, (enq_enable ? A_TRUE: A_FALSE));
	if (err != SW_OK) {
		edma_err("Error in %d PT direct enqueue configuration\n", pt_mode);
		return -EINVAL;
	}

	edma_debug("ddrq enq ctrl cfg successful for %d PT mode\n", pt_mode);
	return 0;
}

/*
 * edma_ddrq_mem_region_init()
 *	API to initialize DDRQ memory regions details in the hardware
 */
static int edma_ddrq_mem_region_init(void)
{
	uint32_t addr, data;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return -EINVAL;
	}

	/*
	 * Update Lower DDRQ data space address in the hardware
	 */
	addr = (uint32_t)(egc->ddrq_def_cfg.ddrq_data_mem_reg.phy_addr & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_DDRQ_RX_DATA_BASE_ADDR_L_OFFSET, addr);

	addr = edma_reg_read(EDMA_REG_DDRQ_RX_DATA_BASE_ADDR_L_OFFSET);
	edma_debug("EDMA_REG_DDRQ_RX_DATA_BASE_ADDR_L_OFFSET: 0x%0x\n", addr);
	/*
	 * Update Lower DDRQ descriptor space address in the hardware
	 */
	addr = (uint32_t)(egc->ddrq_def_cfg.ddrq_desc_mem_reg.phy_addr & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_DDRQ_RX_DESC_BASE_ADDR_L_OFFSET, addr);

	addr = edma_reg_read(EDMA_REG_DDRQ_RX_DESC_BASE_ADDR_L_OFFSET);
	edma_debug("EDMA_REG_DDRQ_RX_DESC_BASE_ADDR_L_OFFSET: 0x%0x\n", addr);
	/*
	 * Update Higher DDRQ data & descriptor space addresses in the hardware
	 */
	addr = 0;
	data = (uint32_t)(((uint64_t)egc->ddrq_def_cfg.ddrq_data_mem_reg.phy_addr >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	addr = EDMA_REG_DDRQ_DATA_BLK_BASE_ADDR_H_SET(data);
	data = (uint32_t)(((uint64_t)egc->ddrq_def_cfg.ddrq_desc_mem_reg.phy_addr >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	addr |= EDMA_REG_DDRQ_DESC_BLK_BASE_ADDR_H_SET(data);
	addr |= EDMA_REG_DDRQ_LINKLIST_INI_EN;
	edma_reg_write(EDMA_REG_DDRQ_RX_BASE_ADDR_H_OFFSET, addr);

	addr = edma_reg_read(EDMA_REG_DDRQ_RX_BASE_ADDR_H_OFFSET);
	edma_debug("EDMA_REG_DDRQ_RX_BASE_ADDR_H_OFFSET: 0x%0x\n", addr);
	return 0;
}

/*
 * edma_ddrq_def_gbl_cfg_set()
 *	API to set default values to the DDRQ global configurations
 */
static int edma_ddrq_def_gbl_cfg_set(edma_ddrq_cfg_t *ddrq_cfg)
{
	edma_ddrq_gbl_cfg_t *ddrq_gbl_cfg = &ddrq_cfg->ddrq_gbl_cfg;
	edma_ddrq_mem_reg_t *ddrq_data_mem = &ddrq_cfg->ddrq_data_mem_reg;
	fal_passthrough_src_profile_t pt_src_profile = {0};
	uint32_t data = 0;

	/*
	 * Get new DDRQ default configurations
	 */
	if (ddrq_gbl_cfg->ddrq_desc_pf_thres != EDMA_DDRQ_NO_OP_DEF_VAL) {
		data |= EDMA_REG_DDRQ_DESC_PF_THRES_SET(ddrq_gbl_cfg->ddrq_desc_pf_thres);
	}
	if (ddrq_gbl_cfg->ddrq_desc_wb_thres != EDMA_DDRQ_NO_OP_DEF_VAL) {
		data |= EDMA_REG_DDRQ_DESC_WB_THRES_SET(ddrq_gbl_cfg->ddrq_desc_wb_thres);
	}

	/*
	 * Validate the DDRQ memory block area configuration
	 */
	if ((edma_ddrq_blk_num_map[ddrq_gbl_cfg->ddrq_blk_num_cfg] * edma_ddrq_blk_size_map[ddrq_gbl_cfg->ddrq_blk_size_cfg]) >
			ddrq_data_mem->size) {
		edma_err("Invalid DDRQ default block configuration. blk_num: %d, blk_size: %d\n",
				ddrq_gbl_cfg->ddrq_blk_num_cfg, ddrq_gbl_cfg->ddrq_blk_size_cfg);
		return -EINVAL;
	}

	data |= EDMA_REG_DDRQ_PKT_DATA_ALIGN_SET(ddrq_gbl_cfg->ddrq_pkt_data_align) |
		EDMA_REG_DDRQ_DATA_OFFSET_SET(ddrq_gbl_cfg->ddrq_data_offset) |
		EDMA_REG_DDRQ_BLK_NUM_CFG_SET(ddrq_gbl_cfg->ddrq_blk_num_cfg) |
		EDMA_REG_DDRQ_BLK_SIZE_CFG_SET(ddrq_gbl_cfg->ddrq_blk_size_cfg);

	/*
	 * Set the new DDRQ default configurations
	 */
	edma_reg_write(EDMA_REG_DDRQ_GBL_CFG_OFFSET, data);

	data = edma_reg_read(EDMA_REG_DDRQ_GBL_CFG_OFFSET);
	edma_debug("EDMA_REG_DDRQ_GBL_CFG_OFFSET value: 0x%0x\n", data);

	/*
	 * Set DDRQ memory regions details in the hardware
	 */
	edma_ddrq_mem_region_init();

	/*
	 * Set the new DDRQ default global EDMA hardware knob configuration
	 */
	data = edma_reg_read(EDMA_REG_PORT_CTRL);
	data |= EDMA_REG_DDRQ_GBL_EN_SET(ddrq_gbl_cfg->ddrq_en_hw);
	edma_reg_write(EDMA_REG_PORT_CTRL, data);

	data = edma_reg_read(EDMA_REG_PORT_CTRL);
	edma_debug("EDMA_REG_PORT_CTRL: 0x%0x\n", data);


	data = EDMA_REG_DDRQ_DATA_OFFSET_REG0_CFG_SET(ddrq_gbl_cfg->ddrq_data_offset0) |
		EDMA_REG_DDRQ_DATA_OFFSET_REG1_CFG_SET(EDMA_DDRQ_GBL_DATA_OFFSET_REG1_VAL);
	edma_reg_write(EDMA_REG_DDRQ_DATA_OFFSET_REG0_OFFSET, data);
	edma_debug("EDMA_REG_DDRQ_DATA_OFFSET_REG0_OFFSET : 0x%0x\n",
				edma_reg_read(EDMA_REG_DDRQ_DATA_OFFSET_REG0_OFFSET));

	data = EDMA_REG_DDRQ_DATA_OFFSET_REG2_CFG_SET(EDMA_DDRQ_GBL_DATA_OFFSET_REG2_VAL) |
		EDMA_REG_DDRQ_DATA_OFFSET_REG3_CFG_SET(EDMA_DDRQ_GBL_DATA_OFFSET_REG3_VAL);
	edma_reg_write(EDMA_REG_DDRQ_DATA_OFFSET_REG1_OFFSET, data);
	edma_debug("EDMA_REG_DDRQ_DATA_OFFSET_REG1_OFFSET : 0x%0x\n",
				edma_reg_read(EDMA_REG_DDRQ_DATA_OFFSET_REG1_OFFSET));

	/*
	 * Configure DDRQ ENQ CTRL configurations for all the passthrough modes
	 */
	pt_src_profile.esramq_src_profile_en = A_TRUE;
	pt_src_profile.isramq_src_profile_en = A_TRUE;
	pt_src_profile.src_profile = EDMA_PORT_SRC_PROFILE;
	if (edma_ddrq_enq_ctrl_cfg(FAL_PASSTHROUGH_MODE_FULL, &pt_src_profile, true)) {
		edma_err("Error in configuring ENQ CTRL for %d PT mode\n", FAL_PASSTHROUGH_MODE_FULL);
		return -EINVAL;
	}

	if (edma_ddrq_enq_ctrl_cfg(FAL_PASSTHROUGH_MODE_192_128, &pt_src_profile, false)) {
		edma_err("Error in configuring ENQ CTRL for %d PT mode\n", FAL_PASSTHROUGH_MODE_192_128);
		return -EINVAL;
	}

	if (edma_ddrq_enq_ctrl_cfg(FAL_PASSTHROUGH_MODE_NO, &pt_src_profile, false)) {
		edma_err("Error in configuring ENQ CTRL for %d PT mode\n", FAL_PASSTHROUGH_MODE_NO);
		return -EINVAL;
	}

	return 0;
}

/*
 * edma_ddrq_def_idv_cfg_set()
 *	API to set default values to the DDRQ AC queue configurations
 */
static int edma_ddrq_def_idv_cfg_set(edma_ddrq_idv_cfg_t *ddrq_idv_cfg)
{
	uint32_t cur_ddrq_bm_word, bit_set;
	nss_dp_ddrq_obj_id_t ddrq_obj_id;

	ddrq_obj_id.cfg_type= NSS_DP_DDRQ_CFG_TYPE_PORT;
	cur_ddrq_bm_word = ddrq_idv_cfg->ddrq_en_port_bm;
	edma_debug("ddrq_en_port_bm 0x%0x\n", cur_ddrq_bm_word);
	while (cur_ddrq_bm_word) {
		bit_set = ffs((uint32_t)cur_ddrq_bm_word);
		ddrq_obj_id.cfg_id = bit_set;
		edma_ddrq_cfg_set(&ddrq_obj_id, &ddrq_idv_cfg->ddrq_ac_cfg);

		cur_ddrq_bm_word &= ~(1 << (bit_set - 1));
	}

	return 0;
}

/*
 * edma_ddrq_def_grp_cfg_set()
 *	API to set default DDRQ group configurations
 */
static int edma_ddrq_def_grp_cfg_set(edma_ddrq_grp_cfg_t *ddrq_grp_cfg)
{
	uint32_t ddrq_grp_bm_word, bit_set, grp_id;

		ddrq_grp_bm_word = ddrq_grp_cfg->ddrq_grp_en_bm;
		edma_debug("ddrq_grp_en_bm: 0x%0x\n", ddrq_grp_bm_word);
		while (ddrq_grp_bm_word) {
			bit_set = ffs((uint32_t)ddrq_grp_bm_word);
			grp_id = bit_set - 1;
			edma_ddrq_grp_cfg_set(grp_id, &ddrq_grp_cfg->ddrq_grp_cfg);

			ddrq_grp_bm_word &= ~(1 << (bit_set - 1));
		}

	return 0;
}

/*
 * edma_ddrq_lp_cc_cfg()
 *	API to configure loopback cpu code configurations
 */
static int edma_ddrq_lp_cc_cfg(edma_ddrq_lp_cfg_t *lp_cfg)
{
	sw_error_t err;
	fal_passthrough_cpucode_t cc = {0};

	/*
	 * Get the CPU_CODE_0/CPU_CODE_1/Drop CPU code values
	 */
	err = fal_qm_passthrough_cpucode_get(EDMA_SWITCH_DEV_ID, &cc);
	if (err != SW_OK) {
		edma_err("Error in getting pt cpu code cfg\n");
		return -EINVAL;
	}
	edma_debug("cpucode0: %d, cpucode1: %d, drop cc: %d, qbase: %d\n", cc.cpucode[0], cc.cpucode[1],
					 cc.drop_cpucode, lp_cfg->queue_base);

	if (!ppe_drv_cc_ucast_qbase_profile_set(cc.cpucode[0], lp_cfg->queue_base)) {
		edma_err("Error in DDRQ loopback CPU code 0 (%d) queue base config\n", cc.cpucode[0]);
		return -EINVAL;
	}

	if (!ppe_drv_cc_ucast_qbase_profile_set(cc.cpucode[1], lp_cfg->queue_base)) {
		edma_err("Error in DDRQ loopback CPU code 1 (%d) queue base config\n", cc.cpucode[1]);
		return -EINVAL;
	}

	if (!ppe_drv_cc_ucast_qbase_profile_set(cc.drop_cpucode, lp_cfg->queue_base)) {
		edma_err("Error in DDRQ loopback drop cpu code (%d) queue base config\n", cc.drop_cpucode);
		return -EINVAL;
	}

	return 0;
}

/*
 * edma_ddrq_lp_fc_grp_id_set()
 *	API to configure loopback ring FC group id
 */
static int edma_ddrq_lp_fc_grp_id_set(edma_ddrq_lp_cfg_t *lp_cfg)
{
	uint32_t reg, data;

	/*
	 * DDRQ_TODO : Add input check for proper lp_id value (0-15)
	 */
	if ((lp_cfg->lp_id >= 0) && (lp_cfg->lp_id <= 5)) {
		reg = EDMA_REG_LP_FC_REG0;
	} else if ((lp_cfg->lp_id >= 6) && (lp_cfg->lp_id <= 11)) {
		reg = EDMA_REG_LP_FC_REG1;
	} else {
		reg = EDMA_REG_LP_FC_REG2;
	}

	data = edma_reg_read(reg);
	data |= ((lp_cfg->lp_fc_grp_id & EDMA_REG_LP_FC_GRP_ID_MASK) << ((lp_cfg->lp_id % 6) * 5));
	edma_reg_write(reg, data);

	edma_debug("fc grp reg : 0x%0x, data: 0x%0x, fc_grp_id: %d\n", reg, data, lp_cfg->lp_fc_grp_id);

	return 0;
}

/*
 * edma_ddrq_qid_to_lp_ring_mapping()
 *	API to configure DDRQ queue to loopback ring mapping
 */
static int edma_ddrq_qid_to_lp_ring_mapping(edma_ddrq_lp_cfg_t *lp_cfg)
{
	uint32_t lp_ring_edma_id = lp_cfg->lp_id + EDMA_LP_RING_ID_BASE;
	uint32_t lp_q  = lp_cfg->queue_base;
	uint32_t lp_max_q = lp_q + lp_cfg->num_queues;
	uint32_t qid, reg_index, data, cur_data;

	edma_debug("lp_edma_id: %d, lp_q: %d, lp_max_q: %d\n",
			lp_ring_edma_id, lp_q, lp_max_q);

	for (qid = lp_q; qid < lp_max_q; qid++) {
		reg_index = qid/EDMA_QID2RID_NUM_PER_REG;
		cur_data = edma_reg_read(EDMA_QID2RID_TABLE_MEM(reg_index));

		if ((qid % EDMA_QID2RID_NUM_PER_REG) == 0) {
			data = EDMA_RX_RING_ID_QUEUE0_SET(lp_ring_edma_id);
			cur_data = EDMA_RX_RING_ID_QUEUE0_RESET(cur_data);
		} else if ((qid % EDMA_QID2RID_NUM_PER_REG) == 1) {
			data = EDMA_RX_RING_ID_QUEUE1_SET(lp_ring_edma_id);
			cur_data = EDMA_RX_RING_ID_QUEUE1_RESET(cur_data);
		} else if ((qid % EDMA_QID2RID_NUM_PER_REG) == 2) {
			data = EDMA_RX_RING_ID_QUEUE2_SET(lp_ring_edma_id);
			cur_data = EDMA_RX_RING_ID_QUEUE2_RESET(cur_data);
		} else if ((qid % EDMA_QID2RID_NUM_PER_REG) == 3) {
			data = EDMA_RX_RING_ID_QUEUE3_SET(lp_ring_edma_id);
			cur_data = EDMA_RX_RING_ID_QUEUE3_RESET(cur_data);
		}

		data |= cur_data;
		edma_reg_write(EDMA_QID2RID_TABLE_MEM(reg_index), data);

		edma_info("LP QID2RID(%d) reg: 0x%0x, data: 0x%0x\n", qid,
				EDMA_QID2RID_TABLE_MEM(reg_index), data);
	}

	return 0;
}

/*
 * edma_ddrq_def_lp_cfg_set()
 *	API to configure default loopback ring configurations
 */
static int edma_ddrq_def_lp_cfg_set(edma_ddrq_lp_cfg_t *lp_cfg, edma_ddrq_idv_cfg_t *ddrq_idv_cfg)
{
	nss_dp_ddrq_obj_id_t ddrq_obj_id;

	/*
	 * Configure LP PPE queue to ring mapping
	 */
	edma_ddrq_qid_to_lp_ring_mapping(lp_cfg);

	/*
	 * Configure FC GRP ID for the loopback ring
	 */
	edma_ddrq_lp_fc_grp_id_set(lp_cfg);

	/*
	 * Map loopback ring queue to CPU_CODE_0, CPU_CODE_1, SPF_BYPASS & DDRQ SPECIAL service codes
	 */
	if (edma_ddrq_lp_cc_cfg(lp_cfg)) {
		edma_err("Error in setting DDRQ LP cpu codes configurations\n");
		return -EINVAL;
	}

	if (!ppe_drv_sc_ucast_qbase_profile_set(PPE_DRV_SC_DDRQ_LP_SC, lp_cfg->queue_base)) {
		edma_err("Error in setting service code queue base for DDRQ special loopback SC:%d\n", PPE_DRV_SC_DDRQ_LP_SC);
		return -EINVAL;
	}

	if (!ppe_drv_sc_ucast_qbase_profile_set(PPE_DRV_SC_DDRQ_LP_SC_BRIDGE, lp_cfg->queue_base)) {
		edma_err("Error in setting service code queue base for DDRQ special bridged loopback SC:%d\n", PPE_DRV_SC_DDRQ_LP_SC_BRIDGE);
		return -EINVAL;
	}

	if (!ppe_drv_sc_ucast_qbase_profile_set(PPE_DRV_SC_SPF_BYPASS, lp_cfg->queue_base)) {
		edma_err("Error in setting service code queue base for DDRQ special loopback SC:%d\n", PPE_DRV_SC_SPF_BYPASS);
		return -EINVAL;
	}

	if (!ppe_drv_sc_ucast_qbase_profile_set(PPE_DRV_SC_DDRQ_LP_SC_IPSEC_ENCAP, lp_cfg->queue_base)) {
		edma_err("Error in setting service code queue base for DDRQ special loopback SC(IPEC_ENCAP):%d\n",
				PPE_DRV_SC_DDRQ_LP_SC_IPSEC_ENCAP);
		return -EINVAL;
	}

	if (!ppe_drv_sc_ucast_qbase_profile_set(PPE_DRV_SC_DDRQ_LP_SC_IPSEC_DECAP, lp_cfg->queue_base)) {
		edma_err("Error in setting service code queue base for DDRQ special loopback SC(IPSEC_DECAP):%d\n",
				PPE_DRV_SC_DDRQ_LP_SC_IPSEC_DECAP);
		return -EINVAL;
	}

	if (fal_qm_passthrough_cpucode_en_set(EDMA_SWITCH_DEV_ID, FAL_PASSTHROUGH_MODE_192_128, A_TRUE)) {
		edma_err("Error in setting cpucode en cfg for %d PT mode\n", FAL_PASSTHROUGH_MODE_192_128);
		return -EINVAL;
	}

	if (fal_qm_passthrough_cpucode_en_set(EDMA_SWITCH_DEV_ID, FAL_PASSTHROUGH_MODE_NO, A_TRUE)) {
		edma_err("Error in setting cpucode en cfg for %d PT mode\n", FAL_PASSTHROUGH_MODE_NO);
		return -EINVAL;
	}

	/*
	 * Configure loopback ring's AC QUEUE configuration
	 */
	ddrq_obj_id.cfg_type = NSS_DP_DDRQ_CFG_TYPE_LP;
	ddrq_obj_id.cfg_id = lp_cfg->lp_id;
	edma_ddrq_cfg_set(&ddrq_obj_id, &ddrq_idv_cfg->ddrq_ac_cfg);

	return 0;
}

/*
 * edma_ddrq_occupancy_stats_reset()
 *	API to reset DDRQ occupancy statistics
 */
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_reset(void)
{
	uint32_t data;

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	data = edma_reg_read(EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_CTR_OFFSET);

	data |= EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_CLEAN_SET(EDMA_DDRQ_DBG_CNT_BYTE_PKT_CLEAN_SET);
	edma_reg_write(EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_CTR_OFFSET, data);

	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_occupancy_stats_start()
 *	API to start DDRQ occupancy test
 */
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_start(void)
{
	uint32_t data;

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	data = edma_reg_read(EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_CTR_OFFSET);

	data |= EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_GO_SET(EDMA_DDRQ_DBG_CNT_BYTE_PKT_GO_START);
	edma_reg_write(EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_CTR_OFFSET, data);

	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_occupancy_stats_stop()
 *	API to stop DDRQ occupancy test
 */
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_stop(void)
{
	uint32_t data;

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	data = edma_reg_read(EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_CTR_OFFSET);

	data |= EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_GO_SET(EDMA_DDRQ_DBG_CNT_BYTE_PKT_GO_STOP);
	edma_reg_write(EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_CTR_OFFSET, data);

	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_occupancy_stats_restart()
 *	API to restart DDRQ occupancy stats test
 */
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_restart(void)
{
	uint32_t data;

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	edma_ddrq_occupancy_stats_reset();

	data = edma_reg_read(EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_CTR_OFFSET);

	data |= EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_GO_SET(EDMA_DDRQ_DBG_CNT_BYTE_PKT_GO_START);
	edma_reg_write(EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_CTR_OFFSET, data);

	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_occupancy_stats_threshold_get()
 *	API to get DDRQ occupancy threshold configuration
 */
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_threshold_get(uint32_t ddrq_id, nss_dp_ddrq_occupancy_threshold_t *threshold)
{
	edma_ddrq_occupancy_threshold_u ddrq_occ_thres_l = {0};

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	edma_ddrq_dbg_cnt_threshold_get(ddrq_id, &ddrq_occ_thres_l);
	edma_ddrq_dbg_cnt_threshold_copy_from_local(threshold, &ddrq_occ_thres_l.ddrq_occ_thres_cfg);
	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_occupancy_stats_threshold_set()
 *	API to set DDRQ occupancy threshold configuration
 */
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_threshold_set(uint32_t ddrq_id, nss_dp_ddrq_occupancy_threshold_t *threshold)
{
	edma_ddrq_occupancy_threshold_u ddrq_occ_thres_l = {0};

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	edma_ddrq_dbg_cnt_threshold_get(ddrq_id, &ddrq_occ_thres_l);
	edma_ddrq_dbg_cnt_threshold_copy_new_cfg(&ddrq_occ_thres_l.ddrq_occ_thres_cfg, threshold);
	edma_ddrq_dbg_cnt_threshold_set(ddrq_id, &ddrq_occ_thres_l);

	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_occupancy_stats_get()
 *	API to get DDRQ occupancy stats
 */
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_get(uint32_t ddrq_id, nss_dp_ddrq_occupancy_stats_t *ddrq_stats)
{
	edma_ddrq_occupancy_stats_u ddrq_occ_stats_l = {0};

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	edma_ddrq_dbg_cnt_occ_stats_get(ddrq_id, &ddrq_occ_stats_l);
	edma_ddrq_dbg_cnt_occ_stats_copy_from_local(ddrq_stats, &ddrq_occ_stats_l.ddrq_occ_stats);
	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_occupancy_stats_status_get()
 *	API to get status of the DDRQ occupancy stats
 */
nss_dp_ddrq_ret_t edma_ddrq_occupancy_stats_status_get(uint32_t ddrq_id, bool *status)
{
	uint32_t data, reg_offset;

	if (!edma_gbl_ctx->ddrq_def_cfg.ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return DDRQ_RET_ERR;
	}

	reg_offset = (ddrq_id >> EDMA_DDRQ_MAX_CNT_WORD);
	data =  edma_reg_read(EDMA_REG_DDRQ_DBG_CNT_BYTE_PKT_STATUS_OFFSET(reg_offset));
	*status = ((data & (1 << (ddrq_id % EDMA_BITS_IN_WORD))) ? true : false);

	return DDRQ_RET_SUCCESS;
}

/*
 * edma_ddrq_set_def_cfg()
 *	API to set DDRQ default configurations
 */
static int edma_ddrq_set_def_cfg(edma_ddrq_cfg_t *ddrq_cfg)
{
	if (!ddrq_cfg->ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return 0;
	}

	if (edma_ddrq_def_gbl_cfg_set(ddrq_cfg)) {
		edma_err("Error in setting the DDRQ global default configurations\n");
		return -EINVAL;
	}

	if (edma_ddrq_def_idv_cfg_set(&ddrq_cfg->ddrq_idv_cfg)) {
		edma_err("Error in setting the DDRQ individual default configurations\n");
		return -EINVAL;
	}

	if (edma_ddrq_def_grp_cfg_set(&ddrq_cfg->ddrq_grp_cfg)) {
		edma_err("Error in setting the DDRQ group default configurations\n");
		return -EINVAL;
	}

	if (edma_ddrq_def_lp_cfg_set(&ddrq_cfg->ddrq_lp_cfg, &ddrq_cfg->ddrq_idv_cfg)) {
		edma_err("Error in setting the DDRQ loopback default configurations\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * edma_ddrq_get_def_cfg()
 *	API to get DDRQ default configurations
 */
static int32_t edma_ddrq_get_def_cfg(edma_ddrq_cfg_t *ddrq_cfg)
{
	edma_ddrq_gbl_cfg_t *ddrq_gbl_cfg = &ddrq_cfg->ddrq_gbl_cfg;
	edma_ddrq_idv_cfg_t *ddrq_idv_cfg = &ddrq_cfg->ddrq_idv_cfg;
	edma_ddrq_grp_cfg_t *ddrq_grp_cfg = &ddrq_cfg->ddrq_grp_cfg;
	edma_ddrq_lp_cfg_t *ddrq_lp_cfg = &ddrq_cfg->ddrq_lp_cfg;

	/*
	 * Get DDRQ global default configurations
	 */
	ddrq_gbl_cfg->ddrq_en_sw = edma_ddrq_gbl_en_sw;
	ddrq_gbl_cfg->ddrq_en_hw = edma_ddrq_gbl_en_hw;
	ddrq_gbl_cfg->ddrq_desc_pf_thres = edma_ddrq_desc_pf_thres;
	ddrq_gbl_cfg->ddrq_desc_wb_thres = edma_ddrq_desc_wb_thres;
	ddrq_gbl_cfg->ddrq_data_offset = edma_ddrq_data_offset;
	ddrq_gbl_cfg->ddrq_data_offset0 = edma_ddrq_gbl_data_offset0;
	ddrq_gbl_cfg->ddrq_pkt_data_align = 1;	// Non configurable

	if ((edma_ddrq_blk_num < 0) || (edma_ddrq_blk_num >= EDMA_DDRQ_BLK_NUM_MAX_ARR_IDX)) {
		edma_err("Invalid DDRQ BLK num value (%d), allowed range is 0 to %d\n", edma_ddrq_blk_num, EDMA_DDRQ_BLK_NUM_MAX_ARR_IDX - 1);
		ddrq_gbl_cfg->ddrq_blk_num_cfg = NSS_DP_EDMA_DDRQ_BLK_NUM_DEF;
	} else {
		ddrq_gbl_cfg->ddrq_blk_num_cfg = edma_ddrq_blk_num;
	}

	if ((edma_ddrq_blk_size < 0) || (edma_ddrq_blk_size >= EDMA_DDRQ_BLK_SIZE_MAX_ARR_IDX)) {
		edma_err("Invalid DDRQ BLK size value (%d), allowed range is 0 to %d\n", edma_ddrq_blk_size, EDMA_DDRQ_BLK_SIZE_MAX_ARR_IDX - 1);
		ddrq_gbl_cfg->ddrq_blk_size_cfg = NSS_DP_EDMA_DDRQ_BLK_SIZE_DEF;
	} else {
		ddrq_gbl_cfg->ddrq_blk_size_cfg = edma_ddrq_blk_size;
	}

	/*
	 * Get DDRQ AC queue default configurations
	 */
	edma_ddrq_cfg_set_inval(&ddrq_idv_cfg->ddrq_ac_cfg);
	ddrq_idv_cfg->ddrq_ac_cfg.ac_cfg_ac_en = edma_ddrq_ac_queue_ac_en;
	ddrq_idv_cfg->ddrq_ac_cfg.ac_cfg_color_aware = edma_ddrq_ac_queue_color_aware;
	ddrq_idv_cfg->ddrq_ac_cfg.ac_cfg_wred_en = edma_ddrq_ac_queue_wred_en;
	ddrq_idv_cfg->ddrq_ac_cfg.ac_cfg_grp_id = edma_ddrq_ac_queue_grp_id;
	ddrq_idv_cfg->ddrq_ac_cfg.ac_cfg_pre_alloc_limit = edma_ddrq_ac_queue_pre_alloc;
	ddrq_idv_cfg->ddrq_ac_cfg.ac_cfg_shared_dynamic = edma_ddrq_ac_queue_shared_dynamic;
	ddrq_idv_cfg->ddrq_ac_cfg.ac_cfg_shared_weight = edma_ddrq_ac_queue_shared_weight;
	ddrq_idv_cfg->ddrq_ac_cfg.ddrq_state = EDMA_DDRQ_AC_QUEUE_STATE_ENABLED;
	ddrq_idv_cfg->ddrq_en_port_bm = edma_ddrq_en_port_bm;
	if (edma_ddrq_ac_queue_shared_ceiling != EDMA_DDRQ_NO_OP_DEF_VAL) {
		ddrq_idv_cfg->ddrq_ac_cfg.ac_cfg_shared_ceiling = edma_ddrq_ac_queue_shared_ceiling;
	} else {
		ddrq_idv_cfg->ddrq_ac_cfg.ac_cfg_shared_ceiling = EDMA_DDRQ_GET_SHARED_CEILING(edma_ddrq_blk_num_map[ddrq_gbl_cfg->ddrq_blk_num_cfg]);
	}

	/*
	 * Get DDRQ group default configurations
	 */
	edma_ddrq_grp_cfg_set_inval(&ddrq_grp_cfg->ddrq_grp_cfg);
	ddrq_grp_cfg->ddrq_grp_cfg.ac_cfg_ac_en= edma_ddrq_grp_ac_en;
	ddrq_grp_cfg->ddrq_grp_cfg.ac_cfg_color_aware = edma_ddrq_grp_color_aware;
	ddrq_grp_cfg->ddrq_grp_en_bm = edma_ddrq_grp_id_bm;
	if (edma_ddrq_grp_shrd_limit != EDMA_DDRQ_NO_OP_DEF_VAL) {
		ddrq_grp_cfg->ddrq_grp_cfg.ac_grp_gap_shrd_limit = edma_ddrq_grp_shrd_limit;
	} else {
		ddrq_grp_cfg->ddrq_grp_cfg.ac_grp_gap_shrd_limit = EDMA_DDRQ_GET_SHARED_LIMIT(edma_ddrq_blk_num_map[ddrq_gbl_cfg->ddrq_blk_num_cfg], EDMA_DDRQ_QUEUE_PRE_ALLOC_LIMIT_MIN);
	}
	if (edma_ddrq_grp_dp_thrd != EDMA_DDRQ_NO_OP_DEF_VAL) {
		ddrq_grp_cfg->ddrq_grp_cfg.ac_grp_dp_thrd = edma_ddrq_grp_dp_thrd;
	} else {
		ddrq_grp_cfg->ddrq_grp_cfg.ac_grp_dp_thrd = ddrq_grp_cfg->ddrq_grp_cfg.ac_grp_gap_shrd_limit -
			EDMA_DDRQ_AC_GRP_GAP_SHRD_LMT_DRP_THRES_VAL;
	}

	/*
	 * Get DDRQ loopback default configurations
	 */
	ddrq_lp_cfg->queue_base = edma_ddrq_lp_queue_base;
	ddrq_lp_cfg->num_queues = edma_ddrq_lp_num_queues;
	ddrq_lp_cfg->lp_id = edma_ddrq_lp_id;
	ddrq_lp_cfg->lp_fc_grp_id = edma_ddrq_lp_fc_grp_id;

	return 0;
}

/*
 * edma_ddrq_get_ddrq_mem_regions()
 *	API to fetch DDRQ memory regions detail from the DTS
 */
static int edma_ddrq_get_ddrq_mem_regions(void)
{
	int index;
	struct device_node *np = edma_gbl_ctx->device_node;
	struct reserved_mem *rmem;

	index = of_property_match_string(edma_gbl_ctx->device_node, "memory-region-names",
						EDMA_DDRQ_DATA_REGION);
	if (index < 0) {
		edma_err("Error in matching %s DDRQ region\n", EDMA_DDRQ_DATA_REGION);
		return index;
	}

	np = of_parse_phandle(edma_gbl_ctx->device_node, "memory-region", index);
	if (!np) {
		edma_err("Error in getting %s region index\n", EDMA_DDRQ_DATA_REGION);
		return -ENODEV;
	}

	rmem = of_reserved_mem_lookup(np);
	of_node_put(np);

	if (!rmem) {
		edma_err("Error in %s mem lookup\n", EDMA_DDRQ_DATA_REGION);
		edma_gbl_ctx->ddrq_def_cfg.ddrq_data_mem_reg.phy_addr = 0;
		edma_gbl_ctx->ddrq_def_cfg.ddrq_data_mem_reg.size = 0;
		return -ENODEV;
	}

	edma_gbl_ctx->ddrq_def_cfg.ddrq_data_mem_reg.phy_addr = rmem->base;
	edma_gbl_ctx->ddrq_def_cfg.ddrq_data_mem_reg.size = rmem->size;

	index = of_property_match_string(edma_gbl_ctx->device_node, "memory-region-names",
						EDMA_DDRQ_DESC_REGION);
	if (index < 0) {
		edma_err("Error in matching %s DDRQ region\n", EDMA_DDRQ_DESC_REGION);
		edma_gbl_ctx->ddrq_def_cfg.ddrq_data_mem_reg.phy_addr = 0;
		edma_gbl_ctx->ddrq_def_cfg.ddrq_data_mem_reg.size = 0;
		return index;
	}

	np = of_parse_phandle(edma_gbl_ctx->device_node, "memory-region", index);
	if (!np) {
		edma_err("Error in getting %s region index\n", EDMA_DDRQ_DESC_REGION);
		edma_gbl_ctx->ddrq_def_cfg.ddrq_data_mem_reg.phy_addr = 0;
		edma_gbl_ctx->ddrq_def_cfg.ddrq_data_mem_reg.size = 0;
		return -ENODEV;
	}

	rmem = of_reserved_mem_lookup(np);
	of_node_put(np);

	if (!rmem) {
		edma_err("Error in %s mem lookup\n", EDMA_DDRQ_DESC_REGION);
		edma_gbl_ctx->ddrq_def_cfg.ddrq_desc_mem_reg.phy_addr = 0;
		edma_gbl_ctx->ddrq_def_cfg.ddrq_desc_mem_reg.size = 0;
		return -ENODEV;
	}

	edma_gbl_ctx->ddrq_def_cfg.ddrq_desc_mem_reg.phy_addr = rmem->base;
	edma_gbl_ctx->ddrq_def_cfg.ddrq_desc_mem_reg.size = rmem->size;

	edma_warn("ddrq data reg addr: %pa, size: %zu, ddrq desc reg addr: %pa, size: %zu\n",
				&edma_gbl_ctx->ddrq_def_cfg.ddrq_data_mem_reg.phy_addr,
				edma_gbl_ctx->ddrq_def_cfg.ddrq_data_mem_reg.size,
				&edma_gbl_ctx->ddrq_def_cfg.ddrq_desc_mem_reg.phy_addr,
				edma_gbl_ctx->ddrq_def_cfg.ddrq_desc_mem_reg.size);
	return 0;
}

/*
 * edma_ddrq_init()
 *	API to initialize DDRQ related configurations
 */
int edma_ddrq_init(edma_ddrq_cfg_t *ddrq_cfg)
{
	if (edma_ddrq_get_def_cfg(ddrq_cfg)) {
		edma_err("Error in getting DDRQ default configuration\n");
		return -EINVAL;
	}

	if (!ddrq_cfg->ddrq_gbl_cfg.ddrq_en_sw) {
		edma_warn("DDRQ global software knob is disabled\n");
		return 0;
	}

	if (edma_ddrq_get_ddrq_mem_regions()) {
		edma_err("Error in reading DDRQ related DTS configurations\n");
		return -EINVAL;
	}

	if (edma_ddrq_set_def_cfg(ddrq_cfg)) {
		edma_err("Error in setting the DDRQ default configurations\n");
		return -EINVAL;
	}

	if (!ppe_drv_isram_queue_profile_init(edma_ddrq_isq_base)) {
		edma_err("Error in configuring ISRAM queue (%d) base for PPE ports\n", edma_ddrq_isq_base);
		return -EINVAL;
	}

	return 0;
}
