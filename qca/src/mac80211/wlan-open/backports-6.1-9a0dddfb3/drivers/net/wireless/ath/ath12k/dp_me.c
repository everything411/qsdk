// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/ipv6.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/kernel.h>
#include <linux/if_vlan.h>
#include <linux/skbuff.h>
#include <linux/errno.h>

#include "dp_peer.h"
#include "dp_ext_desc.h"
#include "me.h"
#include "debug.h"
#include "dp_tx.h"
#include "dp_ext_desc.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/me_snoop_extn.h"
#endif /* CPTCFG_QCN_EXTN */
#include "dp_stats.h"

static inline u16 ath12k_dp_get_me_peer_id(struct ath12k_dp *dp,
					   struct ath12k_dp_peer *dp_peer,
					   u8 hw_link_id)
{
	struct ath12k_dp_link_peer *link_peer;

	if (dp->global_peer_id_supported)
		return dp_peer->peer_id;

	link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, hw_link_id);
	if (!link_peer) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX,
			   "Link Peer NOT FOUND for hw_link_id%u", hw_link_id);
		return HAL_INVALID_PEERID;
	}

	return link_peer->peer_id;
}

static int ath12k_dp_tx_me5(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
			    struct ath12k_dp_link_vif *link_vif,
			    struct ath12k_dp_peer *dp_peer,
			    struct ath12k_me_ctx *me_ctx,
			    struct ath12k_dp_tx_msdu_info *msdu_info)
{
	struct sk_buff *skb = me_ctx->skb;
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	u8 ring_id = smp_processor_id();
	struct ath12k_pdev_dp *dp_pdev = NULL;
	enum ath12k_dp_tx_enq_error err = 0;

	dp_pdev = ath12k_dp_to_dp_pdev(dp, link_vif->pdev_idx);
	if (!dp_pdev) {
		err = DP_TX_ENQ_DROP_INV_PDEV;
		goto fail;
	}

	ether_addr_copy(msdu_info->ext_desc.peer_mac_addr, dp_peer->addr);
	msdu_info->ext_kmem = true;
	msdu_info->ext_desc.ext_feature |= DP_EXT_ME5;
	msdu_info->to_fw = 0;
	msdu_info->meta_data_flags = link_vif->tcl_metadata;
	msdu_info->data_len = skb->len;
	msdu_info->me_convert = true;
	msdu_info->lookup_override = false;
	msdu_info->group_slot = -1;

	err = ath12k_dp_tx_mcast_send(dp_pdev, ahvif, link_vif, ring_id,
				      msdu_info, false, 0,
				      skb_get(skb), NULL, NULL);

	if (unlikely(err != DP_TX_ENQ_SUCCESS))
		goto fail_tx;

	DP_STATS_INC(dp_vif, tx_i.me.me5_hits, 1, ring_id);
	atomic_inc(&dp_pdev->num_tx_pending);
	return 0;

fail_tx:
	/* drop reference to SKB taken in mcast_send */
	dev_kfree_skb_any(skb);
fail:
	DP_STATS_INC(dp_vif, tx_i.drop[err], 1, ring_id);
	DP_STATS_INC(dp_vif, tx_i.me.me_fail, 1, ring_id);
	return -ENOMEM;
}

static int ath12k_dp_tx_me6(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
			    struct ath12k_dp_link_vif *link_vif,
			    struct ath12k_dp_peer *dp_peer,
			    struct ath12k_me_ctx *me_ctx,
			    struct ath12k_dp_tx_msdu_info *msdu_info)
{
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	u16 peer_id;
	struct sk_buff *skb = me_ctx->skb;
	u8 ring_id = smp_processor_id();
	struct ath12k_pdev_dp *dp_pdev = NULL;
	enum ath12k_dp_tx_enq_error err = 0;
	u16 mdata = 0;

	dp_pdev = ath12k_dp_to_dp_pdev(dp, link_vif->pdev_idx);
	if (!dp_pdev) {
		err = DP_TX_ENQ_DROP_INV_PDEV;
		goto fail;
	}

	peer_id = ath12k_dp_get_me_peer_id(dp, dp_peer, dp_pdev->hw_link_id);
	if (peer_id == HAL_INVALID_PEERID) {
		err = DP_TX_ENQ_DROP_INV_PEER;
		goto fail;
	}

	/*
	 * Prepare metadata with peer_id
	 */
	mdata |= ath12k_dp_get_peer_based_tcl_metadata(dp, peer_id, 0);

	msdu_info->to_fw = 0;
	msdu_info->ext_kmem = false;
	msdu_info->meta_data_flags = mdata;
	msdu_info->data_len = skb->len;
	msdu_info->me_convert = true;
	msdu_info->group_slot = -1;

	msdu_info->lookup_override = true;
	ath12k_dp_tx_set_ast(dp, dp_peer, msdu_info, dp_pdev->hw_link_id);

	err = ath12k_dp_tx_mcast_send(dp_pdev, ahvif, link_vif, ring_id,
				      msdu_info, false, 0,
				      skb_get(skb), NULL, NULL);

	if (unlikely(err != DP_TX_ENQ_SUCCESS))
		goto fail_tx;

	DP_STATS_INC(dp_vif, tx_i.me.me6_hits, 1, ring_id);
	atomic_inc(&dp_pdev->num_tx_pending);
	return 0;

fail_tx:
	/* drop reference to SKB taken in mcast_send */
	dev_kfree_skb_any(skb);
fail:
	DP_STATS_INC(dp_vif, tx_i.drop[err], 1, ring_id);
	DP_STATS_INC(dp_vif, tx_i.me.me_fail, 1, ring_id);
	return -ENOMEM;
}

static int ath12k_dp_me_check(struct ath12k_dp_vif *dp_vif, struct ath12k_me_ctx *ctx)
{
	struct sk_buff *skb = ctx->skb;
	u32 flags = ctx->me_flags;
	u16 proto = ntohs(skb->protocol);
	u8 l3_proto;

	/*
	 * Only go for M2U conversion when the flags are enabled
	 */
	if (!(flags & ATH12K_ME_OFFLOAD_MASK))
		goto fail;

	if (skb_is_nonlinear(skb)) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "No support for ME with SG pkts");
		goto fail;
	}

	switch (proto) {
	case ETH_P_IP:
		l3_proto = ip_hdr(skb)->protocol;
		break;

	case ETH_P_IPV6:
		l3_proto = ipv6_hdr(skb)->nexthdr;
		break;
	default:
		l3_proto = 0;
		break;
	}

	if (!is_multicast_ether_addr(skb_eth_hdr(skb)->h_dest))
		goto fail;

	/*
	 * In case of IGMP packets the handling depends upon the configuration
	 * done by the user.
	 */
	if (l3_proto == IPPROTO_IGMP) {
		if (!(flags & ATH12K_ME_FLAGS_BIT_IGMP_EN))
			goto fail;

		/* User intends to force convert all the IGMP into ucast */
		ctx->me_flags |= ATH12K_ME_FLAGS_BIT_FORCE_ME;
	}

	return 0;

fail:
	ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "ME offload check failed");
	return -EINVAL;
}

/**
 * ath12k_dp_me_tx_ucast_peer(): Decide and perform the desired MCUC
 * @dp: Data Path ptr
 * @dp_vif: Data Path Virtual Interface
 * @dp_link_vif : Data path Link specific obj
 * @dp_peer: DP Peer Object
 * @app_data: Desired App_data sent
 *
 * Return: Status for MCUC Success/Failure.
 */
int ath12k_dp_me_tx_ucast_peer(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
			       struct ath12k_dp_link_vif *dp_link_vif,
			       struct ath12k_dp_peer *dp_peer,
			       void *app_data, struct ath12k_dp_tx_msdu_info *msdu_info)
{
	struct ath12k_me_ctx *ctx = app_data;
	u8 ring_id = smp_processor_id();
	int ret = 0;
	u32 flags;

	flags = ctx->me_flags & ATH12K_ME_OFFLOAD_MASK;
	if (dp_peer->dms_disable)
		flags = ATH12K_ME_FLAGS_BIT_ME5;

	switch (flags) {
	case ATH12K_ME_FLAGS_BIT_ME5:
		ret = ath12k_dp_tx_me5(dp, dp_vif, dp_link_vif, dp_peer, ctx, msdu_info);
		if (ret < 0)
			ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "Failed to convert ME5\n");
		break;
	case ATH12K_ME_FLAGS_BIT_ME6:
		ret = ath12k_dp_tx_me6(dp, dp_vif, dp_link_vif, dp_peer, ctx, msdu_info);
		if (ret < 0)
			ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "Failed to convert ME6\n");
		break;
	default:
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "Invalid ME flags(0%x) for TX\n",
			   ctx->me_flags);
		DP_STATS_INC(dp_vif, tx_i.me.me_fail, 1, ring_id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * struct ath12k_dp_me_walk_ctx - Context passed to the per-peer iterator
 *                                used by ath12k_dp_me_tx.
 * @dp_vif:      Data path virtual interface.
 * @action_fn:   Per-peer action callback (MCUC unicast send).
 * @app_data:    Opaque data forwarded unchanged to @action_fn.
 * @ret:         Last non-zero return value from @action_fn; 0 on full success.
 */
struct ath12k_dp_me_walk_ctx {
	struct ath12k_dp_vif      *dp_vif;
	int (*action_fn)(struct ath12k_dp *dp,
			 struct ath12k_dp_vif *dp_vif,
			 struct ath12k_dp_link_vif *dp_link_vif,
			 struct ath12k_dp_peer *dp_peer, void *app_data,
			 struct ath12k_dp_tx_msdu_info *msdu_info);
	void *app_data;
	int   ret;
	struct ath12k_dp_tx_msdu_info *info;
};

/**
 * ath12k_dp_me_peer_walk_cb() - Per-peer iterator callback for MCUC.
 * @link_peer: Current ath12k_dp_link_peer visited by the iterator.
 * @data:      Pointer to struct ath12k_dp_me_walk_ctx.
 *
 * Applies the same peer-selection filters as the removed ath12k_dp_peer_walk_action:
 *   - skip peers with use_4addr set (repeater peers)
 *   - skip peers whose vdev_id does not match dp_link_vif->vdev_id
 *   - skip non-primary-link peers
 * Then invokes ctx->action_fn for the matching peer. A non-zero return
 * from action_fn is stored in ctx->ret and immediately aborts iteration.
 *
 * Return: 0 to continue iteration, non-zero to abort.
 */
static void ath12k_dp_me_peer_walk_cb(struct ath12k_dp_peer *dp_peer,
				      void *data)
{
	struct ath12k_dp_me_walk_ctx *ctx = (struct ath12k_dp_me_walk_ctx *)data;
	struct ath12k_vif *ahvif = container_of(ctx->dp_vif,
						struct ath12k_vif, dp_vif);
	u8 hw_link_id;

	/* Skip repeater peers */
	if (dp_peer->use_4addr)
		return;

	/* Iterate over all link_peers of this dp_peer */
	for (hw_link_id = 0; hw_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS; hw_link_id++) {
		struct ath12k_dp_link_peer *link_peer;
		struct ath12k_link_vif *arvif;
		struct ath12k_dp_link_vif *dp_link_vif;
		struct ath12k_dp *dp;
		struct ath12k *ar;

		link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, hw_link_id);
		if (!link_peer)
			continue;

		/* Process on primary link only */
		if (!link_peer->primary_link)
			continue;

		/* Match vdev_id to the correct dp_link_vif */
		arvif = rcu_dereference(ahvif->link[link_peer->link_id]);
		if (!arvif)
			continue;

		ar = arvif->ar;
		if (!ar || !ar->ab)
			continue;

		dp = ar->ab->dp;
		if (!dp)
			continue;

		dp_link_vif = &ctx->dp_vif->dp_link_vif[link_peer->link_id];

		/* Verify vdev_id matches */
		if (link_peer->vdev_id != dp_link_vif->vdev_id)
			continue;

		ctx->ret = ctx->action_fn(dp, ctx->dp_vif, dp_link_vif,
					  dp_peer, ctx->app_data, ctx->info);
	}
}

/**
 * ath12k_dp_me_tx(): Transmit function for Multicast packets
 * @dp_vif - Pointer to Data path virtual interface structure
 * @skb: Pointer to socket buffer
 *
 * Return: Success or Failure.
 */
int ath12k_dp_me_tx(struct ath12k_dp_vif *dp_vif, struct sk_buff *skb,
		    struct ath12k_dp_tx_msdu_info *msdu_info)
{
	int (*action_fn)(struct ath12k_dp *dp,
			 struct ath12k_dp_vif *dp_vif,
			 struct ath12k_dp_link_vif *dp_link_vif,
			 struct ath12k_dp_peer *dp_peer, void *app_data,
			 struct ath12k_dp_tx_msdu_info *msdu_info);
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	u8 ring_id = smp_processor_id();
	struct ath12k_me_ctx ctx = {0};
	union nf_inet_addr addr = {0};
	struct ath12k_me_db *me_db;
	u8 force_mcuc = 0;
	int action;
	bool is_v6;

	me_db = ath12k_me_db_get(dp_vif);
	if (!me_db)
		return -ENOENT;

	action_fn = ath12k_dp_me_tx_ucast_peer;
	ctx.me_flags = me_db->me_flags;
	ctx.skb = skb;

	if (ahvif->vif->type != NL80211_IFTYPE_AP) {
		ath12k_me_db_put(me_db);
		return -EINVAL;
	}

	if (ath12k_dp_me_check(dp_vif, &ctx) < 0) {
		ath12k_me_db_put(me_db);
		return -EINVAL;
	}

	DP_STATS_INC(dp_vif, tx_i.me.total_mc, 1, ring_id);

	is_v6 = __skb_get_inet_daddr(skb, &addr);

	action = ath12k_me_hmmc_lookup(me_db, (__be32 *)&addr, is_v6);
	ath12k_me_db_put(me_db);

	if (action == ATH12K_ME_DENYLIST_ACTION)
		return -EINVAL;

	force_mcuc += (action == ATH12K_ME_HMMC_ACTION);
	force_mcuc += !!(ctx.me_flags & ATH12K_ME_FLAGS_BIT_FORCE_ME);

	if (!force_mcuc) {
#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
#ifdef CPTCFG_QCN_EXTN
		ctx.grp = ath12k_me_snoop_grp_find(dp_vif, skb);
		if (!ctx.grp)
			return -EINVAL;

		bitmap_zero(ctx.tx_bmap, ATH12K_ME_MAX_SNOOP_PEERS);
		action_fn = ath12k_dp_me_tx_ucast_grp_extn;
#else
		/*
		 * If Snoop lookup is disabled & ME is not enforced then this payload
		 * needs to be sent as multicast
		 */
		return -EINVAL;
#endif /* CPTCFG_QCN_EXTN */
#else
		return -EINVAL;
#endif /* CONFIG_BRIDGE_MCAST_OFFLOAD */
	}

	DP_STATS_INC(dp_vif, tx_i.me.me_mcuc, 1, ring_id);

	/*
	 * Perform MCUC across all the relevant peers.
	 */
	struct ath12k_dp_me_walk_ctx walk_ctx = {
		.dp_vif      = dp_vif,
		.action_fn   = action_fn,
		.app_data    = &ctx,
		.info        = msdu_info,
	};

	/*
	 * Iterate across all the dp link vifs.
	 */
	rcu_read_lock_bh();

	ath12k_dp_peer_iterate_by_vif(&ahvif->ah->dp_hw, ahvif->vif,
				      ath12k_dp_me_peer_walk_cb, &walk_ctx);

	rcu_read_unlock_bh();

	/*
	 * Unconditionally, free the original SKB since the UCAST FN have already
	 * taken the references. This will ensure that intermediate send failures
	 * doesn't leak the SKB.
	 */
	dev_kfree_skb_any(skb);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_me_tx);
