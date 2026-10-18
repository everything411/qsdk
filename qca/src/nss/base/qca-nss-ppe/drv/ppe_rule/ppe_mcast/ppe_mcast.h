/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <ppe_drv_public.h>
#include <ppe_drv_port_mgmt.h>
#include <ppe_mcast.h>
#include "ppe_mcast_stats.h"
#include "ppe_mcast_dump.h"
#include <linux/hashtable.h>

/*
 * PPE Multicast macros
 */
#if (PPE_MCAST_DEBUG_LEVEL == 3)
#define ppe_mcast_assert(c, s, ...)
#else
#define ppe_mcast_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define ppe_mcast_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_mcast_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_mcast_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_MCAST_DEBUG_LEVEL < 2)
#define ppe_mcast_warn(s, ...)
#else
#define ppe_mcast_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_MCAST_DEBUG_LEVEL < 3)
#define ppe_mcast_info(s, ...)
#else
#define ppe_mcast_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_MCAST_DEBUG_LEVEL < 4)
#define ppe_mcast_trace(s, ...)
#else
#define ppe_mcast_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * ppe_mcast_hash_key
 *	Hash key for multicast entries.
 *	Key is (is_v4, sip_valid, SIP, GIP, port_id) to correctly distinguish
 *	SSM entries (sip_valid=true) from ASM entries (sip_valid=false) that
 *	share the same (GIP, port).
 */
struct ppe_mcast_hash_key {
	bool is_v4;                /* IPv4 or IPv6 entry */
	bool sip_valid;            /* Source IP valid (SSM entry) */
	union {
		__be32 v4;             /* IPv4 source address */
		uint32_t v6[4];        /* IPv6 source address */
	} sip;                     /* Source IP address */
	union {
		__be32 v4;             /* IPv4 group address */
		uint32_t v6[4];        /* IPv6 group address */
	} gip;                     /* Group IP address */
	int port_id;               /* Port ID */
};

/*
 * ppe_mcast_entry
 *	PPE multicast entry information
 */
struct ppe_mcast_entry {
	struct ppe_drv_mcast_entry entry;	/* Multicast entry information */
	struct ppe_drv_omci_port_node *node;	/* OMCI managed node */
	struct hlist_node hash_node;         /* Hash table node */
};

/*
 * ppe_mcast_group
 *	PPE multicast group information
 */
struct ppe_mcast_group {
	union {
		uint32_t v4;	/* Multicast group IPv4 address */
		uint32_t v6[4];	/* Multicast group IPv6 address */
	} gip;		/* Union of IPv4 and IPv6 multicast group address */

	bool is_v4;		/* IPv4 or IPv6 entry. */
	uint32_t unsupported_dst_cnt;	/* Not supported destination count */
	uint32_t supported_dst_cnt;	/* supported destination count */
	uint32_t omci_not_managed;	/* ports not managed by OMCI destination count */
	struct list_head list;	/* List of multicast group address. */
};

/*
 * struct ppe_mcast
 *	Structure to store multicast traffic management information
 */
struct ppe_mcast {
	spinlock_t lock;                                	/* PPE multicast lock. */
	struct dentry *dentry;		/* debugfs entry pointer */
	struct ppe_mcast_stats stats;			/* Multicast statistics */
	struct list_head mc_grp_list;	/* Multicast group address list. */
	int mcast_dump_major_id;		/* Dev ID for Multicast dump */
	DECLARE_HASHTABLE(mc_hash_table, 11);  /* Hash table for multicast entries - 11 bits = 2048 buckets */
};

extern struct ppe_mcast gbl_ppe_mcast;

/*
 * IPv4 address comparison macro
 */
#define ppe_drv_mcast_v4_addr_equal(a, b) ((u32)(a) == (u32)(b))

/*
 * ppe_drv_mcast_v6_addr_equal()
 *	Compare ipv6 address
 */
static inline bool ppe_drv_mcast_v6_addr_equal(uint32_t *a1, uint32_t *a2)
{
	return ((a1[0] ^ a2[0]) |
		(a1[1] ^ a2[1]) |
		(a1[2] ^ a2[2]) |
		(a1[3] ^ a2[3])) == 0;
}

void ppe_mcast_deinit(void);
void ppe_mcast_init(struct dentry *dentry);
