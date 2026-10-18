// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/slab.h>
#include <linux/rcupdate.h>
#include "core.h"
#include "me.h"
#include "debug.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/me_snoop_extn.h"
#endif /* CPTCFG_QCN_EXTN */

void ath12k_print_me_configs(struct ath12k_me_db *me_db)
{
	u32 me_mode, igmp_mode;

	if (me_db->me_flags & ATH12K_ME_FLAGS_BIT_ME5)
		me_mode = 5;
	else if (me_db->me_flags & ATH12K_ME_FLAGS_BIT_ME6)
		me_mode = 6;
	else
		me_mode = 0;

	igmp_mode = (me_db->me_flags & ATH12K_ME_FLAGS_BIT_IGMP_EN) ? 1 : 0;

	ath12k_dbg(NULL, ATH12K_DBG_CFG, "ME mode: %u IGMP ME mode: %u grp_limit: %u\n",
		   me_mode, igmp_mode, me_db->grp_limit);
}

/**
 * ath12k_me_db_reset(): Multicast Enhancement database reset
 * @db - ME Database
 */
void ath12k_me_db_reset(struct ath12k_me_db *db)
{
	if (!db)
		return;

	/*
	 * Reset the Lists, flags and limits.
	 */
	spin_lock_bh(&db->lock);
	ath12k_me_hmmc_list_reset(db);
	db->me_flags = 0;
	db->grp_limit = 0;
	spin_unlock_bh(&db->lock);
}

static void ath12k_me_db_free_rcu(struct rcu_head *rcu)
{
	struct ath12k_me_db *db = container_of(rcu, struct ath12k_me_db, rcu_head);

	spin_lock_bh(&db->lock);
	ath12k_me_hmmc_list_flush(db);
#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
#ifdef CPTCFG_QCN_EXTN
	ath12k_me_snoop_list_flush_extn(&db->snoop);
#endif /* CPTCFG_QCN_EXTN */
#endif
	spin_unlock_bh(&db->lock);

	ath12k_me_db_reset(db);
	kfree(db);
}

/**
 * ath12k_me_db_free(): Free the database
 * @ref - Reference pointer of ME database
 */
void ath12k_me_db_free(struct kref *ref)
{
	struct ath12k_me_db *db = container_of(ref, struct ath12k_me_db, ref);

	/* Schedule cleanup and free via RCU callback to ensure all readers are done */
	call_rcu(&db->rcu_head, ath12k_me_db_free_rcu);
}

struct ath12k_me_db *ath12k_me_db_get(struct ath12k_dp_vif *dp_vif)
{
	struct ath12k_me_db *db;

	rcu_read_lock();
	db = rcu_dereference(dp_vif->me_db);
	if (db && !kref_get_unless_zero(&db->ref))
		db = NULL;
	rcu_read_unlock();

	return db;
}

void ath12k_me_db_put(struct ath12k_me_db *db)
{
	if (db)
		kref_put(&db->ref, ath12k_me_db_free);
}

/**
 * ath12k_me_db_deinit(): Multicast Enhancement database deinit
 * @ahvif - Pointer to virtual interface structure
 *
 * Return: status of success or failure.
 */
int ath12k_me_db_deinit(struct ath12k_dp_vif *dp_vif)
{
	struct ath12k_me_db *db;

	if (!dp_vif)
		return -EINVAL;

	/* Prevents new readers from accessing the database */
	db = rcu_replace_pointer(dp_vif->me_db, NULL, true);
	if (!db)
		return 0;

	/* Drop our reference; final free happens in free callback */
	ath12k_me_db_put(db);

	return 0;
}

/**
 * ath12k_me_db_init(): Multicast Enhancement database init
 * @ahvif - Pointer to virtual interface structure
 *
 * Return: status of success or failure.
 */
int ath12k_me_db_init(struct ath12k_dp_vif *dp_vif)
{
	struct ath12k_me_db *db;

	if (!dp_vif)
		return -EINVAL;

	db = kzalloc(sizeof(*db), GFP_KERNEL);
	if (!db)
		return -ENOMEM;

	kref_init(&db->ref);

	/*
	 * Initialize the spin lock for ME DB protection
	 */
	spin_lock_init(&db->lock);

	ath12k_me_db_reset(db);

	/*
	 * Initialize the HMMC list here.
	 */
	ath12k_me_hmmc_list_init(db);

#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
	/* Allocate snoop cache (single block) and map buckets */
#ifdef CPTCFG_QCN_EXTN
	ath12k_me_snoop_list_init_extn(&db->snoop);
#endif /* CPTCFG_QCN_EXTN */
#endif

	rcu_assign_pointer(dp_vif->me_db, db);

	return 0;
}
EXPORT_SYMBOL(ath12k_me_db_init);
