/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
*/
#include <linux/hashtable.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/jhash.h>

#include "sdc.h"

extern struct sdc_ctx sdc_ctx;

/*
 * Consider only the higher 3 bytes for calculating the
 * hash from mac.
 */
static inline unsigned long sdc_macdb_hash(const uint8_t *mac)
{
	return jhash(mac, 6, sdc_ctx.hash_seed);
}

/*
 * Timer which triggers pushing telemetry to userspace for a device.
 */
static void sdc_macdb_timer_cb(struct timer_list *t)
{
	struct sdc_macdb_entry *entry = from_timer(entry, t, timer);
	struct sdc_telemetry_stats_val sv;
	struct sdc_telemetry_tlv tlv = {0};

	DEBUG_INFO("%pM:Timer triggered moving the device info to userspace tx queue\n", entry->mac);

	/*
	 * Serialize per-device counters into a STATS TLV before sending.
	 * Per-type drop_* fields indicate which protocol's data is incomplete.
	 */
	sv.dhcp_discover_count   = atomic_read(&entry->dhcp_discover_count);
	sv.dhcp_offer_count      = atomic_read(&entry->dhcp_offer_count);
	sv.dhcp_ack_count        = atomic_read(&entry->dhcp_ack_count);
	sv.dhcp_nak_count        = atomic_read(&entry->dhcp_nak_count);
	sv.drop_dhcp             = atomic_read(&entry->drop_dhcp);
	sv.dns_response_count    = atomic_read(&entry->dns_response_count);
	sv.dns_nxdomain_count    = atomic_read(&entry->dns_nxdomain_count);
	sv.dns_servfail_count    = atomic_read(&entry->dns_servfail_count);
	sv.drop_dns              = atomic_read(&entry->drop_dns);
	sv.drop_mdns             = atomic_read(&entry->drop_mdns);
	sv.ssdp_pkt_count        = atomic_read(&entry->ssdp_pkt_count);
	sv.drop_ssdp             = atomic_read(&entry->drop_ssdp);
	sv.tls_ch_count          = atomic_read(&entry->tls_ch_count);
	sv.drop_tls              = atomic_read(&entry->drop_tls);
	sv.quic_initial_count    = atomic_read(&entry->quic_initial_count);
	sv.quic_version_bitmap   = atomic_read(&entry->quic_version_bitmap);
	sv.drop_quic             = atomic_read(&entry->drop_quic);

	tlv.type  = SDC_TELEMETRY_TYPE_STATS;
	tlv.len   = sizeof(sv) + SDC_TELEMETRY_TLV_MDATA_SZ;
	tlv.value = (uint8_t *)&sv;
	sdc_telemetry_add_tlv(entry->data, &tlv, sdc_ctx.buf_size);

	entry->state = SDC_MACDB_MAC_STATE_COLLECTED;

	/*
	 * Send out the buffer to the telemetry queue.
	 */
	if (!sdc_telemetry_enqueue(&sdc_ctx.tele_ctx, entry->data)) {
		atomic_inc(&sdc_ctx.tele_queue_drop);
		DEBUG_WARN("%pM: telemetry queue full, device buffer dropped\n", entry->mac);
		kfree(entry->data);
	}
	entry->data = NULL;
}

/*
 * We dont need ref_inc as its done internally.
 */
void sdc_macdb_deref(struct sdc_macdb_ctx *ctx, struct sdc_macdb_entry *entry) {
	if (refcount_dec_and_test(&entry->ref)) {
		kfree(entry->data);
		kfree(entry);
	}
}

/*
 * Look up a MAC DB entry without creating one.
 * Used by the egress hook to update counters for known devices only.
 * Returns entry with refcount incremented; caller must sdc_macdb_deref.
 */
struct sdc_macdb_entry *sdc_macdb_get(struct sdc_macdb_ctx *ctx, const uint8_t *mac)
{
	struct sdc_macdb_entry *entry;
	unsigned long key = sdc_macdb_hash(mac);

	hash_for_each_possible(ctx->htable, entry, hnode, key) {
		if (ether_addr_equal(entry->mac, mac)) {
			refcount_inc(&entry->ref);
			return entry;
		}
	}
	return NULL;
}

/*
 * Create MAC DB entry if not exist.
 */
struct sdc_macdb_entry *sdc_macdb_get_or_create(struct sdc_macdb_ctx *ctx, const uint8_t *smac)
{
	struct sdc_macdb_entry *entry;
	unsigned long key = sdc_macdb_hash(smac);

	hash_for_each_possible(ctx->htable, entry, hnode, key) {
		if (ether_addr_equal(entry->mac, smac)) {
			refcount_inc(&entry->ref);
			return entry;
		}
	}

	entry = kzalloc(sizeof(struct sdc_macdb_entry), GFP_ATOMIC);
	if (!entry) {
		DEBUG_ERR("Macdb entry  alloc failed for %pM\n", smac);
		return NULL;
	}

	ether_addr_copy(entry->mac, smac);
	entry->state = SDC_MACDB_MAC_STATE_INPROGRESS;
	spin_lock_init(&entry->lock);

	/*
	 * TODO: See if this can come from a wrapper API from sdc_telemetry file.
	 */
	entry->data = kzalloc(sdc_ctx.buf_size, GFP_ATOMIC);
	if (!entry->data) {
		DEBUG_ERR("MACDB entry buffer alloc failed for %pM\n", smac);
		kfree(entry);
		return NULL;
	}


	/*
	 * Write the mac address to the buffer to be sent out to userspace.
	 */
	struct sdc_telemetry_buf *tbuf = entry->data;
	ether_addr_copy(tbuf->mac, entry->mac);

	/*
	 * The buffer to be sent to user already has
	 * 6 bytes of mac address and 2 bytes len field.
	 */
	tbuf->len = sizeof(struct sdc_telemetry_buf);

	/*
	 * Collection window — use the runtime-configurable timeout (seconds).
	 */
	timer_setup(&entry->timer, sdc_macdb_timer_cb, 0);
	mod_timer(&entry->timer, jiffies + sdc_ctx.collection_timeout * HZ);

	/*
	 * TODO:Figure out a way to add it only on one core,
	 * theres a possibility of two cores allocating the entry.
	 *
	 * We can just check once again before adding and then add it atomically.
	 */
	hash_add(ctx->htable, &entry->hnode, key);

	refcount_set(&entry->ref, 2);
	DEBUG_INFO("New MAC address %pM, added to db for tracking \n", tbuf->mac);
	return entry;
}

/*
 * MAC DB initialization
 */
void sdc_macdb_init(struct sdc_macdb_ctx *ctx)
{
	hash_init(ctx->htable);
	get_random_bytes(&sdc_ctx.hash_seed, sizeof(uint32_t));
}

/*
 * sdc_macdb_clear_collected
 *	Clear all COLLECTED MAC entries from the database.
 *
 * Only removes entries that have completed data collection (no active timers/buffers).
 * INPROGRESS entries are left untouched to avoid race conditions with active timers.
 */
void sdc_macdb_clear_collected(struct sdc_macdb_ctx *ctx)
{
	struct sdc_macdb_entry *entry;
	struct hlist_node *tmp;
	int bkt;

	spin_lock_bh(&sdc_ctx.lock);

	hash_for_each_safe(ctx->htable, bkt, tmp, entry, hnode) {
		if (entry->state == SDC_MACDB_MAC_STATE_COLLECTED) {
			hash_del(&entry->hnode);
			sdc_macdb_deref(ctx, entry);
		}
	}

	spin_unlock_bh(&sdc_ctx.lock);
	DEBUG_INFO("Cleared all COLLECTED MAC entries\n");
}

/*
 * sdc_macdb_del_by_mac
 *	Delete a specific MAC entry by address.
 *
 * Only deletes entries in COLLECTED state to avoid race conditions.
 * Returns 0 on success, -ENOENT if not found, -EBUSY if INPROGRESS.
 */
int sdc_macdb_del_by_mac(struct sdc_macdb_ctx *ctx, const uint8_t *mac)
{
	struct sdc_macdb_entry *entry;
	unsigned long key = sdc_macdb_hash(mac);
	int ret = -ENOENT;

	spin_lock_bh(&sdc_ctx.lock);

	hash_for_each_possible(ctx->htable, entry, hnode, key) {
		if (ether_addr_equal(entry->mac, mac)) {
			if (entry->state == SDC_MACDB_MAC_STATE_COLLECTED) {
				hash_del(&entry->hnode);
				sdc_macdb_deref(ctx, entry);
				ret = 0;
				DEBUG_INFO("Deleted MAC entry %pM\n", mac);
			} else {
				ret = -EBUSY;
				DEBUG_WARN("MAC address %pM is INPROGRESS, cannot delete\n", mac);
			}
			break;
		}
	}

	spin_unlock_bh(&sdc_ctx.lock);

	if (ret == -ENOENT) {
		DEBUG_WARN("MAC address %pM not found in database\n", mac);
	}

	return ret;
}

/*
 * MAC DB de-initialization
 */
void sdc_macdb_deinit(struct sdc_macdb_ctx *ctx)
{
	struct sdc_macdb_entry *entry;
	int bkt;
	hash_for_each(ctx->htable, bkt, entry, hnode) {
		sdc_macdb_deref(ctx, entry);
	}
}
