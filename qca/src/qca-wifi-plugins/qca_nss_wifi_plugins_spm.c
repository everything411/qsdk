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

#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <linux/version.h>
#include <linux/notifier.h>

#include "qca_nss_wifi_plugins.h"

/*
 * qca_nss_wifi_plugins_fill_wifi_metadata()
 * 	API to fill qca_sawf_wifi_port_params from sp_rule_wifi_plugin_metadata
 */
static inline bool qca_nss_wifi_plugins_fill_wifi_metadata(struct qca_sawf_wifi_port_params_v2 *wifi_param, struct sp_rule_wifi_plugin_metadata *wifi_metadata)
{
	memcpy(wifi_param->ssid, wifi_metadata->ssid, strlen(wifi_metadata->ssid) + 1);
	wifi_param->ac = wifi_metadata->access_class;
	wifi_param->priority = wifi_metadata->priority;
	wifi_param->pcp = wifi_metadata->pcp;
	wifi_param->valid_flags = wifi_metadata->valid_flags;
	wifi_param->dscp = wifi_metadata->dscp;
	wifi_param->ssid_len = wifi_metadata->ssid_len;

	ether_addr_copy(wifi_param->ta_mac, wifi_metadata->ta_mac);
	ether_addr_copy(wifi_param->ra_mac, wifi_metadata->ra_mac);
	ether_addr_copy(wifi_param->bssid, wifi_metadata->bssid);
	memcpy(wifi_param->band.value, wifi_metadata->radio_band, SP_RULE_MAX_VDEV_PER_ML);
	wifi_param->band.flag = (wifi_metadata->band_mode == 1);
	memcpy(wifi_param->channel.value, wifi_metadata->radio_chan, SP_RULE_MAX_VDEV_PER_ML);
	wifi_param->channel.flag = (wifi_metadata->channel_mode == 1);
	memcpy(wifi_param->bw.value, wifi_metadata->radio_bw, SP_RULE_MAX_VDEV_PER_ML);
	wifi_param->bw.flag = (wifi_metadata->bandwidth_mode == 1);

	return true;
}

/*
 * qca_nss_wifi_plugins_emesh_sp_wlan_rule_query()
 * 	Validate wifi_metadata from WLAN deriver
 */
static inline bool qca_nss_wifi_plugins_emesh_sp_wlan_rule_query(struct sp_rule_wifi_plugin_metadata *wifi_metadata)
{
	struct qca_sawf_wifi_port_params_v2 wifi_param = {0};
	bool status;

	if (!qca_nss_wifi_plugins_fill_wifi_metadata(&wifi_param, wifi_metadata)) {
		return false;
	}

	status = qca_sdwf_match_wifi_port_params(wifi_metadata->dest_dev, wifi_metadata->dest_mac, wifi_metadata->src_dev, wifi_metadata->src_mac, wifi_metadata->skb_prio, &wifi_param);
	return status;

}

/*
 * qca_nss_wifi_plugins_emesh_sp_wlan_rule_validate()
 * 	Validate wifi_metadata from WLAN deriver
 */
static inline bool qca_nss_wifi_plugins_emesh_sp_wlan_rule_validate(struct sp_rule_wifi_plugin_metadata *wifi_metadata)
{
	struct qca_sawf_wifi_port_params_v2 wifi_param = {0};
	bool status;

	if (!qca_nss_wifi_plugins_fill_wifi_metadata(&wifi_param, wifi_metadata)) {
		return false;
	}

	qca_wifi_plugins_info("Validating wifi params with WLAN driver\n");
	status = qca_sdwf_validate_wifi_port_params_v2(&wifi_param);
	return status;
}

/*
 * Register wifi callbacks with emesh-sp.
 */
static struct emesh_sp_wifi_sawf_callbacks wifi_plugin_callback = {
	.sawf_rule_query_callback =  qca_nss_wifi_plugins_emesh_sp_wlan_rule_query,
	.sawf_rule_validate_callback = qca_nss_wifi_plugins_emesh_sp_wlan_rule_validate,
};

/*
 * qca_nss_wifi_plugins_emesh_sp_cb_register()
 * 	Registers the callback
 */
int qca_nss_wifi_plugins_emesh_sp_cb_register(void)
{
	if (sp_mapdb_wifi_plugin_validate_wlan_rule_cb_register(&wifi_plugin_callback)) {
		qca_wifi_plugins_info("\nEmesh-sp wifi-plugin validate callback registration failed\n");
		return -1;
	}

	if (sp_mapdb_wifi_plugin_query_wlan_rule_cb_register(&wifi_plugin_callback)) {
		sp_mapdb_wifi_plugin_validate_wlan_rule_cb_unregister();
		qca_wifi_plugins_info("\nEmesh-sp wifi-plugin query callback registration failed\n");
		return -1;
	}

	qca_wifi_plugins_info("\nEmesh-sp wifi-plugin callbacks are registered\n");

	return 0;
}

/*
 * qca_nss_wifi_plugins_emesh_sp_cb_unregister()
 * 	Unregisters the callback
 */
void qca_nss_wifi_plugins_emesh_sp_cb_unregister(void)
{
	sp_mapdb_wifi_plugin_validate_wlan_rule_cb_unregister();
	sp_mapdb_wifi_plugin_query_wlan_rule_cb_unregister();
	qca_wifi_plugins_info_always("\nEmesh-sp wifi-plugin callbacks unregisterd\n");
}
