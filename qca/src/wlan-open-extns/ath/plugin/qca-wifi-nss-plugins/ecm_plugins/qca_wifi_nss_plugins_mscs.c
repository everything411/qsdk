/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/skbuff.h>

#include <qca_wifi_nss_plugins.h>
#include <qca_wifi_nss_plugins_ecm.h>

#ifdef QCA_WIFI_NSS_PLUGINS_MSCS
/*
 * qca_wifi_nss_plugins_get_peer_priority()
 *	Get peer priority callback into wlan driver.
 */
static inline int qca_wifi_nss_plugins_get_peer_priority(struct ecm_classifier_mscs_get_priority_info *get_priority_info)
{
	struct ath_mscs_get_priority_param wlan_get_priority_param = {0};

	wlan_get_priority_param.dst_mac = get_priority_info->dst_mac;
	wlan_get_priority_param.src_mac = get_priority_info->src_mac;
	wlan_get_priority_param.src_dev = get_priority_info->src_dev;
	wlan_get_priority_param.dst_dev = get_priority_info->dst_dev;
	wlan_get_priority_param.skb = get_priority_info->skb;

	return ath_mscs_peer_lookup_n_get_priority(&wlan_get_priority_param);
}

/*
 * 	Register MSCS client callback with ECM MSCS classifier to support MSCS wifi peer lookup.
 */
static struct ecm_classifier_mscs_callbacks qca_wifi_plugin_mscs = {
	.get_peer_priority = qca_wifi_nss_plugins_get_peer_priority,
};

/*
 * qca_wifi_nss_plugins_mscs_register()
 *	register MSCS callbacks.
 */
int qca_wifi_nss_plugins_mscs_register(void)
{
	if (ecm_classifier_mscs_callback_register(&qca_wifi_plugin_mscs)) {
		qca_wifi_nss_plugins_warning("ecm mscs classifier callback registration failed.\n");
		return -1;
	}

	qca_wifi_nss_plugins_info_always("MSCS callbacks registered\n");
	return 0;
}

/*
 *  qca_wifi_nss_plugins_mscs_unregister()
 *	unregister the mscs callbacks.
 */
void qca_wifi_nss_plugins_mscs_unregister(void)
{
	ecm_classifier_mscs_callback_unregister();
	qca_wifi_nss_plugins_info_always("MSCS callbacks unregistered\n");
}
#endif
