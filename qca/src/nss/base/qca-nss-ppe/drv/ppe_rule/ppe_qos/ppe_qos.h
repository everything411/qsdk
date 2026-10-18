/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include <ppe_drv_public.h>
#include <ppe_qos.h>
#include "ppe_qos_stats.h"
#include "ppe_qos_dump.h"

/*
 * PPE RFS macros
 */
#if (PPE_QOS_DEBUG_LEVEL == 3)
#define ppe_qos_assert(c, s, ...)
#else
#define ppe_qos_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define ppe_qos_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_qos_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_qos_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_QOS_DEBUG_LEVEL < 2)
#define ppe_qos_warn(s, ...)
#else
#define ppe_qos_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_QOS_DEBUG_LEVEL < 3)
#define ppe_qos_info(s, ...)
#else
#define ppe_qos_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_QOS_DEBUG_LEVEL < 4)
#define ppe_qos_trace(s, ...)
#else
#define ppe_qos_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * TODO Use already defined macros
 */
#define PPE_DRV_QOS_TCONT_MAX 32
#define PPE_DRV_QOS_UNI_MAX 4
#define PPE_QOS_MAX_MCAST_QUEUES_PER_UNI 4
#define PPE_QOS_MCAST_PRIORITY_MAX 8

struct ppe_drv_queue_info;

/*
 * struct ppe_queue
 *	Qos structure to store ppe queue information
 */
struct ppe_qos {
	struct net_device *dev;			/* Device associated with port qos. */
	uint8_t port_id;			/* PPE port Id on which qos is configured. */
	int8_t int_pri;				/* INT_PRI value of PPE queue. */
	uint32_t handle_id;			/* Qdisc Handle ID / Class ID corresponding to ppe queue. */
};

/*
 * ppe_qos_port_queue
 *	PPE QoS port queue structure
 */
struct ppe_qos_interface_drr {
	uint32_t offset;	/* Offset from base DRR. */
	uint32_t ref_cnt;	/* used to keep track of DRR used for same priority */
};

/*
 * ppe_qos_queue_limit
 *	PPE QoS queue limit configuration
 */
struct ppe_qos_queue_limit {
	uint32_t ceiling;		/* Queue ceiling */
	bool color_en;			/* Color enable */
	bool wred_en;			/* WRED enable */
	uint32_t green_min_off;		/* Green minimum offset */
	uint32_t yellow_max_off;	/* Yellow maximum offset */
	uint32_t yellow_min_off;	/* Yellow minimum offset */
	uint32_t red_max_off;		/* Red maximum offset */
	uint32_t red_min_off;		/* Red minimum offset */
	uint32_t green_resume_off;	/* Green resume offset */
	uint32_t yellow_resume_off;	/* Yellow resume offset */
	uint32_t red_resume_off;	/* Red resume offset */
	bool is_configured;		/* Limit and threshold configured? */
};

/*
 * ppe_qos_port_queue
 *	PPE QoS port queue structure
 */
struct ppe_qos_interface_queue {
	uint32_t offset;	/* Offset from base queue. */
	uint32_t priority;	/* Priority assigned to the queue. */
	uint32_t weight;	/* Weight assigned to the queue. */
	struct ppe_qos_queue_limit limit;	/* queue thresholds configuration. */
	struct list_head list; 	/* List of queues. */
	bool valid;	/* Queue configuration is valid. */
};

/*
 * ppe_qos_port_res
 *	PPE QoS port resource structure
 */
struct ppe_qos_interface_res {
	struct list_head list;	/* List of resources */
	uint32_t id;		/* Interface/T-cont ID. */
	uint32_t num_queues; 	/* Number of unicast queues. */
	uint32_t num_mcast_queues;	/* Number of multicast queues. */
	uint32_t l0sp;	/* L0 SP for this UNI/T-cont. */
	char shaper_name[PPE_QOS_MAX_NAME_LENGTH];	/* Shaper profile name. */
	ppe_qos_interface_type_t type;	/* Physical or T-cont interface. */
	struct ppe_drv_qos_port port;		/* Port structure for QoS resources base and max info. */
	struct ppe_qos_interface_drr l0drr[PPE_DRV_QOS_PRIORITY_MAX];	/* L0 DRR assigned at each priority */
	struct list_head q_list;	/* Port's unicast queue list. */
	struct list_head mq_list;	/* Port's multicast queue list. */
	uint8_t mcast_prio_map[PPE_DRV_MAX_PRIORITY]; /* Multicast priority map (0-3) */
	bool mcast_prio_map_valid;                   /* Whether multicast priority map is configured */
	uint8_t ucast_prio_map[PPE_DRV_MAX_PRIORITY]; /* Unicast priority map (0-7) */
	bool ucast_prio_map_valid;                   /* Whether unicast priority map is configured */
	bool valid;	/* Resource is configured. */
};

/*
 * ppe_qos_shaper_profile
 *	PPE QoS shaper profile
 */
struct ppe_qos_shaper_profile {
	char name[PPE_QOS_MAX_NAME_LENGTH];			/* Shaper profile name. */
	struct ppe_drv_qos_shaper shaper;	/* Shaper parameters. */
	struct list_head list;	/* List of shaper profile. */
};

/*
 * ppe_qos_base
 *	Global ppe QoS global instance
 */
struct ppe_qos_base {
	spinlock_t lock;                                	/* PPE QoS lock. */
	struct dentry *dentry;		/* debugfs entry pointer */
	struct ppe_qos_stats stats;			/* QoS statistics */
	struct list_head shaper_list;	/* Shaper profiles list. */
	struct list_head port_list;	/* List of ports for QoS resources. */
	struct ppe_qos_interface_res tcont[PPE_DRV_QOS_TCONT_MAX];		/* Tcont structure for QoS resources. */
	uint32_t pq_to_tcont_map[PPE_DRV_QOS_TCONT_L0_RES_MAX];	/* List to keep track of PQ to Tcont mapping. */
	bool l0drr_in_use[PPE_DRV_QOS_TCONT_L0_RES_MAX];	/* List to keep track of used L0 DRR. */
	uint32_t pon_port;			/* PON port ID */
	uint32_t pon_port_num_pq;	/* Number of PQs configured for PON port */
	int qos_dump_major_id;		/* Dev ID for QoS dump */
};

extern struct ppe_qos_base gbl_ppe_qos;

struct ppe_qos_interface_res *ppe_qos_res_get(uint32_t id, ppe_qos_interface_type_t type);
void ppe_qos_deinit(void);
void ppe_qos_init(struct dentry *dentry);
