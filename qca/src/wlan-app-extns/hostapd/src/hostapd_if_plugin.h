/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef _HOSTAPD_IF_PLUGIN_H
#define _HOSTAPD_IF_PLUGIN_H
#include <stdint.h>
#include <stdbool.h>
#include "hostapd_if_common.h"
#include "hostapd_external_interface.h"
#include "common.h"
#include "utils/includes.h"

#include "utils/common.h"
#include "utils/bitfield.h"
#include "common/wpa_ctrl.h"

struct hostapd_config_plugin
{
	int external_plugin_auth_policy;
	int external_plugin_remote_auth_policy;
	int external_plugin_assoc_policy;
	int external_plugin_deauth_policy;
	int external_plugin_disassoc_policy;
	int external_plugin_action_policy[HOSTAPD_IF_FRAME_TYPE_ACTION_MAX];
};

/* Main dispatcher function for all plugin commands */
int hostapd_ctrl_iface_configure_plugin(struct hostapd_data *hapd,
					const char *cmd,
					char *buf, size_t buflen);
int hostapd_config_fill_plugin(struct hostapd_bss_config *bss, const char *buf,
			       char *pos);

/*
 * Called directly from wpa_ft_rrb_rx_r1 to hand off the received PMK-R1
 * to the test plugin instead of storing it in the hostapd cache.
 * This function is NOT part of hostapd_external_app_object.
 */
void hostapd_if_plugin_store_pmk_r1(const char *ifname, const u8 *spa,
				    const u8 *pmk_r1, size_t pmk_r1_len,
				    const u8 *pmk_r1_name, int pairwise,
				    int expires_in, int session_timeout,
				    const u8 *identity, size_t identity_len,
				    const u8 *radius_cui,
				    size_t radius_cui_len);

#endif /* HOSTAPD_IF_PLUGIN_H */
