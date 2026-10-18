/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/if.h>
#include <sp_api.h>
#include <qca_sawf_if.h>

#define SP_RULE_BAND_VALID 1

#if defined(CONFIG_DYNAMIC_DEBUG)

/*
 * Compile messages for dynamic enable/disable
 */
#define qca_wifi_plugins_warning(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define qca_wifi_plugins_info(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define qca_wifi_plugins_trace(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels
 */
#if (QCA_WIFI_PLUGINS_DEBUG_LEVEL < 2)
#define qca_wifi_plugins_warning(s, ...)
#else
#define qca_wifi_plugins_warning(s, ...) pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (QCA_WIFI_PLUGINS_DEBUG_LEVEL < 3)
#define qca_wifi_plugins_info(s, ...)
#else
#define qca_wifi_plugins_info(s, ...)   pr_notice("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (QCA_WIFI_PLUGINS_DEBUG_LEVEL < 4)
#define qca_wifi_plugins_trace(s, ...)
#else
#define qca_wifi_plugins_trace(s, ...)  pr_info("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * debug message for module init and exit
 */
#define qca_wifi_plugins_info_always(s, ...) printk(KERN_INFO"%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)

/*
 * qca_nss_wifi_plugins_emesh_sp_cb_register()
 * 	API to register wifi_plugin callbacks
 */
int qca_nss_wifi_plugins_emesh_sp_cb_register(void);

/*
 * qca_nss_wifi_plugins_emesh_sp_cb_unregister()
 * 	API to unregister wifi_plugin callbacks
 */
void qca_nss_wifi_plugins_emesh_sp_cb_unregister(void);
