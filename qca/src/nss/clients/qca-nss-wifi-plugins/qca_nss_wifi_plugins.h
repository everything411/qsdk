/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/types.h>
#include <linux/list.h>
#include <nss_plugins.h>
#include <ppe_ds_wlan.h>

#if defined(CONFIG_DYNAMIC_DEBUG)
#define nss_plugins_warn(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_plugins_info(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_plugins_trace(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)

#else /* CONFIG_DYNAMIC_DEBUG */
/*
 * Statically compile messages at different levels
 */
#if (NSS_PLUGINS_DEBUG_LEVEL < 2)
#define nss_plugins_warn(s, ...)
#else
#define nss_plugins_warn(s, ...) \
		pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PLUGINS_DEBUG_LEVEL < 3)
#define nss_plugins_info(s, ...)
#else
#define nss_plugins_info(s, ...) \
		pr_notice("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PLUGINS_DEBUG_LEVEL < 4)
#define nss_plugins_trace(s, ...)
#else
#define nss_plugins_trace(s, ...) \
	pr_info("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif
#endif /* CONFIG_DYNAMIC_DEBUG */

/*
 * qca_nss_wifi_plugin_ds_gbl
 * 	plugin ds global ctx
 */
struct qca_nss_wifi_plugin_ds_gbl {
	struct list_head active_nodes;	/* List of active nodes */
	spinlock_t lock;		/* Plugin lock */
};

/*
 * qca_nss_wifi_plugin_ds_node
 * 	plugin ds node object
 */
struct qca_nss_wifi_plugin_ds_node {
	struct list_head list;		/* list head */
	struct ppe_ds *node;		/* ds node */
	struct kref ref_cnt;		/* ref count */
	int ppeds_node_id;		/* ds node id */
};
