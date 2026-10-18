/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <qca_wifi_nss_plugins.h>
#include <qca_wifi_nss_plugins_ipa.h>
#include <ipa/wifi8/dp_ipa_fse.h>

/**
 * qca_wifi_nss_plugins_ipa_fse_rule_add() - Add IPA FSE rule
 * @rule: Pointer to the FSE rule to be added
 *
 * This function adds a new FSE rule by calling the underlying
 * ath12k datapath function to create the flow entry.
 *
 * Return: 0 on success, -ve on failure
 */
static inline int
qca_wifi_nss_plugins_ipa_fse_rule_add(struct ipa_fse_rule *rule)
{
	return ath12k_dp_rx_ipa_fse_add_flow_entry(rule);
}

/**
 * qca_wifi_nss_plugins_ipa_fse_rule_del() - Delete IPA FSE rule
 * @rule: Pointer to the FSE rule to be deleted
 *
 * This function deletes an existing FSE rule by calling the underlying
 * ath12k datapath function to remove the flow entry.
 *
 * Return: 0 on success, -ve on failure
 */
static inline int
qca_wifi_nss_plugins_ipa_fse_rule_del(struct ipa_fse_rule *rule)
{
	return ath12k_dp_rx_ipa_fse_del_flow_entry(rule);
}

static struct ipa_fse_ops ipa_fse_ops = {
	.create_fse_rule = qca_wifi_nss_plugins_ipa_fse_rule_add,
	.destroy_fse_rule = qca_wifi_nss_plugins_ipa_fse_rule_del,
};

/**
 * qca_wifi_nss_plugins_ipa_fse_cb_register() - Register IPA FSE callbacks
 *
 * This function registers the FSE operation callbacks with the IPA subsystem.
 * The callbacks enable IPA to create and destroy FSE rules through the
 * WiFi driver.
 *
 * Context: Can be called during module initialization
 * Return: 0 on success, -1 on failure
 */
int qca_wifi_nss_plugins_ipa_fse_cb_register(void)
{
	int ret = ipa_fse_ops_register(&ipa_fse_ops);

	if (ret) {
		qca_wifi_nss_plugins_warning("IPA fse callback registration failed (%d)\n",
					     ret);
		return -1;
	}

	qca_wifi_nss_plugins_info_always("%p IPA fse callbacks are registered\n",
					 &ipa_fse_ops);

	return 0;
}

/**
 * qca_wifi_nss_plugins_ipa_fse_cb_unregister() - Unregister IPA FSE callbacks
 *
 * This function unregisters the FSE operation callbacks from the IPA subsystem.
 * Should be called during module cleanup to properly release resources.
 *
 * Context: Can be called during module exit
 */
void qca_wifi_nss_plugins_ipa_fse_cb_unregister(void)
{
	ipa_fse_ops_unregister();
	qca_wifi_nss_plugins_info_always("IPA fse callbacks unregisterd\n");
}
