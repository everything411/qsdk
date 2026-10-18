/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <linux/version.h>
#include <linux/notifier.h>

#include <qca_wifi_nss_plugins.h>
#include <qca_wifi_nss_plugins_ecm.h>

/*
 * qca_wifi_plugin_fill_fse_wlan_info()
 * 	Fill the FSE info for wlan FSE rule add / delete.
 */
static inline void qca_wifi_plugin_fill_fse_wlan_info(struct ecm_front_end_fse_info *fse_info,
		struct ath_fse_flow_info *fse_wlan_info)
{
	/*
	 * Fill the wlan tuple info.
	 */
	memcpy(&fse_wlan_info->src_ip, &fse_info->src, sizeof(fse_wlan_info->src_ip));
	memcpy(&fse_wlan_info->dest_ip, &fse_info->dest, sizeof(fse_wlan_info->dest_ip));
	fse_wlan_info->src_dev = fse_info->src_dev;
	fse_wlan_info->dest_dev = fse_info->dest_dev;
	fse_wlan_info->src_port = fse_info->src_port;
	fse_wlan_info->dest_port = fse_info->dest_port;
	fse_wlan_info->protocol = fse_info->protocol;
	fse_wlan_info->protocol = fse_info->protocol;
	fse_wlan_info->version = fse_info->ip_version;
	fse_wlan_info->src_mac = fse_info->src_mac;
	fse_wlan_info->dest_mac = fse_info->dest_mac;
	fse_wlan_info->fw_svc_id = ECM_CLASSIFIER_EMESH_SAWF_INVALID_SVID;
	fse_wlan_info->rv_svc_id = ECM_CLASSIFIER_EMESH_SAWF_INVALID_SVID;
}


/*
 * qca_wifi_nss_plugins_fse_create_rule()
 * 	Fill the FSE info and call into wlan driver to create FSE rule.
 */
static inline bool qca_wifi_nss_plugins_fse_create_rule(struct ecm_front_end_fse_info *fse_info)
{
	struct ath_fse_flow_info fse_wlan_info = {0};

	/*
	 * Fill the wlan tuple info and call wlan callback.
	 */
	qca_wifi_plugin_fill_fse_wlan_info(fse_info, &fse_wlan_info);

	return ath_fse_add_rule(&fse_wlan_info);
}

/*
 * qca_wifi_nss_plugins_fse_destroy_rule()
 * 	Fill the FSE info and call into wlan driver to destroy FSE rule.
 */
static inline bool qca_wifi_nss_plugins_fse_destroy_rule(struct ecm_front_end_fse_info *fse_info)
{
	struct ath_fse_flow_info fse_wlan_info = {0};

	/*
	 * Fill the wlan tuple info and call wlan callback.
	 */
	qca_wifi_plugin_fill_fse_wlan_info(fse_info, &fse_wlan_info);

	return ath_fse_delete_rule(&fse_wlan_info);
}

/*
 * qca_wifi_plugin_fse_ops
 *	Register Wi-Fi FSE (Flow Search Engine) related callbacks with
 *	ECM frontend to program FSE rules from ECM.
 */
static struct ecm_front_end_fse_callbacks qca_wifi_plugin_fse_ops = {
	.create_fse_rule = qca_wifi_nss_plugins_fse_create_rule,
	.destroy_fse_rule = qca_wifi_nss_plugins_fse_destroy_rule,
};

/*
 * qca_wifi_nss_plugins_fse_cb_register()
 * 	Register FSE callbacks with ECM frontend.
 */
int qca_wifi_nss_plugins_fse_cb_register(void)
{
	if (ecm_front_end_fse_callbacks_register(&qca_wifi_plugin_fse_ops)) {
		qca_wifi_nss_plugins_warning("FSE callback registration failed for ECM frontend\n");
		return -1;
	}

	qca_wifi_nss_plugins_info_always("FSE callbacks registered with plugin\n");

	return 0;
}

/*
 * qca_wifi_nss_plugins_fse_cb_unregister()
 * 	Unregister FSE callbacks with ECM frontend.
 */
void qca_wifi_nss_plugins_fse_cb_unregister(void)
{
	ecm_front_end_fse_callbacks_unregister();

	qca_wifi_nss_plugins_info_always("ECM frontend FSE callbacks unregistered with plugin\n");
}
