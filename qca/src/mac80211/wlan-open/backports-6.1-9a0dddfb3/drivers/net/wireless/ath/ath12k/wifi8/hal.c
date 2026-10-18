// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/nl80211.h>
#include <linux/types.h>
#include <linux/bitfield.h>
#include <linux/ctype.h>
#include "hw.h"
#include "hal_desc.h"
#include "../hal.h"
#include "hal.h"
#include "../debug.h"
#include "../hif.h"
#include "../pcic.h"
#include "ppeds.h"
#include "../hal.h"

static unsigned int ath12k_hal_reo1_ring_id_offset(struct ath12k_hal *hal)
{
	return HAL_REO1_RING_ID(hal) - HAL_REO1_RING_BASE_LSB(hal);
}

static unsigned int ath12k_hal_reo1_ring_msi1_base_lsb_offset(struct ath12k_hal *hal)
{
	return HAL_REO1_RING_MSI1_BASE_LSB(hal) - HAL_REO1_RING_BASE_LSB(hal);
}

static unsigned int ath12k_hal_reo1_ring_msi1_base_msb_offset(struct ath12k_hal *hal)
{
	return HAL_REO1_RING_MSI1_BASE_MSB(hal) - HAL_REO1_RING_BASE_LSB(hal);
}

static unsigned int ath12k_hal_reo1_ring_msi1_data_offset(struct ath12k_hal *hal)
{
	return HAL_REO1_RING_MSI1_DATA(hal) - HAL_REO1_RING_BASE_LSB(hal);
}

static unsigned int ath12k_hal_reo1_ring_base_msb_offset(struct ath12k_hal *hal)
{
	return HAL_REO1_RING_BASE_MSB(hal) - HAL_REO1_RING_BASE_LSB(hal);
}

static unsigned int ath12k_hal_reo1_ring_producer_int_setup_offset(struct ath12k_hal *hal)
{
	return HAL_REO1_RING_PRODUCER_INT_SETUP(hal) - HAL_REO1_RING_BASE_LSB(hal);
}

static unsigned int ath12k_hal_reo1_ring_hp_addr_lsb_offset(struct ath12k_hal *hal)
{
	return HAL_REO1_RING_HP_ADDR_LSB(hal) - HAL_REO1_RING_BASE_LSB(hal);
}

static unsigned int ath12k_hal_reo1_ring_hp_addr_msb_offset(struct ath12k_hal *hal)
{
	return HAL_REO1_RING_HP_ADDR_MSB(hal) - HAL_REO1_RING_BASE_LSB(hal);
}

static unsigned int ath12k_hal_reo1_ring_misc_offset(struct ath12k_hal *hal)
{
	return HAL_REO1_RING_MISC(hal) - HAL_REO1_RING_BASE_LSB(hal);
}

void ath12k_wifi8_hal_srng_hw_enable(struct ath12k_base *ab,
		struct hal_srng *srng)
{
	u32 reg_base, val, addr;
	struct ath12k_hal *hal = &ab->hal;

	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R0];
	if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
		if (srng->ring_id == HAL_SRNG_RING_ID_WBM_IDLE_LINK)
			addr = HAL_SEQ_WCSS_UMAC_WBM_REG +
				HAL_WBM_IDLE_LINK_RING_MISC_ADDR(hal);
		else
			addr = reg_base + HAL_TCL1_RING_MISC_OFFSET(hal);
		val = ath12k_hif_read32(ab, addr);
		val |= HAL_TCL1_RING_MISC_SRNG_ENABLE;
		ath12k_hif_write32(ab, addr, val);
	} else {
		val = ath12k_hif_read32(ab, reg_base + HAL_REO1_RING_MISC_OFFSET);
		val |= HAL_REO1_RING_MISC_SRNG_ENABLE;
		ath12k_hif_write32(ab, reg_base + HAL_REO1_RING_MISC_OFFSET, val);
	}
}

void ath12k_wifi8_hal_srng_idx_update_addr(struct ath12k_base *ab, struct hal_srng *srng,
					void __iomem *hp_vaddr, dma_addr_t hp_paddr,
					void __iomem *tp_vaddr, dma_addr_t tp_paddr)
{
	struct ath12k_hal *hal = &ab->hal;
	u32 reg_base;

	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R0];

	/*
	 * Disable the ring.
	 */
	ath12k_wifi8_hal_srng_hw_disable(ab, srng);

	if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
		ath12k_hif_write32(ab,
				reg_base + HAL_TCL1_RING_TP_ADDR_LSB_OFFSET(hal),
				tp_paddr & HAL_ADDR_LSB_REG_MASK);
		ath12k_hif_write32(ab,
				reg_base + HAL_TCL1_RING_TP_ADDR_MSB_OFFSET(hal),
				((u64)tp_paddr >> HAL_ADDR_MSB_REG_SHIFT));
		srng->u.src_ring.tp_addr = tp_vaddr;
		ath12k_dbg(ab, ATH12K_DBG_HAL,
				"PPEDS SRC_SRNG tp_paddr:%pad tp_vaddr:%p ring_id:%d\n",
				&tp_paddr, tp_vaddr, srng->ring_id);
	} else {
		ath12k_hif_write32(ab,
				reg_base + ath12k_hal_reo1_ring_hp_addr_lsb_offset(hal),
				hp_paddr & HAL_ADDR_LSB_REG_MASK);
		ath12k_hif_write32(ab,
				reg_base + ath12k_hal_reo1_ring_hp_addr_msb_offset(hal),
				((u64)hp_paddr >> HAL_ADDR_MSB_REG_SHIFT));
		srng->u.dst_ring.hp_addr = hp_vaddr;
		ath12k_dbg(ab, ATH12K_DBG_HAL,
				"PPEDS DST_SRNG hp_paddr:%pad hp_vaddr:%p ring_id:%d\n",
				&hp_paddr, hp_vaddr, srng->ring_id);
	}

	/*
	 * Enable the ring.
	 */
	ath12k_wifi8_hal_srng_hw_enable(ab, srng);
}

void ath12k_wifi8_hal_ce_dst_setup(struct ath12k_base *ab,
				   struct hal_srng *srng, int ring_num)
{
	struct hal_srng_config *srng_config = &ab->hal.srng_config[HAL_CE_DST];
	u32 addr;
	u32 val;

	addr = HAL_CE_DST_RING_CTRL +
	       srng_config->reg_start[HAL_SRNG_REG_GRP_R0] +
	       ring_num * srng_config->reg_size[HAL_SRNG_REG_GRP_R0];

	val = ath12k_hif_read32(ab, addr);
	val &= ~HAL_CE_DST_R0_DEST_CTRL_MAX_LEN;
	val |= u32_encode_bits(srng->u.dst_ring.max_buffer_length,
			       HAL_CE_DST_R0_DEST_CTRL_MAX_LEN);
	ath12k_hif_write32(ab, addr, val);
}

void ath12k_wifi8_hal_srng_dst_hw_init(struct ath12k_base *ab,
				       struct hal_srng *srng,
				       u32 restore_idx)
{
	struct ath12k_hal *hal = &ab->hal;
	u32 val;
	u64 hp_addr;
	u32 reg_base;

	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R0];

	if (srng->flags & HAL_SRNG_FLAGS_MSI_INTR) {
		ath12k_hif_write32(ab, reg_base +
				   ath12k_hal_reo1_ring_msi1_base_lsb_offset(hal),
				   srng->msi_addr);

		val = u32_encode_bits(((u64)srng->msi_addr >> HAL_ADDR_MSB_REG_SHIFT),
				      HAL_REO1_RING_MSI1_BASE_MSB_ADDR) |
				      HAL_REO1_RING_MSI1_BASE_MSB_MSI1_ENABLE;
		ath12k_hif_write32(ab, reg_base +
				   ath12k_hal_reo1_ring_msi1_base_msb_offset(hal), val);

		ath12k_hif_write32(ab,
				   reg_base + ath12k_hal_reo1_ring_msi1_data_offset(hal),
				   srng->msi_data);
	}

	ath12k_hif_write32(ab, reg_base, srng->ring_base_paddr);

	val = u32_encode_bits(((u64)srng->ring_base_paddr >> HAL_ADDR_MSB_REG_SHIFT),
			      HAL_REO1_RING_BASE_MSB_RING_BASE_ADDR_MSB) |
	      u32_encode_bits((srng->entry_size * srng->num_entries),
			      HAL_REO1_RING_BASE_MSB_RING_SIZE);
	ath12k_hif_write32(ab, reg_base + ath12k_hal_reo1_ring_base_msb_offset(hal), val);

	val = u32_encode_bits(srng->ring_id, HAL_REO1_RING_ID_RING_ID) |
	      u32_encode_bits(srng->entry_size, HAL_REO1_RING_ID_ENTRY_SIZE);
	ath12k_hif_write32(ab, reg_base + ath12k_hal_reo1_ring_id_offset(hal), val);

	/* interrupt setup */
	val = u32_encode_bits((srng->intr_timer_thres_us >> 3),
			      HAL_REO1_RING_PRDR_INT_SETUP_INTR_TMR_THOLD);

	val |= u32_encode_bits((srng->intr_batch_cntr_thres_entries * srng->entry_size),
				HAL_REO1_RING_PRDR_INT_SETUP_BATCH_COUNTER_THOLD);

	ath12k_hif_write32(ab,
			   reg_base + ath12k_hal_reo1_ring_producer_int_setup_offset(hal),
			   val);

	hp_addr = hal->rdp.paddr +
		  ((unsigned long)srng->u.dst_ring.hp_addr -
		   (unsigned long)hal->rdp.vaddr);
	ath12k_hif_write32(ab, reg_base + ath12k_hal_reo1_ring_hp_addr_lsb_offset(hal),
			   hp_addr & HAL_ADDR_LSB_REG_MASK);
	ath12k_hif_write32(ab, reg_base + ath12k_hal_reo1_ring_hp_addr_msb_offset(hal),
			   hp_addr >> HAL_ADDR_MSB_REG_SHIFT);

	/* Initialize head and tail pointers to indicate ring is empty */
	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R2];
	ath12k_hif_write32(ab, reg_base,  restore_idx * srng->entry_size);
	ath12k_hif_write32(ab, reg_base + HAL_REO1_RING_TP_OFFSET,
			   restore_idx * srng->entry_size);
	*srng->u.dst_ring.hp_addr =  restore_idx * srng->entry_size;
	srng->u.dst_ring.tp =  restore_idx * srng->entry_size;

	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R0];
	val = 0;
	if (srng->flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP)
		val |= HAL_REO1_RING_MISC_DATA_TLV_SWAP;
	if (srng->flags & HAL_SRNG_FLAGS_RING_PTR_SWAP)
		val |= HAL_REO1_RING_MISC_HOST_FW_SWAP;
	if (srng->flags & HAL_SRNG_FLAGS_MSI_SWAP)
		val |= HAL_REO1_RING_MISC_MSI_SWAP;
	val |= HAL_REO1_RING_MISC_SRNG_ENABLE;
	val |= HAL_REO1_RING_MISC_RING_ID_DISABLE;
	val |= HAL_REO1_RING_MISC_LOOPCNT_DISABLE;

	ath12k_hif_write32(ab, reg_base + ath12k_hal_reo1_ring_misc_offset(hal), val);
}

void ath12k_wifi8_hal_srng_src_hw_init(struct ath12k_base *ab,
				       struct hal_srng *srng,
				       u32 restore_idx)
{
	struct ath12k_hal *hal = &ab->hal;
	u32 val;
	u64 tp_addr;
	u32 reg_base;

	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R0];

	if (srng->flags & HAL_SRNG_FLAGS_MSI_INTR) {
		ath12k_hif_write32(ab, reg_base +
				   HAL_TCL1_RING_MSI1_BASE_LSB_OFFSET(hal),
				   srng->msi_addr);

		val = u32_encode_bits(((u64)srng->msi_addr >> HAL_ADDR_MSB_REG_SHIFT),
				      HAL_TCL1_RING_MSI1_BASE_MSB_ADDR) |
				      HAL_TCL1_RING_MSI1_BASE_MSB_MSI1_ENABLE;
		ath12k_hif_write32(ab, reg_base +
				       HAL_TCL1_RING_MSI1_BASE_MSB_OFFSET(hal),
				   val);

		ath12k_hif_write32(ab, reg_base +
				       HAL_TCL1_RING_MSI1_DATA_OFFSET(hal),
				   srng->msi_data);
	}

	ath12k_hif_write32(ab, reg_base, srng->ring_base_paddr);

	val = u32_encode_bits(((u64)srng->ring_base_paddr >> HAL_ADDR_MSB_REG_SHIFT),
			      HAL_TCL1_RING_BASE_MSB_RING_BASE_ADDR_MSB) |
	      u32_encode_bits((srng->entry_size * srng->num_entries),
			      HAL_TCL1_RING_BASE_MSB_RING_SIZE);
	ath12k_hif_write32(ab, reg_base + HAL_TCL1_RING_BASE_MSB_OFFSET(hal), val);

	val = u32_encode_bits(srng->entry_size, HAL_REO1_RING_ID_ENTRY_SIZE);
	ath12k_hif_write32(ab, reg_base + HAL_TCL1_RING_ID_OFFSET(hal), val);

	val = u32_encode_bits(srng->intr_timer_thres_us,
			      HAL_TCL1_RING_CONSR_INT_SETUP_IX0_INTR_TMR_THOLD);

	val |= u32_encode_bits((srng->intr_batch_cntr_thres_entries * srng->entry_size),
			       HAL_TCL1_RING_CONSR_INT_SETUP_IX0_BATCH_COUNTER_THOLD);

	ath12k_hif_write32(ab,
			   reg_base + HAL_TCL1_RING_CONSR_INT_SETUP_IX0_OFFSET(hal),
			   val);

	val = 0;
	if (srng->flags & HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN) {
		val |= u32_encode_bits(srng->u.src_ring.low_threshold,
				       HAL_TCL1_RING_CONSR_INT_SETUP_IX1_LOW_THOLD);
	}
	ath12k_hif_write32(ab,
			   reg_base + HAL_TCL1_RING_CONSR_INT_SETUP_IX1_OFFSET(hal),
			   val);

	if (srng->ring_id != HAL_SRNG_RING_ID_WBM_IDLE_LINK) {
		tp_addr = hal->rdp.paddr +
			  ((unsigned long)srng->u.src_ring.tp_addr -
			   (unsigned long)hal->rdp.vaddr);
		ath12k_hif_write32(ab,
				   reg_base + HAL_TCL1_RING_TP_ADDR_LSB_OFFSET(hal),
				   tp_addr & HAL_ADDR_LSB_REG_MASK);
		ath12k_hif_write32(ab,
				   reg_base + HAL_TCL1_RING_TP_ADDR_MSB_OFFSET(hal),
				   tp_addr >> HAL_ADDR_MSB_REG_SHIFT);
	}

	/* Initialize head and tail pointers to indicate ring is empty */
	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R2];
	ath12k_hif_write32(ab, reg_base, restore_idx * srng->entry_size);
	ath12k_hif_write32(ab, reg_base + HAL_TCL1_RING_TP_OFFSET,
			   restore_idx * srng->entry_size);
	*srng->u.src_ring.tp_addr =  restore_idx * srng->entry_size;
	srng->u.src_ring.hp =  restore_idx * srng->entry_size;

	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R0];
	val = 0;
	if (srng->flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP)
		val |= HAL_TCL1_RING_MISC_DATA_TLV_SWAP;
	if (srng->flags & HAL_SRNG_FLAGS_RING_PTR_SWAP)
		val |= HAL_TCL1_RING_MISC_HOST_FW_SWAP;
	if (srng->flags & HAL_SRNG_FLAGS_MSI_SWAP)
		val |= HAL_TCL1_RING_MISC_MSI_SWAP;

	/* Loop count is not used for SRC rings */
	val |= HAL_TCL1_RING_MISC_MSI_LOOPCNT_DISABLE;
	val |= HAL_TCL1_RING_MISC_MSI_RING_ID_DISABLE;
	val |= HAL_TCL1_RING_MISC_SRNG_ENABLE;

	/* descriptor/head_ptr is from/to host DDR */
	val |= HAL_TCL1_RING_MISC_TRANSACTION_TYPE;
	ath12k_hif_write32(ab, reg_base + HAL_TCL1_RING_MISC_OFFSET(hal), val);
}

void ath12k_wifi8_hal_set_umac_srng_ptr_addr(struct ath12k_base *ab,
					     struct hal_srng *srng,
					     enum hal_ring_type type, int ring_num)
{
	u32 reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R2];

	if (srng->ring_dir == HAL_SRNG_DIR_DST) {
		if (!ab->hw_params->supports_shadow_regs) {
			srng->u.dst_ring.tp_addr =
				(u32 *)((unsigned long)ab->mem + reg_base +
				(HAL_REO1_RING_TP - HAL_REO1_RING_HP));
#ifdef CPTCFG_EXT_IPA_OFFLOAD
			if (type  == HAL_TX_COMPLETION ||
				    type == HAL_TQM2PPE ||
				    type == HAL_REO_DST) {
#else
			if (type  == HAL_TX_COMPLETION) {
#endif
				if (ab->hif.bus == ATH12K_BUS_PCI ||
				    ab->hif.bus == ATH12K_BUS_HYBRID){
					srng->u.dst_ring.tp_addr_direct =
						(u32 *)((unsigned long)ab->mem +
						(reg_base & WINDOW_RANGE_MASK) +
						HAL_DP_REG_WINDOW_OFFSET +
						(HAL_REO1_RING_TP - HAL_REO1_RING_HP));
				} else {
					srng->u.dst_ring.tp_addr_direct =
						srng->u.dst_ring.tp_addr;
				}
			}
		} else {
			ath12k_dbg(ab, ATH12K_DBG_HAL,
				   "type %d ring_num %d target_reg 0x%x shadow 0x%lx\n",
				   type, ring_num,
				   reg_base + HAL_REO1_RING_TP - HAL_REO1_RING_HP,
				   (unsigned long)srng->u.dst_ring.tp_addr -
				   (unsigned long)ab->mem);
		}
	} else  {
		if (!ab->hw_params->supports_shadow_regs) {
			srng->u.src_ring.hp_addr =
				(u32 *)((unsigned long)ab->mem + reg_base);
#ifdef CPTCFG_EXT_IPA_OFFLOAD
			if (type  == HAL_TCL_DATA ||
				    type == HAL_PPE2TCL ||
				    type == HAL_PPE2WBM_BUF) {
#else
			if (type  == HAL_TCL_DATA) {
#endif
				if (ab->hif.bus == ATH12K_BUS_PCI ||
				    ab->hif.bus == ATH12K_BUS_HYBRID){
					srng->u.src_ring.hp_addr_direct =
						(u32 *)((unsigned long)ab->mem +
							HAL_DP_REG_WINDOW_OFFSET +
							(reg_base & WINDOW_RANGE_MASK));
				} else {
					srng->u.src_ring.hp_addr_direct =
						srng->u.src_ring.hp_addr;
				}
			}
		} else {
			ath12k_dbg(ab, ATH12K_DBG_HAL,
				   "hal type %d ring_num %d reg_base 0x%x shadow 0x%lx\n",
				   type, ring_num,
				   reg_base,
				   (unsigned long)srng->u.src_ring.hp_addr -
				   (unsigned long)ab->mem);
		}
	}
}

int ath12k_wifi8_hal_srng_get_ring_id(struct ath12k_hal *hal,
				      enum hal_ring_type type,
				      int ring_num, int mac_id)
{
	struct hal_srng_config *srng_config = &hal->srng_config[type];
	int ring_id;

	if (ring_num >= srng_config->max_rings) {
		ath12k_warn(hal, "invalid ring number :%d\n", ring_num);
		return -EINVAL;
	}

	ring_id = srng_config->start_ring_id + ring_num;
	if (srng_config->mac_type == ATH12K_HAL_SRNG_PMAC)
		ring_id += mac_id * HAL_SRNG_RINGS_PER_PMAC;

	if (WARN_ON(ring_id >= HAL_SRNG_RING_ID_MAX))
		return -EINVAL;

	return ring_id;
}

static
void ath12k_wifi8_hal_srng_update_hp_tp_addr(struct ath12k_base *ab,
					     int shadow_cfg_idx,
					     enum hal_ring_type ring_type,
					     int ring_num)
{
	struct hal_srng *srng;
	struct ath12k_hal *hal = &ab->hal;
	int ring_id;
	struct hal_srng_config *srng_config = &hal->srng_config[ring_type];

	ring_id = ath12k_wifi8_hal_srng_get_ring_id(hal, ring_type, ring_num,
						    0);
	if (ring_id < 0)
		return;

	srng = &hal->srng_list[ring_id];

	if (srng_config->ring_dir == HAL_SRNG_DIR_DST)
		srng->u.dst_ring.tp_addr = (u32 *)(HAL_SHADOW_REG(shadow_cfg_idx) +
						   (unsigned long)ab->mem);
	else
		srng->u.src_ring.hp_addr = (u32 *)(HAL_SHADOW_REG(shadow_cfg_idx) +
						   (unsigned long)ab->mem);
}

u32 ath12k_wifi8_hal_ce_get_desc_size(enum hal_ce_desc type)
{
	switch (type) {
	case HAL_CE_DESC_SRC:
		return sizeof(struct hal_ce_srng_src_desc);
	case HAL_CE_DESC_DST:
		return sizeof(struct hal_ce_srng_dest_desc);
	case HAL_CE_DESC_DST_STATUS:
		return sizeof(struct hal_ce_srng_dst_status_desc);
	}

	return 0;
}

int ath12k_wifi8_hal_srng_update_shadow_config(struct ath12k_base *ab,
					       enum hal_ring_type ring_type,
					       int ring_num)
{
	struct ath12k_hal *hal = &ab->hal;
	struct hal_srng_config *srng_config = &hal->srng_config[ring_type];
	int shadow_cfg_idx = hal->num_shadow_reg_configured;
	u32 target_reg;

	if (shadow_cfg_idx >= HAL_SHADOW_NUM_REGS_MAX)
		return -EINVAL;

	hal->num_shadow_reg_configured++;

	target_reg = srng_config->reg_start[HAL_HP_OFFSET_IN_REG_START];
	target_reg += srng_config->reg_size[HAL_HP_OFFSET_IN_REG_START] *
		ring_num;

	/* For destination ring, shadow the TP */
	if (srng_config->ring_dir == HAL_SRNG_DIR_DST)
		target_reg += HAL_OFFSET_FROM_HP_TO_TP;

	hal->shadow_reg_addr[shadow_cfg_idx] = target_reg;

	/* update hp/tp addr to hal structure*/
	ath12k_wifi8_hal_srng_update_hp_tp_addr(ab, shadow_cfg_idx, ring_type,
						ring_num);

	ath12k_dbg(ab, ATH12K_DBG_HAL,
		   "target_reg %x, shadow reg 0x%x shadow_idx 0x%x, ring_type %d, ring num %d",
		  target_reg,
		  HAL_SHADOW_REG(shadow_cfg_idx),
		  shadow_cfg_idx,
		  ring_type, ring_num);

	return 0;
}

void ath12k_wifi8_hal_ce_src_set_desc(struct hal_ce_srng_src_desc *desc,
				      dma_addr_t paddr,
				      u32 len, u32 id, u8 byte_swap_data)
{
	desc->buffer_addr_low = cpu_to_le32(paddr & HAL_ADDR_LSB_REG_MASK);
	desc->info0 = le16_encode_bits(((u64)paddr >> HAL_ADDR_MSB_REG_SHIFT),
				       HAL_CE_SRC_DESC_INFO0_ADDR_HI) |
		      le16_encode_bits(byte_swap_data,
				       HAL_CE_SRC_DESC_INFO0_BYTE_SWAP) |
		      le16_encode_bits(0, HAL_CE_SRC_DESC_INFO0_GATHER);
	desc->length = cpu_to_le16(len);
	desc->fw_metadata = cpu_to_le16(id);
}

void ath12k_wifi8_hal_ce_dst_set_desc(struct hal_ce_srng_dest_desc *desc,
				      dma_addr_t paddr)
{
	desc->buffer_addr_low = cpu_to_le32(paddr & HAL_ADDR_LSB_REG_MASK);
	desc->info0 = le16_encode_bits(((u64)paddr >> HAL_ADDR_MSB_REG_SHIFT),
				       HAL_CE_DEST_DESC_INFO0_ADDR_HI);
}

void ath12k_wifi8_hal_set_link_desc_addr(struct hal_wbm_link_desc *desc,
					 u32 cookie, dma_addr_t paddr,
					 u8 rbm)
{
	desc->buf_addr_info.info0 = le32_encode_bits((paddr & HAL_ADDR_LSB_REG_MASK),
						     BUFFER_ADDR_INFO0_ADDR);
	desc->buf_addr_info.info1 =
		le32_encode_bits(((u64)paddr >> HAL_ADDR_MSB_REG_SHIFT),
				 BUFFER_ADDR_INFO1_ADDR) |
		le32_encode_bits(rbm, BUFFER_ADDR_INFO1_RET_BUF_MGR) |
		le32_encode_bits(cookie, BUFFER_ADDR_INFO1_SW_COOKIE);
}

u32 ath12k_wifi8_hal_ce_dst_status_get_length(struct hal_ce_srng_dst_status_desc *desc)
{
	u32 len;

	len = le16_to_cpu(desc->length);

	return len;
}

void
ath12k_wifi8_hal_setup_link_idle_list(struct ath12k_base *ab,
				      struct hal_wbm_idle_scatter_list *sbuf,
				      u32 nsbufs, u32 tot_link_desc,
				      u32 end_offset)
{
	struct ath12k_hal *hal = &ab->hal;
	struct ath12k_buffer_addr *link_addr;
	int i;
	u32 reg_scatter_buf_sz = HAL_WBM_IDLE_SCATTER_BUF_SIZE / 64;
	u32 val;

	link_addr = (void *)sbuf[0].vaddr + HAL_WBM_IDLE_SCATTER_BUF_SIZE;

	for (i = 1; i < nsbufs; i++) {
		link_addr->info0 = cpu_to_le32(sbuf[i].paddr & HAL_ADDR_LSB_REG_MASK);

		link_addr->info1 =
			le32_encode_bits((u64)sbuf[i].paddr >> HAL_ADDR_MSB_REG_SHIFT,
					 HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_39_32) |
			le32_encode_bits(BASE_ADDR_MATCH_TAG_VAL,
					 HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_MATCH_TAG);

		link_addr = (void *)sbuf[i].vaddr +
			     HAL_WBM_IDLE_SCATTER_BUF_SIZE;
	}

	val = u32_encode_bits(reg_scatter_buf_sz, HAL_WBM_SCATTER_BUFFER_SIZE) |
	      u32_encode_bits(0x1, HAL_WBM_LINK_DESC_IDLE_LIST_MODE);

	ath12k_hif_write32(ab,
			   HAL_SEQ_WCSS_UMAC_WBM_REG +
			   HAL_WBM_R0_IDLE_LIST_CONTROL_ADDR(hal),
			   val);

	val = u32_encode_bits(reg_scatter_buf_sz * nsbufs,
			      HAL_WBM_SCATTER_RING_SIZE_OF_IDLE_LINK_DESC_LIST);
	ath12k_hif_write32(ab,
			   HAL_SEQ_WCSS_UMAC_WBM_REG +
			   HAL_WBM_R0_IDLE_LIST_SIZE_ADDR(hal),
			   val);

	val = u32_encode_bits(sbuf[0].paddr & HAL_ADDR_LSB_REG_MASK,
			      BUFFER_ADDR_INFO0_ADDR);
	ath12k_hif_write32(ab,
			   HAL_SEQ_WCSS_UMAC_WBM_REG +
			   HAL_WBM_SCATTERED_RING_BASE_LSB(hal),
			   val);

	val = u32_encode_bits(BASE_ADDR_MATCH_TAG_VAL,
			      HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_MATCH_TAG) |
	      u32_encode_bits((u64)sbuf[0].paddr >> HAL_ADDR_MSB_REG_SHIFT,
			      HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_39_32);
	ath12k_hif_write32(ab,
			   HAL_SEQ_WCSS_UMAC_WBM_REG +
			   HAL_WBM_SCATTERED_RING_BASE_MSB(hal),
			   val);

	/* Setup head and tail pointers for the idle list */
	val = u32_encode_bits(sbuf[nsbufs - 1].paddr, BUFFER_ADDR_INFO0_ADDR);
	ath12k_hif_write32(ab,
			   HAL_SEQ_WCSS_UMAC_WBM_REG +
			   HAL_WBM_SCATTERED_DESC_PTR_HEAD_INFO_IX0(hal),
			   val);

	val = u32_encode_bits(((u64)sbuf[nsbufs - 1].paddr >> HAL_ADDR_MSB_REG_SHIFT),
			      HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_39_32) |
	       u32_encode_bits((end_offset >> 2),
			       HAL_WBM_SCATTERED_DESC_HEAD_P_OFFSET_IX1);
	ath12k_hif_write32(ab,
			   HAL_SEQ_WCSS_UMAC_WBM_REG +
			   HAL_WBM_SCATTERED_DESC_PTR_HEAD_INFO_IX1(hal),
			   val);

	val = u32_encode_bits(sbuf[0].paddr, BUFFER_ADDR_INFO0_ADDR);
	ath12k_hif_write32(ab,
			   HAL_SEQ_WCSS_UMAC_WBM_REG +
			   HAL_WBM_SCATTERED_DESC_PTR_HEAD_INFO_IX0(hal),
			   val);

	val = u32_encode_bits(sbuf[0].paddr, BUFFER_ADDR_INFO0_ADDR);
	ath12k_hif_write32(ab,
			   HAL_SEQ_WCSS_UMAC_WBM_REG +
			   HAL_WBM_SCATTERED_DESC_PTR_TAIL_INFO_IX0(hal),
			   val);

	val = u32_encode_bits(((u64)sbuf[0].paddr >> HAL_ADDR_MSB_REG_SHIFT),
			      HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_39_32) |
	      u32_encode_bits(0, HAL_WBM_SCATTERED_DESC_TAIL_P_OFFSET_IX1);
	ath12k_hif_write32(ab,
			   HAL_SEQ_WCSS_UMAC_WBM_REG +
			   HAL_WBM_SCATTERED_DESC_PTR_TAIL_INFO_IX1(hal),
			   val);

	val = 2 * tot_link_desc;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_WBM_REG +
			   HAL_WBM_SCATTERED_DESC_PTR_HP_ADDR(hal),
			   val);

	/* Enable the SRNG */
	val = u32_encode_bits(1, HAL_WBM_IDLE_LINK_RING_MISC_SRNG_ENABLE) |
	      u32_encode_bits(1, HAL_WBM_IDLE_LINK_RING_MISC_RIND_ID_DISABLE);
	ath12k_hif_write32(ab,
			   HAL_SEQ_WCSS_UMAC_WBM_REG +
			   HAL_WBM_IDLE_LINK_RING_MISC_ADDR(hal),
			   val);
}

void ath12k_wifi8_hal_ppeds_tx_configure_skip_hdr_fetch(struct ath12k_base *ab)
{
	u32 val = 0;

	/*
	 * Disable skip header fetch.
	 * TODO: This can be avoided with AST idx changes.
	 */
	val = ath12k_hif_read32(ab, HAL_TCL1_CMN_CONFIG1_PPE);
	val |= HAL_TCL1_DISABLE_SKIP_HDR_FETCH;
	ath12k_hif_write32(ab, HAL_TCL1_CMN_CONFIG1_PPE, val);
}

void ath12k_wifi8_hal_tx_configure_cmn_reg(struct ath12k_base *ab)
{
	u32 reg_val;

	reg_val = ath12k_hif_read32(ab, HAL_TCL1_CMN_CONFIG);
	reg_val &= ~HAL_TCL1_CMN_CONFIG_CLFY_INVALID_PPE_DESC;
	ath12k_hif_write32(ab, HAL_TCL1_CMN_CONFIG, reg_val);
}

void ath12k_wifi8_hal_tx_configure_bank_register(struct ath12k_base *ab,
						 u32 bank_config,
						 u8 bank_id)
{
	ath12k_hif_write32(ab, HAL_TCL_SW_CONFIG_BANK_ADDR + 4 * bank_id,
			   bank_config);
}

void ath12k_wifi8_hal_tx_configure_bank_register_default(struct ath12k_base *ab)
{
	ath12k_hif_write32(ab, HAL_TCL_SW_CONFIG_BANK_DEFAULT,
			   HAL_TCL_SW_CONFIG_BANK_DEFAULT_VAL);
}

u32 ath12k_wifi8_hal_tx_read_bank_register(struct ath12k_base *ab, u8 bank_id)
{
	u32 reg_val;

	reg_val = ath12k_hif_read32(ab, HAL_TCL_SW_CONFIG_BANK_ADDR + 4 * bank_id);

	return reg_val;
}

void ath12k_wifi8_hal_reoq_lut_addr_read_enable(struct ath12k_base *ab)
{
	struct ath12k_hal *hal = &ab->hal;

	u32 val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_REO_REG +
				    HAL_REO1_QDESC_ADDR(hal));

	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO1_QDESC_ADDR(hal),
			   val | HAL_REO_QDESC_ADDR_READ_LUT_ENABLE);
}

void ath12k_wifi8_hal_reoq_lut_set_max_peerid(struct ath12k_base *ab)
{
	struct ath12k_hal *hal = &ab->hal;

	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO1_QDESC_MAX_PEERID(hal),
			   HAL_REO_QDESC_MAX_PEERID);
}

void ath12k_wifi8_hal_write_reoq_lut_addr(struct ath12k_base *ab,
					  dma_addr_t paddr)
{
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_REO_REG +
			   HAL_REO1_QDESC_LUT_BASE0(&ab->hal), paddr);
}

void ath12k_wifi8_hal_write_ml_reoq_lut_addr(struct ath12k_base *ab,
					     dma_addr_t paddr)
{
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_REO_REG +
			   HAL_REO1_QDESC_LUT_BASE1(&ab->hal), paddr);
}

void ath12k_wifi8_hal_ppeds_reo2ppe_cc_config(struct ath12k_base *ab)
{
	u32 reo_base = HAL_SEQ_WCSS_UMAC_REO_REG;
	u32 val = 0;

	/*
	 * PPEDS - HBM enabled case: Disable cookie conversion disable on REO2PPE
	 */
	if (ab->dp->ppe.hw_buff_mgmt) {
		val = ath12k_hif_read32(ab, reo_base + HAL_REO1_COOKIE_CONV_EN_RING);
		val &= ~HAL_REO2PPE_COOKIE_CONV_EN_RING;
		val &= ~HAL_REO2PPE1_COOKIE_CONV_EN_RING;
		val &= ~HAL_REO2PPE2_COOKIE_CONV_EN_RING;

		ath12k_hif_write32(ab, reo_base + HAL_REO1_COOKIE_CONV_EN_RING, val);
	}
}

void ath12k_wifi8_hal_cc_config(struct ath12k_base *ab)
{
	u32 cmem_base = ab->qmi.dev_mem[ATH12K_QMI_DEVMEM_CMEM_INDEX].start;
	u32 reo_base = HAL_SEQ_WCSS_UMAC_REO_REG;
	u32 tqm_base = HAL_SEQ_WCSS_UMAC_TQM_REG;
	u32 val = 0;
	struct ath12k_hal *hal = &ab->hal;

	if (ath12k_ftm_mode)
		return;

	ath12k_hif_write32(ab, reo_base + HAL_REO1_SW_COOKIE_CFG0(hal), cmem_base);

	val |= u32_encode_bits(ATH12K_CMEM_ADDR_MSB,
			       HAL_REO1_SW_COOKIE_CFG_CMEM_BASE_ADDR_MSB) |
		u32_encode_bits(ATH12K_CC_PPT_MSB,
				HAL_REO1_SW_COOKIE_CFG_COOKIE_PPT_MSB) |
		u32_encode_bits(ATH12K_CC_SPT_MSB,
				HAL_REO1_SW_COOKIE_CFG_COOKIE_SPT_MSB) |
		u32_encode_bits(1, HAL_REO1_SW_COOKIE_CFG_ALIGN);
#ifndef CPTCFG_EXT_IPA_OFFLOAD
	val |= u32_encode_bits(1, HAL_REO1_SW_COOKIE_CFG_ENABLE) |
		u32_encode_bits(1, HAL_REO1_SW_COOKIE_CFG_GLOBAL_ENABLE);
#endif

	ath12k_hif_write32(ab, reo_base + HAL_REO1_SW_COOKIE_CFG1(hal), val);

	/* Enable HW CC for TQM */
	ath12k_hif_write32(ab, tqm_base + HAL_TQM_SW_COOKIE_CFG0, cmem_base);

	val = u32_encode_bits(ATH12K_CMEM_ADDR_MSB,
			      HAL_TQM_SW_COOKIE_CFG1_CMEM_BASE_ADDR_MSB) |
		u32_encode_bits(ATH12K_CC_PPT_MSB,
				HAL_TQM_SW_COOKIE_CFG1_COOKIE_PPT_MSB) |
		u32_encode_bits(ATH12K_CC_SPT_MSB,
				HAL_TQM_SW_COOKIE_CFG1_COOKIE_SPT_MSB) |
		u32_encode_bits(1, HAL_TQM_SW_COOKIE_CFG1_ALIGN);

	ath12k_hif_write32(ab, tqm_base + HAL_TQM_SW_COOKIE_CFG1, val);

	/* Enable conversion complete indication */
	val = ath12k_hif_read32(ab, tqm_base + HAL_TQM_TX_COMPLETION_MISC_CFG);
	val |= u32_encode_bits(1, HAL_TQM_TX_COMPLETION_MISC_CFG_RELEASE_PATH_EN) |
		u32_encode_bits(1, HAL_TQM_TX_COMPLETION_MISC_CFG_ERR_PATH_EN) |
		u32_encode_bits(1, HAL_TQM_TX_COMPLETION_MISC_CFG_CONV_IND_EN);

	ath12k_hif_write32(ab, tqm_base + HAL_TQM_TX_COMPLETION_MISC_CFG, val);

	val = ath12k_hif_read32(ab, tqm_base + HAL_TQM_SW_COOKIE_CONVERT_CFG);
	val |= u32_encode_bits(1, HAL_TQM_SW_COOKIE_CONV_CFG_GLOBAL_EN) |
	       ab->hal.hal_params->tqm2sw_cc_enable1;

	ath12k_hif_write32(ab, tqm_base + HAL_TQM_SW_COOKIE_CONVERT_CFG, val);

	val = ath12k_hif_read32(ab, tqm_base + HAL_TQM_SW_COOKIE_CONVERT_CFG2);
	val |= ab->hal.hal_params->tqm2sw_cc_enable2;

	/* PPEDS - Disable cookie conversion on TQM2PPE ring */
	if (val & HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2PPE_EN)
		val &= ~(HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2PPE_EN);

	ath12k_hif_write32(ab, tqm_base + HAL_TQM_SW_COOKIE_CONVERT_CFG2, val);
}

u8 ath12k_wifi8_hal_get_idle_link_rbm(struct ath12k_hal *hal, u8 device_id)
{
	return HAL_RX_BUF_RBM_WBM_DEV0_IDLE_DESC_LIST;
}

void ath12k_wifi8_hal_srng_hw_disable(struct ath12k_base *ab,
				      struct hal_srng *srng)
{
	u32 reg_base, val, addr;
	struct ath12k_hal *hal = &ab->hal;

	reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R0];
	if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
		if (srng->ring_id == HAL_SRNG_RING_ID_WBM_IDLE_LINK)
			addr = HAL_SEQ_WCSS_UMAC_WBM_REG +
				HAL_WBM_IDLE_LINK_RING_MISC_ADDR(hal);
		else
			addr = reg_base + HAL_TCL1_RING_MISC_OFFSET(hal);
		val = ath12k_hif_read32(ab, addr);
		val &= ~HAL_TCL1_RING_MISC_SRNG_ENABLE;
		ath12k_hif_write32(ab, addr, val);
	} else {
		val = ath12k_hif_read32(ab, reg_base + HAL_REO1_RING_MISC_OFFSET);
		val &= ~HAL_REO1_RING_MISC_SRNG_ENABLE;
		ath12k_hif_write32(ab, reg_base + HAL_REO1_RING_MISC_OFFSET, val);
	}
}

void ath12k_wifi8_get_tlv_tag_params(__le32 tl, uint16_t *tag, uint32_t *id,
				     uint16_t *length)
{
	*tag = le32_get_bits(tl, HAL_TLV_HDR_TAG);
	*length = le32_get_bits(tl, HAL_TLV_HDR_LEN);
	*id = le32_get_bits(tl, HAL_TLV_USR_ID);
}

void ath12k_wifi8_hal_get_hw_hptp(struct ath12k_base *ab, enum hal_ring_type type,
				  struct hal_srng *srng, uint32_t *hp, uint32_t *tp)
{
	struct ath12k_hal *hal = &ab->hal;
	struct hal_srng_config *srng_config = &hal->srng_config[type];
	u32 reg_base;

	if (srng_config->mac_type == ATH12K_HAL_SRNG_UMAC) {
		reg_base = srng->hwreg_base[HAL_SRNG_REG_GRP_R2];

		*hp = ath12k_hif_read32(ab, reg_base);
		*tp = ath12k_hif_read32(ab, reg_base + HAL_TCL1_RING_TP_OFFSET);
	} else if (srng_config->reg_writer_en) {
		if (srng_config->ring_dir == HAL_SRNG_DIR_SRC)
			*hp = ath12k_hif_read32(ab,
						(unsigned long)srng->u.src_ring.hp_addr);
		else
			*tp = ath12k_hif_read32(ab,
						(unsigned long)srng->u.dst_ring.tp_addr);
	}
}

bool ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_get(struct ath12k_base *ab)
{
	u32 cmn_reg_addr;
	u32 regval;

	cmn_reg_addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_CMN_CTRL_REG;
	regval = ath12k_hif_read32(ab, cmn_reg_addr);

	return (regval & 1 <<
		HAL_TCL_CONS_RING_CMN_CTRL_PPE2TCL1_RNG_HALT_SHFT);
}

void ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_set(struct ath12k_base *ab)
{
	u32 cmn_reg_addr;
	u32 regval;

	cmn_reg_addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_CMN_CTRL_REG;
	regval = ath12k_hif_read32(ab, cmn_reg_addr);

	regval |= (1 << HAL_TCL_CONS_RING_CMN_CTRL_PPE2TCL1_RNG_HALT_SHFT);

	/* Enable ring halt for the ppe2tcl ring */
	ath12k_hif_write32(ab, cmn_reg_addr, regval);
}

void ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_reset(struct ath12k_base *ab)
{
	u32 cmn_reg_addr;
	u32 regval;

	cmn_reg_addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_CMN_CTRL_REG;
	regval = ath12k_hif_read32(ab, cmn_reg_addr);

	regval &= ~(1 << HAL_TCL_CONS_RING_CMN_CTRL_PPE2TCL1_RNG_HALT_SHFT);

	/* Disable ring halt for the ppe2tcl ring */
	ath12k_hif_write32(ab, cmn_reg_addr, regval);
}

bool ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_done(struct ath12k_base *ab)
{
	u32 cmn_reg_addr;
	u32 regval;

	cmn_reg_addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_CMN_CTRL_REG;

	regval = ath12k_hif_read32(ab, cmn_reg_addr);

	regval &=
	   (1 << HAL_TCL_R0_CONS_RING_CMN_CTRL_REG_PPE2TCL1_RNG_HALT_STAT_SHFT);

	return !!regval;
}

#define HAL_TCL_RBM_MAPPING0_ADDR_OFFSET 0xd8
#define HAL_TCL_RBM_MAPPING1_ADDR_OFFSET 0xdc
#define HAL_TCL_RBM_MAPPING_SHFT 4
#define HAL_TCL_RBM_MAPPING_BMSK 0xF
#define HAL_TCL_RBM_MAPPING_PPE2TCL_OFFSET  7
#define HAL_TCL_RBM_MAPPING_TCL_CMD_CREDIT_OFFSET  6

void ath12k_wifi8_hal_tx_config_rbm_mapping(struct ath12k_base *ab, u8 ring_num,
					    u8 rbm_id, int ring_type)
{
	u32 curr_map, new_map;

	if (ring_type == HAL_TCL_CMD)
		ring_num = ring_num + HAL_TCL_RBM_MAPPING_TCL_CMD_CREDIT_OFFSET;

	if (ring_type == HAL_PPE2TCL) {
		curr_map = ath12k_hif_read32(ab,
				HAL_SEQ_WCSS_UMAC_TCL_REG +
				HAL_TCL_RBM_MAPPING1_ADDR_OFFSET);
	} else {
		curr_map = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
				     HAL_TCL_RBM_MAPPING0_ADDR_OFFSET);
	}

	/* Protect the other values and clear the specific fields to be updated */
	curr_map &= (~(HAL_TCL_RBM_MAPPING_BMSK <<
		     (HAL_TCL_RBM_MAPPING_SHFT * ring_num)));
	new_map = curr_map | ((HAL_TCL_RBM_MAPPING_BMSK & rbm_id) <<
			      (HAL_TCL_RBM_MAPPING_SHFT * ring_num));

	if (ring_type == HAL_PPE2TCL) {
		ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
				HAL_TCL_RBM_MAPPING1_ADDR_OFFSET, new_map);
	} else {
		ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
				HAL_TCL_RBM_MAPPING0_ADDR_OFFSET, new_map);
	}
}

#define HAL_TX_PPE_VP_CONFIG_TABLE_ADDR  0x00F1336C
#define HAL_TX_PPE_VP_CONFIG_TABLE_OFFSET 4
void ath12k_wifi8_hal_tx_set_ppe_vp_entry(struct ath12k_base *ab,
					  struct ath12k_dp_ppe_vp_profile *ppe_vp_profile,
					  u32 ppe_vp_idx, u32 vdev_id,
					  u32 bank_id, u32 lmac_id)
{
	u32 ppe_vp_config = 0;
	struct ath12k_base *central_ab =
		ath12k_wifi8_ppeds_get_central_ab(ab);

	if (lmac_id == HAL_WILDCARD_LMAC_ID)
		lmac_id = HAL_TX_PPE_VP_CFG_WILDCARD_LMAC_ID;

	if (!ppe_vp_profile) {
		ppe_vp_config = 0;
		goto reg_write;
	}

	ppe_vp_config |=
		u32_encode_bits(ppe_vp_profile->entry_valid,
				HAL_TX_PPE_VP_CFG_ENTRY_VALID) |
		u32_encode_bits(ppe_vp_profile->search_idx_reg_num,
				HAL_TX_PPE_VP_CFG_SRCH_IDX_REG_NUM) |
		u32_encode_bits(ppe_vp_profile->use_ppe_int_pri,
				HAL_TX_PPE_VP_CFG_USE_PPE_INT_PRI) |
		u32_encode_bits(ppe_vp_profile->to_fw,
				HAL_TX_PPE_VP_CFG_TO_FW) |
		u32_encode_bits(ppe_vp_profile->drop_prec_enable,
				HAL_TX_PPE_VP_CFG_DROP_PREC_EN) |
		u32_encode_bits(bank_id, HAL_TX_PPE_VP_CFG_BANK_ID) |
		u32_encode_bits(lmac_id, HAL_TX_PPE_VP_CFG_LMAC_ID) |
		u32_encode_bits(vdev_id, HAL_TX_PPE_VP_CFG_VDEV_ID);

	ath12k_dbg(ab, ATH12K_DBG_HAL,
			"PPE_VP valid:%d srch_idx:%u bank_id:%u lmac_id:%u vdev_id:%u\n",
			ppe_vp_profile->entry_valid,
			ppe_vp_profile->search_idx_reg_num,
			bank_id,
			lmac_id,
			vdev_id);
reg_write:
	ath12k_hif_write32(central_ab, HAL_TX_PPE_VP_CONFIG_TABLE_ADDR +
			   (HAL_TX_PPE_VP_CONFIG_TABLE_OFFSET * ppe_vp_idx),
			   ppe_vp_config);
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_wifi8_hal_ppeds_cfg_ast_override_map_reg(struct ath12k_base *ab, u8 idx,
						     u32 ppeds_idx_map_val)
{
	u32 reg_addr;
	struct ath12k_base *central_ab =
		ath12k_wifi8_ppeds_get_central_ab(ab);

	reg_addr = HAL_TCL_PPE_INDEX_MAPPING_TABLE_n_ADDR(HAL_SEQ_WCSS_UMAC_TCL_REG, idx);

	ath12k_hif_write32(central_ab, reg_addr, ppeds_idx_map_val);
}

bool ath12k_wifi8_hal_ppeds_cfg_ast(struct ath12k_base *ab,
		u32 ppe_vp_num,
		u32 ppeds_idx_map_val)
{
	struct ath12k_dp_ppe_vp_profile *ppe_vp_profile;
	struct ath12k_base *central_ab =
		ath12k_wifi8_ppeds_get_central_ab(ab);

	ppe_vp_profile = ath12k_wifi8_dp_ppeds_get_vp_profile(ab, ppe_vp_num);
	if (!ppe_vp_profile) {
		ath12k_dbg(ab, ATH12K_DBG_HAL, "Invalid ppe profile :%d\n", ppe_vp_num);
		return false;
	}

	ath12k_wifi8_hal_ppeds_cfg_ast_override_map_reg(central_ab,
			ppe_vp_profile->search_idx_reg_num,
			ppeds_idx_map_val);

	ath12k_dbg(ab, ATH12K_DBG_HAL, "ast_idx map:%d reg_num:%d\n",
			ppeds_idx_map_val,
			ppe_vp_profile->search_idx_reg_num);
	return true;
}
#endif

void ath12k_wifi8_hal_reo_config_reo2ppe_dest_info(struct ath12k_base *ab)
{
	u32 reo_base = HAL_SEQ_WCSS_UMAC_REO_REG;
	u32 val = HAL_REO1_REO2PPE_DST_VAL;

	ath12k_hif_write32(ab, reo_base + HAL_REO1_REO2PPE_DST_INFO,
			   val);
	/*
	 * Copying the INT_PRI and DEST_INFO from FSE entry.
	 */
	val = ath12k_hif_read32(ab, reo_base + HAL_REO_MISC_CFG_BN_2);
	val |= HAL_REO_COPY_PPE_INFO_FROM_MSDU_VAL;
	val |= HAL_REO_PPE_DEST_OVERRIDE_EN;
	ath12k_hif_write32(ab, reo_base + HAL_REO_MISC_CFG_BN_2,
			   val);

	/*
	 * Enable PPE multi-MSDU check
	 */
	val = ath12k_hif_read32(ab, reo_base + HAL_REO_PPE_DEST_OVERRIDE);
	val |= HAL_REO_PPE_DEST_OVERRIDE_VAL;
	ath12k_hif_write32(ab, reo_base + HAL_REO_PPE_DEST_OVERRIDE,
			   val);
}

void ath12k_wifi8_hal_hw_ase_init(struct ath12k_base *ab,
				  struct ath12k_hal_ast_param *ast_param)
{
	u32 val;

	val = le32_encode_bits(ast_param->paddr,
			       HAL_TCL_ASE_GST_BASE_ADDR_LOW_MASK);
	ath12k_hif_write32(ab, HAL_TCL_ASE_GST_BASE_ADDR_LOW, val);

	val = le32_encode_bits(((u64)ast_param->paddr >> HAL_ADDR_MSB_REG_SHIFT),
			       HAL_TCL_ASE_GST_BASE_ADDR_HIGH_MASK);
	ath12k_hif_write32(ab, HAL_TCL_ASE_GST_BASE_ADDR_HIGH, val);

	val = le32_encode_bits(ast_param->num_ast_entries,
			       HAL_TCL_ASE_GST_SIZE_MASK);
	ath12k_hif_write32(ab, HAL_TCL_ASE_GST_SIZE, val);

	val = ath12k_hif_read32(ab, HAL_TCL_ASE_SEARCH_CTRL);
	val &= ~(HAL_TCL_ASE_SEARCH_CTRL_MAX_SEARCH |
		 HAL_TCL_ASE_SEARCH_CTRL_CACHE_DISABLE |
		 HAL_TCL_ASE_SEARCH_CTRL_CACHE_FAILURES_ENABLE);
	val |= le32_encode_bits(ast_param->skid_len,
				HAL_TCL_ASE_SEARCH_CTRL_MAX_SEARCH) |
	       le32_encode_bits(!ast_param->ast_cache_en,
				HAL_TCL_ASE_SEARCH_CTRL_CACHE_DISABLE) |
	       le32_encode_bits(ast_param->ast_cache_failure_en,
				HAL_TCL_ASE_SEARCH_CTRL_CACHE_FAILURES_ENABLE);
	ath12k_dbg(ab, ATH12K_DBG_HAL, "ASE search ctrl: 0x%x\n", val);
	ath12k_hif_write32(ab, HAL_TCL_ASE_SEARCH_CTRL, val);

	ath12k_hif_write32(ab, HAL_TCL_ASE_HASH_KEY_31_0, ast_param->ase_hash_key1);
	ath12k_hif_write32(ab, HAL_TCL_ASE_HASH_KEY_63_32, ast_param->ase_hash_key2);
	ath12k_hif_write32(ab, HAL_TCL_ASE_HASH_KEY_64, ast_param->ase_hash_key3);
}

void ath12k_wifi8_hal_vdev_mcast_ctrl_set(struct ath12k_base *ab, u32 vdev_id,
					  u8 mcast_ctrl_val)
{
	u32 reg_addr, val, reg_val;
	u8 reg_idx, index_in_reg;

	reg_idx = HAL_TCL_VDEV_MCAST_PACKET_CTRL_REG_ID(vdev_id);
	index_in_reg = HAL_TCL_VDEV_MCAST_PACKET_CTRL_INDEX_IN_REG(vdev_id);

	reg_addr = HAL_TCL_R0_VDEV_MCAST_PACKET_CTRL_MAP_n_ADDR(reg_idx);
	val = ath12k_hif_read32(ab, reg_addr);

	val &= (~(HAL_TCL_VDEV_MCAST_PACKET_CTRL_MASK <<
		  (HAL_TCL_VDEV_MCAST_PACKET_CTRL_SHIFT * index_in_reg)));

	reg_val = val |
		  ((HAL_TCL_VDEV_MCAST_PACKET_CTRL_MASK & mcast_ctrl_val) <<
		   (HAL_TCL_VDEV_MCAST_PACKET_CTRL_SHIFT * index_in_reg));

	ath12k_hif_write32(ab, reg_addr, reg_val);
}

int ath12k_wifi8_hal_get_rdi_source_cfg(struct ath12k_base *ab, int source)
{
	struct ath12k_hal *hal = &ab->hal;
	const struct ath12k_hal_rdi_mapping *rdi_mapping = hal->rdi_mapping;
	unsigned long rdi_based_source_cfg = 0;
	int i;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);

	for (i = 0; i < HAL_RDI_MAPPING_MAX; i++)
		if (rdi_mapping[i].source == source)
			set_bit(i, &rdi_based_source_cfg);

	/*  include PPE RDI's in SFE when using common pool */
	if ((source == SOURCE_RING_CTRL_SFE) &&
	    !dp_wifi8->dp_ppe2wbm_use_dedicated_pool) {
		rdi_based_source_cfg |=	BIT(DESTINATION_RING_CTRL_PPE) |
					BIT(DESTINATION_RING_CTRL_PPE1) |
					BIT(DESTINATION_RING_CTRL_PPE2);
	}

	return rdi_based_source_cfg;
}

static inline
u32 ath12k_hal_srng_src_get_words_available(u32 hp, u32 tp,
					    u32 ring_size)
{
	/* Keeping 1-word gap so hp==tp means empty */
	if (tp <= hp)
		return (ring_size - hp + tp - 1);
	else
		return (tp - hp - 1);
}

u32 ath12k_hal_srng_get_cmd_size(struct ath12k_base *ab, enum hal_tlv_tag_be type)
{
	if (type == HAL_TQM_REMOVE_MSDU_BO)
		return ((sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_tqm_remove_msdu)) >> 2);
	else if (type == HAL_TQM_REMOVE_MPDU_BO)
		return ((sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_tqm_remove_mpdu)) >> 2);
	else if (type == HAL_TQM_SYNC_CMD_BO)
		return ((sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_tqm_sync_cmd)) >> 2);
	else if (type == HAL_TQM_GET_MPDUQ_STATS_BO)
		return ((sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_tqm_get_mpduq_stats)) >> 2);
	else if (type == HAL_TQM_UPDATE_MSDUQ_BO)
		return ((sizeof(struct hal_tlv_64_hdr) +
			 (ab->hw_rev == ATH12K_HW_QCN9625_HW10
			  ? sizeof(struct hal_tqm_update_tx_msdu_flow_hw10)
			  : sizeof(struct hal_tqm_update_tx_msdu_flow))) >> 2);
	else if (type == HAL_TQM_UPDATE_MPDUQ_BO)
		return ((sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_tqm_update_mpduq)) >> 2);
	else if (type == HAL_SAM_MPDU_QUEUE_CLEAR_PROGRAMMING_BO)
		return ((sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_sam_mpdu_queue_clear_programming)) >> 2);
	else if (type == HAL_SAM_MSDU_QUEUE_CLEAR_PROGRAMMING_BO)
		return ((sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_sam_msdu_queue_clear_programming)) >> 2);
	else if (type == HAL_SAM_PEER_CLEAR_PROGRAMMING_BO)
		return ((sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_sam_peer_clear_programming)) >> 2);
	else if (type == HAL_TQM_FW_COMPLETION_BO)
		return ((sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_tqm2sw_completion_ring)) >> 2);
	return 0;
}

void *ath12k_hal_srng_src_get_next_entry_by_cmd_size(struct ath12k_base *ab,
						     struct hal_srng *srng,
						     enum hal_tlv_tag_be type)
{
	u32 entry_size, ring_size, hp, tp;
	void *desc;
	u32 words_available, next_hp;

	lockdep_assert_held(&srng->lock);

	entry_size = ath12k_hal_srng_get_cmd_size(ab, type);
	ring_size = srng->ring_size;
	hp = srng->u.src_ring.hp;
	tp = READ_ONCE(srng->u.src_ring.cached_tp);

	if (!entry_size || entry_size >= ring_size)
		return NULL;

	words_available = ath12k_hal_srng_src_get_words_available(
						hp, tp, ring_size);

	if (unlikely(words_available < entry_size))
		return NULL; /* not enough total space */

	desc = (u32 *)srng->ring_base_vaddr + hp;

	next_hp = hp + entry_size;
	if (next_hp >= ring_size)
		next_hp -= ring_size;

	srng->u.src_ring.hp = next_hp;
	srng->u.src_ring.reap_hp = next_hp;

	return desc;
}

void ath12k_wifi8_hal_txpt_classify_info_flush(struct ath12k_base *ab)
{
	int max_retry = 10;
	u32 val;

	val = ath12k_hif_read32(ab, HAL_TCL_ASE_PEER_FETCH_CACHE_CTRL);
	/* previous cache flush is in progress */
	if ((val & HAL_TCL_ASE_PEER_FETCH_CACHE_FLUSH) &&
	    !(val & HAL_TCL_ASE_PEER_FETCH_CACHE_FLUSH_STATUS))
		return;

	val |= le32_encode_bits(1, HAL_TCL_ASE_PEER_FETCH_CACHE_FLUSH);
	ath12k_hif_write32(ab, HAL_TCL_ASE_PEER_FETCH_CACHE_CTRL, val);

	while (max_retry--) {
		val = ath12k_hif_read32(ab, HAL_TCL_ASE_PEER_FETCH_CACHE_CTRL);
		if (val & HAL_TCL_ASE_PEER_FETCH_CACHE_FLUSH_STATUS) {
			val &= ~HAL_TCL_ASE_PEER_FETCH_CACHE_FLUSH;
			ath12k_hif_write32(ab, HAL_TCL_ASE_PEER_FETCH_CACHE_CTRL, val);
			return;
		}
	}
	ath12k_warn(ab, "ASE peer fetch cache flush timeout\n");
	BUG_ON(1);
}

void ath12k_wifi8_hal_tasc_peer_tx_cfg(struct ath12k_base *ab, bool enable)
{
	u32 val = 0;

	val = u32_encode_bits(enable,
			      HAL_TX_TELEMETRY_GLOBAL_CTRL_PEER_STATS_ENABLE);
	ath12k_hif_write32(ab, HAL_TX_TELEMETRY_GLOBAL_CTRL_ADDR, val);
}

void ath12k_wifi8_hal_tasc_peer_clk_cycle_config(struct ath12k_base *ab)
{
	ath12k_hif_write32(ab, HAL_TELEMETRY_PEER_CLK_CYCLE_ADDR,
			   HAL_TELEMETRY_PEER_CLK_CYCLE_DEFAULT_VAL);
}

void ath12k_wifi8_hal_tasc_peer_tx_max_peer(struct ath12k_base *ab,
					    u16 ucast, u8 gcast_dl,
					    u8 gcast_ul)
{
	u32 val = 0;

	val = u32_encode_bits(ucast,
			      HAL_TX_NUM_OF_VALID_UNICAST_PEER) |
		u32_encode_bits(gcast_dl,
				HAL_TX_NUM_OF_VALID_DL_GCAST_PEER) |
		u32_encode_bits(gcast_ul,
				HAL_TX_NUM_OF_VALID_UL_GCAST_PEER);

	ath12k_hif_write32(ab, HAL_TX_NUM_OF_VALID_PEER_CFG_ADDR, val);
}

void ath12k_wifi8_hal_tasc_peer_tx_window(struct ath12k_base *ab,
					  u16 time)
{
	u32 val = 0;

	val = u32_encode_bits(time, HAL_TX_PEER_STATS_WINDOW_SIZE_CFG);
	ath12k_hif_write32(ab, HAL_TX_PEER_STATS_WINDOW_SIZE_CFG_ADDR, val);
}

void ath12k_wifi8_hal_tasc_peer_tx_fail_drop_default(struct ath12k_base *ab)
{
	u32 map1 = 0, map2 = 0;

	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_RR_REM_CMD_REM,
			      HAL_TQM_RELEASE_REASON_DROP1);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_RR_REM_CMD_TX,
			      HAL_TQM_RELEASE_REASON_DROP1);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_RR_REM_CMD_NOTX,
			      HAL_TQM_RELEASE_REASON_DROP1);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_RR_REM_CMD_AGED,
			      HAL_TQM_RELEASE_REASON_DROP1);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_FW_REASON1,
			      HAL_TQM_RELEASE_REASON_DROP1);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_FW_REASON2,
			      HAL_TQM_RELEASE_REASON_DROP1);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_FW_REASON3,
			      HAL_TQM_RELEASE_REASON_DROP1);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_RR_REM_CMD_DISABLE_QUEUE,
			      HAL_TQM_RELEASE_REASON_DROP1);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_RR_REM_CMD_TILL_NONMATCHING,
			      HAL_TQM_RELEASE_REASON_DROP1);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_RR_DROP_THRESHOLD,
			      HAL_TQM_RELEASE_REASON_DROP1);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_RR_LINK_DESC_UNAVAILABLE,
			      HAL_TQM_RELEASE_REASON_FAILED);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_RR_DROP_OR_INVALID_MSDU,
			      HAL_TQM_RELEASE_REASON_FAILED);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_RR_MULTICAST_DROP,
			      HAL_TQM_RELEASE_REASON_FAILED);
	HAL_TQM_FAIL_DROP_MAP(map1, HAL_TQM_RR_VDEV_MISMATCH_DROP,
			      HAL_TQM_RELEASE_REASON_FAILED);

	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_GEN_CMD_USED_TREE_EXT,
			      HAL_TQM_RELEASE_REASON_FAILED);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TCL_DROP_FROM_PEER_CCE_OR_FLOW_TABLE,
			      HAL_TQM_RELEASE_REASON_FAILED);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TCL_MULTICAST_REINJECT_FOR_VDEV,
			      HAL_TQM_RELEASE_REASON_FAILED);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TCL_MEC_SEARCH_FAIL_FOR_VDEV,
			      HAL_TQM_RELEASE_REASON_FAILED);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TCL_ASE_SEARCH_FAIL,
			      HAL_TQM_RELEASE_REASON_FAILED);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TCL_SMD_ROAMING_DROP,
			      HAL_TQM_RELEASE_REASON_FAILED);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TCL_STRIP_VLAN_TCI_MISMATCH_DROP,
			      HAL_TQM_RELEASE_REASON_FAILED);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TCL_MEC_KEEP_ALIVE_FOR_VDEV,
			      HAL_TQM_RELEASE_REASON_FAILED);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TCL_RESERVED_DROP_REASON1,
			      HAL_TQM_RELEASE_REASON_DROP2);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TCL_RESERVED_DROP_REASON2,
			      HAL_TQM_RELEASE_REASON_DROP2);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TQM_REM_MSDU_SMD_ROAMING,
			      HAL_TQM_RELEASE_REASON_DROP2);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TQM_REM_MPDU_SMD_ROAMING,
			      HAL_TQM_RELEASE_REASON_DROP2);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TQM_RESERVED_DROP_REASON3,
			      HAL_TQM_RELEASE_REASON_DROP2);
	HAL_TQM_FAIL_DROP_MAP(map2, HAL_TQM_RR_TQM_RESERVED_DROP_REASON4,
			      HAL_TQM_RELEASE_REASON_DROP2);

	ath12k_hif_write32(ab, HAL_TQM_RELEASE_REASON_MAP_1_ADDR, map1);
	ath12k_hif_write32(ab, HAL_TQM_RELEASE_REASON_MAP_2_ADDR, map2);
}

void ath12k_wifi8_hal_tasc_peer_tx_fail_drop_map(struct ath12k_base *ab,
						 u32 map1, u32 map2)
{
	ath12k_hif_write32(ab, HAL_TQM_RELEASE_REASON_MAP_1_ADDR, map1);
	ath12k_hif_write32(ab, HAL_TQM_RELEASE_REASON_MAP_2_ADDR, map2);
}

void ath12k_wifi8_hal_num_transmission_map_default(struct ath12k_base *ab)
{
	u32 map = 0;

	map |= BIT(HAL_TQM_RR_REM_CMD_REM);
	map |= BIT(HAL_TQM_RR_REM_CMD_TX);
	map |= BIT(HAL_TQM_RR_REM_CMD_NOTX);
	map |= BIT(HAL_TQM_RR_REM_CMD_AGED);
	map |= BIT(HAL_TQM_FW_REASON1);
	map |= BIT(HAL_TQM_FW_REASON2);
	map |= BIT(HAL_TQM_FW_REASON3);
	map |= BIT(HAL_TQM_RR_REM_CMD_DISABLE_QUEUE);
	map |= BIT(HAL_TQM_RR_REM_CMD_TILL_NONMATCHING);
	map |= BIT(HAL_TQM_RR_DROP_THRESHOLD);
	map |= BIT(HAL_TQM_RR_LINK_DESC_UNAVAILABLE);
	map |= BIT(HAL_TQM_RR_DROP_OR_INVALID_MSDU);
	map |= BIT(HAL_TQM_RR_MULTICAST_DROP);
	map |= BIT(HAL_TQM_RR_VDEV_MISMATCH_DROP);
	map |= BIT(HAL_TQM_RR_GEN_CMD_USED_TREE_EXT);
	map |= BIT(HAL_TQM_RR_TCL_DROP_FROM_PEER_CCE_OR_FLOW_TABLE);
	map |= BIT(HAL_TQM_RR_TCL_MULTICAST_REINJECT_FOR_VDEV);
	map |= BIT(HAL_TQM_RR_TCL_MEC_SEARCH_FAIL_FOR_VDEV);
	map |= BIT(HAL_TQM_RR_TCL_ASE_SEARCH_FAIL);
	map |= BIT(HAL_TQM_RR_TCL_SMD_ROAMING_DROP);
	map |= BIT(HAL_TQM_RR_TCL_STRIP_VLAN_TCI_MISMATCH_DROP);
	map |= BIT(HAL_TQM_RR_TCL_MEC_KEEP_ALIVE_FOR_VDEV);
	map |= BIT(HAL_TQM_RR_TCL_RESERVED_DROP_REASON1);
	map |= BIT(HAL_TQM_RR_TCL_RESERVED_DROP_REASON2);
	map |= BIT(HAL_TQM_RR_TQM_REM_MSDU_SMD_ROAMING);
	map |= BIT(HAL_TQM_RR_TQM_REM_MPDU_SMD_ROAMING);
	map |= BIT(HAL_TQM_RR_TQM_RESERVED_DROP_REASON3);
	map |= BIT(HAL_TQM_RR_TQM_RESERVED_DROP_REASON4);

	ath12k_hif_write32(ab, HAL_TX_PEER_STATS_NUM_TRANSMISSIONS_CFG_ADDR, map);
}

void ath12k_wifi8_hal_num_transmission_map_(struct ath12k_base *ab, u32 map)
{
	ath12k_hif_write32(ab, HAL_TX_PEER_STATS_NUM_TRANSMISSIONS_CFG_ADDR, map);
}

void ath12k_wifi8_hal_tasc_peer_tx_gcast_id_map(struct ath12k_base *ab,
						u16 mlo_peer_stats_id,
						u16 gcast_peer_stats_id)
{
	switch (gcast_peer_stats_id) {
	case HAL_TX_UL_GCAST_PEER_ID_1616:
		ath12k_hif_write32(ab, HAL_TX_UL_GCAST_PEER_ID_REMAP0,
				   u32_encode_bits(mlo_peer_stats_id,
						   HAL_TX_UL_PEER_ID_REMAP1));
		break;
	case HAL_TX_UL_GCAST_PEER_ID_1617:
		ath12k_hif_write32(ab, HAL_TX_UL_GCAST_PEER_ID_REMAP0,
				   u32_encode_bits(mlo_peer_stats_id,
						   HAL_TX_UL_PEER_ID_REMAP2));
		break;
	case HAL_TX_UL_GCAST_PEER_ID_1618:
		ath12k_hif_write32(ab, HAL_TX_UL_GCAST_PEER_ID_REMAP1,
				   u32_encode_bits(mlo_peer_stats_id,
						   HAL_TX_UL_PEER_ID_REMAP1));
		break;
	case HAL_TX_UL_GCAST_PEER_ID_1619:
		ath12k_hif_write32(ab, HAL_TX_UL_GCAST_PEER_ID_REMAP1,
				   u32_encode_bits(mlo_peer_stats_id,
						   HAL_TX_UL_PEER_ID_REMAP2));
		break;
	case HAL_TX_UL_GCAST_PEER_ID_1620:
		ath12k_hif_write32(ab, HAL_TX_UL_GCAST_PEER_ID_REMAP2,
				   u32_encode_bits(mlo_peer_stats_id,
						   HAL_TX_UL_PEER_ID_REMAP1));
		break;
	case HAL_TX_UL_GCAST_PEER_ID_1621:
		ath12k_hif_write32(ab, HAL_TX_UL_GCAST_PEER_ID_REMAP2,
				   u32_encode_bits(mlo_peer_stats_id,
						   HAL_TX_UL_PEER_ID_REMAP2));
		break;
	case HAL_TX_UL_GCAST_PEER_ID_1622:
		ath12k_hif_write32(ab, HAL_TX_UL_GCAST_PEER_ID_REMAP3,
				   u32_encode_bits(mlo_peer_stats_id,
						   HAL_TX_UL_PEER_ID_REMAP1));
		break;
	case HAL_TX_UL_GCAST_PEER_ID_1623:
		ath12k_hif_write32(ab, HAL_TX_UL_GCAST_PEER_ID_REMAP3,
				   u32_encode_bits(mlo_peer_stats_id,
						   HAL_TX_UL_PEER_ID_REMAP2));
		break;
	}
}

void ath12k_wifi8_hal_tasc_peer_tx_set_id(struct ath12k_base *ab,
					  u16 stats_id)
{
	u32 val = 0;

	val = u32_encode_bits(stats_id,
			      HAL_TX_PEER_TELEMETRY_STATS_ID) |
		u32_encode_bits(1,
				HAL_TX_PEER_STATS_ENABLE_TELEMETRY_STATS_ID);

	ath12k_hif_write32(ab, HAL_TX_PEER_STATS_CONFIG_CTRL_ADDR, val);
}

void ath12k_wifi8_hal_tasc_reset_peer_tx(struct ath12k_base *ab,
					 u16 stats_id)
{
	u32 val = 0;

	val = u32_encode_bits(stats_id,
			      HAL_TX_PEER_TELEMETRY_STATS_ID) |
		u32_encode_bits(1,
				HAL_TX_PEER_STATS_CLEAR_TELEMETRY_STATS_ID);

	ath12k_hif_write32(ab, HAL_TX_PEER_STATS_CONFIG_CTRL_ADDR, val);
}

void ath12k_wifi8_hal_tasc_peer_tx_band(struct ath12k_base *ab,
					u16 band_idx[HAL_TASC_BAND_MAX],
					bool enable)
{
	u32 val;

	val = u32_encode_bits(band_idx[HAL_TASC_BAND_0],
			      HAL_TX_PEER_STATS_BAND_INDEX_0) |
		u32_encode_bits(band_idx[HAL_TASC_BAND_1],
				HAL_TX_PEER_STATS_BAND_INDEX_1) |
		u32_encode_bits(enable, HAL_TX_PEER_STATS_BAND_INDEX_MSB);
	ath12k_hif_write32(ab, HAL_TX_PEER_STATS_CFG0_ADDR, val);

	val = u32_encode_bits(band_idx[HAL_TASC_BAND_2],
			      HAL_TX_PEER_STATS_BAND_INDEX_2) |
		u32_encode_bits(band_idx[HAL_TASC_BAND_3],
				HAL_TX_PEER_STATS_BAND_INDEX_3) |
		u32_encode_bits(enable, HAL_TX_PEER_STATS_BAND_INDEX_MSB);
	ath12k_hif_write32(ab, HAL_TX_PEER_STATS_CFG1_ADDR, val);

	val = u32_encode_bits(band_idx[HAL_TASC_BAND_4],
			      HAL_TX_PEER_STATS_BAND_INDEX_4) |
		u32_encode_bits(enable, HAL_TX_PEER_STATS_BAND_INDEX_MSB);
	ath12k_hif_write32(ab, HAL_TX_PEER_STATS_CFG2_ADDR, val);
}

void ath12k_wifi8_hal_tasc_peer_rx_cfg(struct ath12k_base *ab, bool enable)
{
	u32 val = 0;

	val = u32_encode_bits(enable,
			      HAL_RX_TELEMETRY_GLOBAL_CTRL_PEER_STATS_ENABLE);
	ath12k_hif_write32(ab, HAL_RX_TELEMETRY_GLOBAL_CTRL_ADDR, val);
}

void ath12k_wifi8_hal_tasc_peer_rx_max_peer(struct ath12k_base *ab,
					    u16 ucast, u8 gcast)
{
	u32 val = 0;

	val = u32_encode_bits(ucast,
			      HAL_RX_NUM_OF_VALID_UNICAST_PEER) |
		u32_encode_bits(gcast,
				HAL_RX_NUM_OF_VALID_GCAST_PEER);

	ath12k_hif_write32(ab, HAL_RX_NUM_OF_VALID_PEER_CFG_ADDR, val);
}

void ath12k_wifi8_hal_tasc_peer_rx_window(struct ath12k_base *ab,
					  u16 time)
{
	u32 val = 0;

	val = u32_encode_bits(time, HAL_RX_PEER_STATS_WINDOW_SIZE_CFG);
	ath12k_hif_write32(ab, HAL_RX_PEER_STATS_WINDOW_SIZE_CFG_ADDR, val);
}

void ath12k_wifi8_hal_tasc_peer_rx_fail_drop_default(struct ath12k_base *ab)
{
	u32 map = 0;

	RX_DROP_MAP(map, HAL_RX_NO_DROP, HAL_RX_DROPPED_REASON_1);
	RX_DROP_MAP(map, HAL_RX_BACKPRESSURE_DROP, HAL_RX_DROPPED_REASON_1);
	RX_DROP_MAP(map, HAL_RX_MSDU_DROP, HAL_RX_DROPPED_REASON_1);
	RX_DROP_MAP(map, HAL_RX_MSDU_BACKPRESSURE_DROP, HAL_RX_DROPPED_REASON_1);
	RX_DROP_MAP(map, HAL_RX_SDWF_DROP, HAL_RX_DROPPED_REASON_2);
	RX_DROP_MAP(map, HAL_RX_SDWF_BACKPRESSURE_DROP, HAL_RX_DROPPED_REASON_2);
	RX_DROP_MAP(map, HAL_RX_SDWF_MSDU_DROP, HAL_RX_DROPPED_REASON_2);
	RX_DROP_MAP(map, HAL_RX_SDWF_MSDU_BACKPRESSURE_DROP,
		    HAL_RX_DROPPED_REASON_2);

	ath12k_hif_write32(ab, HAL_RX_DROP_REASON_MAP_ADDR, map);
}

void ath12k_wifi8_hal_tasc_peer_rx_fail_drop_map(struct ath12k_base *ab,
						 u32 map)
{
	ath12k_hif_write32(ab, HAL_RX_DROP_REASON_MAP_ADDR, map);
}

void ath12k_wifi8_hal_tasc_peer_rx_set_id(struct ath12k_base *ab,
					  u16 stats_id)
{
	u32 val = 0;

	val = u32_encode_bits(stats_id,
			      HAL_RX_PEER_TELEMETRY_STATS_ID) |
		u32_encode_bits(1,
				HAL_RX_PEER_STATS_ENABLE_STATS_ID);

	ath12k_hif_write32(ab, HAL_RX_PEER_STATS_CONFIG_CTRL_ADDR, val);
}

void ath12k_wifi8_hal_tasc_reset_peer_rx(struct ath12k_base *ab,
					 u16 stats_id)
{
	u32 val = 0;

	val = u32_encode_bits(stats_id,
			      HAL_RX_PEER_TELEMETRY_STATS_ID) |
		u32_encode_bits(1,
				HAL_RX_PEER_STATS_CLEAR_STATS_ID);

	ath12k_hif_write32(ab, HAL_RX_PEER_STATS_CONFIG_CTRL_ADDR, val);
}

void ath12k_wifi8_hal_tasc_peer_rx_band(struct ath12k_base *ab,
					u16 band_idx[HAL_TASC_BAND_MAX],
					bool enable)
{
	u32 val;

	val = u32_encode_bits(band_idx[HAL_TASC_BAND_0],
			      HAL_RX_PEER_STATS_BAND_INDEX_0) |
		u32_encode_bits(band_idx[HAL_TASC_BAND_1],
				HAL_RX_PEER_STATS_BAND_INDEX_1) |
		u32_encode_bits(enable, HAL_RX_PEER_STATS_BAND_INDEX_MSB);
	ath12k_hif_write32(ab, HAL_RX_PEER_STATS_CFG0_ADDR, val);

	val = u32_encode_bits(band_idx[HAL_TASC_BAND_2],
			      HAL_RX_PEER_STATS_BAND_INDEX_2) |
		u32_encode_bits(band_idx[HAL_TASC_BAND_3],
				HAL_RX_PEER_STATS_BAND_INDEX_3) |
		u32_encode_bits(enable, HAL_RX_PEER_STATS_BAND_INDEX_MSB);
	ath12k_hif_write32(ab, HAL_RX_PEER_STATS_CFG1_ADDR, val);

	val = u32_encode_bits(band_idx[HAL_TASC_BAND_4],
			      HAL_RX_PEER_STATS_BAND_INDEX_4) |
		u32_encode_bits(enable, HAL_RX_PEER_STATS_BAND_INDEX_MSB);
	ath12k_hif_write32(ab, HAL_RX_PEER_STATS_CFG2_ADDR, val);
}
