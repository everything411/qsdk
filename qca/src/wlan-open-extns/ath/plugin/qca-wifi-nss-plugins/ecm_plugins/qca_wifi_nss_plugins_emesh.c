/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>

#include <qca_wifi_nss_plugins.h>
#include <qca_wifi_nss_plugins_ecm.h>

/*
 * qca_wifi_nss_plugins_emesh_sawf_conn_sync()
 *	Connection sync callback for EMESH-SAWF classifier.
 */
static inline void qca_wifi_nss_plugins_emesh_sawf_conn_sync(struct ecm_classifer_emesh_sawf_sync_params *sawf_sync_params)
{
	struct ath_ul_params sawf_params = {0};

	sawf_params.src_dev = sawf_sync_params->src_dev;
	sawf_params.dst_dev = sawf_sync_params->dest_dev;
	sawf_params.dst_mac = sawf_sync_params->dest_mac;
	sawf_params.src_mac = sawf_sync_params->src_mac;
	sawf_params.fw_service_id = sawf_sync_params->fwd_service_id;
	sawf_params.rv_service_id = sawf_sync_params->rev_service_id;
	sawf_params.start_or_stop = sawf_sync_params->add_or_sub;
	sawf_params.fw_mark_metadata = sawf_sync_params->fwd_mark_metadata;
	sawf_params.rv_mark_metadata = sawf_sync_params->rev_mark_metadata;

	qca_wifi_nss_plugins_info("Sync SAWF params:  dst_mac: %pM, src_mac: %pM, fw_service_id: %u, rv_service_id: %u\n, start_or_stop: %u, fw_mark_metadata: %u, rv_mark_metadata: %u\n",
			sawf_params.dst_mac, sawf_params.src_mac, sawf_params.fw_service_id, sawf_params.rv_service_id,
			sawf_params.start_or_stop, sawf_params.fw_mark_metadata, sawf_params.rv_mark_metadata);

	ath_sawf_uplink(&sawf_params);
}

/*
 * qca_wifi_nss_plugins_emesh_ecm_valid_to_wifi_valid()
 *	Convert the ECM SAWF valid flags to Wi-Fi driver valid flags.
 */
static inline uint32_t qca_wifi_nss_plugins_emesh_ecm_valid_to_wifi_valid(uint32_t valid_flag)
{
	if (valid_flag & ECM_CLASSIFIER_EMESH_SAWF_SVID_VALID) {
		return ATH_SAWF_SVID_VALID;
	}

	if (valid_flag & ECM_CLASSIFIER_EMESH_SAWF_DSCP_VALID) {
		return ATH_SAWF_DSCP_VALID;
	}

	if (valid_flag & ECM_CLASSIFIER_EMESH_SAWF_VLAN_PCP_VALID) {
		return ATH_SAWF_PCP_VALID;
	}

	return 0;
}

/*
 * qca_wifi_nss_plugins_emesh_sawf_get_mark_data()
 * 	get skb mark callback for EMESH-SAWF classifier.
 */
static inline uint32_t qca_wifi_nss_plugins_emesh_sawf_get_mark_data(struct ecm_classifier_emesh_sawf_flow_info *sawf_flow_info)
{
	struct ath_dp_metadata_param metadata = {0};
	uint32_t result;

	metadata.is_sawf_param_valid = 1;
	metadata.sawf_param.netdev = sawf_flow_info->netdev;
	metadata.sawf_param.peer_mac = sawf_flow_info->peer_mac;
	metadata.sawf_param.service_id = sawf_flow_info->service_id;
	metadata.sawf_param.dscp = sawf_flow_info->dscp;
	metadata.sawf_param.rule_id = sawf_flow_info->rule_id;
	metadata.sawf_param.sawf_rule_type = sawf_flow_info->sawf_rule_type;
	metadata.sawf_param.pcp = sawf_flow_info->vlan_pcp;
	metadata.sawf_param.dscp = sawf_flow_info->dscp;
	metadata.sawf_param.valid_flag = qca_wifi_nss_plugins_emesh_ecm_valid_to_wifi_valid(sawf_flow_info->valid_flag);
	metadata.sawf_param.mcast_flag = sawf_flow_info->is_mc_flow;
	metadata.is_scs_mscs = sawf_flow_info->is_scs_mscs;

	qca_wifi_nss_plugins_info("Mark SAWF params: rule_type: %u, pcp: %u, dscp: %u, service_id %u, rule_id %u\n, valid_flag %u, mcast_flag: %u, net_device: %s, peer_mac: %pM\n",
			     metadata.sawf_param.sawf_rule_type,
			     metadata.sawf_param.pcp, metadata.sawf_param.dscp, metadata.sawf_param.service_id,
			     metadata.sawf_param.rule_id, metadata.sawf_param.valid_flag, metadata.sawf_param.mcast_flag,
			     metadata.sawf_param.netdev->name, metadata.sawf_param.peer_mac);

	result = ath_get_metadata_info(&metadata);

	sawf_flow_info->out_wifi_arch_info = metadata.out_wifi_arch_info;

	return result;
}

/*
 * qca_wifi_nss_plugins_emesh
 * 	Register EMESH client callback with ECM EMSH classifier to update peer mesh latency parameters.
 */
static struct ecm_classifier_emesh_sawf_callbacks qca_wifi_nss_plugins_emesh = {
	.update_service_id_get_msduq = qca_wifi_nss_plugins_emesh_sawf_get_mark_data,
	.sawf_conn_sync = qca_wifi_nss_plugins_emesh_sawf_conn_sync,
};

/*
 * qca_wifi_nss_plugins_emesh_register()
 *	Register emesh callbacks.
 */
int qca_wifi_nss_plugins_emesh_register(void)
{

	if (ecm_classifier_emesh_sawf_msduq_callback_register(&qca_wifi_nss_plugins_emesh)) {
		qca_wifi_nss_plugins_warning("ecm emesh msduq callback registration failed.\n");
		return -1;
	}

	if (ecm_classifier_emesh_sawf_conn_sync_callback_register(&qca_wifi_nss_plugins_emesh)) {
		ecm_classifier_emesh_sawf_msduq_callback_unregister();
		qca_wifi_nss_plugins_warning("ecm emesh config sawf ul callback registration failed.\n");
		return -1;
	}

	qca_wifi_nss_plugins_info_always("EMESH classifier callbacks registered\n");
	return 0;
}

/*
 * qca_wifi_nss_plugins_emesh_unregister()
 *	unregister the emesh callbacks.
 */
void qca_wifi_nss_plugins_emesh_unregister(void)
{
	ecm_classifier_emesh_sawf_msduq_callback_unregister();
	ecm_classifier_emesh_sawf_conn_sync_callback_unregister();

	qca_wifi_nss_plugins_info("EMESH classifier callbacks unregistered\n");
}
