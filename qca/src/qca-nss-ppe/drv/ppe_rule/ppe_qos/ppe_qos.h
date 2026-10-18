/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

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

struct ppe_drv_qos_port;
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
	struct ppe_drv_qos_port ppe_qos_port;	/* HW res details of PPE qos port details. */
};

void ppe_qos_deinit(void);
void ppe_qos_init(struct dentry *dentry);
