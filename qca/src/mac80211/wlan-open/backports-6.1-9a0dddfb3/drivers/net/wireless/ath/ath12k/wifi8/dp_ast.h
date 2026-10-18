/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_AST_H
#define ATH12K_DP_AST_H

#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/hashtable.h>

#define TOEPLITZ_KEYLEN 9
#define INPUT_HASH_KEY_LEN 7
#define ATH12K_AST_HASH_KEY_1 0x4ab61d84
#define ATH12K_AST_HASH_KEY_2 0xdab4adb4
#define ATH12K_AST_HASH_KEY_3 0
#define MAX_NUM_AST_ENTRIES 2048
#define MAX_SKID_LEN	16

struct ath12k_dp_hw_group;

#define ATH12K_INVALID_AST_INDEX		0xFFFF
#define ATH12K_AST_ENTRY_WILDCARD_LINK_ID	0x7
#define ATH12K_NUM_TX_CLASSIFY_BANKS		4
#define ATH12K_MAX_TX_ASE_CMD_SEQ_NUM		0x7FFF
#define ATH12K_MEC_TIMEOUT_MS			5000

/* TODO: Enable ASE RX cache once HW offload for intra-BSS traffic is available */
#define ATH12K_ASE_RX_CACHE_EN	0
#define ATH12K_AST_INDEX_SHIFT			0x4
#define ATH12K_AST_HASH_MASK			0xF

struct ath12k_dp_hw_group;
struct ath12k_dp;
enum hal_wbm_tqm_rel_reason;
struct ath12k_dp_hw_group_wifi8;
struct ath12k_base;

enum ATH12K_AST_ENTRY_FLAGS {
	ATH12K_AST_ENTRY_EMPTY_FLAGS		= 0x0,
	ATH12K_AST_ENTRY_IS_VALID		= 0x1,
	ATH12K_AST_ENTRY_IS_MCAST		= 0x2,
	ATH12K_AST_ENTRY_IS_MEC			= 0x4,
	ATH12K_AST_ENTRY_IS_USE_ADDRX		= 0x8,
	ATH12K_AST_ENTRY_IS_ACTIVE_MEC		= 0x10,
	ATH12K_AST_ENTRY_IS_AP_BSS_ENTRY	= 0x20,
};

enum ATH12K_AST_ENTRY_INVALIDATE_STATUS_FLAGS {
	ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_0,
	ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_1,
	ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_2,
	ATH12K_AST_ENTRY_RX_INVAL_STATUS_CHIP_3,
	ATH12K_AST_ENTRY_TX_INVAL_STATUS,
};

struct ath12k_ast_entry_config_params {
	u8 mac_addr[ETH_ALEN];
	u16 peer_id;
	dma_addr_t tx_classify_info_paddr;
	u16 ast_index;
	u16 ast_hash;
	u8 mld_id;
	u8 ast_entry_flags;
};

struct ath12k_ast_entry {
	u8 mac_addr[ETH_ALEN];
	u16 peer_id;
	dma_addr_t tx_classify_info_paddr;
	u16 ast_index;
	u16 ast_hash;
	u8 mld_id;
	u8 ast_entry_flags;
	/* peer addr based rhashtable list pointer */
	struct hlist_node hash_node;
	bool hash_added;
	unsigned long ast_create_invalidate_status;
	unsigned long ast_delete_invalidate_status;
	u16 tx_cmd_seq_num;
	struct list_head mec_list;
};

struct ath12k_dp_ast_hash_keys {
	u32 ase_hash_key1;
	u32 ase_hash_key2;
	u32 ase_hash_key3;
};

struct ath12k_dp_global_ast_stats {
	u32 num_ast_entry_create_attempted;
	u32 num_ast_entry_delete;

	/* failure stats */
	u16 invalid_hw_ast_entry;
	u16 delete_in_progress;
	u16 no_free_slot;
	u16 alloc_fail;
	u16 dup_entry_found;
	u16 hw_sync_fail;
	u16 sw_ast_not_found;
	u16 hw_ast_not_found;
};

/* Hash table size: 2^11 = 2048 buckets for up to 2048 max AST entries */
#define ATH12K_AST_HASH_BITS 11

struct ath12k_dp_global_ast_table {
	/* Memory allocation for AST entries.
	 * Will be programming this address in HW.
	 */
	void *ast_vaddr_aligned;
	void *ast_vaddr_unaligned;
	dma_addr_t ast_paddr;
	size_t hw_ast_table_size;
	/* global ast lock */
	spinlock_t ast_lock;
	struct ath12k_dp_ast_hash_keys hash_keys;
	u32 ast_hash_mask;
	u16 num_ast_entries;
	u8 skid_len;
	u8 ase_tx_cache_en:1,
	   ase_rx_cache_en:1,
	   reserved:6;

	struct ath12k_ast_entry **ast_entries;
	spinlock_t ast_hash_lock;
	DECLARE_HASHTABLE(ast_hash, ATH12K_AST_HASH_BITS);
	struct ath12k_dp_global_ast_stats ast_stats;
};

enum ath12k_ase_cache_op {
	ATH12K_INVALIDATE_ALL,
	ATH12K_INVALIDATE_ENTRY,
};

enum ast_entry_op_state {
	AST_ENTRY_DELETE =      0,
	AST_ENTRY_CREATE =      1,
};

#define ATH12K_AST_META_DATA_0_AST_INDEX	GENMASK(15, 0)
#define ATH12K_AST_META_DATA_0_TX_CMD_SEQ_NUM	GENMASK(30, 16)
#define ATH12K_AST_META_DATA_0_STATE		BIT(31)
struct ath12k_ase_cache_op_param {
	int cmd;
	u32 meta_data_0;
	u16 ast_index;
	u32 mac_addr_31_0;
	u16 mac_addr_47_32;
	u16 is_mcast:1,
	   is_mec:1,
	   ad1_match:1,
	   link_id:3,
	   chip_id_bitmap:8,
	   reserved_0:2;
};

int ath12k_wifi8_invalidate_peer_ase_cache_table(struct ath12k_dp_hw_group *dp_hw_grp);
int ath12k_wifi8_dp_tx_cmd_status_handler(struct ath12k_dp *dp,
					  int budget);
bool ath12k_wifi8_dp_ase_tx_cache_enabled(struct ath12k_dp_hw_group *dp_hw_grp);
bool ath12k_wifi8_dp_ase_rx_cache_enabled(struct ath12k_dp_hw_group *dp_hw_grp);
int ath12k_wifi8_dp_rx_ase_cmd_status_handler(struct ath12k_dp *dp, int budget);
int ath12k_dp_ast_table_alloc(struct ath12k_dp *dp);
int ath12k_dp_ast_table_init(struct ath12k_dp_hw_group *dp_hw_grp);
void ath12k_dp_ast_table_deinit(struct ath12k_dp_hw_group *dp_hw_grp);
void ath12k_dp_ast_table_free(struct ath12k_dp_hw_group *dp_hw_grp);
int ath12k_dp_ast_entry_create(struct ath12k_dp_hw_group *dp_hw_grp,
			       struct ath12k_ast_entry_config_params *param);
void ath12k_dp_ast_entry_delete(struct ath12k_dp_hw_group *dp_hw_grp,
				u16 ast_index);
void ath12k_mec_entry_expire_handler(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
				     void *arg);
int ath12k_mec_entry_keep_alive_update(struct ath12k_dp_hw_group *dp_hw_grp,
				       u8 *mac_addr);
void ath12k_wifi8_global_ast_stats_reset(struct ath12k_dp *dp);
ssize_t ath12k_wifi8_global_ast_stats(struct ath12k_dp *dp, char *buf, int size);
void ath12k_wifi8_clean_pending_ast_entries(struct ath12k_base *ab);

#endif
