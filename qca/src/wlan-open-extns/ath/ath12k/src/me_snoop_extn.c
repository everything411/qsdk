// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/slab.h>
#include <linux/hash.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rcupdate.h>
#include <linux/netfilter_bridge.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <linux/ip.h>
#include <linux/errno.h>
#include <linux/if_ether.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <linux/dma-mapping.h>
#include <me.h>
#include <dp_peer.h>
#include <debug.h>

#include "me_snoop_extn.h"

/**
 * ath12k_dp_me_tx_ucast_grp_extn() - Transmit unicast copy for a snooped group member
 * @dp: Datapath instance
 * @dp_vif: Datapath vdev/vif context
 * @dp_link_vif: Link-specific datapath vif context
 * @peer: Datapath peer candidate to receive the unicast copy
 * @app_data: ME context (struct ath12k_me_ctx) carrying skb/group and tx bitmap
 * @msdu_info: MSDU info to be used for TX (filled/consumed by TX path)
 *
 * Looks up @peer in the multicast snoop group carried in @app_data. If the peer
 * is present and has not yet been served (tracked via ctx->tx_bmap), this
 * function marks it served, applies source-address filtering and transmits the
 * unicast copy via ath12k_dp_me_tx_ucast_peer().
 *
 * Return: 0 if no transmission is performed, otherwise the return value from
 * ath12k_dp_me_tx_ucast_peer().
 */
int ath12k_dp_me_tx_ucast_grp_extn(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
				   struct ath12k_dp_link_vif *dp_link_vif,
				   struct ath12k_dp_peer *peer, void *app_data,
				   struct ath12k_dp_tx_msdu_info *msdu_info)
{
	struct ath12k_me_ctx *ctx = app_data;
	union nf_inet_addr src_addr = {0};
	struct ath12k_me_peer_grp *grp;
	struct ath12k_me_peer *me_peer = NULL;
	bool match_found = false;

	grp = ctx->grp;

	rcu_read_lock();
	list_for_each_entry_rcu(me_peer, &grp->peers, list) {
		/*
		 * Skip the Peer if it has already sent the UCAST
		 */
		if (test_bit(me_peer->snoop_id, ctx->tx_bmap))
			continue;

		if (ether_addr_equal(peer->addr, me_peer->mac_addr)) {
			match_found = true;
			set_bit(me_peer->snoop_id, ctx->tx_bmap);
			break;
		}
	}
	rcu_read_unlock();

	if (!match_found)
		return 0;

	/* Check is the peer can accept packets from the source */
	__skb_get_inet_saddr(ctx->skb, &src_addr);

	if (!ath12k_me_snoop_filter_src(me_peer, &src_addr))
		return 0;

	return ath12k_dp_me_tx_ucast_peer(dp, dp_vif, dp_link_vif, peer, ctx,
					  msdu_info);
}

static u32 ath12k_me_snoop_get_hash4(struct ath12k_me_db *db, __be32 *addr)
{
	return hash_32(be32_to_cpu(*addr), ilog2(ATH12K_ME_MAX_GRP_LIMIT));
}

/*
 * ath12k_me_snoop_get_hash6
 * Get hash index based on IPv6 address
 */
static u32 ath12k_me_snoop_get_hash6(struct ath12k_me_db *db, __be32 *addr)
{
	u32 key;

	/* simple fold: XOR all four 32-bit words */
	key = addr[0] ^ addr[1] ^ addr[2] ^ addr[3];

	return hash_32(key, ilog2(ATH12K_ME_MAX_GRP_LIMIT));
}

struct ath12k_me_peer_grp *ath12k_me_snoop_grp_find4(struct ath12k_me_db *db,
						     union nf_inet_addr *addr)
{
	struct ath12k_me_peer_grp *grp = NULL;
	struct hlist_head *head;
	u32 hash_idx;

	hash_idx = ath12k_me_snoop_get_hash4(db, &addr->ip);
	head = &db->snoop.hash_db[hash_idx];

	hlist_for_each_entry_rcu(grp, head, hlist) {
		if (grp->is_v6)
			continue;
		if (grp->grp_id.ip == addr->ip)
			return grp;
	}

	return NULL;
}

struct ath12k_me_peer_grp *ath12k_me_snoop_grp_find6(struct ath12k_me_db *db,
						     union nf_inet_addr *addr)
{
	struct ath12k_me_peer_grp *grp = NULL;
	struct hlist_head *head;
	u32 hash_idx;

	hash_idx = ath12k_me_snoop_get_hash6(db, addr->in6.s6_addr32);
	head = &db->snoop.hash_db[hash_idx];

	hlist_for_each_entry_rcu(grp, head, hlist) {
		if (!grp->is_v6)
			continue;
		if (ipv6_addr_equal(&grp->grp_id.in6, &addr->in6))
			return grp;
	}

	return NULL;
}

struct ath12k_me_peer_grp *ath12k_me_snoop_grp_find(struct ath12k_dp_vif *dp_vif,
						    struct sk_buff *skb)
{
	struct ath12k_me_peer_grp *grp = NULL;
	union nf_inet_addr addr = {0};
	u16 proto;

	proto = ntohs(skb->protocol);
	rcu_read_lock();

	switch (proto) {
	case ETH_P_IP:
		addr.ip = ip_hdr(skb)->daddr;
		if (dp_vif->me_db)
			grp = ath12k_me_snoop_grp_find4(dp_vif->me_db, &addr);
		break;

	case ETH_P_IPV6:
		addr.in6 = ipv6_hdr(skb)->daddr;
		if (dp_vif->me_db)
			grp = ath12k_me_snoop_grp_find6(dp_vif->me_db, &addr);
		break;

	default:
		break;
	}

	rcu_read_unlock();
	return grp;
}

struct ath12k_me_peer *ath12k_me_snoop_peer_find(struct ath12k_me_peer_grp *grp, u8 peer_mac[])
{
	struct ath12k_me_peer *peer;

	list_for_each_entry_rcu(peer, &grp->peers, list) {
		if (ether_addr_equal(peer->mac_addr, peer_mac))
			return peer;
	}

	return NULL;
}

static bool __addr_match4(union nf_inet_addr *src, union nf_inet_addr *dst)
{
	return src->ip == dst->ip;
}

static bool __addr_match6(union nf_inet_addr *src, union nf_inet_addr *dst)
{
	return ipv6_addr_equal(&src->in6, &dst->in6);
}

/*
 * ath12k_me_snoop_filter_src
 * Check whether a packet from src_addr should be accepted
 * based on peer's source filter list.
 */
bool ath12k_me_snoop_filter_src(struct ath12k_me_peer *peer, union nf_inet_addr *addr)
{
	bool (*match_fn)(union nf_inet_addr *p_addr, union nf_inet_addr *addr);
	union nf_inet_addr *p_addr = peer->srcs;
	int nsrcs = peer->nsrcs;

	if (!nsrcs)
		return true;

	match_fn = peer->is_v6 ? __addr_match6 : __addr_match4;

	for (int i = 0; i < nsrcs; i++, p_addr++) {
		if (match_fn(p_addr, addr))
			return peer->filter == BR_MCAST_SRCLIST_INCLUDE;
	}

	return peer->filter != BR_MCAST_SRCLIST_INCLUDE;
}

static struct ath12k_me_peer_grp
*ath12k_me_snoop_grp_alloc(struct ath12k_dp_vif *dp_vif,
			   union nf_inet_addr *grp_id,
			   bool is_v6)
{
	struct ath12k_me_peer_grp *grp;
	struct ath12k_me_db *db;
	u32 hash;

	grp = kzalloc(sizeof(*grp), GFP_ATOMIC);
	if (!grp)
		return NULL;

	INIT_LIST_HEAD(&grp->peers);

	bitmap_zero(grp->npeers, ATH12K_ME_MAX_SNOOP_PEERS);
	grp->is_v6 = is_v6;
	grp->grp_id = *grp_id;

	/* Store DB pointer */
	db = ath12k_me_db_get(dp_vif);
	grp->db = db;

	lockdep_assert_held(&db->lock);

	/*
	 * Calculate the hash and insert into the hash list
	 */
	if (is_v6)
		hash = ath12k_me_snoop_get_hash6(db, grp->grp_id.in6.s6_addr32);
	else
		hash = ath12k_me_snoop_get_hash4(db, &grp->grp_id.ip);

	hlist_add_head_rcu(&grp->hlist, &db->snoop.hash_db[hash]);

	return grp;
}

static void ath12k_me_snoop_grp_free(struct ath12k_me_peer_grp *grp)
{
	struct ath12k_me_db *db = grp->db;
	struct ath12k_me_peer *peer, *tmp;

	if (!db)
		return;

	lockdep_assert_held(&db->lock);

	/* Remove group from hash list first to prevent new lookups */
	hlist_del_rcu(&grp->hlist);

	list_for_each_entry_safe(peer, tmp, &grp->peers, list) {
		clear_bit(peer->snoop_id, grp->npeers);
		list_del_rcu(&peer->list);
		kfree_rcu(peer, rcu);
	}

	/* Clear db pointer before put to prevent re-entry */
	grp->db = NULL;
	/* Drop DB reference */
	ath12k_me_db_put(db);
	/* Free the group */
	kfree_rcu(grp, rcu);
}

int ath12k_me_snoop_peer_add(struct ath12k_me_peer_grp *grp,
			     struct br_mcast_event *ev)
{
	struct ath12k_me_peer *peer;
	size_t src_size;

	peer = ath12k_me_snoop_peer_find(grp, ev->host_mac);
	if (peer)
		return 0;

	if (bitmap_full(grp->npeers, ATH12K_ME_MAX_SNOOP_PEERS))
		return -EINVAL;

	src_size = ev->src_cnt * sizeof(union nf_inet_addr);
	peer = kzalloc(sizeof(*peer) + src_size, GFP_ATOMIC);
	if (!peer)
		return -ENOMEM;

	INIT_LIST_HEAD(&peer->list);
	peer->nsrcs  = ev->src_cnt;
	peer->is_v6  = !ev->is_v4;
	peer->filter = ev->src_filter;

	memcpy(peer->srcs, ev->src_list, src_size);
	memcpy(peer->mac_addr, ev->host_mac, ETH_ALEN);

	peer->snoop_id = find_first_zero_bit(grp->npeers, ATH12K_ME_MAX_SNOOP_PEERS);
	WARN_ON_ONCE(peer->snoop_id >= ATH12K_ME_MAX_SNOOP_PEERS);

	list_add_tail_rcu(&peer->list, &grp->peers);
	set_bit(peer->snoop_id, grp->npeers);

	return 0;
}

static int ath12k_me_snoop_peer_del(struct ath12k_me_peer_grp *grp,
				    struct br_mcast_event *ev)
{
	struct ath12k_me_peer *peer;
	bool is_empty;

	peer = ath12k_me_snoop_peer_find(grp, ev->host_mac);
	if (!peer)
		return -ENOENT;

	clear_bit(peer->snoop_id, grp->npeers);
	list_del_rcu(&peer->list);
	is_empty = list_empty(&grp->peers);

	kfree_rcu(peer, rcu);

	if (is_empty)
		ath12k_me_snoop_grp_free(grp);

	return 0;
}

static int ath12k_me_snoop_peer_upd(struct ath12k_me_peer_grp *grp,
				    struct br_mcast_event *ev)
{
	struct ath12k_me_peer *peer, *tmp;
	size_t src_size;

	peer = ath12k_me_snoop_peer_find(grp, ev->host_mac);
	if (!peer)
		return -ENOENT;

	src_size = ev->src_cnt * sizeof(union nf_inet_addr);

	/*
	 * Update will happen on a new memory
	 */
	tmp = kzalloc(sizeof(*tmp) + src_size, GFP_ATOMIC);
	if (!tmp)
		return -ENOMEM;

	/* Copy all common metadata in one shot */
	INIT_LIST_HEAD(&tmp->list);
	memcpy(&tmp->cmn_data, &peer->cmn_data, sizeof(tmp->cmn_data));

	tmp->nsrcs  = ev->src_cnt;
	tmp->is_v6  = !ev->is_v4;
	tmp->filter = ev->src_filter;
	memcpy(tmp->srcs, ev->src_list, src_size);

	list_replace_rcu(&peer->list, &tmp->list);

	kfree_rcu(peer, rcu);

	return 0;
}

int ath12k_me_snoop_event_notify(struct notifier_block *nb, unsigned long action, void *data)
{
	struct ath12k_me_peer_grp *(*find_grp_fn)(struct ath12k_me_db *db,
						  union nf_inet_addr *addr);
	struct br_mcast_event *event = data;
	struct ath12k_me_peer_grp *grp;
	struct ath12k_dp_vif *dp_vif;
	struct wireless_dev *wdev;
	struct ieee80211_vif *vif;
	union nf_inet_addr grp_id;
	struct ath12k_me_db *db;
	struct net_device *dev;
	bool is_v6;

	if (!event)
		return NOTIFY_DONE;

	dev = dev_get_by_index(&init_net, event->ifindex);
	if (!dev)
		return NOTIFY_BAD;

	/*
	 * TODO: VLAN device needs to be handled separately
	 */
	wdev = dev->ieee80211_ptr;
	if (!wdev)
		goto fail1;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		goto fail1;

	dp_vif = &((struct ath12k_vif *)vif->drv_priv)->dp_vif;

	db = ath12k_me_db_get(dp_vif);
	if (!db)
		goto fail1;

	if (!(db->me_flags & ATH12K_ME_OFFLOAD_MASK)) {
		ath12k_me_db_put(db);
		goto fail1;
	}

	grp_id = event->grp_ip;
	is_v6 = !event->is_v4;

	find_grp_fn = is_v6 ? ath12k_me_snoop_grp_find6 : ath12k_me_snoop_grp_find4;

	switch (action) {
	case BR_MCAST_EVENT_ADD:
		spin_lock_bh(&db->lock);
		grp = find_grp_fn(db, &grp_id);
		if (!grp) {
			grp = ath12k_me_snoop_grp_alloc(dp_vif, &event->grp_ip, is_v6);
			if (!grp) {
				spin_unlock_bh(&db->lock);
				ath12k_me_db_put(db);
				goto fail1;
			}
		}

		 /* Add peer */
		ath12k_me_snoop_peer_add(grp, event);
		spin_unlock_bh(&db->lock);
		break;

	case BR_MCAST_EVENT_DEL:
		spin_lock_bh(&db->lock);
		grp = find_grp_fn(db, &grp_id);
		if (grp)
			ath12k_me_snoop_peer_del(grp, event);
		spin_unlock_bh(&db->lock);
		break;

	case BR_MCAST_EVENT_UPDATE:
		spin_lock_bh(&db->lock);
		grp = find_grp_fn(db, &grp_id);
		if (!grp) {
			spin_unlock_bh(&db->lock);
			goto done;
		}

		ath12k_me_snoop_peer_upd(grp, event);
		spin_unlock_bh(&db->lock);
		break;

	case BR_MCAST_EVENT_FLUSH_ALL:
		spin_lock_bh(&db->lock);
		ath12k_me_snoop_list_flush_extn(&db->snoop);
		spin_unlock_bh(&db->lock);
		break;

	default:
		break;
	}

done:
	/* Drop DB reference we took earlier */
	ath12k_me_db_put(db);
	dev_put(dev);
	return NOTIFY_OK;

fail1:
	dev_put(dev);
	return NOTIFY_BAD;
}
EXPORT_SYMBOL_GPL(ath12k_me_snoop_event_notify);

/**
 * ath12k_me_snoop_list_flush(): Flush snoop list
 * @list - Snoop list
 */
void ath12k_me_snoop_list_flush_extn(struct ath12k_me_snoop_list *list)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(list->hash_db); i++) {
		struct ath12k_me_peer_grp *grp;
		struct hlist_node *h;

		hlist_for_each_entry_safe(grp, h, &list->hash_db[i], hlist) {
			ath12k_me_snoop_grp_free(grp);
		}
	}
}

static const char *__snoop_filter_to_str(u8 mode)
{
    switch (mode) {
    case BR_MCAST_SRCLIST_NONE:    return "None";
    case BR_MCAST_SRCLIST_EXCLUDE: return "Exclude";
    case BR_MCAST_SRCLIST_INCLUDE: return "Include";
    case BR_MCAST_SRCLIST_IGNORE:  return "Any";
    default:                       return "Unknown";
    }
}

int ath12k_me_snoop_print_extn(struct ath12k_base *ab, struct ath12k_me_db *db)
{
	struct ath12k_me_peer_grp *grp;
	struct ath12k_me_peer *peer;
	u16 entries = 0;
	const char *str;
	int i;

	if (!db)
		return -ENOENT;

	ath12k_info(ab, "=== Snoop Print ===\n");

	rcu_read_lock();
	for (i = 0; i < ARRAY_SIZE(db->snoop.hash_db); i++) {
		hlist_for_each_entry_rcu(grp, &db->snoop.hash_db[i], hlist) {
			list_for_each_entry_rcu(peer, &grp->peers, list) {
				if (is_zero_ether_addr(peer->mac_addr))
					continue;

				if (grp->is_v6)
					str = "grp: %pI6 peer: %pM filter: %s nsrcs: %u is_v6: %d\n";
				else
					str = "grp: %pI4 peer: %pM filter: %s nsrcs: %u is_v6: %d\n";

				ath12k_info(ab, str,
					   grp->is_v6 ? (void *)&grp->grp_id.in6 : (void *)&grp->grp_id.ip,
					   peer->mac_addr,
					   __snoop_filter_to_str(peer->filter), peer->nsrcs,
					   grp->is_v6);

				entries++;
			}
		}
	}
	rcu_read_unlock();

	ath12k_info(ab, "Total snoop entries: %u\n", entries);
	return 0;
}

/**
 * ath12k_me_snoop_list_init(): Initialize snoop list
 * @list - Snoop list
 */
void ath12k_me_snoop_list_init_extn(struct ath12k_me_snoop_list *list)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(list->hash_db); i++)
		INIT_HLIST_HEAD(&list->hash_db[i]);
}

/**
 * ath12k_core_me_notifier_register_extn(): Register ME Notifier
 * @ab - ath12k base structure
 */
int ath12k_core_me_notifier_register_extn(struct ath12k_base *ab)
{
	if (test_and_set_bit(__NB_FLAGS_REGISTERED, &ab->me.nb_flags))
		return -EALREADY;

	ab->me.nb.notifier_call = ath12k_me_snoop_event_notify;
	ab->me.nb.priority = 0;
	br_mcast_offload_event_notifier_register(&ab->me.nb);
	return 0;
}

/**
 * ath12k_core_me_notifier_unregister_extn(): Unregister ME Notifier
 * @ab - ath12k base structure
 */
int ath12k_core_me_notifier_unregister_extn(struct ath12k_base *ab)
{
	if (!test_and_clear_bit(__NB_FLAGS_REGISTERED, &ab->me.nb_flags))
		return -EALREADY;

	br_mcast_offload_event_notifier_unregister(&ab->me.nb);
	return 0;
}
