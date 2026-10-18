/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/if.h>

#if defined(CONFIG_DYNAMIC_DEBUG)

/*
 * Compile messages for dynamic enable/disable
 */
#define qca_wifi_nss_plugins_err(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define qca_wifi_nss_plugins_warning(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define qca_wifi_nss_plugins_info(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define qca_wifi_nss_plugins_trace(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)

#else

/*
 * Statically compile messages at different levels
 */
#if (QCA_WIFI_NSS_PLUGINS_DEBUG_LEVEL < 2)
#define qca_wifi_nss_plugins_err(s, ...)
#else
#define qca_wifi_nss_plugins_err(s, ...) pr_err("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (QCA_WIFI_NSS_PLUGINS_DEBUG_LEVEL < 3)
#define qca_wifi_nss_plugins_warning(s, ...)
#else
#define qca_wifi_nss_plugins_warning(s, ...) pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (QCA_WIFI_NSS_PLUGINS_DEBUG_LEVEL < 4)
#define qca_wifi_nss_plugins_info(s, ...)
#else
#define qca_wifi_nss_plugins_info(s, ...)   pr_notice("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (QCA_WIFI_NSS_PLUGINS_DEBUG_LEVEL < 5)
#define qca_wifi_nss_plugins_trace(s, ...)
#else
#define qca_wifi_nss_plugins_trace(s, ...)  pr_info("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * debug message for module init and exit
 */
#define qca_wifi_nss_plugins_info_always(s, ...) printk(KERN_INFO"%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)

