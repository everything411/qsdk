// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_ast.h"
#include "dp.h"
#include "hal.h"
#include "../debug.h"
#include "dp_tx.h"
#include "dp_htt.h"
#include "hal_rx.h"

struct ath12k_ast_entry *
ath12k_dp_get_sw_ast_entry_by_index(struct ath12k_dp_hw_group *dp_hw_grp, u16 ast_index);

/* Whenever we configure HW keys, we need to update the below cache.
 * This is prevent mismatches between HW and SW indexing.
 */
u16 ath12k_ast_key_cache[TOEPLITZ_KEYLEN][1 << 8];

static inline struct ath12k_dp_global_ast_table *
		ath12k_dp_get_global_ast_table(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	return &dp_hw_grp_wifi8->dp_ast_base;
}

static inline void ath12k_dp_copy_mac_addr(u32 *mac_addr_31_0,
					   u16 *mac_addr_47_32,
					   u8 *mac_addr)
{
	memcpy(mac_addr_31_0, mac_addr, sizeof(u32));
	memcpy(mac_addr_47_32, mac_addr + 4, ETH_ALEN - 4);
}

bool ath12k_wifi8_dp_ase_tx_cache_enabled(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);

	if (!ast_base)
		return false;

	if (ast_base->ase_tx_cache_en)
		return true;

	return false;
}

bool ath12k_wifi8_dp_ase_rx_cache_enabled(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);

	if (!ast_base)
		return false;

	if (ast_base->ase_rx_cache_en)
		return true;

	return false;
}

u16 ath12k_dp_compute_ast_hash(u8 *mac_addr,
			       u32 ast_hash_mask,
			       bool is_mcast,
			       bool is_mec)
{
	u8 input_data[INPUT_HASH_KEY_LEN];
	u8 input_i;
	u8 cache_i;
	u8 i;
	u64 final_input;
	u16 hash_out = 0;
	u32 mac_addr_31_0 = 0;
	u16 mac_addr_47_32 = 0;

	ath12k_dp_copy_mac_addr(&mac_addr_31_0, &mac_addr_47_32, mac_addr);

	final_input = ((u64)mac_addr_31_0) |
		      ((u64)mac_addr_47_32 << 32) |
		      ((u64)is_mcast << 48) |
		      ((u64)is_mec << 49);
	final_input <<= 6;

	for (i = 0; i < INPUT_HASH_KEY_LEN; i++)
		input_data[6 - i] = (u8)((final_input >> (8 * i)) & 0xff);

	for (input_i = 0, cache_i = 0;
	     input_i < INPUT_HASH_KEY_LEN;
	     input_i++, cache_i++)
		hash_out ^= ath12k_ast_key_cache[cache_i][input_data[input_i]];

	return hash_out & ast_hash_mask;
}

void ath12k_dp_init_ast_toeplitz_key_cache(const u8 key[TOEPLITZ_KEYLEN])
{
	u16 cur_key = key[0] << 8 | key[1];
	u8  i;

	for (i = 0; i < TOEPLITZ_KEYLEN; i++) {
		u8  new_key_byte;
		u16 shifted_key[8];
		u8  bit;
		u32 val;

		if ((i + 2) < TOEPLITZ_KEYLEN) {
			new_key_byte = key[i + 2];
		} else {
			/* no further key material, shift in zeros */
			new_key_byte = 0;
		}

		shifted_key[0] = cur_key;

		for (bit = 1; bit < 8; bit++) {
			/* for each iteration, shift out one more bit of the
			 * current key and shift in one more bit of the new key
			 * material
			 */
			shifted_key[bit] = cur_key << bit | new_key_byte >> (8 - bit);
		}

		for (val = 0; val < (1 << 8); val++) {
			u16 hash = 0;
			u32 mask;

			for (bit = 0, mask = 1 << 7; bit < 8; bit++, mask >>= 1) {
				if ((val & mask)) {
					/* for each bit set in the input, XOR in
					 * the appropriately shifted key.
					 */
					hash ^= shifted_key[bit];
				}
			}

			ath12k_ast_key_cache[i][val] = hash;
		}

		cur_key = (cur_key << 8) | new_key_byte;
	}
}

void ath12k_dp_init_ast_hash_keys(u32 hash_key1,
				  u32 hash_key2,
				  u32 hash_key3)
{
	u64 temp_hash_key;
	u8 ase_keys[TOEPLITZ_KEYLEN];
	int i;

	temp_hash_key = ((u64)hash_key2 << 32) | ((u64)hash_key1);
	temp_hash_key >>= 1;
	temp_hash_key |= ((u64)hash_key3 << 63);

	for (i = 0; i < (TOEPLITZ_KEYLEN - 1); i++)
		ase_keys[i] = (u8)((temp_hash_key >> ((7 - i) * 8)) & 0xff);

	ase_keys[8] = (u8)((hash_key1 & 0xFF) << 7);

	/* Configure local hash key cache */
	ath12k_dp_init_ast_toeplitz_key_cache(ase_keys);
}

static inline void
ath12k_dp_ast_param_init(struct ath12k_dp_global_ast_table *ast_base,
			 struct ath12k_hal_ast_param *ast_info)
{
	ast_info->num_ast_entries = ast_base->num_ast_entries;
	ast_info->skid_len = ast_base->skid_len;

	ast_info->ast_cache_en = ast_base->ase_tx_cache_en;
	ast_info->ast_cache_failure_en = ast_base->ase_tx_cache_en;

	/* init the hash keys */
	ast_info->ase_hash_key1 = ast_base->hash_keys.ase_hash_key1;
	ast_info->ase_hash_key2 = ast_base->hash_keys.ase_hash_key2;
	ast_info->ase_hash_key3 = ast_base->hash_keys.ase_hash_key3;
	ast_info->paddr = ast_base->ast_paddr;
}

static inline u32 ath12k_ast_addr_hash(const u8 *addr)
{
	return jhash(addr, ETH_ALEN, 0);
}

void ath12k_ast_entry_hash_add(struct ath12k_dp_hw_group *dp_hw_grp,
			       struct ath12k_ast_entry *sw_ast_entry)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	u32 hash;

	if (!ast_base || sw_ast_entry->hash_added)
		return;

	lockdep_assert_held(&ast_base->ast_lock);
	hash = ath12k_ast_addr_hash(sw_ast_entry->mac_addr);

	spin_lock_bh(&ast_base->ast_hash_lock);

	hash_add(ast_base->ast_hash, &sw_ast_entry->hash_node, hash);
	sw_ast_entry->hash_added = true;

	spin_unlock_bh(&ast_base->ast_hash_lock);

	return;
}

void ath12k_ast_entry_hash_delete(struct ath12k_dp_hw_group *dp_hw_grp,
				  struct ath12k_ast_entry *sw_ast_entry)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);

	if (!ast_base || !sw_ast_entry->hash_added)
		return;

	lockdep_assert_held(&ast_base->ast_lock);
	spin_lock_bh(&ast_base->ast_hash_lock);

	hash_del(&sw_ast_entry->hash_node);
	sw_ast_entry->hash_added = false;

	spin_unlock_bh(&ast_base->ast_hash_lock);

	return;
}

struct ath12k_ast_entry *
ath12k_ast_entry_find_by_addr(struct ath12k_dp_hw_group *dp_hw_grp, const u8 *addr)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct ath12k_ast_entry *sw_ast_entry;
	u32 hash;

	if (!ast_base)
		return NULL;

	lockdep_assert_held(&ast_base->ast_lock);

	hash = ath12k_ast_addr_hash(addr);

	spin_lock_bh(&ast_base->ast_hash_lock);

	hash_for_each_possible(ast_base->ast_hash, sw_ast_entry,
			       hash_node, hash) {
		if (ether_addr_equal(sw_ast_entry->mac_addr, addr)) {
			spin_unlock_bh(&ast_base->ast_hash_lock);
			return sw_ast_entry;
		}
	}

	spin_unlock_bh(&ast_base->ast_hash_lock);

	return NULL;
}

int ath12k_dp_ast_table_alloc(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct device *dev = ab->dev;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_global_ast_table *ast_base = NULL;
	int ret;

	if (!dp_hw_grp) {
		ath12k_err(ab, "ASE alloc dp_hw_grp is NULL\n");
		return -EINVAL;
	}

	ast_base = ath12k_dp_get_global_ast_table(dp_hw_grp);
	if (!ast_base) {
		ath12k_err(ab, "ASE alloc unable to fetch ast_base\n");
		return -EINVAL;
	}

	if (ast_base->ast_vaddr_unaligned)
		return 0;

	ast_base->num_ast_entries = MAX_NUM_AST_ENTRIES;
	ast_base->ast_hash_mask = ast_base->num_ast_entries - 1;
	ast_base->skid_len = MAX_SKID_LEN;

	ast_base->hw_ast_table_size = ast_base->num_ast_entries * HAL_HW_AST_ENTRY_SIZE;
	ast_base->ast_vaddr_unaligned =
			kzalloc(ast_base->hw_ast_table_size + HAL_HW_AST_ENTRY_ALIGN - 1,
				GFP_KERNEL);
	if (!ast_base->ast_vaddr_unaligned) {
		ath12k_err(ab, "failed to allocate memory for global AST table");
		return -ENOMEM;
	}

	ast_base->ast_vaddr_aligned = PTR_ALIGN(ast_base->ast_vaddr_unaligned,
						HAL_HW_AST_ENTRY_ALIGN);
	ast_base->ast_paddr = ath12k_core_dma_map_single(dev,
							 ast_base->ast_vaddr_aligned,
							 ast_base->hw_ast_table_size,
							 DMA_BIDIRECTIONAL);
	if (!ast_base->ast_paddr) {
		ath12k_err(ab, "failed to map AST table\n");
		ret = -ENOMEM;
		goto free_hw_ast_table;
	}

	/* index based mapping for SW AST entries */
	ast_base->ast_entries = kzalloc((ast_base->num_ast_entries *
					 sizeof(struct ath12k_ast_entry *)),
					GFP_KERNEL);
	if (!ast_base->ast_entries) {
		ath12k_err(ab, "failed to allocate memory for AST index table\n");
		ret = -ENOMEM;
		goto unmap_hw_ast_table;
	}

	return 0;

unmap_hw_ast_table:
	ath12k_core_dma_unmap_single(dev, ast_base->ast_paddr,
				     ast_base->hw_ast_table_size,
				     DMA_BIDIRECTIONAL);
free_hw_ast_table:
	kfree(ast_base->ast_vaddr_unaligned);
	ast_base->ast_vaddr_unaligned = NULL;
	ast_base->ast_vaddr_aligned = NULL;
	return ret;
}

int ath12k_dp_ast_table_init(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_global_ast_table *ast_base = NULL;
	struct ath12k_hal_ast_param ast_info = {0};
	struct ath12k_base *ab = NULL;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct ath12k_dp *dp = NULL;
	int ret;
	int i = 0;

	if (!dp_hw_grp) {
		ath12k_err(NULL, "ASE init dp_hw_grp is NULL\n");
		return -EINVAL;
	}

	ast_base = ath12k_dp_get_global_ast_table(dp_hw_grp);
	ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (!ast_base || !ab) {
		ath12k_err(NULL, "ASE init unable to fetch ast_base or ab\n");
		return -EINVAL;
	}

	/* Zero out the HW AST table memory on every init */
	memset(ast_base->ast_vaddr_aligned, 0, ast_base->hw_ast_table_size);

	/* Zero out the SW AST entry index table on every init */
	memset(ast_base->ast_entries, 0,
	       ast_base->num_ast_entries * sizeof(struct ath12k_ast_entry *));

	spin_lock_init(&ast_base->ast_lock);

	/* Hash table for SW AST entries */
	hash_init(ast_base->ast_hash);
	spin_lock_init(&ast_base->ast_hash_lock);

	ast_base->hash_keys.ase_hash_key1 = ATH12K_AST_HASH_KEY_1;
	ast_base->hash_keys.ase_hash_key2 = ATH12K_AST_HASH_KEY_2;
	ast_base->hash_keys.ase_hash_key3 = ATH12K_AST_HASH_KEY_3;
	ath12k_dp_init_ast_hash_keys(ast_base->hash_keys.ase_hash_key1,
				     ast_base->hash_keys.ase_hash_key2,
				     ast_base->hash_keys.ase_hash_key3);

	ast_base->ase_tx_cache_en = 1;
	ast_base->ase_rx_cache_en = ATH12K_ASE_RX_CACHE_EN;
	ath12k_dp_ast_param_init(ast_base, &ast_info);
	if (!ath12k_ftm_mode) {
		ath12k_wifi8_hal_hw_ase_init(ab, &ast_info);

		/* send htt to all the chips */
		for (i = 0; i < ATH12K_MAX_SOCS; i++) {
			dp = dp_hw_grp->dp[i];
			if (!dp || dp->ab->is_bypassed)
				continue;

			ret = ath12k_dp_rx_htt_ast_info_setup(dp->ab, &ast_info);
			if (ret) {
				ath12k_err(ab, "failed to send ASE htt ret = %d", ret);
				return ret;
			}
		}
	}

	if (ath12k_wifi8_dp_ase_tx_cache_enabled(dp_hw_grp) ||
	    ath12k_wifi8_dp_ase_rx_cache_enabled(dp_hw_grp))
		init_completion(&dp_hw_grp_wifi8->peer_init_done);

	return 0;
}

void ath12k_dp_ast_table_deinit(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_global_ast_table *ast_base = NULL;
	u16 index;
	struct ath12k_ast_entry *sw_ast_entry;

	ast_base = ath12k_dp_get_global_ast_table(dp_hw_grp);
	if (!ast_base) {
		ath12k_err(NULL, "ASE free unable to fetch ast_base\n");
		return;
	}

	/* cleanup stale AST entries */
	spin_lock_bh(&ast_base->ast_lock);
	for (index = 0; index < ast_base->num_ast_entries; index++) {
		sw_ast_entry = ath12k_dp_get_sw_ast_entry_by_index(dp_hw_grp, index);
		if (!sw_ast_entry)
			continue;

		ast_base->ast_entries[index] = NULL;
		ath12k_ast_entry_hash_delete(dp_hw_grp, sw_ast_entry);
		kfree(sw_ast_entry);
	}
	spin_unlock_bh(&ast_base->ast_lock);
}

void ath12k_dp_ast_table_free(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_global_ast_table *ast_base = NULL;
	struct device *dev = NULL;

	if (!dp_hw_grp) {
		ath12k_err(NULL, "ASE free dp_hw_grp is NULL\n");
		return;
	}

	dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	ast_base = ath12k_dp_get_global_ast_table(dp_hw_grp);
	if (!ast_base) {
		ath12k_err(NULL, "ASE free unable to fetch ast_base\n");
		return;
	}

	if (!ast_base->ast_vaddr_unaligned) {
		ath12k_err(NULL, "ASE free vaddr unaligned is NULL\n");
		return;
	}

	if (dev) {
		ath12k_core_dma_unmap_single(dev, ast_base->ast_paddr,
					     ast_base->hw_ast_table_size,
					     DMA_BIDIRECTIONAL);
	}

	kfree(ast_base->ast_entries);
	ast_base->ast_entries = NULL;
	kfree(ast_base->ast_vaddr_unaligned);
	ast_base->ast_vaddr_unaligned = NULL;
	ast_base->ast_vaddr_aligned = NULL;
}

static inline bool ath12k_dp_hw_ast_entry_is_valid(struct hal_ast_entry *hw_ast_entry)
{
	return !!le32_get_bits(hw_ast_entry->info1, HAL_AST_ENTRY_INFO1_ENTRY_VALID);
}

static struct hal_ast_entry *
ath12k_dp_get_hw_ast_entry(struct ath12k_dp_hw_group *dp_hw_grp, u16 index)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	struct hal_ast_entry *ast_table = NULL;
	struct hal_ast_entry *ast_entry = NULL;
	dma_addr_t paddr;

	if (!dev)
		return NULL;

	/* Validate index bounds */
	if (index >= ast_base->num_ast_entries)
		return NULL;

	ast_table = (struct hal_ast_entry *)ast_base->ast_vaddr_aligned;
	ast_entry = ast_table + index;

	/* invalidate the entry */
	paddr = (dma_addr_t)(((u8 *)ast_base->ast_paddr) + index * HAL_HW_AST_ENTRY_SIZE);
	ath12k_core_dma_sync_single_for_cpu(dev,
					    paddr,
					    HAL_HW_AST_ENTRY_SIZE,
					    DMA_BIDIRECTIONAL);
	return ast_entry;
}

struct ath12k_ast_entry *
ath12k_dp_get_sw_ast_entry_by_index(struct ath12k_dp_hw_group *dp_hw_grp, u16 ast_index)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);

	if (!ast_base || ast_index >= ast_base->num_ast_entries)
		return NULL;

	return ast_base->ast_entries[ast_index];
}

static int
ath12k_wifi8_invalidate_rx_ase_cache(struct ath12k_dp_hw_group *dp_hw_grp,
				     struct ath12k_ase_cache_op_param *config)
{
	struct ath12k_dp_global_ast_table *ast_base =
		ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_wifi8 *dp_wifi8 = NULL;
	struct ath12k_hal_rx_cmd_ring_param param = {0};
	struct hal_srng *wbm_ase_cmd_ring;
	int ret = 0;

	if (!ab || !ast_base)
		return -EINVAL;

	if ((test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags) &&
	     !test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags)) ||
	    test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		return 0;

	dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);
	wbm_ase_cmd_ring = &ab->hal.srng_list[dp_wifi8->rx_ase_cmd_ring.ring_id];

	switch (config->cmd) {
	case ATH12K_INVALIDATE_ENTRY:
		param.hw_link_bitmap = config->chip_id_bitmap;
		param.mac_addr_31_0 = config->mac_addr_31_0;
		param.mac_addr_47_32 = config->mac_addr_47_32;
		param.is_mcast = config->is_mcast;
		param.is_mec = config->is_mec;
		param.ad1_match = config->ad1_match;
		param.link_id = config->link_id;
		param.meta_data_0 = config->meta_data_0;

		param.cmd_num = HAL_ASE_CACHE_OP_INVALIDATE_SINGLE_ENTRY;
		break;

	case ATH12K_INVALIDATE_ALL:
		param.hw_link_bitmap = config->chip_id_bitmap;
		param.cmd_num = HAL_ASE_CACHE_OP_INVALIDATE_ALL;
		break;

	default:
		break;
	}

	ret = ath12k_wifi8_hal_invalidate_rx_cache_cmd_send(ab, wbm_ase_cmd_ring, &param);

	return ret;
}

static int
ath12k_wifi8_invalidate_tx_ase_cache(struct ath12k_dp_hw_group *dp_hw_grp,
				     struct ath12k_ase_cache_op_param *config)
{
	struct ath12k_dp_global_ast_table *ast_base =
		ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_wifi8 *dp_wifi8 = NULL;
	struct ath12k_hal_tx_cmd_ring_param param = {0};
	struct hal_srng *tx_cmd_ring;
	int ret = 0;

	if (!ab || !ast_base)
		return -EINVAL;

	if ((test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags) &&
	     !test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags)) ||
	    test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		return 0;

	dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);
	tx_cmd_ring = &ab->hal.srng_list[dp_wifi8->tcl_cmd_ring.ring_id];

	switch (config->cmd) {
	case ATH12K_INVALIDATE_ENTRY:
		param.ctrl_buf_addr = (dma_addr_t)(((u8 *)ast_base->ast_paddr) +
						   (config->ast_index *
						    HAL_HW_AST_ENTRY_SIZE));
		param.meta_data_0 = config->meta_data_0;

		param.cmd_num = HAL_ASE_CACHE_OP_INVALIDATE_SINGLE_ENTRY;
		break;

	case ATH12K_INVALIDATE_ALL:
		param.cmd_num = HAL_ASE_CACHE_OP_INVALIDATE_ALL;
		break;

	default:
		break;
	}

	ret = ath12k_wifi8_hal_invalidate_tx_cache_cmd_send(ab, tx_cmd_ring, &param);

	return ret;
}

static inline u8
ath12k_dp_wifi8_hw_group_chip_id_bitmap_derive(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp *dp = NULL;
	int i = 0;
	u8 bitmap = 0;

	for (i = 0; i < ATH12K_MAX_SOCS; i++) {
		dp = dp_hw_grp->dp[i];
		if (!dp)
			continue;

		 bitmap |= BIT(dp->device_id);
	}

	return bitmap;
}

static inline void
ath12k_dp_ast_set_rx_invalidate_status_pending(struct ath12k_ast_entry *sw_ast_entry,
					       u8 chip_id_bitmap, bool ast_valid)
{
	if (ast_valid) {
		if (chip_id_bitmap & BIT(0))
			set_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_0,
				&sw_ast_entry->ast_create_invalidate_status);
		if (chip_id_bitmap & BIT(1))
			set_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_1,
				&sw_ast_entry->ast_create_invalidate_status);
		if (chip_id_bitmap & BIT(2))
			set_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_2,
				&sw_ast_entry->ast_create_invalidate_status);
		if (chip_id_bitmap & BIT(3))
			set_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_3,
				&sw_ast_entry->ast_create_invalidate_status);
	} else {
		if (chip_id_bitmap & BIT(0))
			set_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_0,
				&sw_ast_entry->ast_delete_invalidate_status);
		if (chip_id_bitmap & BIT(1))
			set_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_1,
				&sw_ast_entry->ast_delete_invalidate_status);
		if (chip_id_bitmap & BIT(2))
			set_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_2,
				&sw_ast_entry->ast_delete_invalidate_status);
		if (chip_id_bitmap & BIT(3))
			set_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_3,
				&sw_ast_entry->ast_delete_invalidate_status);
	}
}

static inline void
ath12k_dp_ast_clear_rx_invalidate_status_pending(struct ath12k_ast_entry *sw_ast_entry,
						 u8 chip_id_bitmap, bool ast_valid)
{
	if (ast_valid) {
		if (chip_id_bitmap & BIT(0))
			clear_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_0,
				  &sw_ast_entry->ast_create_invalidate_status);
		if (chip_id_bitmap & BIT(1))
			clear_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_1,
				  &sw_ast_entry->ast_create_invalidate_status);
		if (chip_id_bitmap & BIT(2))
			clear_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_2,
				  &sw_ast_entry->ast_create_invalidate_status);
		if (chip_id_bitmap & BIT(3))
			clear_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_3,
				  &sw_ast_entry->ast_create_invalidate_status);
	} else {
		if (chip_id_bitmap & BIT(0))
			clear_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_0,
				  &sw_ast_entry->ast_delete_invalidate_status);
		if (chip_id_bitmap & BIT(1))
			clear_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_1,
				  &sw_ast_entry->ast_delete_invalidate_status);
		if (chip_id_bitmap & BIT(2))
			clear_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_2,
				  &sw_ast_entry->ast_delete_invalidate_status);
		if (chip_id_bitmap & BIT(3))
			clear_bit(ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_3,
				  &sw_ast_entry->ast_delete_invalidate_status);
	}
}

int ath12k_wifi8_invalidate_peer_ase_entry(struct ath12k_dp_hw_group *dp_hw_grp,
					   struct ath12k_ast_entry *sw_ast_entry)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct ath12k_ase_cache_op_param config = {0};
	bool ast_is_valid;
	int ret;

	if (ath12k_wifi8_dp_ase_tx_cache_enabled(dp_hw_grp)) {
		if (sw_ast_entry->tx_cmd_seq_num >= ATH12K_MAX_TX_ASE_CMD_SEQ_NUM)
			sw_ast_entry->tx_cmd_seq_num = 0;
		else
			sw_ast_entry->tx_cmd_seq_num++;

		ast_is_valid = !!(sw_ast_entry->ast_entry_flags &
				  ATH12K_AST_ENTRY_IS_VALID);
		config.ast_index = sw_ast_entry->ast_index;
		config.meta_data_0 =
			u32_encode_bits(sw_ast_entry->ast_index,
					ATH12K_AST_META_DATA_0_AST_INDEX) |
			u32_encode_bits(sw_ast_entry->tx_cmd_seq_num,
					ATH12K_AST_META_DATA_0_TX_CMD_SEQ_NUM) |
			u32_encode_bits(!!ast_is_valid,
					ATH12K_AST_META_DATA_0_STATE);
		config.cmd = ATH12K_INVALIDATE_ENTRY;

		ret = ath12k_wifi8_invalidate_tx_ase_cache(dp_hw_grp, &config);
		if (ret) {
			ath12k_err(NULL, "unable to send tx ase cache cmd %d index %u\n",
				   ret, sw_ast_entry->ast_index);
			return ret;
		}

		if (ast_is_valid)
			set_bit(ATH12K_AST_ENTRY_TX_INVAL_STATUS,
				&sw_ast_entry->ast_create_invalidate_status);
		else
			set_bit(ATH12K_AST_ENTRY_TX_INVAL_STATUS,
				&sw_ast_entry->ast_delete_invalidate_status);
	}

	if (ath12k_wifi8_dp_ase_rx_cache_enabled(dp_hw_grp)) {
		ath12k_dp_copy_mac_addr(&config.mac_addr_31_0,
					&config.mac_addr_47_32,
					sw_ast_entry->mac_addr);
		config.is_mcast = sw_ast_entry->ast_entry_flags &
				  ATH12K_AST_ENTRY_IS_MCAST;
		config.is_mec = sw_ast_entry->ast_entry_flags &
				ATH12K_AST_ENTRY_IS_MEC;
		config.link_id = ATH12K_AST_ENTRY_WILDCARD_LINK_ID;
		config.chip_id_bitmap =
				ath12k_dp_wifi8_hw_group_chip_id_bitmap_derive(dp_hw_grp);
		ret = ath12k_wifi8_invalidate_rx_ase_cache(dp_hw_grp, &config);
		if (ret) {
			ath12k_err(NULL, "unable to send rx ase cache cmd %d index %u\n",
				   ret, sw_ast_entry->ast_index);
			return ret;
		}
		ast_is_valid = !!(sw_ast_entry->ast_entry_flags &
				  ATH12K_AST_ENTRY_IS_VALID);
		ath12k_dp_ast_set_rx_invalidate_status_pending(sw_ast_entry,
							       config.chip_id_bitmap,
							       ast_is_valid);
	}
	reinit_completion(&dp_hw_grp_wifi8->peer_init_done);
	return 0;
}

int ath12k_wifi8_invalidate_peer_ase_cache_table(struct ath12k_dp_hw_group *dp_hw_grp)
{
	int ret = 0;
	struct ath12k_ase_cache_op_param config = {0};

	if (ath12k_wifi8_dp_ase_tx_cache_enabled(dp_hw_grp)) {
		config.cmd = ATH12K_INVALIDATE_ALL;
		ret = ath12k_wifi8_invalidate_tx_ase_cache(dp_hw_grp, &config);
		if (ret) {
			ath12k_err(NULL, "unable to send peer tx ase cache cmd %d\n",
				   ret);
			return ret;
		}
	}
	if (ath12k_wifi8_dp_ase_rx_cache_enabled(dp_hw_grp)) {
		config.cmd = ATH12K_INVALIDATE_ALL;
		config.chip_id_bitmap =
				ath12k_dp_wifi8_hw_group_chip_id_bitmap_derive(dp_hw_grp);
		ret = ath12k_wifi8_invalidate_rx_ase_cache(dp_hw_grp, &config);
		if (ret) {
			ath12k_err(NULL, "unable to send peer rx ase cache cmd %d\n",
				   ret);
			return ret;
		}
	}

	return 0;
}

int ath12k_dp_hw_ast_entry_sync(struct ath12k_dp_hw_group *dp_hw_grp,
				struct ath12k_ast_entry *sw_ast_entry,
				struct hal_ast_entry *hw_ast_entry)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	u32 mac_addr_31_0 = 0;
	u16 mac_addr_47_32 = 0;
	bool is_valid = !!(sw_ast_entry->ast_entry_flags & ATH12K_AST_ENTRY_IS_VALID);
	dma_addr_t paddr;
	int ret = 0;

	if (is_valid) {
		/* HW AST entry setup */
		memset(hw_ast_entry, 0, sizeof(struct hal_ast_entry));
		ath12k_dp_copy_mac_addr(&mac_addr_31_0,
					&mac_addr_47_32,
					sw_ast_entry->mac_addr);

		hw_ast_entry->info0 = le32_encode_bits(mac_addr_31_0,
						       HAL_AST_ENTRY_INFO0_MAC_ADDR_31_0);
		hw_ast_entry->info1 =
			le32_encode_bits(mac_addr_47_32,
					 HAL_AST_ENTRY_INFO1_MAC_ADDR_47_32) |
			le32_encode_bits(!!(sw_ast_entry->ast_entry_flags &
					    ATH12K_AST_ENTRY_IS_MEC),
					 HAL_AST_ENTRY_INFO1_MEC) |
			le32_encode_bits(ATH12K_AST_ENTRY_WILDCARD_LINK_ID,
					 HAL_AST_ENTRY_INFO1_LINK_ID) |
			le32_encode_bits(!!(sw_ast_entry->ast_entry_flags &
					    ATH12K_AST_ENTRY_IS_USE_ADDRX),
					 HAL_AST_ENTRY_INFO1_USE_ADDRX_SEARCH) |
			le32_encode_bits(2,
					 HAL_AST_ENTRY_INFO1_NUM_WHO_CLASSIFY_INFO);
		hw_ast_entry->info2 =
			le32_encode_bits(sw_ast_entry->tx_classify_info_paddr,
					 HAL_AST_ENTRY_INFO2_WHO_CLASSIFY_INFO_31_0);
		hw_ast_entry->info3 =
		le32_encode_bits(((u64)sw_ast_entry->tx_classify_info_paddr) >> 32,
				 HAL_AST_ENTRY_INFO3_WHO_CLASSIFY_INFO_39_32);
		hw_ast_entry->info5 = le32_encode_bits(sw_ast_entry->peer_id,
						       HAL_AST_ENTRY_INFO5_SW_PEER_ID);
		hw_ast_entry->info6 = le32_encode_bits(sw_ast_entry->mld_id,
						       HAL_AST_ENTRY_INFO6_VDEV_ID);
		/* TODO: check for other variable init */

		/* Set Valid only after updating everything else */
		hw_ast_entry->info1 |= le32_encode_bits(is_valid,
							HAL_AST_ENTRY_INFO1_ENTRY_VALID);
	} else {
		hw_ast_entry->info1 &= ~HAL_AST_ENTRY_INFO1_ENTRY_VALID;
	}

	if (!dev) {
		ath12k_err(NULL, "dev is NULL during AST entry sync for index %u\n",
			   sw_ast_entry->ast_index);
		return -EINVAL;
	}

	/* flush the entry */
	paddr =	(dma_addr_t)(((u8 *)ast_base->ast_paddr) +
			     (sw_ast_entry->ast_index * HAL_HW_AST_ENTRY_SIZE));
	ath12k_core_dma_sync_single_for_device(dev,
					       paddr,
					       HAL_HW_AST_ENTRY_SIZE,
					       DMA_BIDIRECTIONAL);

	ret = ath12k_wifi8_invalidate_peer_ase_entry(dp_hw_grp, sw_ast_entry);
	return ret;
}

bool
ath12k_dp_ast_has_matching_entry(struct ath12k_dp_hw_group *dp_hw_grp,
				 struct ath12k_ast_entry_config_params *param,
				 u16 ast_index)
{
	struct ath12k_ast_entry *sw_ast_entry = NULL;
	bool ret = false;

	/* any previous entries present with same config */
	sw_ast_entry = ath12k_dp_get_sw_ast_entry_by_index(dp_hw_grp, ast_index);
	if (sw_ast_entry) {
		if (!(sw_ast_entry->ast_entry_flags & ATH12K_AST_ENTRY_IS_VALID))
			return ret;
		if (memcmp(sw_ast_entry->mac_addr, param->mac_addr, ETH_ALEN))
			return ret;
		if ((sw_ast_entry->ast_entry_flags & ATH12K_AST_ENTRY_IS_MEC) !=
		    (param->ast_entry_flags & ATH12K_AST_ENTRY_IS_MEC))
			return ret;
		if ((sw_ast_entry->ast_entry_flags & ATH12K_AST_ENTRY_IS_MCAST) !=
		     (param->ast_entry_flags & ATH12K_AST_ENTRY_IS_MCAST))
			return ret;
		if (sw_ast_entry->peer_id != param->peer_id)
			return ret;

		return true;
	}

	return false;
}

int ath12k_dp_ast_entry_create(struct ath12k_dp_hw_group *dp_hw_grp,
			       struct ath12k_ast_entry_config_params *param)
{
	bool is_mcast = !!(param->ast_entry_flags & ATH12K_AST_ENTRY_IS_MCAST);
	bool is_mec = !!(param->ast_entry_flags & ATH12K_AST_ENTRY_IS_MEC);
	struct hal_ast_entry *hw_ast_entry;
	struct ath12k_ast_entry *sw_ast_entry = NULL;
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct ath12k_dp_global_ast_stats *ast_stats = &ast_base->ast_stats;
	int ret = -ENOSPC;
	u16 i;
	u16 ast_index;
	u16 ast_hash;
	bool free_slot_found = false;

	param->ast_index = ATH12K_INVALID_AST_INDEX;
	param->ast_hash = ATH12K_INVALID_AST_INDEX;
	ast_hash = ath12k_dp_compute_ast_hash(param->mac_addr,
					      ast_base->ast_hash_mask,
					      is_mcast, is_mec);
	ast_index = ast_hash;
	spin_lock_bh(&ast_base->ast_lock);
	ast_stats->num_ast_entry_create_attempted++;
	for (i = 0; i < ast_base->skid_len; ++i) {
		/* wrap around case */
		if (ast_index == ast_base->num_ast_entries)
			ast_index = 0;

		hw_ast_entry = ath12k_dp_get_hw_ast_entry(dp_hw_grp, ast_index);
		if (!hw_ast_entry) {
			ast_stats->invalid_hw_ast_entry++;
			ath12k_err(NULL,
				   "invalid hw ast entry while finding free slot %u\n",
				   ast_index);
			ret = -EINVAL;
			goto error_handle;
		}
		if (ath12k_dp_hw_ast_entry_is_valid(hw_ast_entry)) {
			if (!(param->ast_entry_flags & ATH12K_AST_ENTRY_IS_AP_BSS_ENTRY)
			    && ath12k_dp_ast_has_matching_entry(dp_hw_grp,
								param, ast_index)) {
				ret = -EALREADY;
				goto error_handle;
			}
			ast_index++;
			continue;
		}
		/* Check if a previous delete operation is in progress */
		sw_ast_entry = ath12k_dp_get_sw_ast_entry_by_index(dp_hw_grp, ast_index);
		if (sw_ast_entry) {
			ast_stats->delete_in_progress++;
			ret = -EBUSY;
			goto error_handle;
		}

		free_slot_found = true;
		break;
	}

	if (!free_slot_found) {
		ast_stats->no_free_slot++;
		ath12k_err(NULL,
			   "Failed to allocate AST entry: no free slot available %u\n",
			   ast_index);
		ret = -EINVAL;
		goto error_handle;
	}

	/* SW AST entry setup */
	sw_ast_entry = kzalloc(sizeof(*sw_ast_entry), GFP_ATOMIC);
	if (!sw_ast_entry) {
		ast_stats->alloc_fail++;
		ret = -ENOMEM;
		goto error_handle;
	}

	memcpy(sw_ast_entry->mac_addr, param->mac_addr, ETH_ALEN);
	sw_ast_entry->tx_classify_info_paddr = param->tx_classify_info_paddr;
	sw_ast_entry->peer_id = param->peer_id;
	sw_ast_entry->mld_id = param->mld_id;
	sw_ast_entry->ast_entry_flags = param->ast_entry_flags;
	sw_ast_entry->ast_index = ast_index;
	sw_ast_entry->ast_hash = ast_hash;
	sw_ast_entry->ast_entry_flags |= ATH12K_AST_ENTRY_IS_VALID;
	if (is_mec) {
		sw_ast_entry->ast_entry_flags |= ATH12K_AST_ENTRY_IS_ACTIVE_MEC;
		list_add_tail(&sw_ast_entry->mec_list,
			      &dp_hw_grp_wifi8->mec_entry_list_head);
	}

	/* Add the SW AST entry in index based and hash tables */
	ast_base->ast_entries[ast_index] = sw_ast_entry;

	if (!(param->ast_entry_flags & ATH12K_AST_ENTRY_IS_AP_BSS_ENTRY) &&
	    ath12k_ast_entry_find_by_addr(dp_hw_grp, param->mac_addr)) {
		ast_base->ast_entries[ast_index] = NULL;
		ast_stats->dup_entry_found++;
		ath12k_err(NULL,
			   "duplicate AST entry for MAC %pM at index %u\n",
			   param->mac_addr, ast_index);
		ret = -EALREADY;
		goto free_sw_entry;
	}
	ath12k_ast_entry_hash_add(dp_hw_grp, sw_ast_entry);
	ret = ath12k_dp_hw_ast_entry_sync(dp_hw_grp, sw_ast_entry, hw_ast_entry);
	if (ret) {
		ast_stats->hw_sync_fail++;
		ast_base->ast_entries[ast_index] = NULL;
		ath12k_ast_entry_hash_delete(dp_hw_grp, sw_ast_entry);
		goto free_sw_entry;
	}

	param->ast_index = ast_index;
	param->ast_hash = ast_hash;
	spin_unlock_bh(&ast_base->ast_lock);
	return 0;

free_sw_entry:
	if (is_mec)
		list_del_init(&sw_ast_entry->mec_list);

	kfree(sw_ast_entry);
error_handle:
	spin_unlock_bh(&ast_base->ast_lock);

	return ret;
}

void ath12k_dp_free_ast_entry(struct ath12k_dp_hw_group *dp_hw_grp,
			      u16 ast_index)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct ath12k_ast_entry *sw_ast_entry = NULL;
	struct ath12k_dp_global_ast_stats *ast_stats;

	if (!ast_base)
		return;

	ast_stats = &ast_base->ast_stats;
	spin_lock_bh(&ast_base->ast_lock);
	sw_ast_entry = ath12k_dp_get_sw_ast_entry_by_index(dp_hw_grp, ast_index);
	if (!sw_ast_entry) {
		ast_stats->sw_ast_not_found++;
		spin_unlock_bh(&ast_base->ast_lock);
		return;
	}

	/* remove SW AST entry from index based and hash tables */
	ast_base->ast_entries[ast_index] = NULL;
	ath12k_ast_entry_hash_delete(dp_hw_grp, sw_ast_entry);

	kfree(sw_ast_entry);
	spin_unlock_bh(&ast_base->ast_lock);
}
void ath12k_dp_ast_entry_delete(struct ath12k_dp_hw_group *dp_hw_grp,
				u16 ast_index)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_ast_entry *sw_ast_entry = NULL;
	struct hal_ast_entry *hw_ast_entry;
	u8 chip_id_bitmap;
	struct ath12k_dp_global_ast_stats *ast_stats = &ast_base->ast_stats;

	spin_lock_bh(&ast_base->ast_lock);

	hw_ast_entry = ath12k_dp_get_hw_ast_entry(dp_hw_grp, ast_index);
	if (!hw_ast_entry) {
		ast_stats->hw_ast_not_found++;
		ath12k_err(ab, "unable to find the hw ast entry %d\n", ast_index);
		spin_unlock_bh(&ast_base->ast_lock);
		return;
	}

	sw_ast_entry = ath12k_dp_get_sw_ast_entry_by_index(dp_hw_grp, ast_index);
	if (!sw_ast_entry) {
		ast_stats->sw_ast_not_found++;
		ath12k_err(ab, "unable to find the sw ast entry %d\n", ast_index);
		spin_unlock_bh(&ast_base->ast_lock);
		return;
	}

	if (sw_ast_entry->ast_entry_flags & ATH12K_AST_ENTRY_IS_ACTIVE_MEC) {
		sw_ast_entry->ast_entry_flags &= ~ATH12K_AST_ENTRY_IS_ACTIVE_MEC;
		spin_unlock_bh(&ast_base->ast_lock);
		return;
	}

	if (sw_ast_entry->ast_entry_flags & ATH12K_AST_ENTRY_IS_MEC)
		list_del_init(&sw_ast_entry->mec_list);
	/* reset the valid flag */
	sw_ast_entry->ast_entry_flags &= ~ATH12K_AST_ENTRY_IS_VALID;
	clear_bit(ATH12K_AST_ENTRY_TX_INVAL_STATUS,
		  &sw_ast_entry->ast_create_invalidate_status);
	chip_id_bitmap = ath12k_dp_wifi8_hw_group_chip_id_bitmap_derive(dp_hw_grp);
	ath12k_dp_ast_clear_rx_invalidate_status_pending(sw_ast_entry,
							 chip_id_bitmap, true);

	ath12k_dp_hw_ast_entry_sync(dp_hw_grp, sw_ast_entry, hw_ast_entry);

	spin_unlock_bh(&ast_base->ast_lock);
	if (!(ath12k_wifi8_dp_ase_tx_cache_enabled(dp_hw_grp) ||
	      ath12k_wifi8_dp_ase_rx_cache_enabled(dp_hw_grp)))
		ath12k_dp_free_ast_entry(dp_hw_grp, ast_index);
}

int ath12k_wifi8_dp_rx_ase_cmd_status_handler(struct ath12k_dp *dp, int budget)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp->dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	struct ath12k_ast_entry *sw_ast_entry = NULL;
	struct ath12k_base *ab = dp->ab;
	struct hal_srng *srng;
	struct hal_ase_status *status_desc;
	int quota = budget;
	int ctrl_cmd;
	u32 meta_data_0;
	u16 ast_index;
	u8 chip_id;

	srng = &ab->hal.srng_list[dp_wifi8->rx_ase_status_ring.ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (budget-- &&
	       (status_desc = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		ctrl_cmd = le32_get_bits(status_desc->info0,
					 HAL_ASE_STATUS_RING_INFO0_GSE_CTRL);
		if (ctrl_cmd != HAL_ASE_CACHE_OP_INVALIDATE_SINGLE_ENTRY)
			continue;

		chip_id = BIT(dp->device_id);
		meta_data_0 = le32_to_cpu(status_desc->cmd_meta_data_31_0);
		ast_index = u32_get_bits(meta_data_0, ATH12K_AST_META_DATA_0_AST_INDEX);
		spin_lock_bh(&ast_base->ast_lock);
		sw_ast_entry = ath12k_dp_get_sw_ast_entry_by_index(dp->dp_hw_grp,
								   ast_index);
		if (!sw_ast_entry) {
			spin_unlock_bh(&ast_base->ast_lock);
			continue;
		}
		/* free the ast entry on reception of status */
		if (u32_get_bits(meta_data_0, ATH12K_AST_META_DATA_0_STATE) ==
				AST_ENTRY_DELETE) {
			spin_unlock_bh(&ast_base->ast_lock);
			ath12k_dp_ast_clear_rx_invalidate_status_pending(sw_ast_entry,
									 chip_id,
									 false);
			if (!sw_ast_entry->ast_delete_invalidate_status)
				ath12k_dp_free_ast_entry(dp->dp_hw_grp, ast_index);
			continue;
		}

		ath12k_dp_ast_clear_rx_invalidate_status_pending(sw_ast_entry,
								 chip_id,
								 true);
		if (!sw_ast_entry->ast_create_invalidate_status)
			complete(&dp_hw_grp_wifi8->peer_init_done);

		spin_unlock_bh(&ast_base->ast_lock);
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);
	return quota - budget;
}

void ath12k_wifi8_global_ast_stats_reset(struct ath12k_dp *dp)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp->dp_hw_grp);

	spin_lock_bh(&ast_base->ast_lock);
	memset(&ast_base->ast_stats, 0, sizeof(struct ath12k_dp_global_ast_stats));
	spin_unlock_bh(&ast_base->ast_lock);
}

ssize_t ath12k_wifi8_global_ast_stats(struct ath12k_dp *dp, char *buf, int size)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp->dp_hw_grp);
	struct ath12k_dp_global_ast_stats *ast_stats = &ast_base->ast_stats;
	struct ath12k_ast_entry *sw_ast_entry = NULL;
	int len = 0;
	int i = 0;

	len += scnprintf(buf + len, size - len, "\nGlobal_AST_stats :\n");
	len += scnprintf(buf + len, size - len, "------------------\n");
	spin_lock_bh(&ast_base->ast_lock);
	len += scnprintf(buf + len, size - len, "max_num_ast_entries:%u\n",
			 ast_base->num_ast_entries);
	len += scnprintf(buf + len, size - len, "skid_len:%u\n", ast_base->skid_len);
	len += scnprintf(buf + len, size - len, "tx_ase_cache_enabled:%u\n",
			 ast_base->ase_tx_cache_en);
	len += scnprintf(buf + len, size - len, "num_ast_entry_create_attempted:%u\n",
			 ast_stats->num_ast_entry_create_attempted);
	len += scnprintf(buf + len, size - len, "num_ast_entry_delete_attempted:%u\n",
			 ast_stats->num_ast_entry_delete);
	len += scnprintf(buf + len, size - len, "\nfailure stats\n");
	len += scnprintf(buf + len, size - len, "invalid_hw_ast_entry:%u\n",
			 ast_stats->invalid_hw_ast_entry);
	len += scnprintf(buf + len, size - len, "delete_in_progress:%u\n",
			 ast_stats->delete_in_progress);
	len += scnprintf(buf + len, size - len, "no_free_slot:%u\n",
			 ast_stats->no_free_slot);
	len += scnprintf(buf + len, size - len, "alloc_fail:%u\n",
			 ast_stats->alloc_fail);
	len += scnprintf(buf + len, size - len, "dup_entry_found:%u\n",
			 ast_stats->dup_entry_found);
	len += scnprintf(buf + len, size - len, "hw_sync_fail:%u\n",
			 ast_stats->hw_sync_fail);

	len += scnprintf(buf + len, size - len, "sw_ast_not_found:%u\n",
			 ast_stats->sw_ast_not_found);
	len += scnprintf(buf + len, size - len, "hw_ast_not_found:%u\n",
			 ast_stats->hw_ast_not_found);

	len += scnprintf(buf + len, size - len, "\nGlobal AST table :\n");
	len += scnprintf(buf + len, size - len, "------------------\n");
	for (i = 0; i < ast_base->num_ast_entries; i++) {
		sw_ast_entry = ath12k_dp_get_sw_ast_entry_by_index(dp->dp_hw_grp, i);
		if (!sw_ast_entry)
			continue;
		len += scnprintf(buf + len, size - len,
				 "mac_addr:%pM ast_index:%u ast_hash:%u peer_id:%u ",
				 sw_ast_entry->mac_addr, sw_ast_entry->ast_index,
				 sw_ast_entry->ast_hash, sw_ast_entry->peer_id);
		len += scnprintf(buf + len, size - len, "flags = %x\n",
				 sw_ast_entry->ast_entry_flags);
	}
	spin_unlock_bh(&ast_base->ast_lock);

	return len;
}

int ath12k_wifi8_dp_tx_cmd_status_handler(struct ath12k_dp *dp,
					  int budget)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp->dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	struct ath12k_base *ab = dp->ab;
	struct hal_srng *srng;
	struct hal_tcl_status_ring *tx_status_desc;
	struct ath12k_ast_entry *sw_ast_entry = NULL;
	int quota = budget;
	int ctrl_cmd;
	u32 meta_data_0;
	u16 tx_cmd_seq_num;
	u16 ast_index;

	srng = &ab->hal.srng_list[dp_wifi8->tcl_status_ring.ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (budget-- &&
	       (tx_status_desc = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		ctrl_cmd = le32_get_bits(tx_status_desc->info0,
					 HAL_TCL_STATUS_RING_INFO0_GSE_CTRL);
		if (ctrl_cmd != HAL_ASE_CACHE_OP_INVALIDATE_SINGLE_ENTRY)
			continue;

		meta_data_0 = le32_to_cpu(tx_status_desc->cmd_meta_data_31_0);
		ast_index = u32_get_bits(meta_data_0, ATH12K_AST_META_DATA_0_AST_INDEX);
		tx_cmd_seq_num = u32_get_bits(meta_data_0,
					      ATH12K_AST_META_DATA_0_TX_CMD_SEQ_NUM);
		spin_lock_bh(&ast_base->ast_lock);
		sw_ast_entry = ath12k_dp_get_sw_ast_entry_by_index(dp->dp_hw_grp,
								   ast_index);
		if (!sw_ast_entry) {
			spin_unlock_bh(&ast_base->ast_lock);
			continue;
		}

		/* free the ast entry on reception of status */
		if (u32_get_bits(meta_data_0, ATH12K_AST_META_DATA_0_STATE) ==
				 AST_ENTRY_DELETE) {
			spin_unlock_bh(&ast_base->ast_lock);
			clear_bit(ATH12K_AST_ENTRY_TX_INVAL_STATUS,
				  &sw_ast_entry->ast_delete_invalidate_status);
			if (!sw_ast_entry->ast_delete_invalidate_status)
				ath12k_dp_free_ast_entry(dp->dp_hw_grp, ast_index);
			continue;
		}

		if (tx_cmd_seq_num != sw_ast_entry->tx_cmd_seq_num) {
			spin_unlock_bh(&ast_base->ast_lock);
			continue;
		}

		clear_bit(ATH12K_AST_ENTRY_TX_INVAL_STATUS,
			  &sw_ast_entry->ast_create_invalidate_status);

		if (!sw_ast_entry->ast_create_invalidate_status)
			complete(&dp_hw_grp_wifi8->peer_init_done);

		spin_unlock_bh(&ast_base->ast_lock);
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);
	return quota - budget;
}

void ath12k_mec_entry_expire_handler(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
				     void *arg)
{
	struct ath12k_dp_global_ast_table *ast_base = NULL;
	struct ath12k_dp_hw_group *dp_hw_grp = NULL;
	struct ath12k_ast_entry *mec_entry, *tmp;
	DECLARE_BITMAP(delete_mec_list_bmap, MAX_NUM_AST_ENTRIES);
	unsigned long i;

	bitmap_zero(delete_mec_list_bmap, MAX_NUM_AST_ENTRIES);

	if (!dp_hw_grp_wifi8 || !dp_hw_grp_wifi8->cumac_dp)
		return;

	dp_hw_grp = ath12k_get_dp_hw_group(dp_hw_grp_wifi8);
	ast_base = ath12k_dp_get_global_ast_table(dp_hw_grp);
	spin_lock_bh(&ast_base->ast_lock);
	if (list_empty(&dp_hw_grp_wifi8->mec_entry_list_head)) {
		spin_unlock_bh(&ast_base->ast_lock);
		return;
	}

	list_for_each_entry_safe(mec_entry, tmp,
				 &dp_hw_grp_wifi8->mec_entry_list_head, mec_list) {
		if (mec_entry->ast_entry_flags & ATH12K_AST_ENTRY_IS_ACTIVE_MEC)
			mec_entry->ast_entry_flags &= ~ATH12K_AST_ENTRY_IS_ACTIVE_MEC;
		else
			set_bit(mec_entry->ast_index, delete_mec_list_bmap);
	}
	spin_unlock_bh(&ast_base->ast_lock);

	for_each_set_bit(i, delete_mec_list_bmap, MAX_NUM_AST_ENTRIES)
		ath12k_dp_ast_entry_delete(dp_hw_grp, i);
}

int ath12k_mec_entry_keep_alive_update(struct ath12k_dp_hw_group *dp_hw_grp,
				       u8 *mac_addr)
{
	struct ath12k_dp_global_ast_table *ast_base =
					ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct ath12k_ast_entry *sw_ast_entry = NULL;

	spin_lock_bh(&ast_base->ast_lock);
	sw_ast_entry = ath12k_ast_entry_find_by_addr(dp_hw_grp, mac_addr);
	if (!sw_ast_entry) {
		ath12k_err(NULL, "unable to find the mec entry %pM ", mac_addr);
		spin_unlock_bh(&ast_base->ast_lock);
		return -EBUSY;
	}
	if (!(sw_ast_entry->ast_entry_flags & ATH12K_AST_ENTRY_IS_MEC)) {
		ath12k_err(NULL, "AST entry is not mec entry %pM ", mac_addr);
		spin_unlock_bh(&ast_base->ast_lock);
		return -EBUSY;
	}
	if (list_empty(&sw_ast_entry->mec_list)) {
		ath12k_err(NULL, "mec entry %pM is aleady delinked", mac_addr);
		spin_unlock_bh(&ast_base->ast_lock);
		return -EBUSY;
	}
	sw_ast_entry->ast_entry_flags |= ATH12K_AST_ENTRY_IS_ACTIVE_MEC;
	spin_unlock_bh(&ast_base->ast_lock);

	return 0;
}

void ath12k_wifi8_clean_pending_ast_entries(struct ath12k_base *ab)
{
	int ast_index;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_global_ast_table *ast_base =
		ath12k_dp_get_global_ast_table(dp->dp_hw_grp);

	for  (ast_index = 0; ast_index < ast_base->num_ast_entries; ast_index++) {
		struct ath12k_ast_entry *sw_ast_entry =
			ast_base->ast_entries[ast_index];

		if (sw_ast_entry && sw_ast_entry->ast_delete_invalidate_status)
			ath12k_dp_free_ast_entry(dp->dp_hw_grp, ast_index);
	}
}
