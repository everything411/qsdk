/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <ath_sawf.h>
#include <ath_fse.h>
#include <ath_dp_accel_cfg.h>

#include <ecm_classifier_emesh_public.h>
#include <ecm_classifier_wifi_public.h>
#ifdef QCA_WIFI_NSS_PLUGINS_MSCS
#include <ecm_classifier_mscs_public.h>
#endif
#include <ecm_front_end_common_public.h>

#define QCA_WIFI_NSS_PLUGINS_SAWF_TAG 0xAA
#define QCA_WIFI_NSS_PLUGINS_SAWF_TAG_SHIFT 8
#define QCA_WIFI_NSS_PLUGINS_SAWF_SERVICE_CLASS_MASK 0xFF
#define QCA_WIFI_NSS_PLUGINS_SAWF_SERVICE_CLASS_SHIFT 16
#define QCA_WIFI_NSS_PLUGINS_SAWF_MSDUQ_MASK 0xFFFF

#define QCA_WIFI_NSS_PLUGINS_METADATA_INVALID_DS_NODE 0xFF

/*
 * qca_wifi_nss_plugins_wifi_cb_register()
 * 	API to register wifi programming callbacks.
 */
int qca_wifi_nss_plugins_wifi_cb_register(void);

/*
 * qca_wifi_nss_plugins_wifi_cb_unregister()
 * 	API to unregister wifi programming callbacks.
 */
void qca_wifi_nss_plugins_wifi_cb_unregister(void);

/*
 * qca_wifi_nss_plugins_fse_cb_register()
 * 	API to register FSE programming callbacks.
 */
int qca_wifi_nss_plugins_fse_cb_register(void);

/*
 * qca_wifi_nss_plugins_fse_cb_unregister()
 * 	API to unregister FSE programming callbacks.
 */
void qca_wifi_nss_plugins_fse_cb_unregister(void);

/*
 * qca_wifi_nss_plugins_emesh_register()
 * 	API to register emesh callbacks.
 */
int qca_wifi_nss_plugins_emesh_register(void);

/*
 * qca_wifi_nss_plugins_emesh_unregister()
 * 	API to unregister emesh callbacks.
 */
void qca_wifi_nss_plugins_emesh_unregister(void);

#ifdef QCA_WIFI_NSS_PLUGINS_MSCS
/*
 * qca_wifi_nss_plugins_mscs_register()
 *	register MSCS callbacks.
 */
int qca_wifi_nss_plugins_mscs_register(void);

/*
 *  qca_wifi_nss_plugins_mscs_unregister()
 *	unregister the mscs callbacks.
 */
void qca_wifi_nss_plugins_mscs_unregister(void);
#endif

/*
 *  qca_wifi_nss_plugins_nl_event_start()
 *	start netlink nl80211 event listening.
 */
int qca_wifi_nss_plugins_nl_event_start(void);

/*
 *  qca_wifi_nss_plugins_nl_event_stop()
 *	stop netlink nl80211 event listening.
 */
int qca_wifi_nss_plugins_nl_event_stop(void);
