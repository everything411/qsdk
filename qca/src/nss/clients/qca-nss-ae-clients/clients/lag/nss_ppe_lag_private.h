/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPE_LAG_PRIVATE_H
#define __NSS_PPE_LAG_PRIVATE_H

#include <ppe_drv.h>

/*
 * Compile messages for dynamic enable/disable
 */
#if (NSS_PPE_LAG_MGR_DEBUG_LEVEL == 3)
#define nss_ppe_lag_assert(c, s, ...)
#else
#define nss_ppe_lag_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
#define nss_ppe_lag_warn(s, ...) \
		pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_ppe_lag_info(s, ...) \
		pr_notice("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_ppe_lag_trace(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#else /* CONFIG_DYNAMIC_DEBUG */
/*
 * Statically compile messages at different levels
 */
#if (NSS_PPE_LAG_MGR_DEBUG_LEVEL < 2)
#define nss_ppe_lag_warn(s, ...)
#else
#define nss_ppe_lag_warn(s, ...) \
		pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PPE_LAG_MGR_DEBUG_LEVEL < 3)
#define nss_ppe_lag_info(s, ...)
#else
#define nss_ppe_lag_info(s, ...) \
		pr_notice("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PPE_LAG_MGR_DEBUG_LEVEL < 4)
#define nss_ppe_lag_trace(s, ...)
#else
#define nss_ppe_lag_trace(s, ...) \
                pr_info("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif
#endif /* CONFIG_DYNAMIC_DEBUG */

#define NSS_PPE_LAG_STATS_DENTRY_SIZE  16

/*
 * Support 24 bond MLO devices and bond0
 */
#define NSS_PPE_LAG_MAX_BOND_DEVICES 25
#define NSS_PPE_LAG_MAX_SLAVES_PER_BOND_ID 16

/*
 * Buffer sizes
 */
#define NSS_PPE_LAG_DUMP_PREFIX_SIZE 128
#define NSS_PPE_LAG_DUMP_PREFIX_MAX 10
#define NSS_PPE_LAG_DUMP_BUFFER_SIZE 3072000

/*
 * LAG manager private structure
 */
struct nss_ppe_lag_bond_entry {
	int32_t bond_id;		/* Bond ID */
	struct net_device *bond_dev;
	bool in_use;
	struct net_device *slaves[NSS_PPE_LAG_MAX_SLAVES_PER_BOND_ID];
	struct ppe_drv_iface *iface;	/* PPE LAG iface */
	uint16_t mtu;			/* MTU for LAG */
	uint8_t dev_addr[ETH_ALEN];	/* MAC address for LAG */
};

/*
 * nss_ppe_lag_stats
 *	LAG client statistics.
 */
struct nss_ppe_lag_stats {
	atomic64_t ppe_lag_no_upper_dev;		/* Failed due to no upper dev */
	atomic64_t ppe_lag_no_bond_slave;		/* Device is not a bond slave */
	atomic64_t ppe_lag_no_bond_master;		/* Device is not a bond master */
	atomic64_t ppe_lag_unknown_slave_dev;		/* Failed due to slave interface not known to PPE */
	atomic64_t ppe_lag_invalid_lag_group_id;	/* Failed due to invalid group ID */
	atomic64_t ppe_lag_slave_full;			/* Failed due to slave maximum limit reached */
	atomic64_t ppe_lag_invalid_ppe_interface;      	/* Failed due to invalid PPE interface */
	atomic64_t ppe_lag_slave_join_fail;		/* Slave failed to join to PPE */
	atomic64_t ppe_lag_init_fail;			/* Failed to initialize LAG session in PPE */
	atomic64_t ppe_lag_deinit_fail;			/* Failed to deinitialize LAG session in PPE */
	atomic64_t ppe_lag_vlan_add_fail;		/* Failed to add VLAN to slave */
	atomic64_t ppe_lag_vlan_del_fail;		/* Failed to delete VLAN to slave */
	atomic64_t ppe_lag_bond_interface_destoyed;	/* Bond interface destroyed */
	atomic64_t ppe_lag_bond_interface_exist;	/* Bond Interface already exists */
	atomic64_t ppe_lag_bond_id_invalid;		/* Invalid bond ID */
	atomic64_t ppe_lag_bond_id_full;		/* No Bond Id remaining */
	atomic64_t ppe_lag_iface_alloc_fail;		/* LAG PPE interface allocation failed */
	atomic64_t ppe_lag_mac_set_fail; 		/* Failed to set MAC address */
	atomic64_t ppe_lag_mac_clear_fail;		/* Failed to clear MAC address */
	atomic64_t ppe_lag_mtu_set_fail;		/* Failed to set MTU */
	atomic64_t ppe_lag_unregister_event_success;	/* LAG unregister event successfull */
	atomic64_t ppe_lag_register_event_success;	/* LAG register event successfull */
	atomic64_t ppe_lag_change_mtu_event_success;	/* LAG change MTU event successfull */
	atomic64_t ppe_lag_change_addr_event_success;	/* LAG change address event successfull */
	atomic64_t ppe_lag_event_fail;			/* LAG event failed */
};

/*
 * nss_ppe_lag_ctx
 *	Context to store LAG stats
 */
struct nss_ppe_lag_ctx {
	struct nss_ppe_lag_stats stats;		/* PPE LAG statistics */
	struct dentry *dentry;			/* Root dentry for LAG client */
	struct nss_ppe_lag_bond_entry *entry;	/* PPE LAG bond entries */
};

extern struct nss_ppe_lag_ctx gbl;

/*
 * struct nss_ppe_lag_dump_instance
 *	Structure used as an instance for lag dump
 */
struct nss_ppe_lag_dump_instance {
	uint16_t bond_cnt;				/* Number of LAG devices  */
	uint16_t slave_cnt;				/* Number of Slave devices  */
	char prefix[NSS_PPE_LAG_DUMP_PREFIX_SIZE];	/* This is the prefix added to every message written */
	int prefix_levels[NSS_PPE_LAG_DUMP_PREFIX_MAX];	/* How many nested prefixes supported */
	int prefix_level;				/* Prefix nest level */
	char msg[NSS_PPE_LAG_DUMP_BUFFER_SIZE];		/* The message written / being returned to the reader */
	char *msgp;					/* Points into the msg buffer as we output it to the reader piece by piece */
	int msg_len;					/* Length of the msg buffer still to be written out */
	bool dump_en;					/* Enable dump once the file is open */
};

/*
 * nss_ppe_lag_stats_inc()
 *	Increment stats counter.
 */
static inline void nss_ppe_lag_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

void nss_ppe_lag_stats_deinit(struct nss_ppe_lag_ctx *ctx);
bool nss_ppe_lag_stats_init(struct nss_ppe_lag_ctx *ctx);
int nss_ppe_lag_dump_init(struct nss_ppe_lag_ctx *ctx);
void nss_ppe_lag_dump_exit(void);

#endif
