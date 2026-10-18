// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_desc.h"
#include "../hal.h"
#include "hal_tx.h"
#include "../hif.h"
#include "hal_rx.h"

#define HAL_TX_BITS_PER_TID 3
#define HAL_TX_NUM_DSCP_REG_SIZE 32

void ath12k_wifi7_hal_tx_cmd_desc_setup(struct ath12k_base *ab,
					struct hal_tcl_data_cmd *tcl_cmd,
					struct hal_tx_info *ti)
{
	tcl_cmd->buf_addr_info.info0 =
		le32_encode_bits(ti->paddr, BUFFER_ADDR_INFO0_ADDR);
	tcl_cmd->buf_addr_info.info1 =
		le32_encode_bits(((uint64_t)ti->paddr >> HAL_ADDR_MSB_REG_SHIFT),
				 BUFFER_ADDR_INFO1_ADDR);
	tcl_cmd->buf_addr_info.info1 |=
		le32_encode_bits((ti->rbm_id), BUFFER_ADDR_INFO1_RET_BUF_MGR) |
		le32_encode_bits(ti->desc_id, BUFFER_ADDR_INFO1_SW_COOKIE);

	tcl_cmd->info0 =
		le32_encode_bits(ti->type, HAL_TCL_DATA_CMD_INFO0_DESC_TYPE) |
		le32_encode_bits(ti->bank_id, HAL_TCL_DATA_CMD_INFO0_BANK_ID);

	tcl_cmd->info1 =
		le32_encode_bits(ti->meta_data_flags,
				 HAL_TCL_DATA_CMD_INFO1_CMD_NUM);

	tcl_cmd->info2 = cpu_to_le32(ti->flags0) |
		le32_encode_bits(ti->data_len, HAL_TCL_DATA_CMD_INFO2_DATA_LEN) |
		le32_encode_bits(ti->pkt_offset, HAL_TCL_DATA_CMD_INFO2_PKT_OFFSET);

	tcl_cmd->info3 = cpu_to_le32(ti->flags1) |
		le32_encode_bits(ti->tid, HAL_TCL_DATA_CMD_INFO3_TID) |
		le32_encode_bits(ti->lmac_id, HAL_TCL_DATA_CMD_INFO3_PMAC_ID) |
		le32_encode_bits(ti->vdev_id, HAL_TCL_DATA_CMD_INFO3_VDEV_ID);

	tcl_cmd->info4 = le32_encode_bits(ti->lookup_override,
					  HAL_TCL_DATA_CMD_INFO4_IDX_LOOKUP_OVERRIDE) |
			 le32_encode_bits(ti->bss_ast_idx,
					  HAL_TCL_DATA_CMD_INFO4_SEARCH_INDEX) |
			 le32_encode_bits(ti->bss_ast_hash,
					  HAL_TCL_DATA_CMD_INFO4_CACHE_SET_NUM);
	tcl_cmd->info5 = 0;
}

void ath12k_update_dscp_register(struct ath12k_base *ab, u32 addr, u32 mask, u32 value)
{
	u32 reg_val;

	reg_val = ath12k_hif_read32(ab, addr);
	reg_val &= ~mask;
	reg_val |= value;
	ath12k_hif_write32(ab, addr, reg_val);
}

void ath12k_wifi7_hal_tx_update_dscp_tid_map(struct ath12k_base *ab, int id, u8 dscp, u8 tid)
{
	u32 ctrl_reg_val;
	u32 addr;
	u32 start_index, end_index;
	u32 mask;
	u32 value;

	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	/* Enable read/write access */
	ctrl_reg_val |= HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG, ctrl_reg_val);

	addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_DSCP_TID_MAP +
	       (4 * id * (HAL_DSCP_TID_TBL_SIZE / 4));

	/* Calculate start and end indices within the register */
	start_index = dscp * HAL_TX_BITS_PER_TID;
	end_index = (start_index + (HAL_TX_BITS_PER_TID - 1)) %
		    HAL_TX_NUM_DSCP_REG_SIZE;
	addr += (4 * (start_index / HAL_TX_NUM_DSCP_REG_SIZE));
	start_index %= HAL_TX_NUM_DSCP_REG_SIZE;

	if (end_index < start_index) {
		/* Handle the case where the TID value spans two registers */
		mask = GENMASK((HAL_TX_NUM_DSCP_REG_SIZE - 1), start_index);
		value = (tid << start_index) & mask;
		/* Update the first register */
		ath12k_update_dscp_register(ab, addr, mask, value);

		/* Update the second register */
		addr = addr+4;
		mask = GENMASK(end_index, 0);
		value = (tid >> (HAL_TX_NUM_DSCP_REG_SIZE - start_index)) & mask;
		ath12k_update_dscp_register(ab, addr, mask, value);
	} else {
		/* Handle the case where the TID value fits within one register */
		mask = GENMASK(end_index, start_index);
		value = (tid << start_index) & mask;
		ath12k_update_dscp_register(ab, addr, mask, value);
	}

	/* Disable read/write access */
	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	ctrl_reg_val &= ~HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG,ctrl_reg_val);
}

void ath12k_wifi7_hal_tx_set_dscp_tid_map(struct ath12k_base *ab, u8 *map, int id)
{
	u32 ctrl_reg_val;
	u32 addr;
	u8 hw_map_val[HAL_DSCP_TID_TBL_SIZE], count = 0;
	int i;
	u32 value;

	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	/* Enable read/write access */
	ctrl_reg_val |= HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG, ctrl_reg_val);

	addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_DSCP_TID_MAP +
	       (4 * id * (HAL_DSCP_TID_TBL_SIZE / 4));

	/* Configure each DSCP-TID mapping in three bits there by configure
	 * three bytes in an iteration.
	 */
	for (i = 0; i < DSCP_TID_MAP_TBL_ENTRY_SIZE; i += 8) {
		value = 0;

		value |= u32_encode_bits(map[i], HAL_TCL1_RING_FIELD_DSCP_TID_MAP0);
		value |= u32_encode_bits(map[i + 1], HAL_TCL1_RING_FIELD_DSCP_TID_MAP1);
		value |= u32_encode_bits(map[i + 2], HAL_TCL1_RING_FIELD_DSCP_TID_MAP2);
		value |= u32_encode_bits(map[i + 3], HAL_TCL1_RING_FIELD_DSCP_TID_MAP3);
		value |= u32_encode_bits(map[i + 4], HAL_TCL1_RING_FIELD_DSCP_TID_MAP4);
		value |= u32_encode_bits(map[i + 5], HAL_TCL1_RING_FIELD_DSCP_TID_MAP5);
		value |= u32_encode_bits(map[i + 6], HAL_TCL1_RING_FIELD_DSCP_TID_MAP6);
		value |= u32_encode_bits(map[i + 7], HAL_TCL1_RING_FIELD_DSCP_TID_MAP7);

		memcpy(&hw_map_val[count], &value, 3);
		count += 3;
	}

	for (i = 0; i < HAL_DSCP_TID_TBL_SIZE; i += 4) {
		ath12k_hif_write32(ab, addr, *(u32 *)&hw_map_val[i]);
		addr += 4;
	}

	/* Disable read/write access */
	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	ctrl_reg_val &= ~HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG,
			   ctrl_reg_val);
}

/**
 * ath12k_wifi7_hal_tx_set_pcp_tid_map() - Write PCP-TID map to all SOC TCL blocks.
 * @dp_hw_grp: Group-level dp structure; pcp_tid_map[] is read from here.
 *
 * Reads the single group-level pcp_tid_map[] from dp_hw_grp (no per-SOC
 * copies).  Computes the 24-bit register value using u32_encode_bits() with
 * the per-PCP _BMSK masks (PCP0 at [2:0], PCP7 at [23:21]).  Iterates all
 * dp_hw_grp->dp[] entries (up to ATH12K_MAX_SOCS) and writes the same
 * register value to every non-NULL SOC via ath12k_hif_write32().
 */
void ath12k_wifi7_hal_tx_set_pcp_tid_map(struct ath12k_base *ab, const u8 *map)
{
	u32 reg_val;

	/* Build the 24-bit register word.
	 * u32_encode_bits(val, mask) handles the shift internally via
	 * __ffs(mask) and WARNs on overflow — no manual _SHFT needed.
	 */
	reg_val = u32_encode_bits(map[0], HAL_TCL_R0_PCP_TID_MAP_PCP_0) |
		u32_encode_bits(map[1], HAL_TCL_R0_PCP_TID_MAP_PCP_1) |
		u32_encode_bits(map[2], HAL_TCL_R0_PCP_TID_MAP_PCP_2) |
		u32_encode_bits(map[3], HAL_TCL_R0_PCP_TID_MAP_PCP_3) |
		u32_encode_bits(map[4], HAL_TCL_R0_PCP_TID_MAP_PCP_4) |
		u32_encode_bits(map[5], HAL_TCL_R0_PCP_TID_MAP_PCP_5) |
		u32_encode_bits(map[6], HAL_TCL_R0_PCP_TID_MAP_PCP_6) |
		u32_encode_bits(map[7], HAL_TCL_R0_PCP_TID_MAP_PCP_7);

	ath12k_hif_write32(ab, HAL_TCL_R0_PCP_TID_MAP_ADDR, reg_val);
}
EXPORT_SYMBOL(ath12k_wifi7_hal_tx_set_pcp_tid_map);

/**
 * TID-map Precedence Table
 *
 * The precedence value controls which QoS marking takes priority
 * when mapping traffic to a TID. Higher priority sources appear first.
 *
 * Val | Interpretation
 * ----+----------------------------------------------------------
 *   0 | DSCP > PCP (S-VLAN > C-VLAN) > HLOS
 *   1 | DSCP > PCP (C-VLAN > S-VLAN) > HLOS
 *   2 | DSCP > HLOS > PCP (S-VLAN > C-VLAN)
 *   3 | DSCP > HLOS > PCP (C-VLAN > S-VLAN)
 *   4 | PCP (S-VLAN > C-VLAN) > DSCP > HLOS
 *   5 | PCP (C-VLAN > S-VLAN) > DSCP > HLOS
 *   6 | PCP (S-VLAN > C-VLAN) > HLOS > DSCP
 *   7 | PCP (C-VLAN > S-VLAN) > HLOS > DSCP
 *   8 | HLOS > PCP (S-VLAN > C-VLAN) > DSCP
 *   9 | HLOS > PCP (C-VLAN > S-VLAN) > DSCP
 *  10 | HLOS > DSCP > PCP (S-VLAN > C-VLAN)
 *  11 | HLOS > DSCP > PCP (C-VLAN > S-VLAN)
 * ----+----------------------------------------------------------
 * DSCP   = Differentiated Services Code Point
 * PCP    = Priority Code Point (802.1Q VLAN tag)
 * S-VLAN = Service VLAN (outer tag)
 * C-VLAN = Customer VLAN (inner tag)
 * HLOS   = High Level OS (software-assigned priority)
 *
 *
 * ath12k_wifi7_hal_tx_set_tid_map_precedence() - Write TID precedence
 * to all SOC TCL blocks.
 *
 * @dp_hw_grp: Group-level dp structure; tid_map_precedence is read from here.
 * Reads the single group-level tid_map_precedence from dp_hw_grp.  Computes
 * the register value using u32_encode_bits() with HAL_TCL_TID_MAP_PRTY_VAL_MASK
 * (GENMASK(3,0)).  Iterates all dp_hw_grp->dp[] entries and writes to every
 * non-NULL SOC via ath12k_hif_write32().
 */
/*
 * wifi7 TID map precedence hardware register values.
 *
 * The unified userspace VAL (0=DSCP, 1=PCP) is mapped to the wifi7
 * 4-bit hardware register value:
 *   0 (DSCP) → hw VAL 0: DSCP > PCP > HLOS (S-VLAN first)
 *   1 (PCP)  → hw VAL 4: PCP > DSCP > HLOS (S-VLAN first)
 *
 * HLOS is always present but implicitly enabled by other features;
 * it is not user-configurable.
 */
static const u8 ath12k_wifi7_prec_val_map[2] = {
	[0] = HAL_TCL_TID_MAP_PRTY_VAL_DSCP,  /* (DSCP > PCP > HLOS, S-VLAN first) */
	[1] = HAL_TCL_TID_MAP_PRTY_VAL_PCP,   /* (PCP > DSCP > HLOS, S-VLAN first) */
};

void ath12k_wifi7_hal_tx_set_tid_map_precedence(struct ath12k_base *ab,
						const u8 precedence)
{
	u32 reg_val;

	/*
	 * Dumb register writer — no validation.
	 * Validation (0 or 1 only) is performed by the caller (dp.c).
	 *
	 * Map unified userspace VAL to wifi7 hardware register value:
	 *   0 (DSCP) → hw VAL 0 (DSCP > PCP > HLOS, S-VLAN first)
	 *   1 (PCP)  → hw VAL 4 (PCP > DSCP > HLOS, S-VLAN first)
	 *
	 * u32_encode_bits() uses HAL_TCL_TID_MAP_PRTY_VAL_MASK = GENMASK(3,0).
	 */
	reg_val = u32_encode_bits(ath12k_wifi7_prec_val_map[precedence],
				  HAL_TCL_TID_MAP_PRTY_VAL_MASK);

	ath12k_hif_write32(ab, HAL_TCL_R0_PCP_TID_MAP_PRTY_OFFSET, reg_val);
}
EXPORT_SYMBOL(ath12k_wifi7_hal_tx_set_tid_map_precedence);
