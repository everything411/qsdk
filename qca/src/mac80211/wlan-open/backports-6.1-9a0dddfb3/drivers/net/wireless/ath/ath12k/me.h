/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __ATH12K_ME_H
#define __ATH12K_ME_H

#include <net/ipv6.h>
#include <linux/ip.h>
#include <linux/bitfield.h>
#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
#include <linux/netfilter_bridge.h>
#endif
#include "core.h"
#include "dp_peer.h"
#include "me_hmmc.h"

#define ATH12K_ME_FLAGS_BIT_UNUSED1 BIT(0)	/* Available for future use */
#define ATH12K_ME_FLAGS_BIT_UNUSED2 BIT(1)	/* Available for future use */
#define ATH12K_ME_FLAGS_BIT_FORCE_ME BIT(2)	/* Force MCUC conversion */
#define ATH12K_ME_FLAGS_BIT_IGMP_EN BIT(3)	/* ME offload for IGMP packets */
#define ATH12K_ME_FLAGS_BIT_BYPASS_ME BIT(4)	/* Bypass ME conversion */
#define ATH12K_ME_FLAGS_BIT_ME5 BIT(5)		/* ME5 offload enable */
#define ATH12K_ME_FLAGS_BIT_ME6 BIT(6)		/* ME6 offload enable */

#define ATH12K_ME_OFFLOAD_MASK (ATH12K_ME_FLAGS_BIT_ME5 | ATH12K_ME_FLAGS_BIT_ME6)

/* Return values for lookup operations */
#define ATH12K_ME_HMMC_ACTION (ATH12K_ME_FLAGS_BIT_ME6 | ATH12K_ME_FLAGS_BIT_ME5)
#define ATH12K_ME_DENYLIST_ACTION ATH12K_ME_FLAGS_BIT_BYPASS_ME

#define ATH12K_ME_MAX_GRP_LIMIT		256

#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
#define ATH12K_ME_MAX_SNOOP_PEERS	512
#endif

struct ath12k_dp_vif;
struct ath12k_dp_link_vif;

/*
 * ATH12K ME Ctx which holds info for every skb
 */
struct ath12k_me_ctx {
	struct sk_buff *skb;    /* Original skb */
	u32 me_flags;		/* ME flag */
#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
	struct ath12k_me_peer_grp *grp;
	DECLARE_BITMAP(tx_bmap, ATH12K_ME_MAX_SNOOP_PEERS);
#endif
};

#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
/*
 * ATH12K Multicast group entry
 */
struct ath12k_me_peer_grp {
	struct hlist_node hlist;        /* Hash list entry */
	struct list_head peers;         /* list of peers per group */
	bool is_v6;                     /* IPv4 or IPv6 */
	union nf_inet_addr grp_id;      /* Multicast IP address */
	DECLARE_BITMAP(npeers, ATH12K_ME_MAX_SNOOP_PEERS);	/* Bitmap for peer cnt */
	struct ath12k_me_db *db; /* keep pointer to parent DB */
	struct rcu_head rcu;		 /* RCU Head */
};

/*
 * ATH12K multicast peer
 */
struct ath12k_me_peer {
	struct list_head list;          /* list peer siblings to for M2U conversion */
	struct rcu_head rcu;

	/* Add new parameters under cmn_data to group all peer metadata for memcpy */
	struct_group(cmn_data,
		     u8 mac_addr[ETH_ALEN];		/* MAC Address of the peer */
	u16 snoop_id;			/* Unique ID for bitmap tracking */
	/* List of sources to include or exclude */
	u32 nsrcs;                      /* No. of source IPs */
	bool is_v6;                     /* Source IPs type v4 or v6 */
	enum br_mcast_filter filter;    /* Filter type */

	);

	union nf_inet_addr srcs[];      /* Source IPs */
};

/*
 * Peer snooped from Bridge events from conversion
 */
struct ath12k_me_snoop_list {
	struct hlist_head  hash_db[ATH12K_ME_MAX_GRP_LIMIT];
				/* Hash database for groups */
};
#endif

/*
 * ATH12K Multicast database
 */
struct ath12k_me_db {
	struct ath12k_me_hmmc_list hmmc_db;	/* HMMC DB */
#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
	struct ath12k_me_snoop_list snoop;	/* Snooped peers for conversion */
#endif
	u32 me_flags;				/* ME Flags  */
	u16 grp_limit;				/* Soft limit for number of groups */
	spinlock_t lock;			/* Spin lock for ME DB protection */
	struct rcu_head rcu_head;		/* RCU Head */
	struct kref ref;			/* Reference count */
};

/**
 * __skb_get_inet_saddr(): Get source address from an skb
 * @skb - Pointer to Socket Buffer
 * @addr: Source address
 *
 * Return: If a packet is v6 or not.
 */
static inline bool __skb_get_inet_saddr(struct sk_buff *skb, union nf_inet_addr *addr)
{
	u16 proto = ntohs(skb->protocol);
	bool is_v6 = false;

	switch (proto) {
	case ETH_P_IP:
		addr->ip = ip_hdr(skb)->saddr;
		break;
	case ETH_P_IPV6:
		addr->in6 = ipv6_hdr(skb)->saddr;
		is_v6 = true;
		break;
	default:
		memset(addr, 0, sizeof(*addr));
		break;
	}

	return is_v6;
}

/**
 * __skb_get_inet_daddr(): Get destination address from an skb
 * @skb - Pointer to Socket Buffer
 * @addr: Destination address
 *
 * Return: If a packet is v6 or not.
 */
static inline bool __skb_get_inet_daddr(struct sk_buff *skb, union nf_inet_addr *addr)
{
	u16 proto = ntohs(skb->protocol);
	bool is_v6 = false;

	switch (proto) {
	case ETH_P_IP:
		addr->ip = ip_hdr(skb)->daddr;
		break;
	case ETH_P_IPV6:
		addr->in6 = ipv6_hdr(skb)->daddr;
		is_v6 = true;
		break;
	default:
		memset(addr, 0, sizeof(*addr));
		break;
	}

	return is_v6;
}

int ath12k_dp_me_tx_ucast_peer(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
			       struct ath12k_dp_link_vif *dp_link_vif,
			       struct ath12k_dp_peer *peer,
			       void *app_data,
			       struct ath12k_dp_tx_msdu_info *msdu_info);

int ath12k_dp_me_tx(struct ath12k_dp_vif *dp_vif, struct sk_buff *skb,
		    struct ath12k_dp_tx_msdu_info *msdu_info);

struct ath12k_me_db *ath12k_me_db_get(struct ath12k_dp_vif *dp_vif);
void ath12k_me_db_put(struct ath12k_me_db *db);

void ath12k_me_db_free(struct kref *ref);
void ath12k_me_db_reset(struct ath12k_me_db *db);
int ath12k_me_db_deinit(struct ath12k_dp_vif *dp_vif);
int ath12k_me_db_init(struct ath12k_dp_vif *dp_vif);

void ath12k_print_me_configs(struct ath12k_me_db *db);

#endif /* __ATH12K_ME_H */
