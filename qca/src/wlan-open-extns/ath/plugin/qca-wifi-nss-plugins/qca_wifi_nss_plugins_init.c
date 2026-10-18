/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <qca_wifi_nss_plugins.h>

#if defined(QCA_WIFI_NSS_PLUGINS_ECM_EMESH) || \
    defined(QCA_WIFI_NSS_PLUGINS_ECM_FSE) || \
    defined(QCA_WIFI_NSS_PLUGINS_MSCS) || \
    defined(QCA_WIFI_NSS_PLUGINS_ECM_WIFI_CLASSIFIER) || \
    defined(QCA_WIFI_NSS_PLUGINS_ECM_NL)
#include <qca_wifi_nss_plugins_ecm.h>
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_* */

#ifdef QCA_WIFI_NSS_PLUGINS_PPE
#include <qca_wifi_nss_plugins_ppe.h>
#endif /* QCA_WIFI_NSS_PLUGINS_PPE */

#ifdef QCA_WIFI_NSS_PLUGINS_PPEDS
#include <qca_wifi_nss_plugins_ppeds.h>
#include <ppe_public.h>
#endif /* QCA_WIFI_NSS_PLUGINS_PPEDS */

#ifdef QCA_WIFI_NSS_PLUGINS_IPA_FSE
#include <qca_wifi_nss_plugins_ipa.h>
#endif /* QCA_WIFI_NSS_PLUGINS_IPA_FSE */

/*
 * qca_wifi_nss_plugin_init_module()
 *	QCA_WIFI_PLUGIN module init function
 */
int __init qca_wifi_nss_plugin_init_module(void)
{
	int ret = 0;

#ifdef QCA_WIFI_NSS_PLUGINS_ECM_EMESH
	ret = qca_wifi_nss_plugins_emesh_register();
	if (ret) {
		qca_wifi_nss_plugins_info_always("EMESH callback registration failed\n");
		return ret;
	}
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_EMESH */

#ifdef QCA_WIFI_NSS_PLUGINS_ECM_FSE
	ret = qca_wifi_nss_plugins_fse_cb_register();
	if (ret) {
		qca_wifi_nss_plugins_info_always("FSE callback registration failed\n");
		goto fail_fse_cb_register;
	}
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_FSE */

#ifdef QCA_WIFI_NSS_PLUGINS_ECM_WIFI_CLASSIFIER
	ret = qca_wifi_nss_plugins_wifi_cb_register();
	if (ret) {
		qca_wifi_nss_plugins_info_always("WIFI callback registration failed\n");
		goto fail_wifi_cb_register;
	}
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_WIFI_CLASSIFIER */

#ifdef QCA_WIFI_NSS_PLUGINS_PPE
	ret = qca_wifi_nss_plugins_ppe_fse_cb_register();
	if (ret) {
		qca_wifi_nss_plugins_info_always("PPE_Plugin callback registration failed\n");
		goto fail_ppe_fse__cb_register;
	}
#endif /* QCA_WIFI_NSS_PLUGINS_PPE */

#ifdef QCA_WIFI_NSS_PLUGINS_PPEDS
	ret = qca_wifi_nss_plugins_ppeds_cb_register();
	if (ret) {
		qca_wifi_nss_plugins_info_always("PPEDS_Plugin callback registration failed\n");
		goto fail_ppeds_cb_register;
	}
#endif /* QCA_WIFI_NSS_PLUGINS_PPEDS */

#ifdef QCA_WIFI_NSS_PLUGINS_MSCS
	ret = qca_wifi_nss_plugins_mscs_register();
	if (ret) {
		qca_wifi_nss_plugins_info_always("MSCS_Plugin callback registration failed\n");
		goto fail_mscs_register;
	}
#endif /* QCA_WIFI_NSS_PLUGINS_MSCS */

#ifdef QCA_WIFI_NSS_PLUGINS_ECM_NL
	ret = qca_wifi_nss_plugins_nl_event_start();
	if (ret) {
		qca_wifi_nss_plugins_info_always("NL_Event listening start failed\n");
		goto fail_nl_event_start;
	}
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_NL */

#ifdef QCA_WIFI_NSS_PLUGINS_IPA_FSE
	ret = qca_wifi_nss_plugins_ipa_fse_cb_register();
	if (ret) {
		qca_wifi_nss_plugins_info_always("IPA_Plugin callback registration failed\n");
		goto fail_ipa_register;
	}
#endif /* QCA_WIFI_NSS_PLUGINS_IPA_FSE */
	qca_wifi_nss_plugins_info_always("QCA_WIFI_PLUGIN module loaded\n");
	return 0;

#ifdef QCA_WIFI_NSS_PLUGINS_IPA_FSE
fail_ipa_register:
#endif /* QCA_WIFI_NSS_PLUGINS_IPA_FSE */

#ifdef QCA_WIFI_NSS_PLUGINS_ECM_NL
	qca_wifi_nss_plugins_nl_event_stop();
fail_nl_event_start:
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_NL */

#ifdef QCA_WIFI_NSS_PLUGINS_MSCS
	qca_wifi_nss_plugins_mscs_unregister();
fail_mscs_register:
#endif /* QCA_WIFI_NSS_PLUGINS_MSCS */

#ifdef QCA_WIFI_NSS_PLUGINS_PPEDS
	qca_wifi_nss_plugins_ppeds_cb_unregister();
fail_ppeds_cb_register:
#endif /* QCA_WIFI_NSS_PLUGINS_PPEDS */

#ifdef QCA_WIFI_NSS_PLUGINS_PPE
	qca_wifi_nss_plugins_ppe_fse_cb_unregister();
fail_ppe_fse__cb_register:
#endif /* QCA_WIFI_NSS_PLUGINS_PPE */

#ifdef QCA_WIFI_NSS_PLUGINS_ECM_WIFI_CLASSIFIER
	qca_wifi_nss_plugins_wifi_cb_unregister();
fail_wifi_cb_register:
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_WIFI_CLASSIFIER */

#ifdef QCA_WIFI_NSS_PLUGINS_ECM_FSE
	qca_wifi_nss_plugins_fse_cb_unregister();
fail_fse_cb_register:
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_FSE */

#ifdef QCA_WIFI_NSS_PLUGINS_ECM_EMESH
	qca_wifi_nss_plugins_emesh_unregister();
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_EMESH */

	return ret;
}

/*
 * qca_wifi_nss_plugin_exit_module()
 *	QCA_WIFI_PLUGIN module exit function
 */
static void __exit qca_wifi_nss_plugin_exit_module(void)
{
#ifdef QCA_WIFI_NSS_PLUGINS_IPA_FSE
	qca_wifi_nss_plugins_ipa_fse_cb_unregister();
#endif /* QCA_WIFI_NSS_PLUGINS_IPA_FSE */

#ifdef QCA_WIFI_NSS_PLUGINS_ECM_NL
	qca_wifi_nss_plugins_nl_event_stop();
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_NL */

#ifdef QCA_WIFI_NSS_PLUGINS_MSCS
	qca_wifi_nss_plugins_mscs_unregister();
#endif /* QCA_WIFI_NSS_PLUGINS_MSCS */

#ifdef QCA_WIFI_NSS_PLUGINS_PPEDS
	qca_wifi_nss_plugins_ppeds_cb_unregister();
#endif /* QCA_WIFI_NSS_PLUGINS_PPEDS */

#ifdef QCA_WIFI_NSS_PLUGINS_PPE
	qca_wifi_nss_plugins_ppe_fse_cb_unregister();
#endif /* QCA_WIFI_NSS_PLUGINS_PPE */

#ifdef QCA_WIFI_NSS_PLUGINS_ECM_WIFI_CLASSIFIER
	qca_wifi_nss_plugins_wifi_cb_unregister();
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_WIFI_CLASSIFIER */

#ifdef QCA_WIFI_NSS_PLUGINS_ECM_FSE
	qca_wifi_nss_plugins_fse_cb_unregister();
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_FSE */

#ifdef QCA_WIFI_NSS_PLUGINS_ECM_EMESH
	qca_wifi_nss_plugins_emesh_unregister();
#endif /* QCA_WIFI_NSS_PLUGINS_ECM_EMESH */

	qca_wifi_nss_plugins_info_always("QCA_WIFI_PLUGIN unloaded\n");
}

module_init(qca_wifi_nss_plugin_init_module);
module_exit(qca_wifi_nss_plugin_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("QCA_WIFI_PLUGIN module");
