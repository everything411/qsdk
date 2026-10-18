/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <qca_wifi_nss_plugins.h>
#include <qca_wifi_nss_plugins_ppe.h>
#include <ppe_public.h>

/*
 * qca_wifi_nss_plugins_ppe_fse_rule_add()
 * 	Call WLAN API to add FSE rule
 */
static inline bool qca_wifi_nss_plugins_ppe_fse_rule_add(struct ppe_drv_fse_rule_info *ppe_rule)
{
	return ath12k_dp_rx_ppeds_fse_add_flow_entry(ppe_rule);
}

/*
 * qca_wifi_nss_plugins_ppe_fse_rule_del()
 * 	Call WLAN API to delete FSE rule
 */
static inline bool qca_wifi_nss_plugins_ppe_fse_rule_del(struct ppe_drv_fse_rule_info *ppe_rule)
{
	return ath12k_dp_rx_ppeds_fse_del_flow_entry(ppe_rule);
}

/*
 * Register wifi callbacks with PPE.
 */
static struct ppe_drv_fse_ops ppe_fse_callbacks = {
	.create_fse_rule = qca_wifi_nss_plugins_ppe_fse_rule_add,
	.destroy_fse_rule = qca_wifi_nss_plugins_ppe_fse_rule_del,
};

/*
 * qca_wifi_nss_plugins_ppe_fse_cb_register()
 * 	Registers the callback
 */
int qca_wifi_nss_plugins_ppe_fse_cb_register(void)
{
	if (!ppe_drv_fse_ops_register(&ppe_fse_callbacks)) {
		qca_wifi_nss_plugins_warning("PPE_FSE callback registration failed\n");
		return -1;
	}

	qca_wifi_nss_plugins_info_always("%p PPE_FSE callbacks are registered\n", &ppe_fse_callbacks);

	return 0;
}

/*
 * qca_wifi_nss_plugins_ppe_fse_cb_unregister()
 * 	Unregisters the callback
 */
void qca_wifi_nss_plugins_ppe_fse_cb_unregister(void)
{
	ppe_drv_fse_ops_unregister();
	qca_wifi_nss_plugins_info_always("PPE_FSE callbacks unregisterd\n");
}
