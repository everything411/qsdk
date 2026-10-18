// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_desc.h"
#include "../hal.h"
#include "hal.h"
#include "../hif.h"
#include "../debug.h"
#include "dp.h"

#define HAL_TX_BITS_PER_TID 3
#define HAL_TX_NUM_DSCP_REG_SIZE 32

#define ATH12K_TQM_CMD_NUM_MASK 0x3FFFFF
/* Bit 25 is reserved and should always set to 0 */
#define ATH12K_TQM_CMD_LINK_ID  GENMASK(24, 22)

void ath12k_wifi8_hal_tx_cmd_desc_setup(struct ath12k_base *ab,
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
		le32_encode_bits(ti->type, HAL_TCL_DATA_CMD_INFO0_BUF_OR_EXT_DESC_TYPE) |
		le32_encode_bits(ti->bank_id, HAL_TCL_DATA_CMD_INFO0_BANK_ID) |
		le32_encode_bits(ti->vdev_id, HAL_TCL_DATA_CMD_INFO0_VDEV_ID) |
		le32_encode_bits(ti->data_len, HAL_TCL_DATA_CMD_INFO0_DATA_LENGTH);

	tcl_cmd->search_index = cpu_to_le32(ti->bss_ast_idx);
	tcl_cmd->info1 =
		le32_encode_bits(ti->bss_ast_hash, HAL_TCL_DATA_CMD_INFO1_CACHE_SET_NUM) |
		le32_encode_bits(ti->lookup_override,
				 HAL_TCL_DATA_CMD_INFO1_INDEX_LOOKUP_OVERRIDE) |
		le32_encode_bits(ti->tid, HAL_TCL_DATA_CMD_INFO1_HLOS_TID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO1_HLOS_TID_OVERWRITE) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO1_HEADER_LENGTH_READ_SEL);

	tcl_cmd->info2 = cpu_to_le32(ti->flags0) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_MSDU_COLOR) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_FLOW_OVERRIDE_ENABLE) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_WHO_CLASSIFY_INFO_SEL) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP_FORMAT) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP_VALID);

	tcl_cmd->info3 =
		cpu_to_le32(ti->flags1) |
		le32_encode_bits(ti->tx_notify_frame,
				 HAL_TCL_DATA_CMD_INFO3_TX_NOTIFY_FRAME) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO3_FLOW_SELECT) |
		le32_encode_bits(ti->pkt_offset, HAL_TCL_DATA_CMD_INFO3_METADATA_LENGTH) |
		le32_encode_bits(ti->link_id, HAL_TCL_DATA_CMD_INFO3_LINK_ID);

	tcl_cmd->tcl_cmd_number = cpu_to_le32(ti->meta_data_flags);

	tcl_cmd->info4 =
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TX_ENQUEUE_TIMESTAMP) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TX_ENQUEUE_TIMESTAMP_VALID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TELEMETRY_STREAM_ID_VALID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TELEMETRY_STREAM_ID);

	tcl_cmd->insert_vlan_tci_override_val = 0;
	tcl_cmd->info5 =
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO5_INSERT_VLAN_TCI_OVERRIDE_EN) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO5_RING_ID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO5_LOOPING_COUNT);
}

void ath12k_update_dscp_register(struct ath12k_base *ab, u32 addr, u32 mask, u32 value)
{
	u32 reg_val;

	reg_val = ath12k_hif_read32(ab, addr);
	reg_val &= ~mask;
	reg_val |= value;
	ath12k_hif_write32(ab, addr, reg_val);
}

void ath12k_wifi8_hal_tx_update_dscp_tid_map(struct ath12k_base *ab,
					     int id, u8 dscp, u8 tid)
{
	u32 ctrl_reg_val;
	u32 addr;
	u32 start_index, end_index;
	u32 mask;
	u32 value;

	/*Converting ab to cumac ab*/
	ab = ath12k_dp_get_ab_from_dp_hw_group(ab->dp->dp_hw_grp);
	if (!ab)
		return;

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
		addr = addr + 4;
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
			   HAL_TCL1_RING_CMN_CTRL_REG, ctrl_reg_val);
}

void ath12k_wifi8_hal_tx_set_dscp_tid_map(struct ath12k_base *ab, u8 *map, int id)
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

int
ath12k_wifi8_hal_invalidate_tx_cache_cmd_send(struct ath12k_base *ab,
					      struct hal_srng *srng,
					      struct ath12k_hal_tx_cmd_ring_param *param)
{
	struct hal_tcl_gse_cmd *tx_gse_cmd;
	int ret = 0;

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);
	tx_gse_cmd = ath12k_hal_srng_src_get_next_entry(ab, srng);
	if (!tx_gse_cmd) {
		ret = -ENOBUFS;
		goto out;
	}
	tx_gse_cmd->control_buffer_addr_31_0 = cpu_to_le32(param->ctrl_buf_addr);
	tx_gse_cmd->info0 =
		le32_encode_bits(((u64)param->ctrl_buf_addr >> HAL_ADDR_MSB_REG_SHIFT),
				 HAL_TCL_GSE_CMD_INFO0_CONTROL_BUFFER_ADDR_39_32) |
		le32_encode_bits(param->cmd_num,
				 HAL_TCL_GSE_CMD_INFO0_GSE_CTRL) |
		le32_encode_bits(0, /*tcl2sw */
				 HAL_TCL_GSE_CMD_INFO0_STATUS_DESTINATION_RING_ID);
	tx_gse_cmd->info1 = le16_encode_bits(1, /*tcl cmd ring */
					     HAL_TCL_GSE_CMD_INFO1_TCL_CMD_TYPE);
	tx_gse_cmd->cmd_meta_data_31_0 = cpu_to_le32(param->meta_data_0);
out:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return ret;
}

int ath12k_wifi8_hal_tqm_remove_msdu_cmd(struct ath12k_base *ab,
					 struct hal_tlv_64_hdr *tlv,
					 struct ath12k_hal_tqm_cmd *cmd)
{
	struct hal_tqm_remove_msdu *desc;
	u32 paddr_lo;
	u8 paddr_hi;
	struct ath12k_dp *dp = ab->dp;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_TQM_REMOVE_MSDU_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_64_HDR_LENGTH) |
		  le64_encode_bits(ab->pdevs[0].hw_link_id, HAL_TLV_64_HDR_SRC_LINK_ID);

	desc = (struct hal_tqm_remove_msdu *)tlv->value;
	memset_startat(desc, 0, cmd_hdr.info0);

	cmd_num = atomic_inc_return(&dp->tqm_cmd_num) & ATH12K_TQM_CMD_NUM_MASK;
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp->tqm_cmd_num) & ATH12K_TQM_CMD_NUM_MASK;

	cmd_num = cmd_num | le32_encode_bits(ab->pdevs[0].hw_link_id,
					     ATH12K_TQM_CMD_LINK_ID);

	desc->cmd_hdr.tqm_cmd_number = cpu_to_le32(cmd_num);

	desc->cmd_hdr.info0 = le32_encode_bits(0x7F, HAL_TQM_INFO0_SESSION_ID) |
		le32_encode_bits(1, HAL_TQM_INFO0_STATUS_REQUIRED_FOR_HOST) |
		le32_encode_bits(HAL_TQM_HOST_STATUS_RING_0,
				 HAL_TQM_INFO0_HOST_STATUS_RING) |
		le32_encode_bits(cmd->std.peer_id,
				 HAL_TQM_INFO0_SW_PEER_ID_FOR_COMPARISON);

	paddr_lo = lower_32_bits(cmd->remove_msdu_params.msdu_q_paddr);
	paddr_hi = (u8)(upper_32_bits(cmd->remove_msdu_params.msdu_q_paddr)
			& 0x000000ff);
	desc->msdu_flow_desc_addr_31_0 = cpu_to_le32(paddr_lo);

	desc->info0 = le32_encode_bits(paddr_hi,
				       HAL_TQM_MSDU_INFO0_FLOW_DESC_ADDR_39_32);

	desc->info1 = le32_encode_bits(cmd->remove_msdu_params.type,
				       HAL_TQM_MSDU_INFO1_REMOVE_MSDU_CMD_TYPE) |
		le32_encode_bits(cmd->remove_msdu_params.count,
				 HAL_TQM_MSDU_INFO1_REMOVE_COUNT) |
		le32_encode_bits(
			cmd->remove_msdu_params.block_tx_notify_frame_removal,
			HAL_TQM_MSDU_INFO1_BLOCK_TX_NOTIFY_FRAME_REMOVAL);

	return le32_to_cpu(desc->cmd_hdr.tqm_cmd_number);
}

int ath12k_wifi8_hal_tqm_remove_mpdu_cmd(struct ath12k_base *ab,
					 struct hal_tlv_64_hdr *tlv,
					 struct ath12k_hal_tqm_cmd *cmd)
{
	struct hal_tqm_remove_mpdu *desc;
	u32 paddr_lo;
	u8 paddr_hi;
	struct ath12k_dp *dp = ab->dp;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_TQM_REMOVE_MPDU_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_64_HDR_LENGTH) |
		  le64_encode_bits(ab->pdevs[0].hw_link_id, HAL_TLV_64_HDR_SRC_LINK_ID);

	desc = (struct hal_tqm_remove_mpdu *)tlv->value;
	memset_startat(desc, 0, cmd_hdr.info0);

	cmd_num = atomic_inc_return(&dp->tqm_cmd_num) & ATH12K_TQM_CMD_NUM_MASK;
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp->tqm_cmd_num) & ATH12K_TQM_CMD_NUM_MASK;

	cmd_num = cmd_num | le32_encode_bits(ab->pdevs[0].hw_link_id,
					     ATH12K_TQM_CMD_LINK_ID);

	desc->cmd_hdr.tqm_cmd_number = cpu_to_le32(cmd_num);

	desc->cmd_hdr.info0 = le32_encode_bits(0x7F, HAL_TQM_INFO0_SESSION_ID) |
		le32_encode_bits(1, HAL_TQM_INFO0_STATUS_REQUIRED_FOR_HOST) |
		le32_encode_bits(HAL_TQM_HOST_STATUS_RING_0,
				 HAL_TQM_INFO0_HOST_STATUS_RING) |
		le32_encode_bits(cmd->std.peer_id,
				 HAL_TQM_INFO0_SW_PEER_ID_FOR_COMPARISON);

	paddr_lo = lower_32_bits(cmd->remove_mpdu_params.mpdu_q_paddr);
	paddr_hi = (u8)(upper_32_bits(cmd->remove_mpdu_params.mpdu_q_paddr)
			& 0x000000ff);
	desc->mpdu_queue_desc_addr_31_0 = cpu_to_le32(paddr_lo);

	desc->info0 = le32_encode_bits(paddr_hi,
				       HAL_TQM_MPDU_INFO0_QUEUE_DESC_ADDR_39_32) |
		le32_encode_bits(
			cmd->remove_mpdu_params.block_tx_notify_frame_removal,
			HAL_TQM_MPDU_INFO0_BLOCK_TX_NOTIFY_FRAME_REMOVAL);

	desc->info1 = le32_encode_bits(cmd->remove_mpdu_params.type,
				       HAL_TQM_MPDU_INFO1_REMOVE_MPDU_CMD_TYPE) |
		le32_encode_bits(cmd->remove_mpdu_params.count,
				 HAL_TQM_MPDU_INFO1_REMOVE_COUNT);

	return le32_to_cpu(desc->cmd_hdr.tqm_cmd_number);
}

int ath12k_wifi8_hal_tqm_sync_cmd(struct ath12k_base *ab,
				  struct hal_tlv_64_hdr *tlv,
				  struct ath12k_hal_tqm_cmd *cmd)
{
	struct hal_tqm_sync_cmd *desc;
	u32 data_lo, data_hi;
	uintptr_t cb_func_addr, cb_ctxt_addr;
	struct ath12k_dp *dp = ab->dp;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_TQM_SYNC_CMD_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_64_HDR_LENGTH) |
		  le64_encode_bits(ab->pdevs[0].hw_link_id, HAL_TLV_64_HDR_SRC_LINK_ID);

	desc = (struct hal_tqm_sync_cmd *)tlv->value;
	memset_startat(desc, 0, cmd_hdr.info0);

	cmd_num = atomic_inc_return(&dp->tqm_cmd_num) & ATH12K_TQM_CMD_NUM_MASK;
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp->tqm_cmd_num) & ATH12K_TQM_CMD_NUM_MASK;

	cmd_num = cmd_num | le32_encode_bits(ab->pdevs[0].hw_link_id,
					     ATH12K_TQM_CMD_LINK_ID);

	desc->cmd_hdr.tqm_cmd_number = cpu_to_le32(cmd_num);

	desc->cmd_hdr.info0 = le32_encode_bits(0x7F, HAL_TQM_INFO0_SESSION_ID) |
		le32_encode_bits(1, HAL_TQM_INFO0_STATUS_REQUIRED_FOR_HOST) |
		le32_encode_bits(HAL_TQM_HOST_STATUS_RING_0,
				 HAL_TQM_INFO0_HOST_STATUS_RING) |
		le32_encode_bits(cmd->std.peer_id,
				 HAL_TQM_INFO0_SW_PEER_ID_FOR_COMPARISON);

	if (cmd->tqm_sync_params.data_only) {
		data_lo = lower_32_bits(cmd->tqm_sync_params.cb_data);
		data_hi = upper_32_bits(cmd->tqm_sync_params.cb_data);
		desc->sync_cmd_sw_metadata_31_0 = cpu_to_le32(data_lo);
		desc->sync_cmd_sw_metadata_63_32 = cpu_to_le32(data_hi);
	} else {
		cb_func_addr = (u64)(uintptr_t)cmd->tqm_sync_params.cb_func;
		cb_ctxt_addr = (u64)(uintptr_t)cmd->tqm_sync_params.cb_ctxt;
		desc->sync_cmd_sw_metadata_31_0 =
			cpu_to_le32(lower_32_bits(cb_func_addr));
		desc->sync_cmd_sw_metadata_63_32 =
			cpu_to_le32(upper_32_bits(cb_func_addr));
		desc->sync_cmd_sw_metadata_95_64 =
			cpu_to_le32(lower_32_bits(cb_ctxt_addr));
		desc->sync_cmd_sw_metadata_127_96 =
			cpu_to_le32(upper_32_bits(cb_ctxt_addr));
		data_lo = lower_32_bits(cmd->tqm_sync_params.cb_data);
		data_hi = upper_32_bits(cmd->tqm_sync_params.cb_data);
		desc->sync_cmd_sw_metadata_159_128 = cpu_to_le32(data_lo);
		desc->sync_cmd_sw_metadata_191_160 = cpu_to_le32(data_hi);
	}
	return le32_to_cpu(desc->cmd_hdr.tqm_cmd_number);
}

static inline
int ath12k_wifi8_hal_srng_write_words(struct ath12k_base *ab,
				      struct hal_srng *srng,
				      u32 cmd_size,
				      void *desc_start,
				      const u32 *src_words)
{
	u32 hp, ring_size, i, ring_idx;
	u32 *ring_base;

	ring_base = (u32 *)srng->ring_base_vaddr;
	hp = (u32)((u32 *)desc_start - ring_base);
	ring_size = srng->ring_size;
	ring_idx = hp;
	for (i = 0; i < cmd_size; i++) {
		ring_base[ring_idx] = src_words[i];
		ring_idx++;
		if (ring_idx == ring_size)
			ring_idx = 0;
	}
	return 0;
}

int ath12k_wifi8_hal_tqm_get_mpduq_stats(struct ath12k_base *ab,
					 struct hal_tlv_64_hdr *tlv,
					 struct ath12k_hal_tqm_cmd *cmd)
{
	struct ath12k_dp *dp = ab->dp;
	struct hal_tqm_get_mpduq_stats *desc;
	u32 paddr_lo;
	u8 paddr_hi;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_TQM_GET_MPDUQ_STATS_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_64_HDR_LENGTH) |
		  le64_encode_bits(ab->pdevs[0].hw_link_id, HAL_TLV_64_HDR_SRC_LINK_ID);

	desc = (struct hal_tqm_get_mpduq_stats *)tlv->value;
	memset_startat(desc, 0, cmd_hdr.info0);

	cmd_num = atomic_inc_return(&dp->tqm_cmd_num) & ATH12K_TQM_CMD_NUM_MASK;
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp->tqm_cmd_num) & ATH12K_TQM_CMD_NUM_MASK;

	cmd_num = cmd_num | le32_encode_bits(ab->pdevs[0].hw_link_id,
					     ATH12K_TQM_CMD_LINK_ID);

	desc->cmd_hdr.tqm_cmd_number = cpu_to_le32(cmd_num);
	desc->cmd_hdr.info0 = le32_encode_bits(0x7F, HAL_TQM_INFO0_SESSION_ID) |
		le32_encode_bits(1, HAL_TQM_INFO0_STATUS_REQUIRED_FOR_HOST) |
		le32_encode_bits(HAL_TQM_HOST_STATUS_RING_0,
				 HAL_TQM_INFO0_HOST_STATUS_RING) |
		le32_encode_bits(cmd->std.peer_id,
				 HAL_TQM_INFO0_SW_PEER_ID_FOR_COMPARISON);

	paddr_lo = lower_32_bits(cmd->get_mpduq_stats.mpdu_q_paddr);
	paddr_hi = (u8)(upper_32_bits(cmd->get_mpduq_stats.mpdu_q_paddr)
			& 0x000000ff);
	desc->mpdu_queue_desc_addr_31_0 = cpu_to_le32(paddr_lo);

	desc->info0 = le16_encode_bits(paddr_hi,
				       HAL_TQM_GET_MPDUQ_STATS_INFO0_MPDUQ_ADDR_HI) |
		      le16_encode_bits(cmd->get_mpduq_stats.clear_stats,
				       HAL_TQM_GET_MPDUQ_STATS_INFO0_CLEAR_STATS);

	return le32_to_cpu(desc->cmd_hdr.tqm_cmd_number);
}

int ath12k_wifi8_hal_tqm_update_mpduq(struct ath12k_base *ab,
				      struct hal_tlv_64_hdr *tlv,
				      struct ath12k_hal_tqm_cmd *cmd)
{
	struct ath12k_dp *dp = ab->dp;
	struct hal_tqm_update_mpduq *desc;
	u32 paddr_lo;
	u8 paddr_hi;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_TQM_UPDATE_MPDUQ_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_64_HDR_LENGTH) |
		  le64_encode_bits(ab->pdevs[0].hw_link_id, HAL_TLV_64_HDR_SRC_LINK_ID);

	desc = (struct hal_tqm_update_mpduq *)tlv->value;
	memset(desc, 0, sizeof(*desc));
	memset_startat(desc, 0, cmd_hdr.info0);

	cmd_num = atomic_inc_return(&dp->tqm_cmd_num) & ATH12K_TQM_CMD_NUM_MASK;
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp->tqm_cmd_num) & ATH12K_TQM_CMD_NUM_MASK;

	cmd_num = cmd_num | le32_encode_bits(ab->pdevs[0].hw_link_id,
					     ATH12K_TQM_CMD_LINK_ID);

	desc->cmd_hdr.tqm_cmd_number = cpu_to_le32(cmd_num);
	desc->cmd_hdr.info0 = le32_encode_bits(0x7F, HAL_TQM_INFO0_SESSION_ID) |
		le32_encode_bits(1, HAL_TQM_INFO0_STATUS_REQUIRED_FOR_HOST) |
		le32_encode_bits(HAL_TQM_HOST_STATUS_RING_0,
				 HAL_TQM_INFO0_HOST_STATUS_RING) |
		le32_encode_bits(cmd->std.peer_id,
				 HAL_TQM_INFO0_SW_PEER_ID_FOR_COMPARISON);

	paddr_lo = lower_32_bits(cmd->update_mpduq.mpdu_q_paddr);
	paddr_hi = (u8)(upper_32_bits(cmd->update_mpduq.mpdu_q_paddr)
			& 0x000000ff);
	desc->mpdu_queue_desc_addr_31_0 = cpu_to_le32(paddr_lo);
	desc->info0 = le32_encode_bits(paddr_hi,
				       HAL_TQM_UPDATE_MPDUQ_INFO0_MPDUQ_ADDR_HI);

	if (cmd->update_mpduq.update_queue_number) {
		desc->info0 |= le32_encode_bits(1,
				HAL_TQM_UPDATE_MPDUQ_INFO0_UPDATE_TX_QUEUE_NUMBER);
		desc->info4 |= le32_encode_bits(cmd->update_mpduq.new_queue_number,
					HAL_TQM_UPDATE_MPDUQ_INFO4_QUEUE_NUMBER_VALUE);
	}

	if (cmd->update_mpduq.update_queue_valid)
		desc->info0 |= le32_encode_bits(1,
					HAL_TQM_UPDATE_MPDUQ_INFO0_UPDATE_QUEUE_VALID);

	if (cmd->update_mpduq.update_peer_id) {
		desc->info0 |= le32_encode_bits(1,
					HAL_TQM_UPDATE_MPDUQ_INFO0_UPDATE_SW_PEER_ID);
		desc->sw_peer_id = cpu_to_le16(cmd->update_mpduq.new_peer_id);
	}

	if (cmd->update_mpduq.update_tid) {
		desc->info0 |= le32_encode_bits(1,
					HAL_TQM_UPDATE_MPDUQ_INFO0_UPDATE_TID);
		desc->info2 |= le16_encode_bits(cmd->update_mpduq.new_tid,
						HAL_TQM_UPDATE_MPDUQ_INFO2_TID_VALUE);
	}

	if (cmd->update_mpduq.update_mpdu_type)
		desc->info0 |= le32_encode_bits(1,
					HAL_TQM_UPDATE_MPDUQ_INFO0_UPDATE_MPDU_TYPE);

	if (cmd->update_mpduq.max_lsn_valid) {
		desc->info3 |=
			le16_encode_bits(cmd->update_mpduq.max_lsn_valid,
					 HAL_TQM_UPDATE_MPDUQ_INFO3_MAX_LSN_VALID) |
			le16_encode_bits(cmd->update_mpduq.max_lsn,
					 HAL_TQM_UPDATE_MPDUQ_INFO3_MAX_LSN);

		desc->info4 |=
		le32_encode_bits(1, HAL_TQM_UPDATE_MPDUQ_INFO4_UPDATE_MAX_LSN_VALID) |
		le32_encode_bits(1, HAL_TQM_UPDATE_MPDUQ_INFO4_UPDATE_MAX_LSN);
	}

	if (cmd->update_mpduq.sn_num_valid) {
		desc->info0 |=
			le16_encode_bits(cmd->update_mpduq.sn_num_valid,
					 HAL_TQM_UPDATE_MPDUQ_INFO0_SEQ_NUM_UPDATE_VALID);
		desc->info1 |=
			le32_encode_bits(cmd->update_mpduq.sn_num,
					 HAL_TQM_UPDATE_MPDUQ_INFO1_START_SEQ_NUM) |
			le32_encode_bits(cmd->update_mpduq.sn_num,
					 HAL_TQM_UPDATE_MPDUQ_INFO1_LAST_SEQ_NUM);
	}

	return le32_to_cpu(desc->cmd_hdr.tqm_cmd_number);
}

/**
 * ath12k_wifi8_hal_tqm_fw_buf_release_cmd() - Build TQM FW-completion command
 * for FW-owned buffer release, routing completion to HAL_TQM_HOST_STATUS_RING_1
 * (fw_tqm2sw_ring).
 *
 * Uses TLV tag HAL_TQM_FW_COMPLETION_BO (637). Extracts paddr, rbm, and cookie
 * from cmd->fw_buf_release_params and encodes them into buffer_addr_info fields
 * so TQM/WBM returns the buffer to the correct FW buffer pool.
 *
 * Called via ath12k_wifi8_hal_tqm_cmd_send() switch case HAL_TQM_FW_COMPLETION_BO.
 */
void ath12k_wifi8_hal_tqm_fw_buf_release_cmd(struct ath12k_base *ab,
					    struct hal_tlv_64_hdr *tlv,
					    struct ath12k_hal_tqm_cmd *cmd)
{
	struct hal_tqm2sw_completion_ring *desc;
	dma_addr_t paddr = cmd->fw_buf_release_params.paddr;
	u8 rbm            = cmd->fw_buf_release_params.rbm;
	u32 cookie        = cmd->fw_buf_release_params.cookie;

	tlv->tl = le64_encode_bits(HAL_TQM_FW_COMPLETION_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN) |
		  le64_encode_bits(ab->pdevs[0].hw_link_id, HAL_TLV_64_HDR_SRC_LINK_ID);

	desc = (struct hal_tqm2sw_completion_ring *)tlv->value;
	memset(desc, 0, sizeof(*desc));

	/*
	 * Fill buffer_addr_info fields so TQM/WBM returns the buffer to the
	 * correct FW buffer pool (return_buffer_manager = rbm):
	 *   info0 = buffer_addr[31:0]
	 *   info1 = buffer_addr[39:32] | return_buffer_manager[11:8] |
	 *           sw_buffer_cookie[31:12]
	 */
	desc->buf_addr_info.info0 = le32_encode_bits(lower_32_bits(paddr),
				       BUFFER_ADDR_INFO0_ADDR);
	desc->buf_addr_info.info1 = le32_encode_bits(upper_32_bits(paddr) & 0xff,
				       BUFFER_ADDR_INFO1_ADDR) |
		      le32_encode_bits(rbm, BUFFER_ADDR_INFO1_RET_BUF_MGR) |
		      le32_encode_bits(cookie, BUFFER_ADDR_INFO1_SW_COOKIE);

	ath12k_dbg(ab, ATH12K_DBG_DP_TX,
		   "fw_tqm2sw cmd: paddr=0x%llx rbm=%u cookie=0x%x\n",
		   (unsigned long long)paddr, rbm, cookie);

}

int ath12k_wifi8_hal_tqm_update_msduq(struct ath12k_base *ab,
				      struct hal_tlv_64_hdr *tlv,
				      struct ath12k_hal_tqm_cmd *cmd)
{
	struct hal_tqm_update_tx_msdu_flow_params *update_params;
	struct hal_tqm_update_tx_msdu_flow *desc;
	struct ath12k_dp *dp = ab->dp;
	u32 paddr_lo, cmd_num;
	u8 paddr_hi;

	update_params = &cmd->update_tx_msdu_params;

	/*
	 * HW1.0 (E3R65) and HW2.0 (E3R86) differ only in the trailing reserved
	 * field of hal_tqm_update_tx_msdu_flow (scalar rsvd0 vs rsvd0[5], a
	 * 16-byte difference). The TLV length must match the layout the hardware
	 * expects; sending the HW2.0 size to HW1.0 would corrupt the TQM command
	 * ring. Select the size from the version-specific struct so it stays
	 * correct if either layout changes.
	 */
	tlv->tl = le64_encode_bits(HAL_TQM_UPDATE_MSDUQ_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(ab->hw_rev == ATH12K_HW_QCN9625_HW10
				   ? sizeof(struct hal_tqm_update_tx_msdu_flow_hw10)
				   : sizeof(struct hal_tqm_update_tx_msdu_flow),
				   HAL_TLV_64_HDR_LENGTH) |
		  le64_encode_bits(ab->pdevs[0].hw_link_id, HAL_TLV_64_HDR_SRC_LINK_ID);

	desc = (struct hal_tqm_update_tx_msdu_flow *)tlv->value;
	memset(desc, 0, sizeof(*desc));
	memset_startat(desc, 0, cmd_hdr.info0);

	cmd_num = atomic_inc_return(&dp->tqm_cmd_num) & ATH12K_TQM_CMD_NUM_MASK;
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp->tqm_cmd_num) & ATH12K_TQM_CMD_NUM_MASK;

	cmd_num = cmd_num | le32_encode_bits(ab->pdevs[0].hw_link_id,
					     ATH12K_TQM_CMD_LINK_ID);

	desc->cmd_hdr.tqm_cmd_number = cpu_to_le32(cmd_num);
	desc->cmd_hdr.info0 = le32_encode_bits(0x7F, HAL_TQM_INFO0_SESSION_ID) |
		le32_encode_bits(1, HAL_TQM_INFO0_STATUS_REQUIRED_FOR_HOST) |
		le32_encode_bits(HAL_TQM_HOST_STATUS_RING_0,
				 HAL_TQM_INFO0_HOST_STATUS_RING) |
		le32_encode_bits(cmd->std.peer_id,
				 HAL_TQM_INFO0_SW_PEER_ID_FOR_COMPARISON);

	paddr_lo = lower_32_bits(update_params->msdu_q_paddr);
	paddr_hi = (u8)(upper_32_bits(update_params->msdu_q_paddr)
			& 0x000000ff);
	desc->flow_queue_addr_31_0 = cpu_to_le32(paddr_lo);

	desc->info0 = le32_encode_bits(paddr_hi, HAL_TQM_FLOW_INFO0_QUEUE_ADDR_39_32);

	if (update_params->update_flow_number) {
		desc->info0 |=
		      le32_encode_bits(1, HAL_TQM_FLOW_INFO0_UPDATE_TX_FLOW_NUMBER);
		desc->info3 |=
			le32_encode_bits(update_params->tx_flow_number,
					 HAL_TQM_FLOW_INFO3_UPDATE_TX_MSDU_FLOW_NUMBER);
	}

	if (update_params->update_flow_valid) {
		desc->info0 |=
		      le32_encode_bits(1, HAL_TQM_FLOW_INFO0_UPDATE_FLOW_VALID);
		desc->info1 |= le32_encode_bits(1, HAL_TQM_FLOW_INFO1_VALID);
	}

	if (update_params->update_peer_id) {
		desc->info0 |=
		      le32_encode_bits(1, HAL_TQM_FLOW_INFO0_UPDATE_SW_PEER_ID);
		desc->sw_peer_id = cpu_to_le16(update_params->new_peer_id);
	}

	if (update_params->update_tid) {
		desc->info0 |=
		      le32_encode_bits(1, HAL_TQM_FLOW_INFO0_UPDATE_TID);
		desc->info1 |= le32_encode_bits(update_params->tid,
						HAL_TQM_FLOW_INFO1_TID);
	}

	if (update_params->svc < HAL_TQM_SERVICE_CATEGORY_MAX) {
		desc->info2 =
		le32_encode_bits(1, HAL_TQM_FLOW_INFO2_SERVICE_CATEGORY_VALID);
		desc->info3 |=
		le32_encode_bits(1, HAL_TQM_FLOW_INFO3_UPDATE_SERVICE_CATEGORY_VALID);
		desc->info5 =
		le32_encode_bits(1, HAL_TQM_FLOW_INFO5_UPDATE_SERVICE_CATEGORY) |
		le32_encode_bits(update_params->svc,
				 HAL_TQM_FLOW_INFO5_SERVICE_CATEGORY);
	}

	if (update_params->update_hard_drop_threshold) {
		desc->info0 |=
		le32_encode_bits(1, HAL_TQM_FLOW_INFO0_UPDATE_HARD_DROP_THRESHOLD);
		desc->hard_drop_threshold =
					cpu_to_le16(update_params->hard_drop_threshold);
	}

	if (update_params->bitmap) {
		u8 bitmap = update_params->bitmap;

		if (bitmap & ATH12K_CHIP0_BITMAP_MASK) {
			desc->info1 |=
		le32_encode_bits(1,
				 HAL_TQM_FLOW_INFO1_UPDATE_STATUS_REQUIRED_FOR_CHIP0);
			desc->info4 |=
			le16_encode_bits(1,
					 HAL_TQM_FLOW_INFO4_STATUS_REQUIRED_FOR_CHIP0);
		}
		if (bitmap & ATH12K_CHIP1_BITMAP_MASK) {
			desc->info1 |=
		le32_encode_bits(1,
				 HAL_TQM_FLOW_INFO1_UPDATE_STATUS_REQUIRED_FOR_CHIP1);
			desc->info4 |=
			le16_encode_bits(1,
					 HAL_TQM_FLOW_INFO4_STATUS_REQUIRED_FOR_CHIP1);
		}
		if (bitmap & ATH12K_CHIP2_BITMAP_MASK) {
			desc->info1 |=
		le32_encode_bits(1,
				 HAL_TQM_FLOW_INFO1_UPDATE_STATUS_REQUIRED_FOR_CHIP2);
			desc->info4 |=
			le16_encode_bits(1,
					 HAL_TQM_FLOW_INFO4_STATUS_REQUIRED_FOR_CHIP2);
		}
		if (bitmap & ATH12K_CHIP3_BITMAP_MASK) {
			desc->info1 |=
		le32_encode_bits(1,
				 HAL_TQM_FLOW_INFO1_UPDATE_STATUS_REQUIRED_FOR_CHIP3);
			desc->info4 |=
			le16_encode_bits(1,
					 HAL_TQM_FLOW_INFO4_STATUS_REQUIRED_FOR_CHIP3);
		}
		if (bitmap & ATH12K_CHIP4_BITMAP_MASK) {
			desc->info1 |=
		le32_encode_bits(1,
				 HAL_TQM_FLOW_INFO1_UPDATE_STATUS_REQUIRED_FOR_CHIP4);
			desc->info4 |=
			le16_encode_bits(1,
					 HAL_TQM_FLOW_INFO4_STATUS_REQUIRED_FOR_CHIP4);
		}
	}

	return le32_to_cpu(desc->cmd_hdr.tqm_cmd_number);
}

int ath12k_wifi8_hal_tqm_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				  enum hal_tlv_tag_be type,
				  struct ath12k_hal_tqm_cmd *cmd)
{
	struct hal_tlv_64_hdr *tqm_desc;
	struct hal_tlv_64_hdr *tlv_desc;
	u32 cmd_size;
	int ret = 0;
	int err = 0;

	if (!ab->tqm_cmd_staging) {
		ath12k_err(ab, "TQM staging buffer not allocated\n");
		return -ENOMEM;
	}
	const char *cmd_type_str;

	/* Map command type to string for debug logging */
	switch (type) {
	case HAL_TQM_REMOVE_MSDU_BO:
		cmd_type_str = "REMOVE_MSDU";
		break;
	case HAL_TQM_REMOVE_MPDU_BO:
		cmd_type_str = "REMOVE_MPDU";
		break;
	case HAL_TQM_SYNC_CMD_BO:
		cmd_type_str = "SYNC_CMD";
		break;
	case HAL_TQM_UPDATE_MSDUQ_BO:
		cmd_type_str = "UPDATE_MSDU_FLOW";
		break;
	case HAL_TQM_UPDATE_MPDUQ_BO:
		cmd_type_str = "UPDATE_MPDU_QUEUE";
		break;
	default:
		cmd_type_str = "UNKNOWN";
		break;
	}

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "hal-tqm-cmd-send: ENTRY type=%s(%d) peer_id=%u\n",
			 cmd_type_str, type, cmd->std.peer_id);
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	tlv_desc = (struct hal_tlv_64_hdr *)ab->tqm_cmd_staging;
	if (!tlv_desc) {
		ret = -ENOMEM;
		ath12k_warn(ab, "hal-tqm-cmd-send: No buffer space for %s command peer_id=%u\n",
			    cmd_type_str, cmd->std.peer_id);
		goto out;
	}
	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
			 "hal-tqm-cmd-send: Got TQM descriptor, processing %s command\n",
			 cmd_type_str);

	switch (type) {
	case HAL_TQM_REMOVE_MSDU_BO:
		ret = ath12k_wifi8_hal_tqm_remove_msdu_cmd(ab, tlv_desc, cmd);
		break;
	case HAL_TQM_REMOVE_MPDU_BO:
		ret = ath12k_wifi8_hal_tqm_remove_mpdu_cmd(ab, tlv_desc, cmd);
		break;
	case HAL_TQM_SYNC_CMD_BO:
		ret = ath12k_wifi8_hal_tqm_sync_cmd(ab, tlv_desc, cmd);
		break;
	case HAL_TQM_GET_MPDUQ_STATS_BO:
		ret = ath12k_wifi8_hal_tqm_get_mpduq_stats(ab, tlv_desc, cmd);
		break;
	case HAL_TQM_UPDATE_MSDUQ_BO:
		ret = ath12k_wifi8_hal_tqm_update_msduq(ab, tlv_desc, cmd);
		break;
	case HAL_TQM_UPDATE_MPDUQ_BO:
		ret = ath12k_wifi8_hal_tqm_update_mpduq(ab, tlv_desc, cmd);
		break;
	case HAL_TQM_FW_COMPLETION_BO:
		ath12k_wifi8_hal_tqm_fw_buf_release_cmd(ab, tlv_desc, cmd);
		break;
	default:
		ath12k_warn(ab, "Unknown tqm command %d\n", type);
		ret = -EINVAL;
		goto out;
	}

	tqm_desc = (struct hal_tlv_64_hdr *)
			ath12k_hal_srng_src_get_next_entry_by_cmd_size(ab, srng,
								       type);
	if (!tqm_desc) {
		ret = -ENOBUFS;
		goto out;
	}

	cmd_size = ath12k_hal_srng_get_cmd_size(ab, type);
	if (!cmd_size || cmd_size >= srng->ring_size ||
	    cmd_size > HAL_TQM_CMD_MAX_WORDS) {
		ret = -EINVAL;
		goto out;
	}

	err = ath12k_wifi8_hal_srng_write_words(ab, srng, cmd_size,
						(void *)tqm_desc,
						ab->tqm_cmd_staging);
	if (err) {
		ret = err;
		goto out;
	}

out:

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "hal-tqm-cmd-send: EXIT %s ret=%d peer_id=%u\n",
			 cmd_type_str, ret, cmd->std.peer_id);

	return ret;
}

void ath12k_wifi8_hal_tqm_remove_msdu_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_tqm_status *status)
{
	struct hal_tqm_remove_msdu_status *desc =
		(struct hal_tqm_remove_msdu_status *)tlv->value;

	status->status_hdr.status_num =  le32_to_cpu(desc->status_hdr.tqm_status_number);
	status->status_hdr.cmd_execution_status =
		le32_get_bits(desc->status_hdr.info0,
			      HAL_TQM_STATUS_INFO0_CMD_EXECUTION_STATUS);
	status->status_hdr.tqm_status_ring = le32_get_bits(desc->status_hdr.info0,
							   HAL_TQM_STATUS_INFO0_RING);
	status->remove_msdu.remove_msdu_cmd_type =
		le32_get_bits(desc->info0,
			      HAL_TQM_MSDU_RMSTAT_INFO0_REMOVE_MSDU_CMD_TYPE);
	status->remove_msdu.removed_msdu_count =
		le32_get_bits(desc->info0,
			      HAL_TQM_MSDU_RMSTAT_INFO0_REMOVED_MSDU_COUNT);
	status->remove_msdu.tx_flow_number = le32_get_bits(
						desc->info1,
						HAL_TQM_MSDU_RMSTAT_INFO1_TX_FLOW_NUMBER);
}

void ath12k_wifi8_hal_tqm_remove_mpdu_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_tqm_status *status)
{
	struct hal_tqm_remove_mpdu_status *desc =
		(struct hal_tqm_remove_mpdu_status *)tlv->value;

	status->status_hdr.status_num =  le32_to_cpu(desc->status_hdr.tqm_status_number);
	status->status_hdr.cmd_execution_status =
		le32_get_bits(desc->status_hdr.info0,
			      HAL_TQM_STATUS_INFO0_CMD_EXECUTION_STATUS);
	status->status_hdr.tqm_status_ring = le32_get_bits(desc->status_hdr.info0,
							   HAL_TQM_STATUS_INFO0_RING);
	status->remove_mpdu.remove_mpdu_cmd_type =
		le32_get_bits(desc->info0,
			      HAL_TQM_MPDU_RMSTAT_INFO0_REMOVE_MPDU_CMD_TYPE);
	status->remove_mpdu.removed_mpdu_count =
		le32_get_bits(desc->info0,
			      HAL_TQM_MPDU_RMSTAT_INFO0_REMOVED_MPDU_COUNT);
	status->remove_mpdu.tx_mpdu_queue_number =
		le32_get_bits(desc->info5,
			      HAL_TQM_MPDU_RMSTAT_INFO5_TX_MPDU_QUEUE_NUMBER);
}

void ath12k_wifi8_hal_tqm_get_mpduq_stats_cmd_status(struct ath12k_base *ab,
						     struct hal_tlv_64_hdr *tlv,
						     struct hal_tqm_status *status)
{
	struct hal_tqm_get_mpduq_stats_cmd_status *desc =
		(struct hal_tqm_get_mpduq_stats_cmd_status *)tlv->value;

	status->status_hdr.status_num =  le32_to_cpu(desc->status_hdr.tqm_status_number);
	status->status_hdr.cmd_execution_status =
				le32_get_bits(desc->status_hdr.info0,
					      HAL_TQM_STATUS_INFO0_CMD_EXECUTION_STATUS);
	status->status_hdr.tqm_status_ring = le32_get_bits(desc->status_hdr.info0,
							   HAL_TQM_STATUS_INFO0_RING);

	status->mpduq_stats.mpdu_cnt = le16_to_cpu(desc->mpdu_count);
	status->mpduq_stats.start_seq_num =
			le32_get_bits(desc->info0,
				      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO0_START_SEQ_NUM);
	status->mpduq_stats.tid =
			le32_get_bits(desc->info0,
				      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO0_TID);
	status->mpduq_stats.pn_31_0 = le32_to_cpu(desc->pn_31_0);
	status->mpduq_stats.pn_47_32 = le16_to_cpu(desc->pn_47_32);
	status->mpduq_stats.sw_peer_id = le16_to_cpu(desc->sw_peer_id);
	status->mpduq_stats.last_seq_num =
			le32_get_bits(desc->info1,
				      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO1_LAST_SEQ_NUM);
	status->mpduq_stats.max_lsn_valid =
		le32_get_bits(desc->info1,
			      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO1_MAX_LSN_VALID);
	status->mpduq_stats.max_lsn =
			le32_get_bits(desc->info1,
				      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO1_MAX_LSN);
}

void ath12k_wifi8_hal_tqm_update_mpduq_cmd_status(struct ath12k_base *ab,
						  struct hal_tlv_64_hdr *tlv,
						  struct hal_tqm_status *status)
{
	struct hal_tqm_update_mpduq_cmd_status *desc =
		(struct hal_tqm_update_mpduq_cmd_status *)tlv->value;
	u32 tx_mpdu_queue_number;
	u8 update_req_not_met;
	u16 allocated_seq_num;
	u64 allocated_pn;
	u16 sw_peer_id;
	u8 tid;

	status->status_hdr.status_num = le32_to_cpu(desc->status_hdr.tqm_status_number);
	status->status_hdr.cmd_execution_status =
				le32_get_bits(desc->status_hdr.info0,
					      HAL_TQM_STATUS_INFO0_CMD_EXECUTION_STATUS);
	status->status_hdr.tqm_status_ring = le32_get_bits(desc->status_hdr.info0,
							   HAL_TQM_STATUS_INFO0_RING);

	update_req_not_met =
		le32_get_bits(desc->info0,
			      HAL_TQM_UPDATE_MPDUQ_CMD_STATUS_INFO0_UPDATE_REQ_MET);
	tx_mpdu_queue_number =
		le32_get_bits(desc->info0,
			      HAL_TQM_UPDATE_MPDUQ_CMD_STATUS_INFO0_TX_MPDU_QUEUE_NUM);
	sw_peer_id = le32_get_bits(desc->info1,
				   HAL_TQM_UPDATE_MPDUQ_CMD_STATUS_INFO1_SW_PEER_ID);
	tid = le32_get_bits(desc->info1, HAL_TQM_UPDATE_MPDUQ_CMD_STATUS_INFO1_TID);
	allocated_seq_num =
		le32_get_bits(desc->info1,
			      HAL_TQM_UPDATE_MPDUQ_CMD_STATUS_INFO1_ALLOC_SEQ_NUM);

	allocated_pn = le32_to_cpu(desc->allocated_pn_31_0);
	allocated_pn |= (u64)le16_to_cpu(desc->allocated_pn_47_32) << 32;

	status->update_mpdu_queue.update_requirements_not_met = update_req_not_met;
	status->update_mpdu_queue.tx_mpdu_queue_number = tx_mpdu_queue_number;
	status->update_mpdu_queue.sw_peer_id = sw_peer_id;
	status->update_mpdu_queue.tid = tid;
	status->update_mpdu_queue.allocated_sequence_number = allocated_seq_num;
	status->update_mpdu_queue.allocated_pn = allocated_pn;

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
			 "tqm-mpdu-status: s=%u e=%u r=%u nm=%u q=%u p=%u tid=%u sn=%u pn=0x%llx\n",
			 status->status_hdr.status_num,
			 status->status_hdr.cmd_execution_status,
			 status->status_hdr.tqm_status_ring,
			 update_req_not_met, tx_mpdu_queue_number,
			 sw_peer_id, tid, allocated_seq_num, allocated_pn);
}

void ath12k_wifi8_hal_tqm_sync_cmd_status(struct ath12k_base *ab,
					  struct hal_tlv_64_hdr *tlv,
					  struct hal_tqm_status *status)
{
	struct hal_tqm_sync_cmd_status *desc =
		(struct hal_tqm_sync_cmd_status *)tlv->value;

	status->status_hdr.status_num =  le32_to_cpu(desc->status_hdr.tqm_status_number);
	status->status_hdr.cmd_execution_status =
		le32_get_bits(desc->status_hdr.info0,
			      HAL_TQM_STATUS_INFO0_CMD_EXECUTION_STATUS);
	status->status_hdr.tqm_status_ring = le32_get_bits(desc->status_hdr.info0,
							   HAL_TQM_STATUS_INFO0_RING);
	status->sync_status.metadata_0 = le32_to_cpu(desc->sync_cmd_sw_metadata_31_0);
	status->sync_status.metadata_1 = le32_to_cpu(desc->sync_cmd_sw_metadata_63_32);
	status->sync_status.metadata_2 = le32_to_cpu(desc->sync_cmd_sw_metadata_95_64);
}

int ath12k_wifi8_hal_tqm_cmd_staging_alloc(struct ath12k_base *ab)
{
	/* Allocate single staging buffer in ab for the TQM command ring. */
	ab->tqm_cmd_staging = kzalloc(HAL_TQM_CMD_MAX_BYTES,
				      GFP_ATOMIC);

	if (!ab->tqm_cmd_staging) {
		ath12k_err(ab, "failed to alloc TQM staging");
		return -ENOMEM;
	}
	return 0;
}

void ath12k_wifi8_hal_tqm_cmd_staging_free(struct ath12k_base *ab)
{
	kfree(ab->tqm_cmd_staging);
	ab->tqm_cmd_staging = NULL;
}

int ath12k_wifi8_hal_tx_sam_mpduq_clear_cmd(struct ath12k_dp_wifi8 *dp_wifi8,
					    struct hal_tlv_64_hdr *tlv,
					    int id, bool clear_all)
{
	struct ath12k_base *ab = dp_wifi8->dp->ab;
	struct hal_sam_mpdu_queue_clear_programming *desc;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_SAM_MPDU_QUEUE_CLEAR_PROGRAMMING_BO,
				   HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_64_HDR_LENGTH);

	desc = (struct hal_sam_mpdu_queue_clear_programming *)tlv->value;

	cmd_num = atomic_inc_return(&dp_wifi8->sam_cmd_num);
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp_wifi8->sam_cmd_num);

	/* In v2 hardware, the HAL_SAM_CMD_STATUS_REQUIRED_TO_SW bit
	 * must be set in the command header to receive status.
	 */
	desc->cmd_hdr.sam_cmd_number = cpu_to_le16(cmd_num);
	desc->cmd_hdr.info0 =
			le32_encode_bits((ab->hw_rev == ATH12K_HW_QCN9625_HW20 ? 1 : 0),
					 HAL_SAM_CMD_HDR_INFO0_STATUS_REQUIRED_TO_SW);

	/* If `clear_all` is true, update mpduq id range from 0 to max supported mpduq.*/
	if (clear_all)
		desc->info0 =
		le32_encode_bits(0, HAL_SAM_MPDU_INFO0_START_MPDU_QUEUE_SAM_ID) |
		le32_encode_bits(MAX_NUM_SAM_MPDU_QUEUES_SUPPORTED - 1,
				 HAL_SAM_MPDU_INFO0_END_MPDU_QUEUE_SAM_ID) |
		le32_encode_bits((ab->hw_rev == ATH12K_HW_QCN9625_HW20 ? 0x1F : 0),
				 HAL_SAM_MPDU_INFO0_LINK_MASK);
	else
		desc->info0 =
		le32_encode_bits(id, HAL_SAM_MPDU_INFO0_START_MPDU_QUEUE_SAM_ID) |
		le32_encode_bits(id, HAL_SAM_MPDU_INFO0_END_MPDU_QUEUE_SAM_ID) |
		le32_encode_bits((ab->hw_rev == ATH12K_HW_QCN9625_HW20 ? 0x1F : 0),
				 HAL_SAM_MPDU_INFO0_LINK_MASK);

	return le16_to_cpu(desc->cmd_hdr.sam_cmd_number);
}

int ath12k_wifi8_hal_tx_sam_msduq_clear_cmd(struct ath12k_dp_wifi8 *dp_wifi8,
					    struct hal_tlv_64_hdr *tlv,
					    int id, bool clear_all)
{
	struct ath12k_base *ab = dp_wifi8->dp->ab;
	struct hal_sam_msdu_queue_clear_programming *desc;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_SAM_MSDU_QUEUE_CLEAR_PROGRAMMING_BO,
				   HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_64_HDR_LENGTH);

	desc = (struct hal_sam_msdu_queue_clear_programming *)tlv->value;

	cmd_num = atomic_inc_return(&dp_wifi8->sam_cmd_num);
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp_wifi8->sam_cmd_num);

	/* In v2 hardware, the HAL_SAM_CMD_STATUS_REQUIRED_TO_SW bit
	 * must be set in the command header to receive status.
	 */
	desc->cmd_hdr.sam_cmd_number = cpu_to_le16(cmd_num);
	desc->cmd_hdr.info0 =
			le32_encode_bits((ab->hw_rev == ATH12K_HW_QCN9625_HW20 ? 1 : 0),
					 HAL_SAM_CMD_HDR_INFO0_STATUS_REQUIRED_TO_SW);

	/* If `clear_all` is true, update msduq id range from 0 to max supported msduq.*/
	if (clear_all)
		desc->info0 =
		le32_encode_bits(0, HAL_SAM_MSDU_INFO0_START_MSDU_QUEUE_SAM_ID) |
		le32_encode_bits(MAX_NUM_SAM_MSDU_QUEUES_SUPPORTED - 1,
				 HAL_SAM_MSDU_INFO0_END_MSDU_QUEUE_SAM_ID) |
		le32_encode_bits((ab->hw_rev == ATH12K_HW_QCN9625_HW20 ? 0x1F : 0),
				 HAL_SAM_MSDU_INFO0_LINK_MASK);
	else
		desc->info0 =
		le32_encode_bits(id, HAL_SAM_MSDU_INFO0_START_MSDU_QUEUE_SAM_ID) |
		le32_encode_bits(id, HAL_SAM_MSDU_INFO0_END_MSDU_QUEUE_SAM_ID) |
		le32_encode_bits((ab->hw_rev == ATH12K_HW_QCN9625_HW20 ? 0x1F : 0),
				 HAL_SAM_MSDU_INFO0_LINK_MASK);

	return le16_to_cpu(desc->cmd_hdr.sam_cmd_number);
}

int ath12k_wifi8_hal_tx_sam_peer_clear_cmd(struct ath12k_dp_wifi8 *dp_wifi8,
					   struct hal_tlv_64_hdr *tlv, int src_link_id,
					   int id, bool clear_all)
{
	struct ath12k_base *ab = dp_wifi8->dp->ab;
	struct hal_sam_peer_clear_programming *desc;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_SAM_PEER_CLEAR_PROGRAMMING_BO,
				   HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_64_HDR_LENGTH) |
		  le64_encode_bits((ab->hw_rev == ATH12K_HW_QCN9625_HW10
				    ? src_link_id : 0), HAL_TLV_64_HDR_SRC_LINK_ID);

	desc = (struct hal_sam_peer_clear_programming *)tlv->value;

	cmd_num = atomic_inc_return(&dp_wifi8->sam_cmd_num);
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp_wifi8->sam_cmd_num);

	/* TODO: In v2 hardware, the HAL_SAM_CMD_STATUS_REQUIRED_TO_SW bit
	 * must be set in the command header to receive status.
	 */
	desc->cmd_hdr.sam_cmd_number = cpu_to_le16(cmd_num);
	desc->cmd_hdr.info0 =
			le32_encode_bits((ab->hw_rev == ATH12K_HW_QCN9625_HW20 ? 1 : 0),
					 HAL_SAM_CMD_HDR_INFO0_STATUS_REQUIRED_TO_SW);

	/* If `clear_all` is true, update peer id range from 0 to max supported peer.*/
	if (clear_all)
		desc->info0 = le32_encode_bits(0, HAL_SAM_PEER_INFO0_START_PEER_ID) |
			      le32_encode_bits(ATH12K_MAX_STA_ID - 1,
					       HAL_SAM_PEER_INFO0_END_PEER_ID) |
			      le32_encode_bits((ab->hw_rev == ATH12K_HW_QCN9625_HW20
						? 0x1F : 0),
						HAL_SAM_PEER_INFO0_LINK_MASK);
	else
		desc->info0 = le32_encode_bits(id, HAL_SAM_PEER_INFO0_START_PEER_ID) |
			      le32_encode_bits(id, HAL_SAM_PEER_INFO0_END_PEER_ID) |
			      le32_encode_bits((ab->hw_rev == ATH12K_HW_QCN9625_HW20
						? 0x1F : 0),
						HAL_SAM_PEER_INFO0_LINK_MASK);

	return le16_to_cpu(desc->cmd_hdr.sam_cmd_number);
}

/**
 * ath12k_wifi8_hal_tx_sam_cmd_send() - Send a TX SAM command to HAL
 * @ab: ath12k base device context
 * @srng: HAL source ring
 * @src_link_id: Source link ID associated with the command
 * @type: HAL TLV tag indicating the SAM command type
 * @id: Identifier associated with the SAM command (e.g., peer_id, msduq_sam_id
 *      or mpduq_sam_id) based on type.
 * @clear_all: Flag indicating whether all SAM data structures should be cleared
 *             else only one data structure is cleared based on `type` and `id`.
 *
 */
int ath12k_wifi8_hal_tx_sam_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				     int src_link_id, enum hal_tlv_tag_be type, int id,
				     bool clear_all, bool force)
{
	struct hal_tlv_64_hdr *sam_desc, *tlv_desc;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);
	u32 cmd_size;
	int ret;

	if (!dp_wifi8->sam_cmd_staging) {
		ath12k_err(ab, "SAM staging buffer not allocated\n");
		return -ENOMEM;
	}

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags) && !force)
		return 0;

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	tlv_desc = (struct hal_tlv_64_hdr *)dp_wifi8->sam_cmd_staging;
	memset(tlv_desc, 0, sizeof(*tlv_desc));

	switch (type) {
	case HAL_SAM_MPDU_QUEUE_CLEAR_PROGRAMMING_BO:
		ret = ath12k_wifi8_hal_tx_sam_mpduq_clear_cmd(dp_wifi8, tlv_desc, id,
							      clear_all);
		break;
	case HAL_SAM_MSDU_QUEUE_CLEAR_PROGRAMMING_BO:
		ret = ath12k_wifi8_hal_tx_sam_msduq_clear_cmd(dp_wifi8, tlv_desc, id,
							      clear_all);
		break;
	case HAL_SAM_PEER_CLEAR_PROGRAMMING_BO:
		ret = ath12k_wifi8_hal_tx_sam_peer_clear_cmd(dp_wifi8, tlv_desc,
							     src_link_id, id, clear_all);
		break;
	default:
		ath12k_warn(ab, "Unknown sam command %d\n", type);
		ret = -EINVAL;
		goto out;
	}

	sam_desc = (struct hal_tlv_64_hdr *)
			ath12k_hal_srng_src_get_next_entry_by_cmd_size(ab, srng, type);
	if (!sam_desc) {
		ret = -ENOBUFS;
		goto out;
	}

	cmd_size = ath12k_hal_srng_get_cmd_size(ab, type);
	if (cmd_size > HAL_SAM_CMD_MAX_WORDS) {
		ret = -EINVAL;
		goto out;
	}
	ret = ath12k_wifi8_hal_srng_write_words(ab, srng, cmd_size,
						(void *)sam_desc,
						dp_wifi8->sam_cmd_staging);
	if (ret)
		ath12k_warn(ab, "Failed to write SAM cmd to ring: %d\n", ret);
out:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);
	return ret;
}

void ath12k_wifi8_hal_tx_sam_program_clear(struct ath12k_base *ab)
{
	struct hal_srng *srng;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);
	int i, ret;

	srng = &ab->hal.srng_list[dp_wifi8->sam_cmd_ring.ring_id];
	ret = ath12k_wifi8_hal_tx_sam_cmd_send(ab, srng, -1,
					       HAL_SAM_MPDU_QUEUE_CLEAR_PROGRAMMING_BO,
					       -1, true, true);

	if (ret < 0)
		ath12k_warn(ab, "failed to send SAM mpdu clear command: %d\n", ret);

	ret = ath12k_wifi8_hal_tx_sam_cmd_send(ab, srng, -1,
					       HAL_SAM_MSDU_QUEUE_CLEAR_PROGRAMMING_BO,
					       -1, true, true);

	if (ret < 0)
		ath12k_warn(ab, "failed to send SAM msdu clear command: %d\n", ret);

	/* In v1 hardware, send peer clear command over all links.
	 * In v2 hardware, a new link_mask field allows a single SAM command
	 * to be sent for all links, with bits set for each link.
	 */
	if (ab->hw_rev == ATH12K_HW_QCN9625_HW10) {
		for (i = 0; i < HAL_TX_NUM_MAX_LINKS; i++) {
			ret =
			ath12k_wifi8_hal_tx_sam_cmd_send
						(ab, srng, i,
						 HAL_SAM_PEER_CLEAR_PROGRAMMING_BO,
						 -1, true, true);

			if (ret < 0)
				ath12k_dbg(ab, ATH12K_DBG_HAL,
					   "failed to send SAM peer clear command for link %d: %d\n",
					   i, ret);
		}
	} else {
		ret = ath12k_wifi8_hal_tx_sam_cmd_send(ab, srng, -1,
						       HAL_SAM_PEER_CLEAR_PROGRAMMING_BO,
						       -1, true, true);
		if (ret < 0)
			ath12k_dbg(ab, ATH12K_DBG_HAL,
				   "failed to send SAM peer clear command: %d\n", ret);
	}
}

void ath12k_wifi8_hal_tx_sam_status(struct ath12k_base *ab, struct hal_tlv_64_hdr *tlv)
{
	struct hal_sam_cmd_status *desc = (struct hal_sam_cmd_status *)tlv->value;
	int cmd_num = le16_to_cpu(desc->status_hdr.sam_cmd_number);
	ath12k_dbg(ab, ATH12K_DBG_HAL, "cmd_num %d\n", cmd_num);
}

int ath12k_wifi8_hal_sam_cmd_staging_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);

	/* Allocate single staging buffer for the SAM command ring. */
	dp_wifi8->sam_cmd_staging = kzalloc(HAL_SAM_CMD_MAX_BYTES, GFP_ATOMIC);

	if (!dp_wifi8->sam_cmd_staging)
		return -ENOMEM;

	return 0;
}

void ath12k_wifi8_hal_sam_cmd_staging_free(struct ath12k_base *ab)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);

	kfree(dp_wifi8->sam_cmd_staging);
	dp_wifi8->sam_cmd_staging = NULL;
}

void ath12k_wifi8_hal_enable_service_category_sorting(struct ath12k_hal *hal)
{
	struct ath12k_base *ab = container_of(hal, struct ath12k_base, hal);
	u32 tqm_base = HAL_SEQ_WCSS_UMAC_TQM_REG;
	u32 val;

	val = ath12k_hif_read32(ab, tqm_base + HAL_TQM_MISC_CFG_1);
	val |= HAL_TQM_MISC_CFG_1_SORT_BIT_MASK;
	ath12k_hif_write32(ab, tqm_base + HAL_TQM_MISC_CFG_1, val);
}

void ath12k_wifi8_hal_tqm_sorting_latch(struct ath12k_hal *hal)
{
	struct ath12k_base *ab = container_of(hal, struct ath12k_base, hal);

	ath12k_hif_write32(ab, HAL_TQM_R0_SORTING_REG, HAL_TQM_R0_SORTING_LATCH);
}

u32 ath12k_wifi8_hal_tqm_get_active_msdu(struct ath12k_hal *hal,
					 enum hal_tqm_service_category svc)
{
	struct ath12k_base *ab = container_of(hal, struct ath12k_base, hal);
	u32 msdu_count;

	switch (svc) {
	case HAL_TQM_SERVICE_CATEGORY_SC0:
		msdu_count = ath12k_hif_read32(ab, HAL_TQM_R0_SC0_ACTIVE_MSDU_CNT);
		break;
	case HAL_TQM_SERVICE_CATEGORY_SC1:
		msdu_count = ath12k_hif_read32(ab, HAL_TQM_R0_SC1_ACTIVE_MSDU_CNT);
		break;
	case HAL_TQM_SERVICE_CATEGORY_SC2:
		msdu_count = ath12k_hif_read32(ab, HAL_TQM_R0_SC2_ACTIVE_MSDU_CNT);
		break;
	case HAL_TQM_SERVICE_CATEGORY_SC3:
		msdu_count = ath12k_hif_read32(ab, HAL_TQM_R0_SC3_ACTIVE_MSDU_CNT);
		break;
	case HAL_TQM_SERVICE_CATEGORY_MAX:
		msdu_count = ath12k_hif_read32(ab, HAL_TQM_R0_ACTIVE_MSDU_CNT);
		break;
	default:
		ath12k_warn(ab, "active msdu cnt failed due to invalid svc %d\n", svc);
		msdu_count = 0;
		break;
	}

	return msdu_count;
}

int ath12k_wifi8_hal_tqm_get_svc_sorted_list(struct ath12k_hal *hal,
					     enum hal_tqm_service_category svc,
					     u8 idx,
					     u32 *flow_number,
					     u32 *msdu_count)
{
	struct ath12k_base *ab = container_of(hal, struct ath12k_base, hal);
	u32 reg_val, low, high, svc_offset, flow_offset;
	int ret;

	if (svc >= HAL_TQM_SERVICE_CATEGORY_MAX || idx >= HAL_TQM_MAX_SORTED_FLOW) {
		ath12k_warn(hal, "get sorted list failed due to invalid svc %d or idx %d\n",
			    svc, idx);
		ret = -EINVAL;
		goto err;
	}

	svc_offset = HAL_TQM_R0_SC1_SORTED_FLOW_0_LOW - HAL_TQM_R0_SC0_SORTED_FLOW_0_LOW;
	svc_offset = svc * svc_offset;

	flow_offset = HAL_TQM_R0_SC0_SORTED_FLOW_1_LOW - HAL_TQM_R0_SC0_SORTED_FLOW_0_LOW;
	flow_offset = idx * flow_offset;

	low = HAL_TQM_R0_SC0_SORTED_FLOW_0_LOW + svc_offset + flow_offset;
	high = HAL_TQM_R0_SC0_SORTED_FLOW_0_HIGH + svc_offset + flow_offset;

	reg_val = ath12k_hif_read32(ab, low);
	if (!u32_get_bits(reg_val, HAL_TQM_R0_SC_SORTED_FLOW_LOW_VALID)) {
		ret = -ENOENT;
		goto err;
	}

	*flow_number = u32_get_bits(reg_val, HAL_TQM_R0_SC_SORTED_FLOW_LOW_FLOW_NUM);
	reg_val = ath12k_hif_read32(ab, high);
	*msdu_count = u32_get_bits(reg_val,
				   HAL_TQM_R0_SC_SORTED_FLOW_HIGH_MSDU_CNT);

	return 0;

err:
	*flow_number = -1;
	*msdu_count = -1;

	return ret;
}

/**
 * ath12k_wifi8_hal_tx_set_pcp_tid_map() - Write PCP-TID map to a single
 *   wifi8 SOC TCL block.
 * @ab:  Base structure identifying the single SOC to program.
 * @map: 8-entry PCP-to-TID array; index = PCP (0-7), value = TID (0-7).
 *
 * Dumb register writer — no validation, no SOC iteration.
 * SOC iteration is handled by the caller (ath12k_dp_program_pcp_tid_map).
 */
void ath12k_wifi8_hal_tx_set_pcp_tid_map(struct ath12k_base *ab, const u8 *map)
{
	u32 reg_val;

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
EXPORT_SYMBOL(ath12k_wifi8_hal_tx_set_pcp_tid_map);

/**
 * ath12k_wifi8_hal_tx_set_tid_map_precedence() - Write TID precedence to a
 *   single wifi8 SOC TCL block.
 * @ab:         Base structure identifying the single SOC to program.
 * @precedence: Unified TID map precedence value (0 = DSCP, 1 = PCP).
 *
 * On wifi8 the unified userspace VAL maps directly to the hardware register
 * value (no normalization required):
 *   0 (DSCP) -> hw VAL 0: HLOS > DSCP > PCP
 *   1 (PCP)  -> hw VAL 1: HLOS > PCP  > DSCP
 *
 * Any value other than 0 or 1 is rejected with ath12k_warn(); the register
 * is not written.
 *
 * wifi8 register field is 1-bit (HAL_TCL_TID_MAP_PRTY_VAL_MASK = GENMASK(0,0)).
 * SOC iteration is handled by the caller (ath12k_dp_program_tid_map_precedence).
 */
void ath12k_wifi8_hal_tx_set_tid_map_precedence(struct ath12k_base *ab,
						const u8 precedence)
{
	u32 reg_val;

	/*
	 * Dumb register writer — no validation.
	 * Validation (0 or 1 only) is performed by the caller (dp.c).
	 *
	 * wifi8 register field is 1-bit: HAL_TCL_TID_MAP_PRTY_VAL_MASK = GENMASK(0,0).
	 * The unified VAL maps directly to the hardware value:
	 *   0 (DSCP) -> hw VAL 0 (HLOS > DSCP > PCP)
	 *   1 (PCP)  -> hw VAL 1 (HLOS > PCP  > DSCP)
	 */
	reg_val = u32_encode_bits(precedence, HAL_TCL_TID_MAP_PRTY_VAL_MASK);
	ath12k_hif_write32(ab, HAL_TCL_R0_PCP_TID_MAP_PRTY_OFFSET, reg_val);
}
EXPORT_SYMBOL(ath12k_wifi8_hal_tx_set_tid_map_precedence);

void ath12k_wifi8_hal_tqm_update_msduq_cmd_status(struct ath12k_base *ab,
						  struct hal_tlv_64_hdr *tlv,
						  struct hal_tqm_status *status)
{
	struct hal_tqm_update_tx_msdu_flow_status *desc =
		(struct hal_tqm_update_tx_msdu_flow_status *)tlv->value;
	u8 update_req_not_met;
	u32 tx_flow_number;
	u16 sw_peer_id;
	u8 tid;

	status->status_hdr.status_num = le32_to_cpu(desc->status_hdr.tqm_status_number);
	status->status_hdr.cmd_execution_status =
		le32_get_bits(desc->status_hdr.info0,
			      HAL_TQM_STATUS_INFO0_CMD_EXECUTION_STATUS);
	status->status_hdr.tqm_status_ring = le32_get_bits(desc->status_hdr.info0,
							   HAL_TQM_STATUS_INFO0_RING);

	update_req_not_met =
		le32_get_bits(desc->info0,
			      HAL_TQM_FLOW_UPDSTAT_INFO0_REQUIREMENTS_NOT_MET);
	tx_flow_number = le32_get_bits(desc->info0,
				       HAL_TQM_FLOW_UPDSTAT_INFO0_TX_FLOW_NUMBER);
	sw_peer_id = le16_to_cpu(desc->sw_peer_id);
	tid = le32_get_bits(desc->info1, HAL_TQM_FLOW_UPDSTAT_INFO1_TID);

	status->update_msdu_flow.update_requirements_not_met = update_req_not_met;
	status->update_msdu_flow.tx_flow_number = tx_flow_number;
	status->update_msdu_flow.sw_peer_id = sw_peer_id;
	status->update_msdu_flow.tid = tid;

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
			 "tqm-msdu-status: s=%u e=%u r=%u nm=%u flow=0x%x peer=%u tid=%u\n",
			 status->status_hdr.status_num,
			 status->status_hdr.cmd_execution_status,
			 status->status_hdr.tqm_status_ring, update_req_not_met,
			 tx_flow_number, sw_peer_id, tid);

	if (status->status_hdr.cmd_execution_status == HAL_TQM_SUCCESSFUL_EXECUTION &&
	    !update_req_not_met)
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "tqm-msdu-status: UPDATE OK peer=%u flow=0x%x tid=%u\n",
				 sw_peer_id, tx_flow_number, tid);
	else
		ath12k_warn(ab,
			    "tqm-msdu-status: FAILED peer=%u flow=0x%x tid=%u exec=%u nm=%u\n",
			    sw_peer_id, tx_flow_number, tid,
			    status->status_hdr.cmd_execution_status, update_req_not_met);
}
