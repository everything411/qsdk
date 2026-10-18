/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <ppe_mcast.h>
#include "ppe_mcast.h"
#include <linux/jhash.h>

struct ppe_mcast gbl_ppe_mcast = {0};

/*
 * ppe_mcast_hash_key()
 *	Generate hash key for multicast entry.
 *	SIP is included in the hash when sip_valid is set to correctly
 *	distinguish SSM entries from ASM entries with the same (GIP, port).
 */
static u32 ppe_mcast_hash_key(struct ppe_mcast_hash_key *key)
{
	u32 hash;

	if (key->is_v4) {
		hash = jhash_1word(key->gip.v4, 0);
		if (key->sip_valid)
			hash = jhash_1word(key->sip.v4, hash);
	} else {
		hash = jhash2(key->gip.v6, 4, 0);
		if (key->sip_valid)
			hash = jhash2(key->sip.v6, 4, hash);
	}

	return jhash_1word(key->port_id, hash);
}

/*
 * ppe_mcast_hash_find()
 *	Find multicast entry in hash table.
 *	Matches on (port, is_v4, sip_valid, GIP, SIP) so that SSM entries
 *	(sip_valid=true) and ASM entries (sip_valid=false) for the same
 *	(GIP, port) are treated as distinct entries.
 */
static struct ppe_mcast_entry *ppe_mcast_hash_find(struct ppe_mcast_hash_key *key)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	struct ppe_mcast_entry *entry;
	u32 hash = ppe_mcast_hash_key(key);

	hash_for_each_possible(g_mcast->mc_hash_table, entry, hash_node, hash) {
		if (entry->entry.port != key->port_id ||
			entry->entry.is_v4 != key->is_v4 ||
			entry->entry.sip_valid != key->sip_valid)
			continue;

		if (key->is_v4) {
			if (entry->entry.gip.v4 != key->gip.v4)
				continue;
			if (key->sip_valid && entry->entry.sip.v4 != key->sip.v4)
				continue;
		} else {
			if (!ppe_drv_mcast_v6_addr_equal(entry->entry.gip.v6, key->gip.v6))
				continue;
			if (key->sip_valid && !ppe_drv_mcast_v6_addr_equal(entry->entry.sip.v6, key->sip.v6))
				continue;
		}

		return entry;
	}

	return NULL;
}

/*
 * ppe_mcast_hash_add()
 *	Add multicast entry to hash table.
 *	SIP is included in the key so SSM and ASM entries for the same
 *	(GIP, port) land in different buckets.
 */
static void ppe_mcast_hash_add(struct ppe_mcast_entry *entry)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	struct ppe_mcast_hash_key key;

	key.is_v4 = entry->entry.is_v4;
	key.sip_valid = entry->entry.sip_valid;
	if (key.is_v4) {
		key.gip.v4 = entry->entry.gip.v4;
		key.sip.v4 = entry->entry.sip.v4;
	} else {
		memcpy(key.gip.v6, entry->entry.gip.v6, sizeof(key.gip.v6));
		memcpy(key.sip.v6, entry->entry.sip.v6, sizeof(key.sip.v6));
	}
	key.port_id = entry->entry.port;

	hash_add(g_mcast->mc_hash_table, &entry->hash_node, ppe_mcast_hash_key(&key));
}

/*
 * ppe_mcast_hash_del()
 *	Remove multicast entry from hash table
 */
static void ppe_mcast_hash_del(struct ppe_mcast_entry *entry)
{
	hash_del(&entry->hash_node);
}


/*
 * ppe_mcast_get_group()
 *	Get multicast group.
 */
static struct ppe_mcast_group* ppe_mcast_get_group(union ppe_mcast_ip *ip, bool is_v4)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	struct ppe_mcast_group *mc_grp;

	if (!list_empty(&g_mcast->mc_grp_list)) {
		list_for_each_entry(mc_grp, &g_mcast->mc_grp_list, list) {
			if (is_v4) {
				if ((mc_grp->is_v4) && (ppe_drv_mcast_v4_addr_equal(mc_grp->gip.v4, ip->v4))) {
					return mc_grp;
				}
			} else if (!(mc_grp->is_v4) && (ppe_drv_mcast_v6_addr_equal(mc_grp->gip.v6, ip->v6))) {
				return mc_grp;
			}
		}
	}

	return NULL;
}

/*
 * ppe_mcast_delete_group()
 *	Delete multicast group.
 */
static void ppe_mcast_delete_group(struct ppe_mcast_group *mc_grp)
{
	/*
	 * If no supported entry, delete multicast group
	 */
	if (!mc_grp->unsupported_dst_cnt && !mc_grp->omci_not_managed && !mc_grp->supported_dst_cnt) {
		list_del(&mc_grp->list);
		kfree(mc_grp);
	}
}

/*
 * ppe_mcast_ssm_entry_exists()
 *	Check if any SSM entry (sip_valid=true) exists in the hash table for
 *	the given (GIP, port) combination.
 *
 *	Must be called with g_mcast->lock held.
 *
 *	This is used to suppress wildcard ASM hardware programming when SSM
 *	entries already cover the forwarding for the same (GIP, port).  The
 *	Linux bridge kernel creates both a per-source SSM entry and a
 *	group-level "ASM" entry (with source_list) for the same IGMPv3
 *	INCLUDE(S,G) membership.  Programming the ASM entry as a GIP-only
 *	hardware entry would incorrectly forward traffic from unintended
 *	sources.
 */
static bool ppe_mcast_ssm_entry_exists(union ppe_mcast_ip *gip, bool is_v4, int port_id)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	struct ppe_mcast_entry *entry;
	int i;

	hash_for_each(g_mcast->mc_hash_table, i, entry, hash_node) {
		if (entry->entry.port != port_id ||
			entry->entry.is_v4 != is_v4 ||
			!entry->entry.sip_valid)
			continue;

		if (is_v4 && entry->entry.gip.v4 == gip->v4)
			return true;

		if (!is_v4 && ppe_drv_mcast_v6_addr_equal(entry->entry.gip.v6, gip->v6))
			return true;
	}

	return false;
}

/*
 * ppe_mcast_delete_entry()
 *	Remove a multicast entry
 */
ppe_mcast_ret_t ppe_mcast_delete_entry(struct ppe_mcast_entry_info *info)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	struct ppe_mcast_group *mc_grp = NULL;
	struct ppe_mcast_entry *mc_entry = NULL;
	struct ppe_drv_omci_port_node *node = NULL;
	struct net_device *dev = NULL;
	int port_id;

	/*
	 * Check if multicast group does not exist
	 */
	spin_lock_bh(&g_mcast->lock);
	mc_grp = ppe_mcast_get_group(&info->gip, info->is_v4);
	if (!mc_grp) {
		ppe_mcast_stats_inc(&g_mcast->stats.mcast_delete_entry_fail);
		spin_unlock_bh(&g_mcast->lock);
		ppe_mcast_warn("multicast group deletion failed for dev %s", info->dev);
		return PPE_MCAST_MC_ENTRY_DELETE_FAIL;
	}

	/*
	 * Check if destinaton info L2 physical port
	 */
	dev = dev_get_by_name(&init_net, info->dev);
	if (!dev) {
		ppe_mcast_stats_inc(&g_mcast->stats.mcast_delete_entry_fail);
		spin_unlock_bh(&g_mcast->lock);
		ppe_mcast_warn("multicast group deletion failed for dev %s, invalid device", info->dev);
		return PPE_MCAST_MC_ENTRY_DELETE_FAIL;
	}

	port_id = ppe_drv_port_num_from_dev(dev);
	if ((port_id < 0) || (port_id >= PPE_DRV_PHYSICAL_MAX)) {
		mc_grp->unsupported_dst_cnt--;
		goto update_mc_group;
	}

	/*
	 * Check if destinaton info L2 managed by OMCI
	 */
	node = ppe_drv_port_mgmt_omci_port_find_and_ref(dev->name);
	if (!node) {
		mc_grp->omci_not_managed--;
		ppe_mcast_trace("Port:%d not managed by OMCI", port_id);
		goto update_mc_group;
	}

	ppe_drv_port_mgmt_omci_port_deref(node);

	/*
	 * Find entry using hash table.
	 * Key includes SIP so that SSM and ASM entries for the same (GIP, port)
	 * are looked up independently.
	 */
	struct ppe_mcast_hash_key key;
	key.is_v4 = info->is_v4;
	key.sip_valid = info->sip_enabled;
	if (key.is_v4) {
		key.gip.v4 = info->gip.v4;
		key.sip.v4 = info->sip.v4;
	} else {
		memcpy(key.gip.v6, info->gip.v6, sizeof(key.gip.v6));
		memcpy(key.sip.v6, info->sip.v6, sizeof(key.sip.v6));
	}
	key.port_id = port_id;

	mc_entry = ppe_mcast_hash_find(&key);
	if (!mc_entry) {
		dev_put(dev);
		ppe_mcast_stats_inc(&g_mcast->stats.mcast_delete_entry_fail);
		spin_unlock_bh(&g_mcast->lock);
		ppe_mcast_warn("multicast group deletion failed for dev %s, mcast entry does not exist", info->dev);
		return PPE_MCAST_MC_ENTRY_DELETE_FAIL;
	}

	/*
	 * Delete entry from PPE
	 */
	if (ppe_drv_mcast_entry_delete(&mc_entry->entry) != PPE_DRV_RET_SUCCESS) {
		dev_put(dev);
		spin_unlock_bh(&g_mcast->lock);
		ppe_mcast_warn("multicast entry deletion failed in ppe for dev %s", info->dev);
		return PPE_MCAST_MC_ENTRY_CREATE_FAIL;
	}

	/*
	 * Deref the OMCI node, refernce taken during addition
	 */
	if (mc_entry->node) {
		ppe_drv_port_mgmt_omci_port_deref(mc_entry->node);
		mc_entry->node = NULL;
	}

	mc_grp->supported_dst_cnt--;
	ppe_mcast_hash_del(mc_entry);
	kfree(mc_entry);

	ppe_mcast_delete_group(mc_grp);

	dev_put(dev);
	ppe_mcast_stats_inc(&g_mcast->stats.mcast_delete_entry_success);
	spin_unlock_bh(&g_mcast->lock);
	return PPE_MCAST_SUCCESS;

update_mc_group:
	/*
	 * If destination entry is an unsupported port (virtual: wlan or DSA interface),
	 * check if multicast group already exists, if unsupported interface count is 0,
	 * then add all other entries in that group to forward
	 */
	if (!mc_grp->unsupported_dst_cnt && !mc_grp->omci_not_managed) {
		/*
		 * Re-add all entries for this group from hash table
		 */
		struct ppe_mcast_entry *entry;
		int i;

		hash_for_each(g_mcast->mc_hash_table, i, entry, hash_node) {
			if (entry->entry.is_v4 == mc_grp->is_v4) {
				if ((entry->entry.is_v4 && entry->entry.gip.v4 == mc_grp->gip.v4) ||
					(!entry->entry.is_v4 && ppe_drv_mcast_v6_addr_equal(entry->entry.gip.v6, mc_grp->gip.v6))) {
						ppe_drv_mcast_entry_add(&entry->entry);
				}
			}
		}
	}

	dev_put(dev);
	spin_unlock_bh(&g_mcast->lock);
	return PPE_MCAST_SUCCESS;
}
EXPORT_SYMBOL(ppe_mcast_delete_entry);

/*
 * ppe_mcast_create_entry()
 *	Create a multicast entry
 */
ppe_mcast_ret_t ppe_mcast_create_entry(struct ppe_mcast_entry_info *info)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	struct ppe_mcast_group *mc_grp = NULL;
	struct ppe_mcast_entry *mc_entry = NULL;
	struct ppe_drv_omci_port_node *node = NULL;
	struct net_device *dev = NULL;
	int port_id;
	bool is_added = false;

	/*
	 * Check if multicast group already exists, add entry to that group
	 * else create a multicast group and add entry
	 */
	spin_lock_bh(&g_mcast->lock);
	mc_grp = ppe_mcast_get_group(&info->gip, info->is_v4);
	if (!mc_grp) {
		mc_grp = kzalloc(sizeof(struct ppe_mcast_group), GFP_ATOMIC);
		if (!mc_grp) {
			ppe_mcast_stats_inc(&g_mcast->stats.mcast_create_entry_fail);
			spin_unlock_bh(&g_mcast->lock);
			ppe_mcast_warn("multicast group creation failed for dev %s", info->dev);
			return PPE_MCAST_MC_ENTRY_CREATE_FAIL;
		}

		mc_grp->is_v4 = info->is_v4;
		mc_grp->gip.v4 = info->gip.v4;
		memcpy(mc_grp->gip.v6, info->gip.v6, sizeof(mc_grp->gip.v6));

		list_add(&mc_grp->list, &g_mcast->mc_grp_list);
	}

	/*
	 * Check if destinaton info L2 physical port
	 */
	dev = dev_get_by_name(&init_net, info->dev);
	if (!dev) {
		ppe_mcast_stats_inc(&g_mcast->stats.mcast_create_entry_fail);
		spin_unlock_bh(&g_mcast->lock);
		ppe_mcast_warn("multicast group deletion failed for dev %s, invalid device", info->dev);
		return PPE_MCAST_MC_ENTRY_CREATE_FAIL;
	}

	port_id = ppe_drv_port_num_from_dev(dev);
	if ((port_id < 0) || (port_id >= PPE_DRV_PHYSICAL_MAX)) {
		mc_grp->unsupported_dst_cnt++;
		goto update_mc_group;
	}

	/*
	 * Check if destinaton info L2 managed by OMCI
	 */
	node = ppe_drv_port_mgmt_omci_port_find_and_ref(dev->name);
	if (!node) {
		mc_grp->omci_not_managed++;
		ppe_mcast_trace("Port:%d not managed by OMCI", port_id);
		goto update_mc_group;
	}

	/*
	 * If this is an ASM (no SIP) entry and SSM entries already exist for
	 * the same (GIP, port), do not program a wildcard hardware entry.
	 *
	 * The Linux bridge kernel creates both a per-source SSM entry and a
	 * group-level "ASM" entry (filter_mode include, source_list) for the
	 * same IGMPv3 INCLUDE(S,G) membership.  The ASM entry is NOT a true
	 * wildcard — it represents the same source-specific membership.
	 * Programming it as a GIP-only hardware entry would incorrectly
	 * forward traffic from unintended sources.
	 */
	if (!info->sip_enabled && ppe_mcast_ssm_entry_exists(&info->gip, info->is_v4, port_id)) {
		ppe_drv_port_mgmt_omci_port_deref(node);
		dev_put(dev);
		ppe_mcast_delete_group(mc_grp);
		spin_unlock_bh(&g_mcast->lock);
		ppe_mcast_info("Skipping ASM entry for dev %s port %d - SSM entries exist for group",
			info->dev, port_id);
		return PPE_MCAST_SUCCESS;
	}

	/*
	 * If this is an SSM (SIP) entry and an ASM (wildcard) entry already
	 * exists in hardware for the same (GIP, port), evict the ASM entry
	 * from both hardware and the hash table.  A wildcard GIP entry must
	 * not coexist with an SSM entry for the same (GIP, port) as it would
	 * incorrectly forward traffic from unintended sources.
	 */
	if (info->sip_enabled) {
		struct ppe_mcast_hash_key asm_key;
		struct ppe_mcast_entry *asm_entry;

		asm_key.is_v4 = info->is_v4;
		asm_key.sip_valid = false;
		asm_key.port_id = port_id;
		if (asm_key.is_v4) {
			asm_key.gip.v4 = info->gip.v4;
			asm_key.sip.v4 = 0;
		} else {
			memcpy(asm_key.gip.v6, info->gip.v6, sizeof(asm_key.gip.v6));
			memset(asm_key.sip.v6, 0, sizeof(asm_key.sip.v6));
		}

		asm_entry = ppe_mcast_hash_find(&asm_key);
		if (asm_entry) {
			ppe_mcast_info("Evicting ASM entry for dev %s port %d - SSM entry being added",
				info->dev, port_id);
			ppe_drv_mcast_entry_delete(&asm_entry->entry);
			if (asm_entry->node) {
				ppe_drv_port_mgmt_omci_port_deref(asm_entry->node);
				asm_entry->node = NULL;
			}
			mc_grp->supported_dst_cnt--;
			ppe_mcast_hash_del(asm_entry);
			kfree(asm_entry);
		}
	}

	/*
	 * Check if entry already exists using hash table.
	 * Key includes SIP so that SSM and ASM entries for the same (GIP, port)
	 * are looked up independently.
	 */
	struct ppe_mcast_hash_key key;
	key.is_v4 = info->is_v4;
	key.sip_valid = info->sip_enabled;
	if (key.is_v4) {
		key.gip.v4 = info->gip.v4;
		key.sip.v4 = info->sip.v4;
	} else {
		memcpy(key.gip.v6, info->gip.v6, sizeof(key.gip.v6));
		memcpy(key.sip.v6, info->sip.v6, sizeof(key.sip.v6));
	}
	key.port_id = port_id;

	mc_entry = ppe_mcast_hash_find(&key);
	if (mc_entry) {
		/*
		 * Entry exists - delete from hardware first
		 */
		ppe_mcast_info("Replacing existing multicast entry for dev %s port %d, group %pI4h",
			info->dev, port_id, &info->gip.v4);

		/*
		 * Delete entry from PPE
		 */
		if (ppe_drv_mcast_entry_delete(&mc_entry->entry) != PPE_DRV_RET_SUCCESS) {
			dev_put(dev);
			spin_unlock_bh(&g_mcast->lock);
			ppe_mcast_warn("Failed to delete existing entry for replacement");
			return PPE_MCAST_MC_ENTRY_CREATE_FAIL;
		}

		/*
		 * Remove from hash table
		 */
		mc_grp->supported_dst_cnt--;
		ppe_mcast_hash_del(mc_entry);
		kfree(mc_entry);
	}

	mc_entry = kzalloc(sizeof(struct ppe_mcast_entry), GFP_ATOMIC);
	if (!mc_entry) {
		dev_put(dev);
		ppe_mcast_delete_group(mc_grp);
		ppe_mcast_stats_inc(&g_mcast->stats.mcast_create_entry_fail);
		spin_unlock_bh(&g_mcast->lock);
		ppe_mcast_warn("multicast entry creation failed for dev %s", info->dev);
		return PPE_MCAST_MC_ENTRY_CREATE_FAIL;
	}

	mc_entry->node = node;
	mc_entry->entry.is_v4 = info->is_v4;
	mc_entry->entry.sip_valid = info->sip_enabled;
	if (info->is_v4) {
		mc_entry->entry.sip.v4 = info->sip.v4;
		mc_entry->entry.gip.v4 = info->gip.v4;
	} else {
		memcpy(mc_entry->entry.sip.v6, info->sip.v6, sizeof(mc_entry->entry.sip.v6));
		memcpy(mc_entry->entry.gip.v6, info->gip.v6, sizeof(mc_entry->entry.gip.v6));
	}

	mc_entry->entry.vlan_valid = info->vlan_enabled;
	mc_entry->entry.vid = info->vlan_id;
	mc_entry->entry.port = port_id;
	mc_entry->entry.fwd_cmd = PPE_DRV_MCAST_FWD_CMD_FWD;

	/*
	 * Configure multicast entry in PPE
	 */
	if (!mc_grp->unsupported_dst_cnt && !mc_grp->omci_not_managed) {
		if (ppe_drv_mcast_entry_add(&mc_entry->entry) != PPE_DRV_RET_SUCCESS) {
			dev_put(dev);
			ppe_mcast_delete_group(mc_grp);
			ppe_mcast_stats_inc(&g_mcast->stats.mcast_create_entry_fail);
			spin_unlock_bh(&g_mcast->lock);
			ppe_mcast_warn("multicast entry creation failed in ppe for dev %s", info->dev);
			return PPE_MCAST_MC_ENTRY_CREATE_FAIL;
		}
		is_added = true;
	}

	mc_grp->supported_dst_cnt++;

	/*
	 * Add to hash table for supported/unsupported port
	 */
	ppe_mcast_hash_add(mc_entry);
	dev_put(dev);

	if (is_added) {
		ppe_mcast_stats_inc(&g_mcast->stats.mcast_create_entry_success);
		spin_unlock_bh(&g_mcast->lock);

		ppe_mcast_info("multicast entry creation success for dev %s", info->dev);
		return PPE_MCAST_SUCCESS;
	} else {
		ppe_mcast_stats_inc(&g_mcast->stats.mcast_create_entry_fail);
		spin_unlock_bh(&g_mcast->lock);
		ppe_mcast_warn("multicast entry creation failed for dev %s, unsupported interface present in group", info->dev);
		return PPE_MCAST_MC_ENTRY_CREATE_FAIL;
	}

update_mc_group:
	/*
	 * If destination entry is an unsupported port (virtual: wlan or DSA interface),
	 * delete the entries for supported ports from PPE
	 */
	dev_put(dev);

	/*
	 * Delete all MC entries from PPE using hash table
	 */
	struct ppe_mcast_entry *entry;
	int i;

	hash_for_each(g_mcast->mc_hash_table, i, entry, hash_node) {
		if (entry->entry.is_v4 == mc_grp->is_v4) {
			if ((entry->entry.is_v4 && entry->entry.gip.v4 == mc_grp->gip.v4) ||
				(!entry->entry.is_v4 && ppe_drv_mcast_v6_addr_equal(entry->entry.gip.v6, mc_grp->gip.v6))) {
				if (entry->node) {
					ppe_drv_mcast_entry_delete(&entry->entry);
				}
			}
		}
	}

	ppe_mcast_delete_group(mc_grp);
	ppe_mcast_stats_inc(&g_mcast->stats.mcast_create_entry_fail);
	spin_unlock_bh(&g_mcast->lock);
	ppe_mcast_warn("multicast entry creation failed for dev %s, unsupported interface or port not managed by OMCI", info->dev);
	return PPE_MCAST_MC_ENTRY_CREATE_FAIL;
}
EXPORT_SYMBOL(ppe_mcast_create_entry);

/*
 * ppe_mcast_deinit()
 *	Multicast deinit API
 */
void ppe_mcast_deinit(void)
{
	ppe_mcast_stats_debugfs_exit();
	ppe_mcast_dump_exit();
}

/*
 * ppe_mcast_init()
 *	Multicast init API
 */
void ppe_mcast_init(struct dentry *d_rule)
{
	/* Multicast global config*/
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;

	spin_lock_init(&g_mcast->lock);
	INIT_LIST_HEAD(&g_mcast->mc_grp_list);

	/* Initialize hash table */
	hash_init(g_mcast->mc_hash_table);

	ppe_mcast_stats_debugfs_init(d_rule);
	ppe_mcast_dump_init(d_rule);
}
