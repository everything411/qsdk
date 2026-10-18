// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../debug.h"
#include "../hif.h"
#include "hal_rx.h"
#include "dp.h"

static
void ath12k_wifi8_hal_reo_set_desc_hdr(struct hal_desc_header *hdr,
				       u8 owner, u8 buffer_type, u32 magic)
{
	hdr->info0 = le32_encode_bits(owner, HAL_DESC_HDR_INFO0_OWNER) |
		     le32_encode_bits(buffer_type, HAL_DESC_HDR_INFO0_BUF_TYPE);

	/* Magic pattern in reserved bits for debugging */
	hdr->info0 |= le32_encode_bits(magic, HAL_DESC_HDR_INFO0_DBG_RESERVED);
}

static int ath12k_wifi8_hal_reo_cmd_queue_stats(struct hal_tlv_64_hdr *tlv,
						struct ath12k_hal_reo_cmd *cmd)
{
	struct hal_reo_get_queue_stats *desc;

	tlv->tl = le64_encode_bits(HAL_REO_GET_QUEUE_STATS, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_reo_get_queue_stats *)tlv->value;
	memset_startat(desc, 0, queue_addr_lo);

	desc->cmd.info0 &= ~cpu_to_le16(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);
	if (cmd->flag & HAL_REO_CMD_FLG_NEED_STATUS)
		desc->cmd.info0 |= cpu_to_le16(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	desc->queue_addr_lo = cpu_to_le32(cmd->addr_lo);
	desc->info0 = le16_encode_bits(cmd->addr_hi,
				       HAL_REO_GET_QUEUE_STATS_INFO0_QUEUE_ADDR_HI);
	if (cmd->flag & HAL_REO_CMD_FLG_STATS_CLEAR)
		desc->info0 |= cpu_to_le16(HAL_REO_GET_QUEUE_STATS_INFO0_CLEAR_STATS);

	return le16_to_cpu(desc->cmd.reo_cmd_number);
}

static int ath12k_wifi8_hal_reo_cmd_flush_cache(struct ath12k_hal *hal,
						struct hal_tlv_64_hdr *tlv,
						struct ath12k_hal_reo_cmd *cmd)
{
	struct hal_reo_flush_cache *desc;
	u8 avail_slot = ffz(hal->avail_blk_resource);

	if (cmd->flag & HAL_REO_CMD_FLG_FLUSH_BLOCK_LATER) {
		if (avail_slot >= HAL_MAX_AVAIL_BLK_RES)
			return -ENOSPC;

		hal->current_blk_index = avail_slot;
	}

	tlv->tl = le64_encode_bits(HAL_REO_FLUSH_CACHE, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_reo_flush_cache *)tlv->value;
	memset_startat(desc, 0, cache_addr_lo);

	desc->cmd.info0 &= ~cpu_to_le16(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);
	if (cmd->flag & HAL_REO_CMD_FLG_NEED_STATUS)
		desc->cmd.info0 |= cpu_to_le16(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	desc->cache_addr_lo = cpu_to_le32(cmd->addr_lo);
	desc->info0 = le16_encode_bits(cmd->addr_hi,
				       HAL_REO_FLUSH_CACHE_INFO0_CACHE_ADDR_HI);

	if (cmd->flag & HAL_REO_CMD_FLG_FLUSH_FWD_ALL_MPDUS)
		desc->info0 |= cpu_to_le16(HAL_REO_FLUSH_CACHE_INFO0_FWD_ALL_MPDUS);

	if (cmd->flag & HAL_REO_CMD_FLG_FLUSH_BLOCK_LATER) {
		desc->info0 |= cpu_to_le16(HAL_REO_FLUSH_CACHE_INFO0_BLOCK_CACHE_USAGE);
		desc->info0 |=
			le16_encode_bits(avail_slot,
					 HAL_REO_FLUSH_CACHE_INFO0_BLOCK_RESRC_IDX);
	}

	if (cmd->flag & HAL_REO_CMD_FLG_FLUSH_NO_INVAL)
		desc->info0 |= cpu_to_le16(HAL_REO_FLUSH_CACHE_INFO0_FLUSH_WO_INVALIDATE);

	if (cmd->flag & HAL_REO_CMD_FLG_FLUSH_ALL)
		desc->info0 |= cpu_to_le16(HAL_REO_FLUSH_CACHE_INFO0_FLUSH_ALL);

	if (cmd->flag & HAL_REO_CMD_FLG_FLUSH_QUEUE_1K_DESC)
		desc->info0 |= cpu_to_le16(HAL_REO_FLUSH_CACHE_INFO0_FLUSH_QUEUE_1K_DESC);

	return le16_to_cpu(desc->cmd.reo_cmd_number);
}

static int
ath12k_wifi8_hal_reo_cmd_update_rx_queue(struct hal_tlv_64_hdr *tlv,
					 struct ath12k_hal_reo_cmd *cmd)
{
	struct hal_reo_update_rx_queue *desc;

	tlv->tl = le64_encode_bits(HAL_REO_UPDATE_RX_REO_QUEUE, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_reo_update_rx_queue *)tlv->value;
	memset_startat(desc, 0, queue_addr_lo);

	desc->cmd.info0 &= ~cpu_to_le16(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);
	if (cmd->flag & HAL_REO_CMD_FLG_NEED_STATUS)
		desc->cmd.info0 |= cpu_to_le16(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	desc->queue_addr_lo = cpu_to_le32(cmd->addr_lo);
	desc->info0 =
		le32_encode_bits(cmd->addr_hi,
				 HAL_REO_UPD_RX_QUEUE_INFO0_QUEUE_ADDR_HI) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_RX_QUEUE_NUM),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_RX_QUEUE_NUM) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_VLD),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_VLD) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_ALDC),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_ASSOC_LNK_DESC_CNT) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_DIS_DUP_DETECTION),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_DIS_DUP_DETECTION) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_SOFT_REORDER_EN),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SOFT_REORDER_EN) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_AC),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_AC) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_BAR),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_BAR) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_RETRY),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_RETRY) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_CHECK_2K_MODE),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_CHECK_2K_MODE) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_OOR_MODE),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_OOR_MODE) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_BA_WINDOW_SIZE),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_BA_WINDOW_SIZE) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_PN_CHECK),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_CHECK) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_EVEN_PN),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_EVEN_PN) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_UNEVEN_PN),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_UNEVEN_PN) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_PN_HANDLE_ENABLE),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_HANDLE_ENABLE) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_PN_SIZE),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_SIZE) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_IGNORE_AMPDU_FLG),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_IGNORE_AMPDU_FLG) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_SVLD),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SVLD) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_SSN),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SSN) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_SEQ_2K_ERR),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SEQ_2K_ERR) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_PN_ERR),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_ERR) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_PN_VALID),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_VALID) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_PN),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN);

	if (cmd->flag & HAL_REO_CMD_FLG_STATS_CLEAR)
		desc->info0 |= cpu_to_le32(HAL_REO_UPD_RX_QUEUE_INFO0_CLR_STATS_COUNTERS);

	desc->receive_queue_number = cpu_to_le16(cmd->rx_queue_num);
	desc->info1 =
		le16_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_VLD),
				 HAL_REO_UPD_RX_QUEUE_INFO1_VLD) |
		le16_encode_bits(u32_get_bits(cmd->upd1, HAL_REO_CMD_UPD1_ALDC),
				 HAL_REO_UPD_RX_QUEUE_INFO1_ASSOC_LNK_DESC_COUNTER) |
		le16_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_DIS_DUP_DETECTION),
				 HAL_REO_UPD_RX_QUEUE_INFO1_DIS_DUP_DETECTION) |
		le16_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_SOFT_REORDER_EN),
				 HAL_REO_UPD_RX_QUEUE_INFO1_SOFT_REORDER_EN) |
		le16_encode_bits(u32_get_bits(cmd->upd1, HAL_REO_CMD_UPD1_LL_TIMER_ID),
				 HAL_REO_UPD_RX_QUEUE_INFO1_LINK_LIST_TIMER_ID) |
		le16_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_BAR),
				 HAL_REO_UPD_RX_QUEUE_INFO1_BAR) |
		le16_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_CHECK_2K_MODE),
				 HAL_REO_UPD_RX_QUEUE_INFO1_CHECK_2K_MODE) |
		le16_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_RETRY),
				 HAL_REO_UPD_RX_QUEUE_INFO1_RETRY) |
		le16_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_OOR_MODE),
				 HAL_REO_UPD_RX_QUEUE_INFO1_OOR_MODE) |
		le16_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_PN_CHECK),
				 HAL_REO_UPD_RX_QUEUE_INFO1_PN_CHECK) |
		le16_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_EVEN_PN),
				 HAL_REO_UPD_RX_QUEUE_INFO1_EVEN_PN) |
		le16_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_UNEVEN_PN),
				 HAL_REO_UPD_RX_QUEUE_INFO1_UNEVEN_PN);

	if (cmd->pn_size == 24)
		cmd->pn_size = HAL_RX_REO_QUEUE_PN_SIZE_24;
	else if (cmd->pn_size == 48)
		cmd->pn_size = HAL_RX_REO_QUEUE_PN_SIZE_48;
	else if (cmd->pn_size == 128)
		cmd->pn_size = HAL_RX_REO_QUEUE_PN_SIZE_128;

	if (cmd->ba_window_size < 1)
		cmd->ba_window_size = 1;

	if (cmd->ba_window_size == 1)
		cmd->ba_window_size++;

	desc->info2 =
		le32_encode_bits(!!(cmd->upd2 & HAL_REO_CMD_UPD2_PN_HANDLE_ENABLE),
				 HAL_REO_UPD_RX_QUEUE_INFO2_PN_HANDLE_ENABLE) |
		le32_encode_bits(!!(cmd->upd2 & HAL_REO_CMD_UPD2_IGNORE_AMPDU_FLG),
				 HAL_REO_UPD_RX_QUEUE_INFO2_IGNORE_AMPDU_FLG) |
		le32_encode_bits(cmd->ba_window_size - 1,
				 HAL_REO_UPD_RX_QUEUE_INFO2_BA_WINDOW_SIZE) |
		le32_encode_bits(cmd->pn_size, HAL_REO_UPD_RX_QUEUE_INFO2_PN_SIZE) |
		le32_encode_bits(!!(cmd->upd2 & HAL_REO_CMD_UPD2_SVLD),
				 HAL_REO_UPD_RX_QUEUE_INFO2_SVLD) |
		le32_encode_bits(u32_get_bits(cmd->upd2, HAL_REO_CMD_UPD2_SSN),
				 HAL_REO_UPD_RX_QUEUE_INFO2_SSN) |
		le32_encode_bits(!!(cmd->upd2 & HAL_REO_CMD_UPD2_SEQ_2K_ERR),
				 HAL_REO_UPD_RX_QUEUE_INFO2_SEQ_2K_ERR) |
		le32_encode_bits(!!(cmd->upd2 & HAL_REO_CMD_UPD2_PN_ERR),
				 HAL_REO_UPD_RX_QUEUE_INFO2_PN_ERR) |
		le32_encode_bits(!!(cmd->upd2 & HAL_REO_CMD_UPD2_PN_VALID),
				 HAL_REO_UPD_RX_QUEUE_INFO2_PN_VALID);

	/* Enable write back to DDR for every Update queue command */
	desc->info2 |= cpu_to_le32(HAL_REO_UPD_RX_QUEUE_INFO2_FLUSH_FROM_CACHE);

	if (cmd->upd0 & HAL_REO_CMD_UPD0_PN) {
		desc->pn_31_0 = cpu_to_le32(cmd->pn[0]);
		desc->pn_47_32 = cpu_to_le16(cmd->pn[1]);
		desc->info3 = le16_encode_bits(cmd->pn_127_48_info,
					       HAL_REO_UPD_RX_QUEUE_INFO3_PN_127_48_INFO);
	}

	return le16_to_cpu(desc->cmd.reo_cmd_number);
}

static int ath12k_hal_reo_cmd_flush_queue(struct hal_tlv_64_hdr *tlv,
					  struct ath12k_hal_reo_cmd *cmd)
{
	struct hal_reo_flush_queue *desc = (struct hal_reo_flush_queue *)tlv->value;

	tlv->tl = u32_encode_bits(HAL_REO_FLUSH_QUEUE, HAL_TLV_HDR_TAG) |
		  u32_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	memset_startat(desc, 0, desc_addr_lo);

	desc->cmd.info0 &= ~cpu_to_le16(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	if (cmd->flag & HAL_REO_CMD_FLG_NEED_STATUS)
		desc->cmd.info0 |= cpu_to_le16(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	desc->desc_addr_lo = cpu_to_le32(cmd->addr_lo);
	desc->info0 = le16_encode_bits(cmd->addr_hi,
				       HAL_REO_FLUSH_QUEUE_INFO0_DESC_ADDR_HI);

	return le16_to_cpu(desc->cmd.reo_cmd_number);
}

static int ath12k_hal_reo_cmd_unblock_cache(struct hal_tlv_64_hdr *tlv,
					    struct ath12k_hal_reo_cmd *cmd)
{
	struct hal_reo_unblock_cache *desc =
		(struct hal_reo_unblock_cache *)tlv->value;

	tlv->tl = u32_encode_bits(HAL_REO_UNBLOCK_CACHE, HAL_TLV_HDR_TAG) |
		u32_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	memset_startat(desc, 0, info0);

	desc->cmd.info0 &= ~cpu_to_le16(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	if (cmd->flag & HAL_REO_CMD_FLG_NEED_STATUS)
		desc->cmd.info0 |= cpu_to_le16(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	desc->info0 = HAL_REO_UNBLOCK_CACHE_INFO0_UNBLK_CACHE;

	return le16_to_cpu(desc->cmd.reo_cmd_number);
}

static int ath12k_wifi8_hal_reo_cmd_fill(struct ath12k_base *ab,
					 struct hal_tlv_64_hdr *reo_desc,
					 enum hal_reo_cmd_type type,
					 struct ath12k_hal_reo_cmd *cmd)
{
	switch (type) {
	case HAL_REO_CMD_GET_QUEUE_STATS:
		return ath12k_wifi8_hal_reo_cmd_queue_stats(reo_desc, cmd);
	case HAL_REO_CMD_FLUSH_CACHE:
		return ath12k_wifi8_hal_reo_cmd_flush_cache(&ab->hal, reo_desc,
							  cmd);
	case HAL_REO_CMD_UPDATE_RX_QUEUE:
		return ath12k_wifi8_hal_reo_cmd_update_rx_queue(reo_desc, cmd);
	case HAL_REO_CMD_FLUSH_QUEUE:
		return ath12k_hal_reo_cmd_flush_queue(reo_desc, cmd);
	case HAL_REO_CMD_UNBLOCK_CACHE:
		return ath12k_hal_reo_cmd_unblock_cache(reo_desc, cmd);
	case HAL_REO_CMD_FLUSH_TIMEOUT_LIST:
		ath12k_warn(ab, "Unsupported reo command %d\n", type);
		return -EOPNOTSUPP;
	default:
		ath12k_warn(ab, "Unknown reo command %d\n", type);
		return -EINVAL;
	}
}

int ath12k_wifi8_hal_reo_cmd_send_n(struct ath12k_base *ab,
				    struct hal_srng *srng,
				    struct ath12k_reo_cmd_entry *entries,
				    int n)
{
	int ret;

	if (WARN_ON(n <= 0))
		return -EINVAL;

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	ret = ath12k_wifi8_hal_reo_cmd_send_n_locked(ab, srng, entries, n);

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return ret;
}

int ath12k_wifi8_hal_reo_cmd_send_n_locked(struct ath12k_base *ab,
					   struct hal_srng *srng,
					   struct ath12k_reo_cmd_entry *entries,
					   int n)
{
	struct hal_tlv_64_hdr *reo_desc;
	u32 saved_hp;
	int i, ret;

	if (WARN_ON(n <= 0))
		return -EINVAL;

	lockdep_assert_held(&srng->lock);

	saved_hp = srng->u.src_ring.hp;

	for (i = 0; i < n; i++) {
		reo_desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
		if (!reo_desc) {
			ret = -ENOBUFS;
			goto err_restore_hp;
		}

		ret = ath12k_wifi8_hal_reo_cmd_fill(ab, reo_desc,
						    entries[i].type,
						    &entries[i].cmd);
		if (ret <= 0) {
			if (!ret)
				ret = -EINVAL;
			goto err_restore_hp;
		}

		entries[i].cmd_num = ret;
	}

	return 0;

err_restore_hp:
	srng->u.src_ring.hp = saved_hp;
	srng->u.src_ring.reap_hp = saved_hp;

	return ret;
}

int ath12k_wifi8_hal_reo_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				  enum hal_reo_cmd_type type,
				  struct ath12k_hal_reo_cmd *cmd)
{
	struct hal_tlv_64_hdr *reo_desc;
	int ret;

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);
	reo_desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
	if (!reo_desc) {
		ret = -ENOBUFS;
		goto out;
	}

	ret = ath12k_wifi8_hal_reo_cmd_fill(ab, reo_desc, type, cmd);

out:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return ret;
}

void
ath12k_wifi8_hal_rx_msdu_link_info_get(struct hal_rx_msdu_link *link,
				       u32 *num_msdus, u32 *msdu_cookies,
				       enum hal_wifi8_rx_buf_return_buf_manager *rbm)
{
	struct hal_rx_msdu_details *msdu_ptrs[] = {
		&link->msdu_0,
		&link->msdu_1,
		&link->msdu_2,
		&link->msdu_3,
		&link->msdu_4,
		&link->msdu_5,
		&link->msdu_6,
		&link->msdu_7,
		&link->msdu_8,
		&link->msdu_9,
		&link->msdu_10,
	};

	int max_msdus = sizeof(msdu_ptrs) / sizeof(*msdu_ptrs);
	int i;
	u32 val;

	*num_msdus = max_msdus;

	*rbm = le32_get_bits(msdu_ptrs[0]->buf_addr_info.info1,
			     BUFFER_ADDR_INFO1_RET_BUF_MGR);

	for (i = 0; i < max_msdus; i++) {
		val = le32_get_bits(msdu_ptrs[i]->buf_addr_info.info0,
				    BUFFER_ADDR_INFO0_ADDR);
		if (val == 0) {
			*num_msdus = i;
			break;
		}
		*msdu_cookies = le32_get_bits(msdu_ptrs[i]->buf_addr_info.info1,
					      BUFFER_ADDR_INFO1_SW_COOKIE);
		msdu_cookies++;
	}
}

int ath12k_wifi8_hal_reo_rel_parse_err(struct ath12k_dp *dp, void *desc,
				       struct hal_rx_reo_dest_rel_info *rel_info)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_reo_dest_ring *reo_desc = desc;
	struct hal_rx_mpdu_ext_desc_info *mpdu_desc_ext_info =
		(struct hal_rx_mpdu_ext_desc_info *)&reo_desc->rx_mpdu_ext_info;
	enum hal_reo_dest_rel_src_module rel_src;
	bool hw_cc_done;
	u64 desc_va;
	u32 val;
	bool is_frag = false;
	u8 rxdma_push_reason, rxdma_error_code, reo_push_reason, reo_error_code;

	rel_src = le32_get_bits(mpdu_desc_ext_info->info0,
				HAL_RX_MPDU_EXT_DESC_INFO_INFO0_RELEASE_SOURCE_MODULE);

	reo_push_reason =
		le32_get_bits(mpdu_desc_ext_info->info0,
			      HAL_RX_MPDU_EXT_DESC_INFO_INFO0_REO_PUSH_REASON);
	reo_error_code =
		le32_get_bits(mpdu_desc_ext_info->info0,
			      HAL_RX_MPDU_EXT_DESC_INFO_INFO0_REO_ERROR_CODE);
	rxdma_push_reason =
		le32_get_bits(mpdu_desc_ext_info->info0,
			      HAL_RX_MPDU_EXT_DESC_INFO_INFO0_RXDMA_PUSH_REASON);
	rxdma_error_code =
		le32_get_bits(mpdu_desc_ext_info->info0,
			      HAL_RX_MPDU_EXT_DESC_INFO_INFO0_RXDMA_ERROR_CODE);

	if (rel_src != HAL_REO_REL_SRC_MODULE_REO) {
		ath12k_warn(ab, "Invalid src rxmda(%d %d) reo(%d %d)",
			    rxdma_push_reason, rxdma_error_code,
			    reo_push_reason, reo_error_code);
		return -EINVAL;
	}

	rel_info->buffer_type = le32_get_bits(mpdu_desc_ext_info->info0,
			     HAL_RX_MPDU_EXT_DESC_INFO_INFO0_REO_DEST_BUFFER_TYPE);

	if (rel_info->buffer_type == HAL_REO_DEST_RING_BUFFER_TYPE_LINK_DESC) {
		is_frag = !!(le32_to_cpu(reo_desc->rx_mpdu_info.info0) &
			     HAL_RX_MPDU_DESC_INFO_INFO0_FRAGMENT_FLAG);
		if (!is_frag)
			ath12k_warn(ab, "Invalid link desc release for non fragment rxmda(%d %d) reo(%d %d)",
				    rxdma_push_reason,
				    rxdma_error_code,
				    reo_push_reason,
				    reo_error_code);
		WARN_ON(1);
		return 0;
	}

	rel_info->first_msdu =
		le32_get_bits(reo_desc->info0,
			      HAL_RX_MSDU_DESC_INFO_INFO0_FIRST_MSDU_IN_MPDU_FLAG);
	rel_info->last_msdu =
		le32_get_bits(reo_desc->info0,
			      HAL_RX_MSDU_DESC_INFO_INFO0_LAST_MSDU_IN_MPDU_FLAG);
	rel_info->continuation =
		le32_get_bits(reo_desc->info0,
			      HAL_RX_MSDU_DESC_INFO_INFO0_MSDU_CONTINUATION);

	if (rxdma_push_reason != HAL_REO_DEST_RING_PUSH_REASON_ROUTING_INSTRUCTION) {
		rel_info->err_code = rxdma_error_code;
		rel_info->push_reason = rxdma_push_reason;
		rel_src = HAL_REO_REL_SRC_MODULE_RXDMA;
	} else {
		rel_info->err_code = reo_error_code;
		rel_info->push_reason = reo_push_reason;
	}

	/* The format of REO DEST ring desc changes based on the
	 * hw cookie conversion status
	 */
	hw_cc_done =
		le32_get_bits(reo_desc->info0,
			      HAL_REO_DESTINATION_RING_INFO0_COOKIE_CONVERSION_STATUS);

	if (hw_cc_done) {
		val = le32_get_bits(reo_desc->info0,
				    BUFFER_ADDR_INFO1_RET_BUF_MGR);
		rel_info->cookie =
			le32_get_bits(reo_desc->info0,
				      HAL_REO_DESTINATION_RING_INFO0_SW_BUFFER_COOKIE);

		desc_va = ((u64)le32_to_cpu(reo_desc->buf_addr_info.info1) << 32 |
			   le32_to_cpu(reo_desc->buf_addr_info.info0));
		rel_info->rx_desc =
			(struct ath12k_rx_desc_info *)((unsigned long)desc_va);
	} else {
		rel_info->cookie = le32_get_bits(reo_desc->buf_addr_info.info1,
						 BUFFER_ADDR_INFO1_SW_COOKIE);
		rel_info->rx_desc = ath12k_dp_get_rx_desc(dp, rel_info->cookie);
		if (!rel_info->rx_desc) {
			ath12k_warn(ab, "CC did not happened on rx error %u",
				    rel_info->cookie);
			print_hex_dump(KERN_ERR, "rx error desc: ", DUMP_PREFIX_ADDRESS,
				       32, 4, reo_desc, sizeof(*reo_desc), false);
			BUG_ON(1);
		}
	}

	rel_info->err_rel_src = rel_src;
	rel_info->hw_cc_done = hw_cc_done;
	rel_info->peer_metadata = reo_desc->rx_mpdu_info.peer_meta_data;

	return 0;
}

void ath12k_wifi8_hal_rx_reo_ent_paddr_get(struct ath12k_buffer_addr *buff_addr,
					   dma_addr_t *paddr, u32 *cookie)
{
	*paddr = ((u64)(le32_get_bits(buff_addr->info1,
				      BUFFER_ADDR_INFO1_ADDR)) << 32) |
		le32_get_bits(buff_addr->info0, BUFFER_ADDR_INFO0_ADDR);

	*cookie = le32_get_bits(buff_addr->info1, BUFFER_ADDR_INFO1_SW_COOKIE);
}

void ath12k_wifi8_hal_rx_reo_ent_buf_paddr_get(void *rx_desc, dma_addr_t *paddr,
					       u32 *sw_cookie,
					       struct ath12k_buffer_addr **pp_buf_addr,
					       u8 *rbm, u32 *msdu_cnt)
{
	struct hal_reo_entrance_ring *reo_ent_ring =
		(struct hal_reo_entrance_ring *)rx_desc;
	struct ath12k_buffer_addr *buf_addr_info;
	struct hal_rx_reo_mpdu_desc_info *rx_mpdu_desc_info_details;

	rx_mpdu_desc_info_details =
		(struct hal_rx_reo_mpdu_desc_info *)&reo_ent_ring->rx_reo_mpdu_info;

	*msdu_cnt = le32_get_bits(rx_mpdu_desc_info_details->info0,
				  HAL_RX_REO_MPDU_DESC_INFO_INFO0_MSDU_COUNT);

	buf_addr_info = (struct ath12k_buffer_addr *)&reo_ent_ring->buf_addr_info;

	*paddr = (((u64)le32_get_bits(buf_addr_info->info1,
				      BUFFER_ADDR_INFO1_ADDR)) << 32) |
			le32_get_bits(buf_addr_info->info0,
				      BUFFER_ADDR_INFO0_ADDR);

	*sw_cookie = le32_get_bits(buf_addr_info->info1,
				   BUFFER_ADDR_INFO1_SW_COOKIE);
	*rbm = le32_get_bits(buf_addr_info->info1,
			     BUFFER_ADDR_INFO1_RET_BUF_MGR);

	*pp_buf_addr = (void *)buf_addr_info;
}

void ath12k_wifi8_hal_rx_msdu_list_get(void *desc,
				       void *list,
				       u16 *num_msdus)
{
	struct hal_rx_msdu_link *link_desc = (struct hal_rx_msdu_link *)desc;
	struct hal_rx_msdu_list *msdu_list = (struct hal_rx_msdu_list *)list;
	struct hal_rx_msdu_desc *msdu_desc_info = NULL;
	    struct hal_rx_msdu_details *msdu_ptrs[] = {
		&link_desc->msdu_0,
		&link_desc->msdu_1,
		&link_desc->msdu_2,
		&link_desc->msdu_3,
		&link_desc->msdu_4,
		&link_desc->msdu_5,
		&link_desc->msdu_6,
		&link_desc->msdu_7,
		&link_desc->msdu_8,
		&link_desc->msdu_9,
		&link_desc->msdu_10,
	};
	u32 last = 0, first = 0;
	u8 tmp = 0;
	int i;

	last = u32_encode_bits(last, HAL_RX_MSDU_DESC_INFO_INFO0_LAST_MSDU_IN_MPDU_FLAG);
	first = u32_encode_bits(first,
				HAL_RX_MSDU_DESC_INFO_INFO0_FIRST_MSDU_IN_MPDU_FLAG);

	for (i = 0; i < HAL_RX_NUM_MSDU_DESC; i++) {
		if (!i && le32_get_bits(msdu_ptrs[i]->buf_addr_info.info0,
					BUFFER_ADDR_INFO0_ADDR) == 0)
			break;
		if (le32_get_bits(msdu_ptrs[i]->buf_addr_info.info0,
				  BUFFER_ADDR_INFO0_ADDR) == 0) {
			msdu_desc_info = &msdu_ptrs[i - 1]->rx_msdu_info;
			msdu_desc_info->info0 |= cpu_to_le32(last);
			break;
		}
		msdu_desc_info = &msdu_ptrs[i]->rx_msdu_info;

		if (!i)
			msdu_desc_info->info0 |= cpu_to_le32(first);
		else if (i == (HAL_RX_NUM_MSDU_DESC - 1))
			msdu_desc_info->info0 |= cpu_to_le32(last);
		msdu_list->msdu_info[i].msdu_flags = le32_to_cpu(msdu_desc_info->info0);
		msdu_list->msdu_info[i].msdu_len =
			 HAL_RX_MSDU_PKT_LENGTH_GET(msdu_desc_info->info0);
		msdu_list->sw_cookie[i] =
			le32_get_bits(msdu_ptrs[i]->buf_addr_info.info1,
				      BUFFER_ADDR_INFO1_SW_COOKIE);
		tmp = le32_get_bits(msdu_ptrs[i]->buf_addr_info.info1,
				    BUFFER_ADDR_INFO1_RET_BUF_MGR);
		msdu_list->paddr[i] =
			((u64)(le32_get_bits(msdu_ptrs[i]->buf_addr_info.info1,
					     BUFFER_ADDR_INFO1_ADDR)) << 32) |
			le32_get_bits(msdu_ptrs[i]->buf_addr_info.info0,
				      BUFFER_ADDR_INFO0_ADDR);
		msdu_list->rbm[i] = tmp;
	}
	*num_msdus = i;
}

void
ath12k_wifi8_hal_rx_msdu_link_desc_set(struct ath12k_base *ab,
				       struct hal_wbm_release_ring *desc,
				       struct ath12k_buffer_addr *buf_addr_info,
				       enum hal_wbm_rel_bm_act action)
{
	desc->buf_addr_info = *buf_addr_info;
	desc->info0 |= le16_encode_bits(HAL_WBM_REL_SRC_MODULE_SW,
					HAL_WBM_RELEASE_INFO0_REL_SRC_MODULE) |
		    //le16_encode_bits(action, HAL_WBM_RELEASE_INFO0_BM_ACTION) |
		    le16_encode_bits(HAL_WBM_REL_DESC_TYPE_MSDU_LINK,
				     HAL_WBM_RELEASE_INFO0_DESC_TYPE);
}

/* Retuns last buffered SN given SSN and complete REO bitmaps */
int
ath12k_wifi8_hal_reo_highest_sn_from_bitmap(u16 ssn,
					    const u32 *bitmap_words,
					    int num_words,
					    u16 *high_off)
{
	int word;
	u16 offset;

	for (word = num_words - 1; word >= 0; word--) {
		if (bitmap_words[word])
			break;
	}

	if (word < 0)
		return -ENOENT;

	offset = (word * 32) + __fls(bitmap_words[word]);
	if (high_off)
		*high_off = offset;

	return (ssn + offset) & IEEE80211_SN_MASK;
}

void ath12k_wifi8_hal_reo_status_queue_1k_stats(struct ath12k_base *ab,
						struct hal_tlv_64_hdr *tlv,
						struct hal_reo_status *status)
{
	struct hal_reo_get_queue_1k_stats_status *desc =
		(struct hal_reo_get_queue_1k_stats_status *)tlv->value;

	status->uniform_hdr.cmd_num =
		le16_to_cpu(desc->hdr.reo_status_number);
	status->uniform_hdr.cmd_status =
		le16_get_bits(desc->hdr.info0,
			      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);

	/* Fill 1k extension bitmap (bits 288..1023) */
	status->u.queue_1k_stats.bitmap.rx_bitmap_319_288 =
		le32_to_cpu(desc->rx_bitmap1023_288[0]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_351_320 =
		le32_to_cpu(desc->rx_bitmap1023_288[1]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_383_352 =
		le32_to_cpu(desc->rx_bitmap1023_288[2]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_415_384 =
		le32_to_cpu(desc->rx_bitmap1023_288[3]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_447_416 =
		le32_to_cpu(desc->rx_bitmap1023_288[4]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_479_448 =
		le32_to_cpu(desc->rx_bitmap1023_288[5]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_511_480 =
		le32_to_cpu(desc->rx_bitmap1023_288[6]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_543_512 =
		le32_to_cpu(desc->rx_bitmap1023_288[7]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_575_544 =
		le32_to_cpu(desc->rx_bitmap1023_288[8]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_607_576 =
		le32_to_cpu(desc->rx_bitmap1023_288[9]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_639_608 =
		le32_to_cpu(desc->rx_bitmap1023_288[10]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_671_640 =
		le32_to_cpu(desc->rx_bitmap1023_288[11]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_703_672 =
		le32_to_cpu(desc->rx_bitmap1023_288[12]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_735_704 =
		le32_to_cpu(desc->rx_bitmap1023_288[13]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_767_736 =
		le32_to_cpu(desc->rx_bitmap1023_288[14]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_799_768 =
		le32_to_cpu(desc->rx_bitmap1023_288[15]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_831_800 =
		le32_to_cpu(desc->rx_bitmap1023_288[16]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_863_832 =
		le32_to_cpu(desc->rx_bitmap1023_288[17]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_895_864 =
		le32_to_cpu(desc->rx_bitmap1023_288[18]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_927_896 =
		le32_to_cpu(desc->rx_bitmap1023_288[19]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_959_928 =
		le32_to_cpu(desc->rx_bitmap1023_288[20]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_991_960 =
		le32_to_cpu(desc->rx_bitmap1023_288[21]);
	status->u.queue_1k_stats.bitmap.rx_bitmap_1023_992 =
		le32_to_cpu(desc->rx_bitmap1023_288[22]);

	ath12k_dbg(ab, ATH12K_DBG_HAL, "Queue 1K stats status:\n");
	ath12k_dbg(ab, ATH12K_DBG_HAL,
		   "rx_bitmap[288..543] [%08x %08x %08x %08x %08x %08x %08x %08x]\n",
		   status->u.queue_1k_stats.bitmap.rx_bitmap_319_288,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_351_320,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_383_352,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_415_384,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_447_416,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_479_448,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_511_480,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_543_512);
	ath12k_dbg(ab, ATH12K_DBG_HAL,
		   "rx_bitmap[544..799] [%08x %08x %08x %08x %08x %08x %08x %08x]\n",
		   status->u.queue_1k_stats.bitmap.rx_bitmap_575_544,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_607_576,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_639_608,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_671_640,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_703_672,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_735_704,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_767_736,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_799_768);
	ath12k_dbg(ab, ATH12K_DBG_HAL,
		   "rx_bitmap[800..1023] [%08x %08x %08x %08x %08x %08x %08x]\n",
		   status->u.queue_1k_stats.bitmap.rx_bitmap_831_800,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_863_832,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_895_864,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_927_896,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_959_928,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_991_960,
		   status->u.queue_1k_stats.bitmap.rx_bitmap_1023_992);
	ath12k_dbg(ab, ATH12K_DBG_HAL, "looping count %u\n",
		   le32_get_bits(desc->info0,
				 HAL_REO_GET_Q_1K_STATS_STATUS_INFO0_LOOPING_COUNT));
}

void ath12k_wifi8_hal_reo_status_queue_stats(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status)
{
	struct hal_reo_get_queue_stats_status *desc =
		(struct hal_reo_get_queue_stats_status *)tlv->value;

	status->uniform_hdr.cmd_num =
				le16_to_cpu(desc->hdr.reo_status_number);
	status->uniform_hdr.cmd_status =
				le16_get_bits(desc->hdr.info0,
					      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);
	/* Parse queue stats into new hal_reo_status_queue_stats layout */
	status->u.queue_stats.ssn =
		le32_get_bits(desc->info0,
			      HAL_REO_GET_Q_STATS_STATUS_INFO0_SSN);
	status->u.queue_stats.curr_idx =
		le32_get_bits(desc->info0,
			      HAL_REO_GET_Q_STATS_STATUS_INFO0_CURRENT_INDEX);

	/* PN fields */
	status->u.queue_stats.pn_31_0 = le32_to_cpu(desc->pn_31_0);
	status->u.queue_stats.pn_47_32 = le16_to_cpu(desc->pn_47_32);
	status->u.queue_stats.pn_127_48_info =
		le16_get_bits(desc->info1,
			      HAL_REO_GET_Q_STATS_STATUS_INFO1_PN_127_48_INFO);

	/* Derive PN length from PN_127_48_INFO */
	switch (status->u.queue_stats.pn_127_48_info) {
	case 0: /* PN_MSBs_0: 48-bit PN */
		status->u.queue_stats.pn_len = 6;
		break;
	case 1: /* PN_MSBs_lt_WAPI: non-zero MSBs */
	case 2: /* PN_MSBs_WAPI: exact WAPI pattern */
	case 3: /* PN_MSBs_gt_WAPI: non-zero MSBs */
		status->u.queue_stats.pn_len = 16;
		break;
	default:
		status->u.queue_stats.pn_len = 0;
		break;
	}

	/* Bitmaps 0..287 */
	status->u.queue_stats.bitmap.rx_bitmap_31_0 =
		le32_to_cpu(desc->rx_bitmap_31_0);
	status->u.queue_stats.bitmap.rx_bitmap_63_32 =
		le32_to_cpu(desc->rx_bitmap_63_32);
	status->u.queue_stats.bitmap.rx_bitmap_95_64 =
		le32_to_cpu(desc->rx_bitmap_95_64);
	status->u.queue_stats.bitmap.rx_bitmap_127_96 =
		le32_to_cpu(desc->rx_bitmap_127_96);
	status->u.queue_stats.bitmap.rx_bitmap_159_128 =
		le32_to_cpu(desc->rx_bitmap_159_128);
	status->u.queue_stats.bitmap.rx_bitmap_191_160 =
		le32_to_cpu(desc->rx_bitmap_191_160);
	status->u.queue_stats.bitmap.rx_bitmap_223_192 =
		le32_to_cpu(desc->rx_bitmap_223_192);
	status->u.queue_stats.bitmap.rx_bitmap_255_224 =
		le32_to_cpu(desc->rx_bitmap_255_224);
	status->u.queue_stats.bitmap.rx_bitmap_287_256 =
		le32_to_cpu(desc->rx_bitmap_287_256);

	/* Follow-up flag for 1k extension */
	status->u.queue_stats.to_follow_1k =
		le32_get_bits(desc->info5,
			      HAL_REO_GET_Q_STATS_STATUS_INFO5_GET_Q_1K_SSTAT_FOLLOW);

	/* Timestamps */
	status->u.queue_stats.last_rx_queue_ts =
		le32_to_cpu(desc->last_rx_enqueue_timestamp);
	status->u.queue_stats.last_rx_dequeue_ts =
		le32_to_cpu(desc->last_rx_dequeue_timestamp);

	/* Current counts */
	status->u.queue_stats.curr_mpdu_cnt =
		le32_get_bits(desc->info2,
			      HAL_REO_GET_Q_STATS_STATUS_INFO2_CURRENT_MPDU_COUNT);
	status->u.queue_stats.curr_msdu_cnt =
		le32_get_bits(desc->info2,
			      HAL_REO_GET_Q_STATS_STATUS_INFO2_CURRENT_MSDU_COUNT);

	/* Counters */
	status->u.queue_stats.fwd_due_to_bar_cnt =
		le16_get_bits(desc->info3,
			      (u16)HAL_REO_GET_Q_STATS_STATUS_INFO3_FWD_DUE_TO_BAR_COUNT);
	status->u.queue_stats.dup_cnt = le16_to_cpu(desc->duplicate_count);
	status->u.queue_stats.frames_in_order_cnt =
		le32_get_bits(desc->info4,
			      HAL_REO_GET_Q_STATS_STATUS_INFO4_FRAMES_IN_ORDER_COUNT);
	status->u.queue_stats.num_mpdu_processed_cnt =
		le32_to_cpu(desc->mpdu_frames_processed_count);
	status->u.queue_stats.num_msdu_processed_cnt =
		le32_to_cpu(desc->msdu_frames_processed_count);
	status->u.queue_stats.total_num_processed_byte_cnt =
		le32_to_cpu(desc->total_processed_byte_count);
	status->u.queue_stats.late_rx_mpdu_cnt =
		le32_get_bits(desc->info5,
			      HAL_REO_GET_Q_STATS_STATUS_INFO5_LATE_RCV_MPDU_COUNT);
	status->u.queue_stats.reorder_hole_cnt =
		le32_get_bits(desc->info5,
			      HAL_REO_GET_Q_STATS_STATUS_INFO5_HOLE_COUNT);
	status->u.queue_stats.timeout_cnt =
		le16_get_bits(desc->info3,
			      (u16)HAL_REO_GET_Q_STATS_STATUS_INFO3_TIMEOUT_COUNT);
	status->u.queue_stats.bar_rx_cnt =
		le32_get_bits(desc->info4,
			      HAL_REO_GET_Q_STATS_STATUS_INFO4_BAR_RECEIVED_COUNT);
	status->u.queue_stats.num_window_2k_jump_cnt =
		le16_get_bits(desc->info3,
			      (u16)HAL_REO_GET_Q_STATS_STATUS_INFO3_WINDOW_JUMP_2K);

	ath12k_dbg(ab, ATH12K_DBG_HAL, "Queue stats status:\n");
	ath12k_dbg(ab, ATH12K_DBG_HAL, "header: cmd_num %d status %d\n",
		   status->uniform_hdr.cmd_num,
		   status->uniform_hdr.cmd_status);
	ath12k_dbg(ab, ATH12K_DBG_HAL, "ssn %u cur_idx %u\n",
		   le32_get_bits(desc->info0,
				 HAL_REO_GET_Q_STATS_STATUS_INFO0_SSN),
		   le32_get_bits(desc->info0,
				 HAL_REO_GET_Q_STATS_STATUS_INFO0_CURRENT_INDEX));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "pn = [%08x, %08x]\n",
		   le32_to_cpu(desc->pn_31_0), le16_to_cpu(desc->pn_47_32));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "last_rx: enqueue_tstamp %08x dequeue_tstamp %08x\n",
		   le32_to_cpu(desc->last_rx_enqueue_timestamp),
		   le32_to_cpu(desc->last_rx_dequeue_timestamp));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "rx_bitmap [%08x %08x %08x %08x %08x %08x %08x %08x %08x]\n",
		   le32_to_cpu(desc->rx_bitmap_31_0),
		   le32_to_cpu(desc->rx_bitmap_63_32),
		   le32_to_cpu(desc->rx_bitmap_95_64),
		   le32_to_cpu(desc->rx_bitmap_127_96),
		   le32_to_cpu(desc->rx_bitmap_159_128),
		   le32_to_cpu(desc->rx_bitmap_191_160),
		   le32_to_cpu(desc->rx_bitmap_223_192),
		   le32_to_cpu(desc->rx_bitmap_255_224),
		   le32_to_cpu(desc->rx_bitmap_287_256));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "count: cur_mpdu %u cur_msdu %u\n",
		   le32_get_bits(desc->info2,
				 HAL_REO_GET_Q_STATS_STATUS_INFO2_CURRENT_MPDU_COUNT),
		   le32_get_bits(desc->info2,
				 HAL_REO_GET_Q_STATS_STATUS_INFO2_CURRENT_MSDU_COUNT));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "fwd_timeout %u fwd_bar %u dup_count %u\n",
		   le16_get_bits(desc->info3,
				 (u16)HAL_REO_GET_Q_STATS_STATUS_INFO3_TIMEOUT_COUNT),
		   le16_get_bits(desc->info3,
				 (u16)
				 HAL_REO_GET_Q_STATS_STATUS_INFO3_FWD_DUE_TO_BAR_COUNT),
		   le16_to_cpu(desc->duplicate_count));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "frames_in_order %u bar_rcvd %u\n",
		   le32_get_bits(desc->info4,
				 HAL_REO_GET_Q_STATS_STATUS_INFO4_FRAMES_IN_ORDER_COUNT),
		   le32_get_bits(desc->info4,
				 HAL_REO_GET_Q_STATS_STATUS_INFO4_BAR_RECEIVED_COUNT));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "num_mpdus %d num_msdus %d total_bytes %d\n",
		   le32_to_cpu(desc->mpdu_frames_processed_count),
		   le32_to_cpu(desc->msdu_frames_processed_count),
		   le32_to_cpu(desc->total_processed_byte_count));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "late_rcvd %u win_jump_2k %u hole_cnt %u\n",
		   le32_get_bits(desc->info5,
				 HAL_REO_GET_Q_STATS_STATUS_INFO5_LATE_RCV_MPDU_COUNT),
		   le16_get_bits(desc->info3,
				 (u16)HAL_REO_GET_Q_STATS_STATUS_INFO3_WINDOW_JUMP_2K),
		   le32_get_bits(desc->info5,
				 HAL_REO_GET_Q_STATS_STATUS_INFO5_HOLE_COUNT));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "looping count %u\n",
		   le16_get_bits(desc->info6,
				 (u16)HAL_REO_GET_Q_STATS_STATUS_INFO6_LOOPING_COUNT));
}

void ath12k_wifi8_hal_reo_flush_queue_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status)
{
	struct hal_reo_flush_queue_status *desc =
		(struct hal_reo_flush_queue_status *)tlv->value;

	status->uniform_hdr.cmd_num =
			le16_to_cpu(desc->hdr.reo_status_number);
	status->uniform_hdr.cmd_status =
			le16_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);
	status->u.flush_queue.err_detected =
			le32_get_bits(desc->info0,
				      HAL_REO_FLUSH_QUEUE_INFO0_ERR_DETECTED);
	if (status->u.flush_queue.err_detected)
		ath12k_dbg(ab, ATH12K_DBG_HAL, "REO FLUSH QUEUE ERR DETECTED %u\n",
			   status->u.flush_queue.err_detected);
}

void
ath12k_wifi8_hal_reo_flush_cache_status(struct ath12k_base *ab,
					struct hal_tlv_64_hdr *tlv,
					struct hal_reo_status *status)
{
	struct ath12k_hal *hal = &ab->hal;
	struct hal_reo_flush_cache_status *desc =
		(struct hal_reo_flush_cache_status *)tlv->value;

	status->uniform_hdr.cmd_num =
			le16_to_cpu(desc->hdr.reo_status_number);
	status->uniform_hdr.cmd_status =
			le16_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);

	status->u.flush_cache.err_detected =
			le32_get_bits(desc->info0,
				      HAL_REO_FLUSH_CACHE_STATUS_INFO0_IS_ERR);
	status->u.flush_cache.err_code =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_CACHE_STATUS_INFO0_BLOCK_ERR_CODE);
	if (!status->u.flush_cache.err_code)
		hal->avail_blk_resource |= BIT(hal->current_blk_index);

	status->u.flush_cache.cache_controller_flush_status_hit =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_STATUS_HIT);

	status->u.flush_cache.cache_controller_flush_status_desc_type =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_DESC_TYPE);
	status->u.flush_cache.cache_controller_flush_status_client_id =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_CLIENT_ID);
	status->u.flush_cache.cache_controller_flush_status_err =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_ERR);
	status->u.flush_cache.cache_controller_flush_status_cnt =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_COUNT);
}

void ath12k_wifi8_hal_reo_unblk_cache_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status)
{
	struct ath12k_hal *hal = &ab->hal;
	struct hal_reo_unblock_cache_status *desc =
		(struct hal_reo_unblock_cache_status *)tlv->value;

	status->uniform_hdr.cmd_num =
			le16_to_cpu(desc->hdr.reo_status_number);
	status->uniform_hdr.cmd_status =
			le16_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);

	status->u.unblock_cache.err_detected =
			le32_get_bits(desc->info0,
				      HAL_REO_UNBLOCK_CACHE_STATUS_INFO0_IS_ERR);
	status->u.unblock_cache.unblock_type =
			le32_get_bits(desc->info0,
				      HAL_REO_UNBLOCK_CACHE_STATUS_INFO0_TYPE);

	if (!status->u.unblock_cache.err_detected &&
	    status->u.unblock_cache.unblock_type ==
	    HAL_REO_STATUS_UNBLOCK_BLOCKING_RESOURCE)
		hal->avail_blk_resource &= ~BIT(hal->current_blk_index);
}

void
ath12k_wifi8_hal_reo_flush_timeout_list_status(struct ath12k_base *ab,
					       struct hal_tlv_64_hdr *tlv,
					       struct hal_reo_status *status)
{
	struct hal_reo_flush_timeout_list_status *desc =
		(struct hal_reo_flush_timeout_list_status *)tlv->value;

	status->uniform_hdr.cmd_num =
			le16_to_cpu(desc->hdr.reo_status_number);
	status->uniform_hdr.cmd_status =
			le16_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);

	status->u.timeout_list.err_detected =
			le32_get_bits(desc->info0,
				      HAL_REO_FLUSH_TIMEOUT_STATUS_INFO0_IS_ERR);
	status->u.timeout_list.list_empty =
			le32_get_bits(desc->info0,
				      HAL_REO_FLUSH_TIMEOUT_STATUS_INFO0_LIST_EMPTY);

	status->u.timeout_list.release_desc_cnt =
		le16_to_cpu(desc->release_desc_count);
	status->u.timeout_list.fwd_buf_cnt =
		le16_to_cpu(desc->forward_buf_count);
}

void
ath12k_wifi8_hal_reo_desc_thresh_reached_status(struct ath12k_base *ab,
						struct hal_tlv_64_hdr *tlv,
						struct hal_reo_status *status)
{
	struct hal_reo_desc_thresh_reached_status *desc =
		(struct hal_reo_desc_thresh_reached_status *)tlv->value;

	status->uniform_hdr.cmd_num =
			le16_to_cpu(desc->hdr.reo_status_number);
	status->uniform_hdr.cmd_status =
			le16_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);

	status->u.desc_thresh_reached.threshold_idx =
		le32_get_bits(desc->info0,
			      HAL_REO_DESC_THRESH_STATUS_INFO0_THRESH_INDEX);

	status->u.desc_thresh_reached.link_desc_counter0 =
		le32_get_bits(desc->info1,
			      HAL_REO_DESC_THRESH_STATUS_INFO1_LINK_DESC_COUNTER0);

	status->u.desc_thresh_reached.link_desc_counter1 =
		le32_get_bits(desc->info2,
			      HAL_REO_DESC_THRESH_STATUS_INFO2_LINK_DESC_COUNTER1);

	status->u.desc_thresh_reached.link_desc_counter2 =
		le32_get_bits(desc->info2,
			      HAL_REO_DESC_THRESH_STATUS_INFO3_LINK_DESC_COUNTER2);

	status->u.desc_thresh_reached.link_desc_counter_sum =
		le32_get_bits(desc->info2,
			      HAL_REO_DESC_THRESH_STATUS_INFO4_LINK_DESC_COUNTER_SUM);
}

void ath12k_wifi8_hal_reo_update_rx_reo_queue_status(struct ath12k_base *ab,
						     struct hal_tlv_64_hdr *tlv,
						     struct hal_reo_status *status)
{
	struct hal_reo_status_hdr *desc =
		(struct hal_reo_status_hdr *)tlv->value;

	status->uniform_hdr.cmd_num =
			le16_to_cpu(desc->reo_status_number);
	status->uniform_hdr.cmd_status =
			le16_get_bits(desc->info0,
				      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);
}

u32 ath12k_wifi8_hal_reo_qdesc_size(u32 ba_window_size, u8 tid)
{
	u32 num_ext_desc;

	if (ba_window_size <= 1) {
		num_ext_desc = 1;
	} else if (ba_window_size <= 217) {
		num_ext_desc = 1;
	} else if (ba_window_size <= 434) {
		num_ext_desc = 2;
	} else {
		num_ext_desc = 5;
	}

	return sizeof(struct hal_rx_reo_queue) +
		(num_ext_desc * sizeof(struct hal_rx_reo_queue_ext));
}

void ath12k_wifi8_hal_reo_qdesc_setup(struct hal_rx_reo_queue *qdesc,
				      int tid, u32 ba_window_size,
				      u32 start_seq, enum hal_pn_type type,
				      u16 stats_id)
{
	struct hal_rx_reo_queue_ext *ext_desc;

	ath12k_wifi8_hal_reo_set_desc_hdr(&qdesc->desc_hdr, HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_0);

	qdesc->receive_queue_number = cpu_to_le16(tid);

	qdesc->info0 =
	     le32_encode_bits(1, HAL_RX_REO_QUEUE_INFO0_VLD) |
	     le32_encode_bits(1,
			      HAL_RX_REO_QUEUE_INFO0_ASSOCIATED_LINK_DESCRIPTOR_COUNTER) |
	     le32_encode_bits(ath12k_tid_to_ac(tid),
			      HAL_RX_REO_QUEUE_INFO0_LINK_LIST_TIMER_IDX);

	if (ba_window_size < 1)
		ba_window_size = 1;

	if (ba_window_size == 1 && !ath12k_wifi8_hal_is_reo_nonqos_mgmt_tid(tid))
		ba_window_size++;

	if (ba_window_size == 1)
		qdesc->info0 |= le32_encode_bits(1, HAL_RX_REO_QUEUE_INFO0_RTY);

	qdesc->info0 |= le32_encode_bits(ba_window_size - 1,
					 HAL_RX_REO_QUEUE_INFO0_BA_WINDOW_SIZE);
	switch (type) {
	case HAL_PN_TYPE_NONE:
	case HAL_PN_TYPE_WAPI_EVEN:
	case HAL_PN_TYPE_WAPI_UNEVEN:
		break;
	case HAL_PN_TYPE_WPA:
		qdesc->info0 |=
			le32_encode_bits(1, HAL_RX_REO_QUEUE_INFO0_PN_CHECK_NEEDED) |
			le32_encode_bits(HAL_RX_REO_QUEUE_PN_SIZE_48,
					 HAL_RX_REO_QUEUE_INFO0_PN_SIZE);
		break;
	}

	/* TODO: Set Ignore ampdu flags based on BA window size and/or
	 * AMPDU capabilities
	 */
	qdesc->info0 |= le32_encode_bits(1, HAL_RX_REO_QUEUE_INFO0_IGNORE_AMPDU_FLAG);

	qdesc->info1 = le16_encode_bits(0, HAL_RX_REO_QUEUE_INFO1_SVLD);

	if (start_seq <= 0xfff)
		qdesc->info1 = le16_encode_bits(start_seq,
						HAL_RX_REO_QUEUE_INFO1_SSN);

	qdesc->info2 = le16_encode_bits(stats_id, HAL_RX_REO_QUEUE_INFO2_STATS_ID);

	if (ath12k_wifi8_hal_is_reo_nonqos_mgmt_tid(tid))
		return;

	ext_desc = qdesc->ext_desc;

	/* TODO: HW queue descriptors are currently allocated for max BA
	 * window size for all QOS TIDs so that same descriptor can be used
	 * later when ADDBA request is received. This should be changed to
	 * allocate HW queue descriptors based on BA window size being
	 * negotiated (0 for non BA cases), and reallocate when BA window
	 * size changes and also send WMI message to FW to change the REO
	 * queue descriptor in Rx peer entry as part of dp_rx_tid_update.
	 */
	memset(ext_desc, 0, REO_QUEUE_EXT_DESC_MAX * sizeof(*ext_desc));
	ath12k_wifi8_hal_reo_set_desc_hdr(&ext_desc->desc_hdr,
					  HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_1);
	ext_desc++;
	ath12k_wifi8_hal_reo_set_desc_hdr(&ext_desc->desc_hdr,
					  HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_2);
	ext_desc++;
	ath12k_wifi8_hal_reo_set_desc_hdr(&ext_desc->desc_hdr,
					  HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_3);
	ext_desc++;
	ath12k_wifi8_hal_reo_set_desc_hdr(&ext_desc->desc_hdr,
					  HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_4);
	ext_desc++;
	ath12k_wifi8_hal_reo_set_desc_hdr(&ext_desc->desc_hdr,
					  HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_5);
}

void ath12k_wifi8_hal_reo_init_cmd_ring_offset(struct ath12k_base *ab,
					       struct hal_srng *srng,
					       u16 cmd_num)
{
	struct hal_srng_params params;
	struct hal_tlv_64_hdr *tlv;
	struct hal_reo_get_queue_stats *desc;
	int i;
	int entry_size;
	u8 *entry;

	memset(&params, 0, sizeof(params));

	entry_size = ath12k_hal_srng_get_entrysize(ab, HAL_REO_CMD);
	ath12k_hal_srng_get_params(ab, srng, &params);
	entry = (u8 *)params.ring_base_vaddr;

	for (i = 0; i < params.num_entries; i++) {
		tlv = (struct hal_tlv_64_hdr *)entry;
		desc = (struct hal_reo_get_queue_stats *)tlv->value;
		desc->cmd.reo_cmd_number = cpu_to_le16(cmd_num++);
		entry += entry_size;
	}
}

void ath12k_wifi8_hal_reo_init_cmd_ring(struct ath12k_base *ab,
					struct hal_srng *srng)
{
	ath12k_wifi8_hal_reo_init_cmd_ring_offset(ab, srng, 1);
}

static void ath12k_wifi8_reo_dest_ring_ctrl_setup(struct ath12k_base *ab,
						  u32 start, u32 end, u32 reg_add)
{
	struct ath12k_hal *hal = &ab->hal;
	const struct ath12k_hal_rdi_mapping *rdi_mapping = hal->rdi_mapping;
	u32 reo_base = HAL_SEQ_WCSS_UMAC_REO_REG;
	u32 hash_map = 0;
	u8 shift = 0;
	u8 i;

	for (i = start; i <= end; i++, shift += HAL_REO_IX_FIELD_WIDTH)
		hash_map |= rdi_mapping[i].rd << shift;

	ath12k_hif_write32(ab, reo_base + reg_add, hash_map);
}

void ath12k_wifi8_hal_reo_hw_setup(struct ath12k_base *ab)
{
	struct ath12k_hal *hal = &ab->hal;
	struct ath12k_dp_wifi8 *dp_wifi8 =
			ath12k_get_dp_wifi8(ath12k_ab_to_dp(ab));
	u32 reo_base = HAL_SEQ_WCSS_UMAC_REO_REG;
	u32 val, VI_reorder_timeout;

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_GEN_ENABLE);

	val |= u32_encode_bits(1, HAL_REO1_GEN_ENABLE_AGING_LIST_ENABLE) |
	       u32_encode_bits(1, HAL_REO1_GEN_ENABLE_AGING_FLUSH_ENABLE);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_GEN_ENABLE, val);

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_MISC_CTRL_ADDR(hal));

	val &= ~(HAL_REO1_MISC_CTL_SPARE_CTRL_DST_RING |
		 HAL_REO1_MISC_CTL_FRAG_DST_RING |
		 HAL_REO1_MISC_CTL_BAR_DST_RING);
	val |= u32_encode_bits(1,
			       HAL_REO1_MISC_CTL_SPARE_CTRL_DST_RING);
	val |= u32_encode_bits(HAL_SRNG_RING_ID_REO2SW0,
			       HAL_REO1_MISC_CTL_FRAG_DST_RING);
	val |= u32_encode_bits(HAL_SRNG_RING_ID_REO2SW0,
			       HAL_REO1_MISC_CTL_BAR_DST_RING);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_MISC_CTRL_ADDR(hal), val);

	ath12k_hif_write32(ab, reo_base + HAL_REO1_AGING_THRESH_IX_0(hal),
			   HAL_DEFAULT_BE_BK_VI_REO_TIMEOUT_USEC);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_AGING_THRESH_IX_1(hal),
			   HAL_DEFAULT_BE_BK_VI_REO_TIMEOUT_USEC);

	if (ath12k_reorder_VI_timeout &&
	    ath12k_reorder_VI_timeout >= MIN_VI_REORDER_TIMEOUT_MS &&
	    ath12k_reorder_VI_timeout <= MAX_VI_REORDER_TIMEOUT_MS) {
		VI_reorder_timeout = ath12k_reorder_VI_timeout * 1000;
	} else {
		VI_reorder_timeout = HAL_DEFAULT_BE_BK_VI_REO_TIMEOUT_USEC;
	}

	ath12k_hif_write32(ab, reo_base + HAL_REO1_AGING_THRESH_IX_2(hal),
			   VI_reorder_timeout);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_AGING_THRESH_IX_3(hal),
			   HAL_DEFAULT_VO_REO_TIMEOUT_USEC);

	/* RDI values 0-6 are programmed in IX_0 register */
	ath12k_wifi8_reo_dest_ring_ctrl_setup(ab, 0, 6,
					      HAL_REO1_DEST_RING_CTRL_AP_IX_0);
	/* RDI values 6-12 are programmed in IX_1 register */
	ath12k_wifi8_reo_dest_ring_ctrl_setup(ab, 6, 12,
					      HAL_REO1_DEST_RING_CTRL_AP_IX_1);
	/* RDI values 12-18 are programmed in IX_2 register */
	ath12k_wifi8_reo_dest_ring_ctrl_setup(ab, 12, 18,
					      HAL_REO1_DEST_RING_CTRL_AP_IX_2);
	/* RDI values 18-24 are programmed in IX_3 register */
	ath12k_wifi8_reo_dest_ring_ctrl_setup(ab, 18, 24,
					      HAL_REO1_DEST_RING_CTRL_AP_IX_3);
	/* RDI values 24-30 are programmed in IX_4 register */
	ath12k_wifi8_reo_dest_ring_ctrl_setup(ab, 24, 30,
					      HAL_REO1_DEST_RING_CTRL_AP_IX_4);
	/* RDI values 31 and 32 are programmed in IX_5 register */
	ath12k_wifi8_reo_dest_ring_ctrl_setup(ab, 30, 31,
					      HAL_REO1_DEST_RING_CTRL_AP_IX_5);

	/* Override ring selection for error packets instead of using RDI */
	ath12k_hif_write32(ab, reo_base + HAL_REO_RXDMA_ERROR_CODE_RBM_OVERRIDE,
			   0x3FFFFF);
	ath12k_hif_write32(ab, reo_base + HAL_REO_ERROR_CODE_RBM_OVERRIDE, 0xFFFF);

	/* Enable delinking for error packets */
	ath12k_hif_write32(ab, reo_base + HAL_REO_RXDMA_ERROR_CODE_REO_DELINK, 0x3FFFFF);
	ath12k_hif_write32(ab, reo_base + HAL_REO_ERROR_CODE_REO_DELINK, 0xFFFF);
	ath12k_hif_write32(ab, reo_base + HAL_BAR_REO_ERROR_CODE_DELINK, 0xFFFF);

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_MISC_CFG_1);
	val |= u32_encode_bits(1, HAL_REO1_MISC_CFG_1_REO_ERR_DELINK_ENABLE);
	val |= u32_encode_bits(1, HAL_REO1_MISC_CFG_1_RXDMA_ERR_DELINK_ENABLE);
	/*
	 * WAR enablement for HW issue on tresles V1 for TRSLONE-1155
	 * This setting should be skipped for trestles V2
	 */
	if (ab->hw_rev == ATH12K_HW_QCN9625_HW10)
		val &= ~HAL_REO1_MISC_CFG_1_STOP_DELINKING_DURING_CORRUPTION;
	val &= ~HAL_REO1_MISC_CFG_1_REO_MSDU_FETCH_OPTIMIZE;
	val &= ~HAL_REO1_MISC_CFG_1_REO_MSDU_LINK_SHARING_EN;
	ath12k_hif_write32(ab, reo_base + HAL_REO1_MISC_CFG_1, val);

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_MISC_CFG_2);
	val |= u32_encode_bits(1, HAL_REO1_MISC_CFG_2_BAR_REO_ERR_DELINK_ENABLE);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_MISC_CFG_2, val);

	/* disable cookie conversion for mgmt and REO2SW0 rings*/
	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_COOKIE_CONV_EN_RING);
	val &= ~HAL_REO1_COOKIE_CONV_REO2SW0_EN;
	val &= ~HAL_REO1_COOKIE_CONV_REO2SW8_EN;
	val &= ~HAL_REO1_COOKIE_CONV_REO2SW9_EN;
	val &= ~HAL_REO1_COOKIE_CONV_REO2SW11_EN;
	ath12k_hif_write32(ab, reo_base + HAL_REO1_COOKIE_CONV_EN_RING, val);

	/* Configure error ring RDs for mgmt rings */
	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_ERROR_RING_CFG_FOR_DEST_IX1);
	val |= u32_encode_bits(8, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2PPE_ERR);
	val |= u32_encode_bits(8, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW5_ERR);
	val |= u32_encode_bits(8, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW6_ERR);
	val |= u32_encode_bits(10, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW8_ERR);
	val |= u32_encode_bits(10, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW9_ERR);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_ERROR_RING_CFG_FOR_DEST_IX1, val);

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_ERROR_RING_CFG_FOR_DEST_IX2);
	val |= u32_encode_bits(10, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW11_ERR);
	val |= u32_encode_bits(8, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2PPE1_ERR);
	val |= u32_encode_bits(8, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2PPE2_ERR);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_ERROR_RING_CFG_FOR_DEST_IX2, val);

	/* Configure REO Error ring RDI to REO2SW6.*/
	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_ERROR_RING_CFG_FOR_DEST_IX0);
	val |= u32_encode_bits(0, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW0_ERR);
	val |= u32_encode_bits(8, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW1_ERR);
	val |= u32_encode_bits(8, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW2_ERR);
	val |= u32_encode_bits(8, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW3_ERR);
	val |= u32_encode_bits(8, HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW4_ERR);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_ERROR_RING_CFG_FOR_DEST_IX0, val);

	/* Configure FRAG Error ring RDI to REO2SW6.*/
	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_IX0);
	val |= u32_encode_bits(0, HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW0_ERR);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW1_ERR);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW2_ERR);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW3_ERR);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW4_ERR);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_IX0, val);

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_IX1);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW5_ERR);
	val |= u32_encode_bits(10, HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW8_ERR);
	val |= u32_encode_bits(10, HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW9_ERR);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2PPE_ERR);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_IX1, val);

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_IX2);
	val |= u32_encode_bits(10, HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW11_ERR);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2PPE1_ERR);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2PPE2_ERR);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_IX2, val);

	/* Configure BAR Error ring RDI to REO2SW6.*/
	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_IX0);
	val |= u32_encode_bits(0, HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2SW0_ERR);
	val |= u32_encode_bits(8, HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2SW1_ERR);
	val |= u32_encode_bits(8, HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2SW2_ERR);
	val |= u32_encode_bits(8, HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2SW3_ERR);
	val |= u32_encode_bits(8, HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2SW4_ERR);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_IX0, val);

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_IX1);
	val |= u32_encode_bits(8, HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2SW5_ERR);
	val |= u32_encode_bits(8, HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2PPE_ERR);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_IX1, val);

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_IX2);
	val |= u32_encode_bits(8, HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2PPE1_ERR);
	val |= u32_encode_bits(8, HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2PPE2_ERR);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_IX2, val);

	/* Configure RxDMA Error ring RDI to REO2SW6.*/
	val = ath12k_hif_read32(ab, reo_base +
				HAL_REO1_RXDMA_ERROR_DESTINATION_MAPPING_AP_IX0);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_0);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_1);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_2);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_3);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_4);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_5);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_RXDMA_ERROR_DESTINATION_MAPPING_AP_IX0,
			   val);

	val = ath12k_hif_read32(ab, reo_base +
				HAL_REO1_RXDMA_ERROR_DESTINATION_MAPPING_AP_IX1);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_6);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_7);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_8);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_9);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_10);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_11);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_RXDMA_ERROR_DESTINATION_MAPPING_AP_IX1,
			   val);

	val = ath12k_hif_read32(ab, reo_base +
				HAL_REO1_RXDMA_ERROR_DESTINATION_MAPPING_AP_IX2);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_12);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_13);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_14);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_15);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_16);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_17);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_RXDMA_ERROR_DESTINATION_MAPPING_AP_IX2,
			   val);

	val = ath12k_hif_read32(ab, reo_base +
				HAL_REO1_RXDMA_ERROR_DESTINATION_MAPPING_AP_IX3);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_18);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_19);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_20);
	val |= u32_encode_bits(8, HAL_REO1_RXDMA_ERROR_DESTINATION_RING_OTHER);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_RXDMA_ERROR_DESTINATION_MAPPING_AP_IX3,
			   val);

	/* Configure FRAG ring RDI to REO2SW6.*/
	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_FRAG_MAP_IX_0);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_MAP_REO2SW0);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_MAP_REO2SW1);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_MAP_REO2SW2);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_MAP_REO2SW3);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_MAP_REO2SW4);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_FRAG_MAP_IX_0, val);

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_FRAG_MAP_IX_1);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_MAP_REO2SW5);
	val |= u32_encode_bits(10, HAL_REO1_FRAG_MAP_REO2SW8);
	val |= u32_encode_bits(10, HAL_REO1_FRAG_MAP_REO2SW9);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_MAP_REO2PPE);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_FRAG_MAP_IX_1, val);

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_FRAG_MAP_IX_2);
	val |= u32_encode_bits(10, HAL_REO1_FRAG_MAP_REO2SW11);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_MAP_REO2PPE1);
	val |= u32_encode_bits(8, HAL_REO1_FRAG_MAP_REO2PPE2);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_FRAG_MAP_IX_2, val);

	val = ath12k_hif_read32(ab, reo_base + HAL_REO_MISC_CTL_AP);
	val |= u32_encode_bits(8, HAL_REO_MISC_CTL_BAR_DEST_RING_AP);
	ath12k_hif_write32(ab, reo_base + HAL_REO_MISC_CTL_AP, val);

	/* Route all RD rings to REO2SW_ROAMING1 (encoding 0) when roaming
	 * commands are issued via REO_CMD1 ring.
	 *
	 * REO/frag/BAR/RXDMA error packets keep using the dedicated error-ring
	 * mappings programmed above. ROAMING1 is only used for the standard
	 * REO destination descriptors flushed by the roaming command path.
	 */
	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_ROAMING_DEST_CFG);
	val &= ~HAL_REO1_ROAMING_DEST_CFG_MASK;
	ath12k_hif_write32(ab, reo_base + HAL_REO1_ROAMING_DEST_CFG, val);

	/* Configure descriptor type for ROAMING1 ring:
	 * REO_DESTINATION_RING descriptor (bit 0 = 0)
	 */
	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_DESCRIPTOR_TYPE_ROAMING);
	val &= ~HAL_REO1_DESC_TYPE_PPE_FOR_SW_ROAMING1;
	ath12k_hif_write32(ab, reo_base + HAL_REO1_DESCRIPTOR_TYPE_ROAMING, val);

	/* Enable backpressure-drop for REO2SW and REO2PPE rings.
	 *
	 * Register: UMAC_REO_R0_BACKPRESSURE_DROP_EN (offset 0x1C98,
	 *           full address 0xF23C98, reset state 0x00000000)
	 *
	 * When a bit is set, REO detects backpressure for that ring and
	 * routes packets to the buffer-release ring (configured via
	 * BACKPRESSURE_BUFFER_RELEASE_RING_SELECT) instead of stalling.
	 * This prevents the Reo_dst ring from filling up (Ring Usage=100%)
	 * and causing a complete RX stall.
	 *
	 * Bits enabled:
	 *   REO2SW0..REO2SW11 (bits  0-11) : all SW destination rings
	 *   REO2PPE, REO2PPE1, REO2PPE2    : PPE offload rings
	 */
	val = ath12k_hif_read32(ab, reo_base + HAL_REO_BACKPRESSURE_DROP_EN);
	val |= HAL_REO_BP_DROP_REO2SW_ALL | HAL_REO_BP_DROP_REO2PPE_ALL;
	ath12k_hif_write32(ab, reo_base + HAL_REO_BACKPRESSURE_DROP_EN, val);

	/* Configure buffer-release ring selection for backpressure-dropped packets.
	 *
	 * Register IX_0: UMAC_REO_R0_BACKPRESSURE_BUFFER_RELEASE_RING_SELECT_IX_0
	 *   (offset 0x1C9C, full address 0xF23C9C, reset state 0x00000000)
	 *   Each 2-bit field selects the buffer-release ring for the corresponding
	 *   REO destination ring. Covers REO2SW0..REO2SW11, REO2FW, REO2FW_MGMT,
	 *   REO2PPE, REO2PPE1.
	 *
	 * Register IX_1: UMAC_REO_R0_BACKPRESSURE_BUFFER_RELEASE_RING_SELECT_IX_1
	 *   (offset 0x1CA0, full address 0xF23CA0, reset state 0x00000000)
	 *   Covers REO2PPE2 only (bits 1:0).
	 *
	 * Routing policy:
	 *   REO2SW rings  → always SW0 buffer pool (HAL_REO_BP_REL_RING_SW0 = 0)
	 *   REO2PPE rings → SW0 by default; dedicated DS pool
	 *                   (HAL_REO_BP_REL_RING_DS = 2) when
	 *                   dp_ppe2wbm_use_dedicated_pool is enabled
	 */

	u8 ppe_ring = dp_wifi8->dp_ppe2wbm_use_dedicated_pool ?
		      HAL_REO_BP_REL_RING_DS : HAL_REO_BP_REL_RING_SW0;
	ath12k_hif_write32(ab,
			   reo_base +
			   HAL_REO_BACKPRESSURE_BUFFER_RELEASE_RING_SELECT_IX_0,
			   HAL_REO_BP_REL_SEL_IX0_VAL(ppe_ring));

	ath12k_hif_write32(ab,
			   reo_base +
			   HAL_REO_BACKPRESSURE_BUFFER_RELEASE_RING_SELECT_IX_1,
			   HAL_REO_BP_REL_SEL_IX1_VAL(ppe_ring));
}

void ath12k_wifi8_hal_reo_shared_qaddr_cache_clear(struct ath12k_base *ab)
{
	struct ath12k_hal *hal = &ab->hal;
	u32 val;

	lockdep_assert_held(&ab->base_lock);

	if (!ab->hw_params->reoq_lut_support)
		return;

	if (test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
		return;

	val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_REO_REG +
				HAL_REO1_QDESC_ADDR(hal));

	val |= u32_encode_bits(1, HAL_REO_QDESC_ADDR_READ_CLEAR_QDESC_ARRAY);
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_REO_REG +
			   HAL_REO1_QDESC_ADDR(hal), val);

	val &= ~HAL_REO_QDESC_ADDR_READ_CLEAR_QDESC_ARRAY;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_REO_REG +
			   HAL_REO1_QDESC_ADDR(hal), val);
}

#define PMM_REG_OFFSET  4

static void ath12k_hal_get_tsf_reg(u8 mac_id, enum hal_scratch_reg_enum *tsf_enum_low,
				   enum hal_scratch_reg_enum *tsf_enum_hi)
{
	if (!mac_id) {
		*tsf_enum_low = PMM_MAC0_TSF2_OFFSET_LO_US;
		*tsf_enum_hi = PMM_MAC0_TSF2_OFFSET_HI_US;
	} else if (mac_id == 1) {
		*tsf_enum_low = PMM_MAC1_TSF2_OFFSET_LO_US;
		*tsf_enum_hi = PMM_MAC1_TSF2_OFFSET_HI_US;
	}
}

void ath12k_hal_qcn9625_get_tsf2_scratch_reg(struct ath12k_base *ab,
					     u8 mac_id, u64 *value)
{
	u32 pmm_reg_base = HAL_PMM_REG_BASE(&ab->hal);
	enum hal_scratch_reg_enum enum_lo, enum_hi;
	u32 offset_lo, offset_hi;

	ath12k_hal_get_tsf_reg(mac_id, &enum_lo, &enum_hi);

	/* This needs to be fixed for qcn9625 */
	if (ab->hif.ops->pmm_read32) {
		offset_lo = ath12k_hif_pmm_read32(ab, ATH12K_PPT_ADDR_OFFSET(enum_lo));
		offset_hi = ath12k_hif_pmm_read32(ab, ATH12K_PPT_ADDR_OFFSET(enum_hi));
	} else if (ab->hw_rev == ATH12K_HW_QCN6432_HW10) {
		offset_lo = ath12k_hif_cmem_read32(ab, pmm_reg_base +
						   ATH12K_PPT_ADDR_OFFSET(enum_lo));
		offset_hi = ath12k_hif_cmem_read32(ab, pmm_reg_base +
						   ATH12K_PPT_ADDR_OFFSET(enum_hi));
	} else {
		offset_lo = ath12k_hif_read32(ab, pmm_reg_base +
					      ATH12K_PPT_ADDR_OFFSET(enum_lo));
		offset_hi = ath12k_hif_read32(ab, pmm_reg_base +
					      ATH12K_PPT_ADDR_OFFSET(enum_hi));
	}

	*value = ((u64)(offset_hi) << 32 | offset_lo);
}

void ath12k_hal_qcn9625_get_tqm_scratch_reg(struct ath12k_base *ab, u64 *value)
{
	u32 pmm_reg_base = HAL_PMM_REG_BASE(&ab->hal);
	u32 offset_lo, offset_hi;

	/* This needs to be fixed for qcn9625 */
	if (ab->hif.ops->pmm_read32) {
		offset_lo = ath12k_hif_pmm_read32(ab, ATH12K_PPT_ADDR_OFFSET(PMM_TQM_CLOCK_OFFSET_LO_US));
		offset_hi = ath12k_hif_pmm_read32(ab, ATH12K_PPT_ADDR_OFFSET(PMM_TQM_CLOCK_OFFSET_HI_US));
	} else if (ab->hw_rev == ATH12K_HW_QCN6432_HW10) {
		offset_lo = ath12k_hif_cmem_read32(ab, pmm_reg_base +
						   ATH12K_PPT_ADDR_OFFSET(PMM_TQM_CLOCK_OFFSET_LO_US));
		offset_hi = ath12k_hif_cmem_read32(ab, pmm_reg_base +
						   ATH12K_PPT_ADDR_OFFSET(PMM_TQM_CLOCK_OFFSET_HI_US));
	} else {
		offset_lo = ath12k_hif_read32(ab, pmm_reg_base +
					      ATH12K_PPT_ADDR_OFFSET(PMM_TQM_CLOCK_OFFSET_LO_US));
		offset_hi = ath12k_hif_read32(ab, pmm_reg_base +
					      ATH12K_PPT_ADDR_OFFSET(PMM_TQM_CLOCK_OFFSET_HI_US));
	}

	*value = ((u64)(offset_hi) << 32 | offset_lo);
}

static void
ath12k_wifi8_key_bitwise_left_shift(u8 *key, int length, int shift)
{
	int i;
	int next;

	while (shift--) {
		for (i = length - 1; i >= 0; i--) {
			if (i > 0)
				next = (key[i - 1] & 0x80 ? 1 : 0);
			else
				next = 0;

			key[i] = (key[i] << 1) | next;
		}
	}
}

static void
ath12k_wifi8_reverse_key(u8 *dest, const u8 *src, int length)
{
	int i, j;

	for (i = 0, j = length  - 1; i < length; i++, j--)
		dest[i] = src[j];
}

static void
ath12k_wifi8_hal_fst_key_configure(struct hal_rx_fst *fst)
{
	u8 key[HAL_FST_HASH_KEY_SIZE_BYTES];

	memcpy(key, fst->key, HAL_FST_HASH_KEY_SIZE_BYTES);

	ath12k_wifi8_key_bitwise_left_shift(key, HAL_FST_HASH_KEY_SIZE_BYTES, 5);
	ath12k_wifi8_reverse_key(fst->shifted_key, key, HAL_FST_HASH_KEY_SIZE_BYTES);
}

static void
ath12k_wifi8_hal_flow_toeplitz_create_cache(struct hal_rx_fst *fst)
{
	int bit;
	int value;
	int i;
	u8 *key = fst->shifted_key;
	u32 current_key = (key[0] << 24) | (key[1] << 16) | (key[2] << 8) |
		key[3];

	for (i = 0; i < HAL_FST_HASH_KEY_SIZE_BYTES; i++) {
		u8 new_key;
		u32 shifted_key[8];

		if (i + 4 < HAL_FST_HASH_KEY_SIZE_BYTES)
			new_key = key[i + 4];
		else
			new_key = 0;

		shifted_key[0] = current_key;

		for (bit = 1; bit < 8; bit++)
			shifted_key[bit] = current_key << bit | new_key >> (8 - bit);

		for (value = 0; value < (1 << 8); value++) {
			u32 hash = 0;
			int mask;

			for (bit = 0, mask = 1 << 7; bit < 8; bit++, mask >>= 1)
				if ((value & mask))
					hash ^= shifted_key[bit];

			fst->key_cache[i][value] = hash;
		}

		current_key = current_key << 8 | new_key;
	}
}

u32 ath12k_wifi8_hal_flow_toeplitz_hash(struct ath12k_base *ab,
					struct hal_rx_fst *fst,
					struct hal_flow_tuple_info *tuple_info)
{
	int i, j;
	u32 hash = 0;
	u32 input[HAL_FST_HASH_KEY_SIZE_WORDS] = {0};
	u8 *tuple;

	input[0] = htonl(tuple_info->src_ip_127_96);
	input[1] = htonl(tuple_info->src_ip_95_64);
	input[2] = htonl(tuple_info->src_ip_63_32);
	input[3] = htonl(tuple_info->src_ip_31_0);
	input[4] = htonl(tuple_info->dest_ip_127_96);
	input[5] = htonl(tuple_info->dest_ip_95_64);
	input[6] = htonl(tuple_info->dest_ip_63_32);
	input[7] = htonl(tuple_info->dest_ip_31_0);
	input[8] = (tuple_info->dest_port << 16) | (tuple_info->src_port);
	input[9] = tuple_info->l4_protocol;

	tuple = (u8 *)input;

	for (i = 0, j = HAL_FST_HASH_DATA_SIZE - 1;
	     i < HAL_FST_HASH_KEY_SIZE_BYTES && j >= 0; i++, j--)
		hash ^= fst->key_cache[i][tuple[j]];

	hash >>= 12;
	hash &= (fst->max_entries - 1);

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Hash value %u\n", hash);

	return hash;
}

u32 ath12k_wifi8_hal_rx_get_trunc_hash(struct hal_rx_fst *fst, u32 hash)
{
	if (hash >= fst->max_entries)
		hash &= (fst->max_entries - 1);

	return hash;
}

void *ath12k_wifi8_hal_rx_flow_get_tuple_info(struct ath12k_base *ab,
					      struct hal_rx_fst *fst,
					      u32 hal_hash,
					      struct hal_flow_tuple_info *tuple_info)
{
	struct hal_rx_fse *hal_fse = &fst->base_vaddr[hal_hash];

	if (!hal_fse || !tuple_info)
		return NULL;

	if (u32_get_bits(hal_fse->info2, HAL_RX_FSE_VALID) == 0)
		return NULL;

	tuple_info->src_ip_127_96 = ntohl(hal_fse->src_ip_127_96);
	tuple_info->src_ip_95_64 = ntohl(hal_fse->src_ip_95_64);
	tuple_info->src_ip_63_32 = ntohl(hal_fse->src_ip_63_32);
	tuple_info->src_ip_31_0 = ntohl(hal_fse->src_ip_31_0);
	tuple_info->dest_ip_127_96 = ntohl(hal_fse->dest_ip_127_96);
	tuple_info->dest_ip_95_64 = ntohl(hal_fse->dest_ip_95_64);
	tuple_info->dest_ip_63_32 = ntohl(hal_fse->dest_ip_63_32);
	tuple_info->dest_ip_31_0 = ntohl(hal_fse->dest_ip_31_0);
	tuple_info->dest_port =
		u32_get_bits(hal_fse->info1, HAL_RX_FSE_DEST_PORT);
	tuple_info->src_port =
		u32_get_bits(hal_fse->info1, HAL_RX_FSE_SRC_PORT);
	tuple_info->l4_protocol =
		u32_get_bits(hal_fse->info2, HAL_RX_FSE_L4_PROTOCOL);

	return hal_fse;
}

int ath12k_wifi8_hal_rx_find_flow_from_tuple(struct ath12k_base *ab,
					     struct hal_rx_fst *fst,
					     u32 flow_hash,
					     void *flow_tuple_info,
					     u32 *flow_idx)
{
	int i;
	void *hal_fse = NULL;
	u32 hal_hash = 0;
	struct hal_flow_tuple_info hal_tuple_info = { 0 };

	for (i = 0; i < fst->max_skid_length; i++) {
		hal_hash = ath12k_wifi8_hal_rx_get_trunc_hash(fst, (flow_hash + i));

		hal_fse = ath12k_wifi8_hal_rx_flow_get_tuple_info(ab, fst, hal_hash,
								  &hal_tuple_info);
		if (!hal_fse)
			continue;

		/* Find the matching flow entry in HW FST */
		if (!memcmp(&hal_tuple_info, flow_tuple_info,
			    sizeof(struct hal_flow_tuple_info)))
			break;
	}

	if (i == fst->max_skid_length) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Max skid length reached for hash %u",
			   flow_hash);
		return -ERANGE;
	}

	*flow_idx = hal_hash;
	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "flow_hash = %u, skid_entry = %d, flow_addr = %pK flow_idx = %d",
		   flow_hash, i, hal_fse, *flow_idx);

	return 0;
}

ssize_t ath12k_wifi8_hal_rx_dump_fst_table(struct ath12k_base *ab,
					   struct hal_rx_fst *fst,
					   char *buf, int size)
{
	int i;
	struct hal_rx_fse *hal_fse;
	int len = 0;

	if (!fst) {
		ath12k_warn(ab, "FST table is NULL\n");
		return -ENODEV;
	}

	for (i = 0; i < fst->max_entries; i++) {
		hal_fse = &fst->base_vaddr[i];

		if (!u32_get_bits(hal_fse->info2, HAL_RX_FSE_VALID))
			continue;

		len += scnprintf(buf + len, size - len, "Index: %d\n", i);
		len += scnprintf(buf + len, size - len,
				 "src_ip: 0x%x 0x%x 0x%x 0x%x\n",
				 hal_fse->src_ip_127_96,
				 hal_fse->src_ip_95_64,
				 hal_fse->src_ip_63_32,
				 hal_fse->src_ip_31_0);
		len += scnprintf(buf + len, size - len,
				 "dest_ip: 0x%x 0x%x 0x%x 0x%x\n",
				 hal_fse->dest_ip_127_96,
				 hal_fse->dest_ip_95_64,
				 hal_fse->dest_ip_63_32,
				 hal_fse->dest_ip_31_0);
		len += scnprintf(buf + len, size - len,
				 "src_port: 0x%x dest port: 0x%x\n",
				 u32_get_bits(hal_fse->info1, HAL_RX_FSE_SRC_PORT),
				 u32_get_bits(hal_fse->info1, HAL_RX_FSE_DEST_PORT));
		len += scnprintf(buf + len, size - len,
				"protocol: 0x%x\n",
				u32_get_bits(hal_fse->info2, HAL_RX_FSE_L4_PROTOCOL));
		len += scnprintf(buf + len, size - len,
				"reo_indication: 0x%x reo_handler: 0x%x\n",
				u32_get_bits(hal_fse->info2, HAL_RX_FSE_REO_INDICATION),
				u32_get_bits(hal_fse->info2,
					     HAL_RX_FSE_REO_DESTINATION_HANDLER));
		len += scnprintf(buf + len, size - len,
				"drop 0x%x service_code 0x%x\n",
				u32_get_bits(hal_fse->info2, HAL_RX_FSE_MSDU_DROP),
				u32_get_bits(hal_fse->info2, HAL_RX_FSE_SERVICE_CODE));
		len += scnprintf(buf + len, size - len, "metadata: 0x%x\n\n",
				 hal_fse->metadata);
	}

	return len;
}

static const u8 ath12k_wifi8_hal_rx_fst_toeplitz_key[HAL_RX_FST_TOEPLITZ_KEYLEN] = {
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa
};

struct hal_rx_fst *
ath12k_wifi8_hal_rx_fst_attach(struct ath12k_base *ab)
{
	struct hal_rx_fst *fst;
	u32 alloc_size;

	fst = kzalloc(sizeof(*fst), GFP_KERNEL);
	if (!fst)
		return NULL;

	fst->key = (u8 *)ath12k_wifi8_hal_rx_fst_toeplitz_key;
	fst->max_skid_length = HAL_RX_FST_MAX_SEARCH;
	fst->max_entries = HAL_RX_FLOW_SEARCH_TABLE_SIZE;
	fst->fst_entry_size = HAL_RX_FST_ENTRY_SIZE;
	alloc_size = fst->max_entries * fst->fst_entry_size;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "HAL FST allocation %pK entries %u entry size %u alloc_size %u\n",
		   fst, fst->max_entries, fst->fst_entry_size, alloc_size);

	fst->base_vaddr = ath12k_hal_dma_alloc_coherent(ab->dev, alloc_size,
							&fst->base_paddr, GFP_KERNEL);
	if (!fst->base_vaddr) {
		kfree(fst);
		return NULL;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "hal_rx_fst base address 0x%pK",
		   (void *)fst->base_paddr);

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_FST, NULL, "FST Key: ",
			(void *)fst->key, HAL_FST_HASH_KEY_SIZE_BYTES);

	memset((u8 *)fst->base_vaddr, 0, alloc_size);
	ath12k_wifi8_hal_fst_key_configure(fst);
	ath12k_wifi8_hal_flow_toeplitz_create_cache(fst);

	return fst;
}

void ath12k_wifi8_hal_rx_fst_detach(struct ath12k_base *ab, struct hal_rx_fst *fst)
{
	if (!fst)
		return;

	if (fst->base_vaddr)
		ath12k_hal_dma_free_coherent(ab->dev,
					     (fst->max_entries * fst->fst_entry_size),
					     fst->base_vaddr, fst->base_paddr);
	kfree(fst);
}

int ath12k_wifi8_hal_rx_flow_insert_entry(struct ath12k_base *ab,
					  struct hal_rx_fst *fst,
					  u32 flow_hash,
					  void *flow_tuple_info,
					  u32 *flow_idx)
{
	int i;
	void *hal_fse = NULL;
	u32 hal_hash = 0;
	struct hal_flow_tuple_info hal_tuple_info = { 0 };

	for (i = 0; i < fst->max_skid_length; i++) {
		hal_hash = ath12k_wifi8_hal_rx_get_trunc_hash(fst, (flow_hash + i));

		hal_fse = ath12k_wifi8_hal_rx_flow_get_tuple_info(ab, fst, hal_hash,
								  &hal_tuple_info);
		if (!hal_fse)
			break;

		/* Find the matching flow entry in HW FST */
		if (!memcmp(&hal_tuple_info, flow_tuple_info,
			    sizeof(struct hal_flow_tuple_info))) {
			ath12k_err(ab,
				   "Flow already exists in FST %u at skid %u\n",
				   hal_hash, i);
			*flow_idx = hal_hash;
			return -EEXIST;
		}
	}

	if (i == fst->max_skid_length) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Max skid length reached for hash %u",
			   flow_hash);
		return -ERANGE;
	}

	*flow_idx = hal_hash;
	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "flow_hash = %u, skid_entry = %d, flow_addr = %pK flow_idx = %d",
		    flow_hash, i, hal_fse, *flow_idx);

	return 0;
}

void *ath12k_wifi8_hal_rx_flow_setup_fse(struct ath12k_base *ab,
					 struct hal_rx_fst *fst,
					 u32 table_offset,
					 struct hal_rx_flow *flow)
{
	struct hal_rx_fse *hal_fse;

	if (table_offset >= fst->max_entries) {
		ath12k_err(ab, "HAL FSE table offset %u exceeds max entries %u",
			   table_offset, fst->max_entries);
		return NULL;
	}

	hal_fse = &fst->base_vaddr[table_offset];

	hal_fse->src_ip_127_96 = htonl(flow->tuple_info.src_ip_127_96);
	hal_fse->src_ip_95_64 = htonl(flow->tuple_info.src_ip_95_64);
	hal_fse->src_ip_63_32 = htonl(flow->tuple_info.src_ip_63_32);
	hal_fse->src_ip_31_0 = htonl(flow->tuple_info.src_ip_31_0);
	hal_fse->dest_ip_127_96 = htonl(flow->tuple_info.dest_ip_127_96);
	hal_fse->dest_ip_95_64 = htonl(flow->tuple_info.dest_ip_95_64);
	hal_fse->dest_ip_63_32 = htonl(flow->tuple_info.dest_ip_63_32);
	hal_fse->dest_ip_31_0 = htonl(flow->tuple_info.dest_ip_31_0);
	hal_fse->metadata |= flow->fse_metadata;
	hal_fse->msdu_byte_count = 0;
	hal_fse->timestamp = flow->timestamp;

	hal_fse->info1 = u32_encode_bits(flow->tuple_info.dest_port,
					 HAL_RX_FSE_DEST_PORT);
	hal_fse->info1 |= u32_encode_bits(flow->tuple_info.src_port,
					  HAL_RX_FSE_SRC_PORT);

	hal_fse->info2 = u32_encode_bits(flow->tuple_info.l4_protocol,
					 HAL_RX_FSE_L4_PROTOCOL);
	hal_fse->info2 |= u32_encode_bits(1, HAL_RX_FSE_VALID);
	hal_fse->info2 |= u32_encode_bits(flow->service_code, HAL_RX_FSE_SERVICE_CODE);
	hal_fse->info2 |= u32_encode_bits(flow->reo_indication,
					  HAL_RX_FSE_REO_INDICATION);
	hal_fse->info2 |= u32_encode_bits(flow->drop, HAL_RX_FSE_MSDU_DROP);
	hal_fse->info2 |= u32_encode_bits(flow->reo_destination_handler,
					  HAL_RX_FSE_REO_DESTINATION_HANDLER);
	hal_fse->info3 = 0;

	/* Program info4 fields if provided */
	hal_fse->info4 = u32_encode_bits(flow->dest_info,
					 HAL_RX_FSE_DEST_INFO);
	hal_fse->info4 |= u32_encode_bits(flow->dest_info_valid,
					  HAL_RX_FSE_DEST_INFO_VALID);
	hal_fse->info4 |= u32_encode_bits(flow->int_priority,
					  HAL_RX_FSE_INT_PRIORITY);
	hal_fse->info4 |= u32_encode_bits(flow->int_priority_valid,
					  HAL_RX_FSE_INT_PRIORITY_VALID);
	hal_fse->info4 |= u32_encode_bits(flow->ppe_classify_read_hint,
					  HAL_RX_FSE_PPE_CLASSIFY_READ_HINT);
	hal_fse->info4 |= u32_encode_bits(flow->c_tdma_lut_ptr,
					  HAL_RX_FSE_C_TDMA_LUT_PTR);
	hal_fse->info4 |= u32_encode_bits(flow->ll_pkt, HAL_RX_FSE_LL_PKT);
	hal_fse->info4 |= u32_encode_bits(flow->rx_sdwf_policer_id,
					  HAL_RX_FSE_RX_SDWF_POLICER_ID);

	/* Program info5 fields if provided */
	hal_fse->info5 = u32_encode_bits(flow->telemetry_stream_id_valid,
					 HAL_RX_FSE_TELEMETRY_STREAM_ID_VALID);
	hal_fse->info5 |= u32_encode_bits(flow->telemetry_stream_id,
					  HAL_RX_FSE_TELEMETRY_STREAM_ID);
	hal_fse->info5 |= u32_encode_bits(flow->sw_peer_id_check_enable,
					  HAL_RX_FSE_SW_PEER_ID_CHECK_ENABLE);
	hal_fse->info5 |= u32_encode_bits(flow->sw_peer_id,
					  HAL_RX_FSE_SW_PEER_ID);
	hal_fse->info5 |= u32_encode_bits(flow->rx_sdwf_priority,
					  HAL_RX_FSE_RX_SDWF_PRIORITY);

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_FST, NULL, "Hal FSE setup:",
			hal_fse, sizeof(*hal_fse));

	return hal_fse;
}

void ath12k_wifi8_hal_rx_flow_delete_entry(struct ath12k_base *ab,
					   struct hal_rx_fse *hal_fse)
{
	if (!u32_get_bits(hal_fse->info2, HAL_RX_FSE_VALID)) {
		ath12k_err(ab, "HAL FSE %pK is invalid", hal_fse);
	} else {
		hal_fse->info2 = u32_encode_bits(0, HAL_RX_FSE_VALID);
		/*
		 * Clear the metadata to avoid it being used incorrectly during FSE create
		 */
		hal_fse->metadata = 0;
	}
}

void ath12k_wifi8_hal_reset_rx_reo_tid_q(void *vaddr,
					 u32 ba_window_size, u8 tid)
{
	struct hal_rx_reo_queue *qdesc = (struct hal_rx_reo_queue *)vaddr;
	struct hal_rx_reo_queue_ext *ext_desc;
	u32 size, info0, rx_queue_num;

	size = ath12k_wifi8_hal_reo_qdesc_size(ba_window_size, tid);

	rx_queue_num = qdesc->receive_queue_number;
	info0 = le32_to_cpu(qdesc->info0);

	memset(qdesc, 0, size);

	ath12k_wifi8_hal_reo_set_desc_hdr(&qdesc->desc_hdr, HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_0);

	qdesc->receive_queue_number = rx_queue_num;
	qdesc->info0 = cpu_to_le32(info0);

	qdesc->info1 = le16_encode_bits(0, HAL_RX_REO_QUEUE_INFO1_SVLD) |
			le16_encode_bits(0, HAL_RX_REO_QUEUE_INFO1_SSN);

	if (ath12k_wifi8_hal_is_reo_nonqos_mgmt_tid(tid))
		return;

	ext_desc = qdesc->ext_desc;
	memset(ext_desc, 0, REO_QUEUE_EXT_DESC_MAX * sizeof(*ext_desc));

	ath12k_wifi8_hal_reo_set_desc_hdr(&ext_desc->desc_hdr, HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_1);
	ext_desc++;

	ath12k_wifi8_hal_reo_set_desc_hdr(&ext_desc->desc_hdr, HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_2);
	ext_desc++;

	ath12k_wifi8_hal_reo_set_desc_hdr(&ext_desc->desc_hdr, HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_3);
	ext_desc++;

	ath12k_wifi8_hal_reo_set_desc_hdr(&ext_desc->desc_hdr, HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_4);
	ext_desc++;

	ath12k_wifi8_hal_reo_set_desc_hdr(&ext_desc->desc_hdr, HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_5);
}

int ath12k_wifi8_hal_fse_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				  struct hal_fse_cmd *fse_cmd)
{
	struct hal_fse_cmd *fse_desc;
	int ret = 0;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FSE CMD send: ring_id=%u hp=%u tp=%u",
		    srng->ring_id, srng->u.src_ring.hp,
		    srng->u.src_ring.tp_addr ? *srng->u.src_ring.tp_addr : 0);

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);
	fse_desc =
		(struct hal_fse_cmd *)ath12k_hal_srng_src_get_next_entry(ab, srng);

	if (!fse_desc) {
		ret = -ENOBUFS;
		ath12k_dbg(ab, ATH12K_DBG_DP_FST,
			   "FSE CMD send: no space in ring_id=%u new_hp=%u new_tp=%u",
			   srng->ring_id, srng->u.src_ring.hp,
			   srng->u.src_ring.tp_addr ? *srng->u.src_ring.tp_addr : 0);
		goto out;
	}
	memcpy(fse_desc, fse_cmd, sizeof(*fse_cmd));
	ath12k_dbg_dump(ab, ATH12K_DBG_DP_FST, NULL, "FSE CMD:",
			fse_cmd, sizeof(*fse_cmd));

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FSE CMD send: done ring_id=%u ret=%d new_hp=%u new_tp=%u",
		   srng->ring_id, ret, srng->u.src_ring.hp,
		   srng->u.src_ring.tp_addr ? *srng->u.src_ring.tp_addr : 0);

out:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return ret;
}

int
ath12k_wifi8_hal_invalidate_rx_cache_cmd_send(struct ath12k_base *ab,
					      struct hal_srng *srng,
					      struct ath12k_hal_rx_cmd_ring_param *param)
{
	struct hal_ase_cmd *ase_cmd;
	int ret = 0;

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		return -ENOBUFS;

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);
	ase_cmd = ath12k_hal_srng_src_get_next_entry(ab, srng);
	if (!ase_cmd) {
		ret = -ENOBUFS;
		goto out;
	}
	memset(ase_cmd, 0x0, sizeof(*ase_cmd));

	ase_cmd->info0 = le16_encode_bits(param->hw_link_bitmap,
					  HAL_ASE_CMD_RING_INFO0_CMD_TO_CHIP_0) |
			 le16_encode_bits(param->hw_link_bitmap,
					  HAL_ASE_CMD_RING_INFO0_CMD_TO_CHIP_1) |
			 le16_encode_bits(param->hw_link_bitmap,
					  HAL_ASE_CMD_RING_INFO0_CMD_TO_CHIP_2) |
			 le16_encode_bits(param->hw_link_bitmap,
					  HAL_ASE_CMD_RING_INFO0_CMD_TO_CHIP_3) |
			 le16_encode_bits(param->hw_link_bitmap,
					  HAL_ASE_CMD_RING_INFO0_CMD_TO_CHIP_4);
	ase_cmd->mac_addr_31_0 = cpu_to_le32(param->mac_addr_31_0);
	ase_cmd->mac_addr_47_32 = cpu_to_le16(param->mac_addr_47_32);
	ase_cmd->info1 = le16_encode_bits(param->is_mcast,
					  HAL_ASE_CMD_RING_INFO1_IS_MCAST) |
			 le16_encode_bits(param->is_mec,
					  HAL_ASE_CMD_RING_INFO1_IS_MEC) |
			 le16_encode_bits(param->ad1_match,
					  HAL_ASE_CMD_RING_INFO1_AD1_MATCH) |
			 le16_encode_bits(param->link_id,
					  HAL_ASE_CMD_RING_INFO1_LINK_ID) |
			 le16_encode_bits(param->cmd_num,
					  HAL_ASE_CMD_RING_INFO1_GSE_CTRL);
	ase_cmd->cmd_meta_data_31_0 = cpu_to_le32(param->meta_data_0);
out:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return ret;
}

#ifdef CPTCFG_EXT_IPA_OFFLOAD
/**
 * ath12k_wifi8_hal_reo_ring_ipa_ctrl_hash_ix0_setup() - Set IPA RDI slot in IX_0
 *
 * All data peers are programmed with ATH12K_DP_WIFI8_REO2SW5_RDI (=5) via
 * WMI_PEER_SET_DEFAULT_ROUTING (DP_RX_HASH_ENABLE=0 for IPA builds, so RDI
 * mode is active).  Only slot 5 in IX_0 needs to point to SW5 (reo2sw5).
 *
 * Slots 0-4 are left as-is: slot 0 (error/exception) stays SW0 → host, and
 * slots 1-4 (CPU rings) are never reached since no peer is programmed with
 * those RDIs when IPA is active.
 */
void ath12k_wifi8_hal_reo_ring_ipa_ctrl_hash_ix0_setup(struct ath12k_base *ab)
{
	u32 reo_base = HAL_SEQ_WCSS_UMAC_REO_REG;
	u32 slot = HAL_WIFI8_IPA_REO_RDI;  /* = 5 */
	u32 val;

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_DEST_RING_CTRL_AP_IX_0);
	val &= ~(GENMASK(4, 0) << (slot * HAL_REO_IX_FIELD_WIDTH));
	val |= DESTINATION_RING_CTRL_SW5 << (slot * HAL_REO_IX_FIELD_WIDTH);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_DEST_RING_CTRL_AP_IX_0, val);
}
#endif /* CPTCFG_EXT_IPA_OFFLOAD */
