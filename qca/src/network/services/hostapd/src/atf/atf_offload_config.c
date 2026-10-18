/*
 * Airtime Fairness offload config parsing and cli apis
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.*
 */

#include <sys/un.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "ap/hostapd.h"
#include "ap/ap_drv_ops.h"
#include "ap/sta_info.h"

#include "atf_offload.h"
#include "atf_offload_config.h"

bool atf_validate_group_configured_airtime(struct atf_algo *algo,
					   struct atf_group *group,
					   u32 airtime)
{
	if ((algo->user_cfg_airtime - group->user_cfg_airtime) +
	    airtime > ATF_RADIO_DEFAULT_AIRTIME) {
		wpa_printf(MSG_ERROR, "ATF: Air time should be between 0 and %d\n",
			   100 - (algo->user_cfg_airtime / 10));
		return false;
	}

	return true;
}


bool atf_validate_ssid_configured_airtime(struct atf_algo *algo,
					  struct atf_ssid_config *ssid_config,
					  u32 airtime)
{
	if ((algo->user_cfg_airtime - ssid_config->user_cfg_airtime) +
	    airtime > ATF_RADIO_DEFAULT_AIRTIME) {
		wpa_printf(MSG_ERROR, "ATF: Air time should be between 0 and %d\n",
			   100 - (algo->user_cfg_airtime / 10));
		return false;
	}

	return true;
}


bool atf_validate_peer_configured_airtime(struct atf_group *group,
					  struct atf_peer_config *peer_config,
					  u32 airtime)
{
	if ((group->expl_peers_airtime - peer_config->user_cfg_airtime) +
	    airtime > ATF_RADIO_DEFAULT_AIRTIME) {
		wpa_printf(MSG_ERROR,
				"New explicit peers airtime for SSID/SSID GROUP exceeds 100, config between 0 and %d\n",
				100 - (group->expl_peers_airtime / 10));
		return false;
	}

	return true;
}


int
atf_set_ssid_config(char *ssidname, int airtime, struct hostapd_iface *iface)
{
	struct atf_ssid_config *ssid_config = NULL;
	struct atf_group *group = NULL;
	struct atf_algo *algo;

	if (!iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;
	ssid_config = atf_find_ssid_config_by_name(ssidname, algo);
	if (!ssid_config) {
		ssid_config = atf_allocate_ssid_config(ssidname, iface->atf_algo);
		if (!ssid_config) {
			wpa_printf(MSG_ERROR, "ATF: Failed to allocate ssid_config\n");
			return -1;
		}

		if (!atf_validate_ssid_configured_airtime(algo, ssid_config, airtime)) {
			atf_free_ssid_config(ssid_config);
			return -1;
		}

		group = atf_allocate_group(ssidname, algo);
		if (!group) {
			atf_free_ssid_config(ssid_config);
			wpa_printf(MSG_ERROR, "ATF: Failed to allocate SSID group\n");
			return -1;
		}

		atf_add_ssid_to_group(group, ssidname);
		ssid_config->group = group;
	} else {
		if (!atf_validate_ssid_configured_airtime(algo, ssid_config, airtime))
			return -1;

		algo->user_cfg_airtime -= ssid_config->user_cfg_airtime;
		group = ssid_config->group;
	}

	ssid_config->user_cfg_airtime = airtime;
	algo->user_cfg_airtime += airtime;
	group->user_cfg_airtime = airtime;

	wpa_printf(MSG_INFO, "ATF: Added SSID name %s airtime :%.1f\n",
		   ssid_config->name, ((double)(ssid_config->user_cfg_airtime) / 10));
	return 0;
}


static int
parse_ssid_set(struct atf_group *group, char *buf, int line)
{
	char ssid_list[WLAN_SSID_MAX][WLAN_SSID_MAX_LEN + 1] = { 0 };
	int count = 0;
	char *p;
	int i;

	if (!buf)
		return -1;

	p = buf;
	while (*p != '\0') {
		while (*p == ' ')
			p++;

		if (*p == '\0')
			break;

		i = 0;
		while (*p != ' ' && *p != '\0' && i < WLAN_SSID_MAX_LEN) {
			ssid_list[count][i++] = *p++;
		}
		ssid_list[count][i] = '\0';

		count++;
		if (count >= WLAN_SSID_MAX) {
			wpa_printf(MSG_ERROR, "ATF: Line %d: Too many SSIDs in '%s'",
				   line, buf);
			return -1;
		}
	}

	for (i = 0; i < count; i++) {
		os_strlcpy(group->ssidname[i], ssid_list[i], WLAN_SSID_MAX_LEN);
	}

	group->num_of_ssid = count;

	return 0;
}


int
atf_read_config(struct atf_algo *algo, const char *conf_file)
{
	FILE *file;
	char buffer[4096], *pos;
	int line = 0, invalid_config = 0, error = 0;
	struct atf_group *atf_group;
	struct atf_ssid_config *ssid_config;
	struct atf_peer_config *peer_config;
	u8 sta_mac[ETH_ALEN];
	int val;
	u32 scaled_airtime;

	if (!conf_file || !algo)
		return 0;

	file = fopen(conf_file, "r");
	if (!file) {
		wpa_printf(MSG_ERROR, "ATF: atf_offload_config file '%s' not found.", conf_file);
		return -1;
	}

	while (fgets(buffer, sizeof(buffer), file)) {
		line++;

		/* skip any commented config */
		if (buffer[0] == '#')
			continue;

		pos = buffer;

		/* Remove the new line character */
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos++;
		}

		/* if the string is empty continue. */
		if (buffer[0] == '\0')
			continue;

		pos = os_strchr(buffer, '=');
		if (!pos) {
			wpa_printf(MSG_ERROR, "ATF: Line %d: Invalid line '%s'", line,
			           buffer);
			invalid_config++;
			continue;
		}

		/* Replaces the '=' character with a null terminator. */
		*pos = '\0';
		pos++;

		if (os_strcmp(buffer, "atf-group") == 0) {
			if (!algo->ssid_group_enabled) {
				wpa_printf(MSG_ERROR, "ATF: ssid group config is not enabled");
				invalid_config++;
				continue;
			}

			atf_group = atf_find_group_by_name(pos, algo);
			if (!atf_group) {
				atf_group = atf_allocate_group(pos, algo);
				if (!atf_group) {
					wpa_printf(MSG_ERROR, "ATF: could not create group");
					error++;
					goto exit;
				}
			}
			algo->last_group = atf_group;

		} else if (os_strcmp(buffer, "atf-group-ssid") == 0) {
			if (!algo->ssid_group_enabled) {
				wpa_printf(MSG_ERROR, "ATF: ssid group config is not enabled");
				invalid_config++;
				continue;
			}

			if (*pos == '\0') {
				invalid_config++;
				continue;
			}

			if (parse_ssid_set(algo->last_group, pos, line) < 0) {
				invalid_config++;
				goto exit;
			}

		} else if (os_strcmp(buffer, "atf-group-airtime") == 0) {
			if (!algo->ssid_group_enabled) {
				wpa_printf(MSG_ERROR, "ATF: ssid group config is not enabled");
				invalid_config++;
				continue;
			}

			val = atoi(pos);
			atf_group = algo->last_group;

			if (val < 0 || val > 100) {
				wpa_printf(MSG_ERROR, "ATF: invalid airtime. Valid values (0 - 100)");
				invalid_config++;
				continue;
			}

			scaled_airtime = SCALE_PERCENTAGE_TO_U32(val);

			if (!atf_validate_group_configured_airtime(algo, atf_group,
								   scaled_airtime)) {
				invalid_config++;
				goto exit;
			}

			algo->user_cfg_airtime -= atf_group->user_cfg_airtime;
			algo->user_cfg_airtime += scaled_airtime;
			atf_group->user_cfg_airtime = scaled_airtime;
		} else if (os_strcmp(buffer, "atf-del-group") == 0) {
			if (!algo->ssid_group_enabled) {
				wpa_printf(MSG_ERROR, "ATF: ssid group config is not enabled");
				invalid_config++;
				continue;
			}

			if (*pos == '\0') {
				invalid_config++;
				continue;
			}

			atf_group = atf_find_group_by_name(pos, algo);
			if (!atf_group) {
				wpa_printf(MSG_ERROR, "ATF: group not found for %s", pos);
				error++;
				continue;
			}
			algo->user_cfg_airtime -= atf_group->user_cfg_airtime;
			atf_free_group(atf_group);
		} else if (os_strcmp(buffer, "atf-ssid") == 0) {

			if (algo->ssid_group_enabled) {
				wpa_printf(MSG_ERROR, "ATF: ssid config is not allowed when ssid group is enabled");
				invalid_config++;
				continue;
			}

			ssid_config = atf_find_ssid_config_by_name(pos, algo);
			if (!ssid_config) {
				ssid_config = atf_allocate_ssid_config(pos, algo);
				if (!ssid_config) {
					wpa_printf(MSG_ERROR, "ATF: could not allocate ssid config");
					error++;
					goto exit;
				}

				/* Add group for each ssid, this group
				 * will be used in distribution algorithm
				 */
				atf_group = atf_allocate_group(pos, algo);
				if (!atf_group) {
					wpa_printf(MSG_ERROR, "ATF: could not allocate group for ssid config");
					error++;
					goto exit;
				}
				ssid_config->group = atf_group;
			} else {
				atf_group = atf_find_group_by_name(pos, algo);
				if (!atf_group) {
					wpa_printf(MSG_ERROR, "ATF: group not found for %s", pos);
					error++;
					goto exit;
				}
			}
			algo->last_ssid_cfg = ssid_config;
			algo->last_group = atf_group;
		} else if (os_strcmp(buffer, "atf-ssid-airtime") == 0) {
			if (algo->ssid_group_enabled) {
				wpa_printf(MSG_ERROR, "ATF: ssid config is not allowed when ssid group is enabled");
				invalid_config++;
				continue;
			}

			val = atoi(pos);
			ssid_config = algo->last_ssid_cfg;

			if (val < 0 || val > 100) {
				wpa_printf(MSG_ERROR, "ATF: incorrect airtime");
				invalid_config++;
				continue;
			}
			scaled_airtime = SCALE_PERCENTAGE_TO_U32(val);
			if (!atf_validate_ssid_configured_airtime(algo,
								  ssid_config,
								  scaled_airtime)) {
				invalid_config++;
				goto exit;
			}

			algo->user_cfg_airtime -= ssid_config->user_cfg_airtime;
			ssid_config->user_cfg_airtime = scaled_airtime;
			algo->last_group->user_cfg_airtime = scaled_airtime;
			algo->user_cfg_airtime += scaled_airtime;
		} else if (os_strcmp(buffer, "atf-del-ssid") == 0) {
			if (algo->ssid_group_enabled) {
				wpa_printf(MSG_ERROR, "ATF: ssid config is not allowed when ssid group is enabled");
				invalid_config++;
				continue;
			}

			ssid_config = atf_find_ssid_config_by_name(pos, algo);
			if (!ssid_config) {
				wpa_printf(MSG_ERROR, "ATF: could not find ssid_config");
				invalid_config++;
				continue;
			}

			algo->user_cfg_airtime -= ssid_config->user_cfg_airtime;
			atf_free_group(ssid_config->group);
			atf_free_ssid_config(ssid_config);
		} else if (os_strcmp(buffer, "atf-sta") == 0) {
			if (hwaddr_aton(pos, sta_mac)) {
				wpa_printf(MSG_ERROR,
				           "ATF: Line %d: invalid sta mac address", line);
				invalid_config++;
				goto exit;
			}
			peer_config = atf_find_peer_config_by_mac(sta_mac, algo);
			if (!peer_config) {
				peer_config = atf_allocate_peer_config(sta_mac, algo);
				if (!peer_config) {
					wpa_printf(MSG_ERROR,
					           "ATF: could not allocate peer config");
					error++;
					goto exit;
				}
			}
			algo->last_peer_cfg = peer_config;
		} else if (os_strcmp(buffer, "atf-sta-airtime") == 0) {
			val = atoi(pos);
			if (val < 0 || val > 100) {
				wpa_printf(MSG_ERROR,
				           "ATF: invalid airtime %d (valid range 0 -100)",
				           val);
				invalid_config++;
				continue;
			}

			scaled_airtime = SCALE_PERCENTAGE_TO_U32(val);

			if (!atf_validate_peer_configured_airtime(algo->last_peer_cfg->group,
								  algo->last_peer_cfg,
								  scaled_airtime)) {
				invalid_config++;
				goto exit;
			}
			atf_group = algo->last_peer_cfg->group;
			atf_group->expl_peers_airtime += scaled_airtime;
			algo->last_peer_cfg->user_cfg_airtime = scaled_airtime;
		} else if (os_strcmp(buffer, "atf-sta-ssid") == 0) {
			if (*pos != '\0') {
				peer_config = algo->last_peer_cfg;
				atf_group = atf_find_group_by_name(pos, algo);
				if (!atf_group) {
					wpa_printf(MSG_ERROR,
					           "ATF: ssid/group %s not found for "
					           "station" MACSTR " ",
					           pos,
					           MAC2STR(algo->last_peer_cfg->addr));
					error++;
					goto exit;
				}

				if (peer_config->group && peer_config->group != atf_group) {
                                        peer_config->group->expl_peers_airtime -= peer_config->user_cfg_airtime;
                                        peer_config->user_cfg_airtime = 0;
                                }

				os_strlcpy(algo->last_peer_cfg->group_name, pos,
				           WLAN_SSID_MAX_LEN);
				peer_config->group = atf_group;
			}
		} else if (os_strcmp(buffer, "atf-del-sta") == 0) {
			if (hwaddr_aton(pos, sta_mac)) {
				wpa_printf(MSG_ERROR,
				           "ATF: Line %d: invalid sta mac address", line);
				invalid_config++;
				continue;
			}
			peer_config = atf_find_peer_config_by_mac(sta_mac, algo);
			if (!peer_config) {
				wpa_printf(MSG_ERROR,
				           "ATF: could not find the station " MACSTR
				           " config",
				           MAC2STR(sta_mac));
				invalid_config++;
				continue;
			}
			atf_free_peer_config(peer_config);
		}
	}

	if (invalid_config)
		wpa_printf(MSG_ERROR, "ATF: Ignoring %d minor errors in config file", invalid_config);
	fclose(file);
	return 0;

exit:
	wpa_printf(MSG_ERROR, "ATF: Fatal error %d occurred, clean up algo configs", error);
	fclose(file);

	/* In case of fatal error, clean up all configurations */
	atf_free_algo_configs(algo, true);

	return -1;
}


static int
hostapd_ctrl_iface_atf_offload_commitatf(struct hostapd_data *hapd,
					 const char *cmd)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	u8 enabled, radio_index;
	int ret;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	if (!hapd->drv_priv) {
		wpa_printf(MSG_ERROR, "ATF: Invalid hapd data %s\n", __func__);
		return -1;
	}

	algo = iface->atf_algo;
	enabled = atoi(cmd);

	if (enabled != 0 && enabled != 1) {
		wpa_printf(MSG_ERROR, "ATF: Invalid input for atf enabled\n");
		return -1;
	}

	radio_index = atf_get_hw_idx(iface);
	if (algo->atf_enabled != enabled) {
		ret = nl80211_atf_offload_enable_disable(hapd->drv_priv, radio_index, enabled);
		if (ret) {
			wpa_printf(MSG_ERROR, "ATF: Failed to enable ATF\n");
			return -1;
		}
	}

	algo->atf_enabled = enabled;
	iface->conf->commitatf = enabled;

	if (algo->atf_enabled) {
		ATF_OFFLOAD_SET_FULL_UPDATE(algo);
		atf_trigger_config_timer(iface);
	}

	return 0;
}


static int
hostapd_ctrl_iface_atf_offload_get_commitatf(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	int ret, len = 0;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	ret = os_snprintf(buf, buflen, "ATF is %s\n",
			  algo->atf_enabled ? "enabled" : "disabled");
	if (!os_snprintf_error(buflen, ret))
		len += ret;

	return len;
}


static int
hostapd_ctrl_iface_atf_offload_atfstrictsched(struct hostapd_data *hapd,
					      const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	u8 atf_scheduling, radio_index;
	int ret;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!hapd->drv_priv) {
		wpa_printf(MSG_ERROR, "ATF: Invalid hapd data %s\n", __func__);
		return -1;
	}

	atf_scheduling = atoi(cmd);

	if (algo->atfstrictsched_enabled == atf_scheduling) {
		wpa_printf(MSG_ERROR, "ATF: Current ATF scheduling is %d\n",
			   algo->atfstrictsched_enabled);
		return -1;
	}

	if (atf_scheduling > 1) {
		wpa_printf(MSG_ERROR, "ATF: Invalid atf sctrict scheduling\n");
		return -1;
	}

	radio_index = atf_get_hw_idx(iface);

	ret = nl80211_atf_offload_strict_scheduling_enable_disable(hapd->drv_priv,
								   radio_index,
								   atf_scheduling);
	if (ret) {
		wpa_printf(MSG_ERROR, "ATF: Failed to enable strict scheduling\n");
		return ret;
	}

	algo->atfstrictsched_enabled = atf_scheduling;

	wpa_printf(MSG_INFO, "ATF: ATF strict scheduling is %s",
		   algo->atfstrictsched_enabled ? "enabled" : "disabled");

	return 0;
}


static int
hostapd_ctrl_iface_atf_offload_gatfstrictsched(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	int ret, len = 0;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	ret = os_snprintf(buf, buflen, "ATF scheduling is %s\n",
			  algo->atfstrictsched_enabled ? "strict" : "fair");
	if (!os_snprintf_error(buflen, ret))
		len += ret;

	return len;
}


static int
hostapd_ctrl_iface_atf_offload_atfssidgroup(struct hostapd_data *hapd,
					    const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	u8 atf_ssid_group;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;
	atf_ssid_group = atoi(cmd);

	if (atf_ssid_group != 0 && atf_ssid_group != 1) {
		wpa_printf(MSG_ERROR, "ATF: Invalid input for atfssidgroup\n");
		return -1;
	}

	if (algo->ssid_group_enabled != atf_ssid_group)
		atf_free_algo_configs(algo, true);

	algo->ssid_group_enabled = atf_ssid_group;
	iface->conf->atf_ssid_grp = atf_ssid_group;

	wpa_printf(MSG_INFO, "ATF: ATF SSID group is %s\n",
		   algo->ssid_group_enabled ? "enabled" : "disabled");

	return 0;
}


static int
hostapd_ctrl_iface_atf_offload_g_atfssidgroup(struct hostapd_data *hapd,
					      char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	int ret, len = 0;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	ret = os_snprintf(buf, buflen, "ATF SSID group is %s\n",
			  algo->ssid_group_enabled ? "enabled" : "disabled");
	if (!os_snprintf_error(buflen, ret))
		len += ret;

	return len;
}


static int
hostapd_ctrl_iface_atf_offload_addatfgroup(struct hostapd_data *hapd,
					   const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	char *input, *group_name, *ssid_name, *context = NULL;
	struct atf_group *group;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!algo->ssid_group_enabled) {
		wpa_printf(MSG_ERROR, "ATF: ATF ssid group is not enabled\n");
		return -1;
	}

	input = os_strdup(cmd);
	if (!input)
		return -1;

	group_name = str_token(input, " ", &context);
	if (!group_name) {
		wpa_printf(MSG_ERROR, "ATF: Group name not found\n");
		goto fail;
	}

	if (os_strlen(group_name) > WLAN_SSID_MAX_LEN) {
		wpa_printf(MSG_ERROR, "ATF: Group length exceeds the allowed limit");
		goto fail;
	}

	ssid_name = str_token(input, " ", &context);
	if (!ssid_name) {
		wpa_printf(MSG_ERROR, "ATF: SSID name not found\n");
		goto fail;
	}

	if (os_strlen(ssid_name) > WLAN_SSID_MAX_LEN) {
		wpa_printf(MSG_ERROR, "ATF: SSID length exceeds the allowed limit");
		goto fail;
	}

	group = atf_find_group_by_name(group_name, algo);
	if (group) {
		if (group->num_of_ssid == WLAN_SSID_MAX) {
			wpa_printf(MSG_ERROR, "ATF: group has maximum allowed number of SSIDs\n");
			return -1;
		}

		atf_delete_ssid_from_group(algo, ssid_name);
		atf_add_ssid_to_group(group, ssid_name);
	} else {
		group = atf_allocate_group(group_name, algo);
		if (!group) {
			wpa_printf(MSG_ERROR, "ATF: Failed to allocate greoup config\n");
			goto fail;
		}
		atf_delete_ssid_from_group(algo, ssid_name);
		atf_add_ssid_to_group(group, ssid_name);
		wpa_printf(MSG_INFO, "ATF: group name %s num_of_ssid %d\n",
			   group->name, group->num_of_ssid);
	}

	os_free(input);
	return 0;
fail:
	os_free(input);
	return -1;
}


static int
hostapd_ctrl_iface_atf_offload_configatfgroup(struct hostapd_data *hapd,
					      const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_group *group;
	char *input, *group_name, *a_time, *context = NULL;
	int airtime;
	u32 scaled_airtime;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!algo->ssid_group_enabled) {
		wpa_printf(MSG_ERROR, "ATF: ATF ssid group is not enabled\n");
		return -1;
	}

	input = os_strdup(cmd);
	if (!input)
		return -1;

	group_name = str_token(input, " ", &context);
	if (!group_name) {
		wpa_printf(MSG_ERROR, "ATF: Group name not found\n");
		goto fail;
	}

	if (os_strlen(group_name) > WLAN_SSID_MAX_LEN) {
		wpa_printf(MSG_ERROR, "ATF: Group length exceeds the allowed limit");
		goto fail;
	}

	a_time = str_token(input, " ", &context);
	if (!a_time) {
		wpa_printf(MSG_ERROR, "ATF: airtime not found\n");
		goto fail;
	}

	airtime = atoi(a_time);
	scaled_airtime = SCALE_PERCENTAGE_TO_U32(airtime);

	group = atf_find_group_by_name(group_name, algo);
	if (!group) {
		wpa_printf(MSG_ERROR, "ATF: Group not found\n");
		goto fail;
	}

	if (!atf_validate_group_configured_airtime(algo, group, scaled_airtime))
		goto fail;

	algo->user_cfg_airtime -= group->user_cfg_airtime;
	algo->user_cfg_airtime += scaled_airtime;
	group->user_cfg_airtime = scaled_airtime;

	os_free(input);
	return 0;
fail:
	wpa_printf(MSG_ERROR, "ATF: Failed to config SSID group\n");
	os_free(input);
	return -1;
}


static int
hostapd_ctrl_iface_atf_offload_delatfgroup(struct hostapd_data *hapd,
					   const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_group *group;
	char group_name[WLAN_SSID_MAX_LEN + 1];

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!algo->ssid_group_enabled) {
		wpa_printf(MSG_ERROR, "ATF: ATF ssid group is not enabled\n");
		return -1;
	}

	if (os_strlen(cmd) > WLAN_SSID_MAX_LEN) {
		wpa_printf(MSG_ERROR, "ATF: Group length exceeds the allowed limit");
		return -1;
	}

	os_strlcpy(group_name, cmd, sizeof(group_name));
	group_name[sizeof(group_name) - 1] = '\0';

	group = atf_find_group_by_name(group_name, algo);
	if (!group) {
		wpa_printf(MSG_ERROR, "ATF: Group not found\n");
		return -1;
	}

	algo->user_cfg_airtime -= group->user_cfg_airtime;
	atf_free_group(group);

	return 0;
}


static int
hostapd_ctrl_iface_atf_offload_showatfgroup(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_group *group;
	int len = 0, j, ret;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!iface->conf->atf_offload) {
		wpa_printf(MSG_ERROR, "ATF: ATF is not enabled\n");
		return -1;
	}

	if (!algo->ssid_group_enabled) {
		wpa_printf(MSG_ERROR, "ATF: ATF Group is not enabled\n");
		return -1;
	}

	if (dl_list_empty(&algo->groups)) {
		wpa_printf(MSG_DEBUG, "ATF: No entry is present in list\n");
		return -1;
	}

	ret = os_snprintf(buf, buflen, "\n%-20s%-12s%s", "Group", "Airtime", "SSID List");
	if (!os_snprintf_error(buflen, ret))
		len += ret;

	dl_list_for_each(group, &algo->groups, struct atf_group, list)
	{
		ret = os_snprintf(buf + len, buflen - len, "\n%-20s",
				  group->name);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;

		ret = os_snprintf(buf + len, buflen - len, "%-10d",
				  group->user_cfg_airtime / 10);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;

		for (j = 0; j < group->num_of_ssid; j++) {
			ret = os_snprintf(buf + len, buflen - len, "%-2s ",
					  group->ssidname[j]);
			if (!os_snprintf_error(buflen - len, ret))
				len += ret;
		}
	}
	ret = os_snprintf(buf + len, buflen - len, "\n");
	if (!os_snprintf_error(buflen - len, ret))
		len += ret;

	return len;
}


static int
hostapd_ctrl_iface_atf_offload_atfgroupsched(struct hostapd_data *hapd,
					     const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_group *group;
	char *input, *group_name, *atf_sched_policy, *context = NULL;
	u8 sched_policy;
	int ret = 0;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!algo->ssid_group_enabled) {
		wpa_printf(MSG_ERROR, "ATF: ATF ssid group is not enabled\n");
		return -1;
	}

	input = os_strdup(cmd);
	if (!input)
		return -1;

	group_name = str_token(input, " ", &context);
	if (!group_name) {
		wpa_printf(MSG_ERROR, "ATF: Group name not found\n");
		ret = -1;
		goto fail;
	}

	atf_sched_policy = str_token(input, " ", &context);
	if (!atf_sched_policy) {
		wpa_printf(MSG_ERROR, "ATF: atf_sched_policy not found\n");
		ret = -1;
		goto fail;
	}

	sched_policy = atoi(atf_sched_policy);

	if (sched_policy < ATF_FAIR_SCHEDULING ||
	    sched_policy > ATF_FAIR_WITH_UPPER_BOUND_SCHEDULING) {
		ret = -1;
		wpa_printf(MSG_ERROR, "ATF: Sched_policy  is not within limits\n");
		goto fail;
	}

	group = atf_find_group_by_name(group_name, algo);
	if (!group) {
		ret = -1;
		wpa_printf(MSG_ERROR, "ATF: Group not found\n");
		goto fail;
	}
	group->sched_policy = sched_policy;

fail:
	os_free(input);
	return ret;
}


static int
hostapd_ctrl_iface_atf_offload_g_atfgroupsched(struct hostapd_data *hapd,
					       const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_group *group;
	char *input, *group_name, *context = NULL;
	int ret, len = 0;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing iface\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!algo->ssid_group_enabled) {
		wpa_printf(MSG_ERROR, "ATF: ssid group is not enabled\n");
		return -1;
	}

	input = os_strdup(cmd);
	if (!input)
		return -1;

	group_name = str_token(input, " ", &context);
	if (!group_name) {
		wpa_printf(MSG_ERROR, "ATF: Group name not found\n");
		os_free(input);
		return -1;
	}

	group = atf_find_group_by_name(group_name, algo);
	if (!group) {
		wpa_printf(MSG_ERROR, "ATF: Group not found\n");
		os_free(input);
		return -1;
	}

	ret = os_snprintf(buf, buflen, "ATF group scheduling is %s\n",
			 group->sched_policy == 0 ? "fair" :
			 group->sched_policy == 1 ? "strict" :
			 group->sched_policy == 2 ? "restricted fair" :
                         "unknown");
	if (!os_snprintf_error(buflen, ret))
		len += ret;

	return len;
}


static int
hostapd_ctrl_iface_atf_offload_atfssidsched(struct hostapd_data *hapd,
					    const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	u8 atf_ssid_scheduling, radio_index;
	int link_id = -1, ret;
	struct atf_group *group = NULL;
	char ssid_buf[SSID_MAX_LEN + 1];
	struct hostapd_ssid *ssid;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	ssid = &hapd->conf->ssid;
	os_memset(ssid_buf, 0, sizeof(ssid_buf));
	os_memcpy(ssid_buf, ssid->ssid, ssid->ssid_len);
	ssid_buf[ssid->ssid_len] = '\0';

	algo = iface->atf_algo;

	group = atf_find_group(algo, ssid_buf);
	if (!group ||
	    os_strncmp(group->name, "default-group", strlen("default-group")) == 0) {
		wpa_printf(MSG_ERROR, "ATF: SSID is not configured\n");
		return -1;
	}

	if (algo->ssid_group_enabled) {
		wpa_printf(MSG_ERROR, "ATF: SSID config not allowed when ssid group is enabled\n");
		return -1;
	}

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	radio_index = atf_get_hw_idx(iface);

	atf_ssid_scheduling = atoi(cmd);

	if (atf_ssid_scheduling < ATF_FAIR_SCHEDULING ||
	    atf_ssid_scheduling > ATF_FAIR_WITH_UPPER_BOUND_SCHEDULING) {
		wpa_printf(MSG_ERROR, "ATF: Invalid SSID scheduling\n");
		return -1;
	}

	ret = nl80211_atf_offload_ssid_sched_policy(hapd->drv_priv, radio_index,
						    atf_ssid_scheduling, link_id);
	if (ret) {
		wpa_printf(MSG_ERROR, "ATF: Failed to set ssid scheduling policy\n");
		return ret;
	}

	hapd->conf->atf_ssid_sched = atf_ssid_scheduling;
	wpa_printf(MSG_INFO, "ATF: ATF SSID scheduling is %s",
		   hapd->conf->atf_ssid_sched ? "enabled" : "disabled");
	return 0;
}


static int
hostapd_ctrl_iface_atf_offload_g_atfssidsched(struct hostapd_data *hapd,
					      char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	int ret, len = 0;

	if (!iface) {
		wpa_printf(MSG_ERROR, "ATF: Missing iface\n");
		return -1;
	}

	ret = os_snprintf(buf, buflen, "ATF SSID scheduling is %s\n",
			  hapd->conf->atf_ssid_sched == 0 ? "fair" :
			  hapd->conf->atf_ssid_sched == 1 ? "strict" :
			  hapd->conf->atf_ssid_sched == 2 ? "restricted fair" :
			  "unknown");
	if (!os_snprintf_error(buflen, ret))
		len += ret;

	return len;
}


static int
hostapd_ctrl_iface_atf_offload_addssid(struct hostapd_data *hapd,
				       const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	char *input, *ssid, *a_time, *context = NULL;
	int airtime, ret = 0;
	u32 scaled_airtime;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (algo->ssid_group_enabled) {
		wpa_printf(MSG_ERROR, "ATF: SSID config not allowed when ssid group is enabled\n");
		return -1;
	}

	input = os_strdup(cmd);
	if (!input)
		return -1;

	ssid = str_token(input, " ", &context);
	if (!ssid || os_strlen(ssid) > WLAN_SSID_MAX_LEN) {
		wpa_printf(MSG_ERROR, "ATF: Invalid SSID\n");
		ret = -1;
		goto fail;
	}

	a_time = str_token(input, " ", &context);
	if (!a_time) {
		wpa_printf(MSG_ERROR, "ATF: Airtime not found\n");
		ret = -1;
		goto fail;
	}

	airtime = atoi(a_time);
	scaled_airtime = SCALE_PERCENTAGE_TO_U32(airtime);

	if (atf_set_ssid_config(ssid, scaled_airtime, iface)) {
		wpa_printf(MSG_ERROR, "ATF: Failed to add ssid config\n");
		ret = -1;
		goto fail;
	}

fail:
	os_free(input);
	return ret;
}


static int
hostapd_ctrl_iface_atf_offload_delssid(struct hostapd_data *hapd,
				       const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_ssid_config *ssid_config = NULL;
	char ssid[WLAN_SSID_MAX_LEN + 1];

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (algo->ssid_group_enabled) {
		wpa_printf(MSG_ERROR, "ATF: ATF ssid group is enabled\n");
		return -1;
	}

	if (os_strlen(cmd) > WLAN_SSID_MAX_LEN) {
		wpa_printf(MSG_ERROR, "ATF: SSID length exceeds the allowed limit");
		return -1;
	}

	os_strlcpy(ssid, cmd, sizeof(ssid));
	ssid[sizeof(ssid) - 1] = '\0';

	ssid_config = atf_find_ssid_config_by_name(ssid, algo);
	if (!ssid_config) {
		wpa_printf(MSG_ERROR, "ATF: No entry is present in list\n");
		return -1;
	}

	algo->user_cfg_airtime -= ssid_config->user_cfg_airtime;
	atf_free_group(ssid_config->group);
	atf_free_ssid_config(ssid_config);

	return 0;
}


static int
hostapd_ctrl_iface_atf_offload_addsta(struct hostapd_data *hapd,
				      const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_ssid_config *ssid_config;
	struct atf_group *group;
	struct atf_peer_config *peer_config;
	char *input, *name, *a_time, *token, *context = NULL;
	u8 addr[ETH_ALEN];
	int airtime, ret = 0;
	u32 scaled_airtime;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!iface->conf->atf_offload) {
		wpa_printf(MSG_ERROR, "ATF: ATF is not enabled\n");
		return -1;
	}

	input = os_strdup(cmd);
	if (!input)
		return -1;

	token = str_token(input, " ", &context);
	if (!token || hwaddr_aton(token, addr)) {
		wpa_printf(MSG_ERROR, "ATF: Invalid or missing STA MAC address\n");
		ret = -1;
		goto fail;
	}

	a_time = str_token(input, " ", &context);
	if (!a_time) {
		wpa_printf(MSG_ERROR, "ATF: airtime not found\n");
		ret = -1;
		goto fail;
	}

	name = str_token(input, " ", &context);
	if (!name || os_strlen(name) > WLAN_SSID_MAX_LEN) {
		wpa_printf(MSG_ERROR, "ATF: SSID/SSID group is invalid\n");
		ret = -1;
		goto fail;
	}

	airtime = atoi(a_time);
	scaled_airtime = SCALE_PERCENTAGE_TO_U32(airtime);

	if (algo->ssid_group_enabled) {
		group = atf_find_group_by_name(name, algo);
		if (!group) {
			wpa_printf(MSG_ERROR, "ATF: Invalid group name\n");
			ret = -1;
			goto fail;
		}
	} else {
		ssid_config = atf_find_ssid_config_by_name(name, algo);
		if (!ssid_config) {
			wpa_printf(MSG_ERROR, "ATF: Invalid SSID name\n");
			ret = -1;
			goto fail;
		}
		group = ssid_config->group;
	}

	peer_config = atf_find_peer_config_by_mac(addr, algo);
	if (!peer_config) {
		peer_config = atf_allocate_peer_config(addr, algo);
		if (!peer_config) {
			wpa_printf(MSG_ERROR, "ATF: Failed to allocate peer config\n");
			ret = -1;
			goto fail;
		}

		if (!atf_validate_peer_configured_airtime(group, peer_config, scaled_airtime)) {
			atf_free_peer_config(peer_config);
			ret = -1;
			goto fail;
		}

		peer_config->user_cfg_airtime = scaled_airtime;
	} else {
		if (peer_config->group != group) {
			peer_config->group->expl_peers_airtime -= peer_config->user_cfg_airtime;
			peer_config->user_cfg_airtime = 0;
		}

		if (!atf_validate_peer_configured_airtime(group, peer_config, scaled_airtime)) {
			ret = -1;
			goto fail;
		}

		group->expl_peers_airtime -= peer_config->user_cfg_airtime;
		peer_config->user_cfg_airtime = scaled_airtime;
	}

	peer_config->group = group;
	group->expl_peers_airtime += scaled_airtime;

fail:
	os_free(input);
	return ret;
}


static int
hostapd_ctrl_iface_atf_offload_delsta(struct hostapd_data *hapd,
				      const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_peer_config *peer_config;
	char *input, *token, *context = NULL;
	u8 addr[ETH_ALEN];
	int ret = 0;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!iface->conf->atf_offload) {
		wpa_printf(MSG_ERROR, "ATF: ATF is not enabled\n");
		return -1;
	}

	input = os_strdup(cmd);
	if (!input)
		return -1;

	token = str_token(input, " ", &context);
	if (!token || hwaddr_aton(token, addr)) {
		wpa_printf(MSG_ERROR, "ATF: Invalid or missing STA MAC address\n");
		ret = -1;
		goto fail;
	}

	peer_config = atf_find_peer_config_by_mac(addr, algo);
	if (!peer_config) {
		wpa_printf(MSG_DEBUG, "ATF: Peer entry not found\n");
		ret = -1;
		goto fail;
	} else {
		peer_config->group->expl_peers_airtime -= peer_config->user_cfg_airtime;
		atf_free_peer_config(peer_config);
	}

fail:
	os_free(input);
	return ret;
}


static int
hostapd_ctrl_iface_atf_offload_showatftable(struct hostapd_data *hapd,
					    char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_group *group;
	struct atf_peer_config *peer_config;
	struct sta_info *sta, *tmp;
	struct hostapd_data *bss;
	u8 addr[ETH_ALEN];
	int len = 0, ret;
	struct atf_bh_peer *bh;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!iface->conf->atf_offload) {
		wpa_printf(MSG_ERROR, "ATF: ATF is not enabled\n");
		return -1;
	}

	wpa_printf(MSG_INFO, "\n\n                      ");
	wpa_printf(MSG_INFO, "SHOW   ATF    TABLE\n");
	wpa_printf(MSG_INFO, "%-16s %-22s %-20s %-24s %-24s\n",
		   "SSID","Client(MAC Address)","Air time(Percentage)",
		   "Config ATF(Percentage)","Peer_Assoc_Status(1--Assoc,0-No-Assoc)");

	if (dl_list_empty(&algo->groups)) {
		wpa_printf(MSG_ERROR, "ATF: No ssid configuration found\n");
		return -1;
	}

	dl_list_for_each(group, &algo->groups, struct atf_group, list) {
		wpa_printf(MSG_INFO, "%-50s %-15.1f %.1f\n",
			   group->name,
			   group->is_configured ? group->calculated_airtime / 10.0 :
			   group->user_cfg_airtime / 10.0,
			   group->user_cfg_airtime / 10.0);

		if (dl_list_empty(&group->explicit_peers))
			goto implicit_peers;

		dl_list_for_each_safe(sta, tmp, &group->explicit_peers,
				      struct sta_info, atf_candidate_list) {
			if (sta->atf_peer.peer_cfg_ref) {
				wpa_printf(MSG_INFO, "%-20s" MACSTR " %-12s %-20.1f %-24.1f %s\n",
					   "", MAC2STR(sta->atf_peer.peer_cfg_ref->addr)," ",
					   sta->atf_peer.calculated_airtime / 10.0,
					   sta->atf_peer.peer_cfg_ref->user_cfg_airtime / 10.0, "1");
			}
		}

implicit_peers:
		/* To print unconfigured and associated peers */
		if (dl_list_empty(&group->implicit_peers))
			goto explicit_peers;

		dl_list_for_each_safe(sta, tmp, &group->implicit_peers,
				      struct sta_info, atf_candidate_list) {
			if (sta->atf_peer.calculated_airtime) {
				bss = sta->atf_peer.bss;

				if (ap_sta_is_mld(bss, sta))
					memcpy(addr, sta->mld_info.links[hapd->mld_link_id].peer_addr, 6);
				else
					memcpy(addr, sta->addr, 6);

				wpa_printf(MSG_INFO, "%-20s" MACSTR " %-12s %-20.1f %-24d %s\n",
					   "", MAC2STR(addr), " ",
					   sta->atf_peer.calculated_airtime / 10.0,
					   0, "1");
			}
		}

explicit_peers:
		/* To print the airtime of configured and unassociated peers */
		if (dl_list_empty(&algo->peer_cfgs))
			continue;

		dl_list_for_each(peer_config, &algo->peer_cfgs,
				 struct atf_peer_config, list) {
			if (peer_config->calculated_for_airtime == 0 &&
			    peer_config->group == group) {
				wpa_printf(MSG_INFO, "%-20s" MACSTR " %-12s %-20d %-24.1f %s\n",
					   "", MAC2STR(peer_config->addr), " ",
					   0, peer_config->user_cfg_airtime / 10.0, "0");
			}
		}
	}

	/* Print backhaul peers (root AP) stored in algo->bh_peers */
	dl_list_for_each(bh, &algo->bh_peers, struct atf_bh_peer, list) {
		wpa_printf(MSG_INFO, "%-20s" MACSTR " %-12s %-20.1f %-24d %s\n",
			   "", MAC2STR(bh->addr), " ",
			   bh->atf_peer.calculated_airtime / 10.0, 0, "1");
	}

	ret = os_snprintf(buf, buflen, "Check hostapd logs for atf table\n");

	if (!os_snprintf_error(buflen, ret))
		len += ret;

	return len;
}


static int
hostapd_ctrl_iface_atf_offload_showairtime(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_peer_config *peer;
	int len = 0, ret;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!iface->conf->atf_offload) {
		wpa_printf(MSG_ERROR, "ATF: ATF is not enabled\n");
		return -1;
	}

	if (!algo->num_peer_cfg) {
		wpa_printf(MSG_ERROR, "ATF: No peers configured\n");
		return -1;
	}

	ret = os_snprintf(buf, buflen,
			  "\n\n                   SHOW   AIRTIME    TABLE\n");
	if (!os_snprintf_error(buflen, ret))
		len += ret;
	ret = os_snprintf(buf + len, buflen - len,
			  " Client(MAC Address)        Air time(Percentage)\n");
	if (!os_snprintf_error(buflen - len, ret))
		len += ret;

	if (dl_list_empty(&algo->peer_cfgs)) {
		ret = os_snprintf(buf + len, buflen - len,
				  "ATF: Peer configuration table is empty\n");
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

	dl_list_for_each(peer, &algo->peer_cfgs, struct atf_peer_config, list) {
		ret = os_snprintf(buf + len, buflen - len,
				  " " MACSTR "         ",
				  MAC2STR(peer->addr));
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;

		ret = os_snprintf(buf + len, buflen - len,
				  "	%.1f\n",
				  peer->user_cfg_airtime / 10.0);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

	return len;
}


static int
hostapd_ctrl_iface_atf_offload_flushatftable(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!iface->conf->atf_offload) {
		wpa_printf(MSG_ERROR, "ATF: ATF is not enabled\n");
		return -1;
	}

	atf_free_algo_configs(algo, true);

	wpa_printf(MSG_INFO, "ATF: ATF table id flushed\n");

	return 0;
}


static int
hostapd_ctrl_iface_atf_offload_enable_atf_stats(struct hostapd_data *hapd,
						const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	int ret, atf_stats;
	u8 radio_index;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	atf_stats = atoi(cmd);

	if (atf_stats != 0 && atf_stats != 1) {
		wpa_printf(MSG_ERROR, "ATF: Invalid input for atf stats, expected 0/1\n");
		return -1;
	}

	if (algo->atf_stats_enabled == atf_stats) {
		wpa_printf(MSG_ERROR, "ATF: ATF stats already %s, skipping NL command\n",
			   atf_stats ? "enabled" : "disabled");
		return -1;
	}

	radio_index = atf_get_hw_idx(iface);
	ret = nl80211_atf_offload_stats_enable_disable(hapd->drv_priv,
						       radio_index,
						       atf_stats);
	if (ret) {
		wpa_printf(MSG_ERROR, "ATF: Failed to enable ATF stats\n");
		return -1;
	}

	if (!algo->atf_stats_timeout)
		algo->atf_stats_timeout = ATF_OFFLOAD_STATS_DEFAULT_TIMEOUT;

	algo->atf_stats_enabled = atf_stats;

	wpa_printf(MSG_INFO, "ATF: ATF stats is %s\n", atf_stats ? "enabled" : "disabled");

	return ret;
}


static int
hostapd_ctrl_iface_atf_offload_g_enable_atf_stats(struct hostapd_data *hapd,
						  char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	int len = 0, ret;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	ret = os_snprintf(buf, buflen, "ATF stats is %s\n",
			  iface->atf_algo->atf_stats_enabled ? "enabled" : "disabled");
	if (!os_snprintf_error(buflen, ret))
		len += ret;

	return len;
}


static int
hostapd_ctrl_iface_atf_offload_atf_stats_timeout(struct hostapd_data *hapd,
						 const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	u8 stats_timeout;
	int ret, timeout;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!algo->atf_stats_enabled) {
		wpa_printf(MSG_ERROR, "ATF: ATF stats is not enabled\n");
		return -1;
	}

	timeout = atoi(cmd);

	if (timeout > 50 || timeout < 10) {
		wpa_printf(MSG_ERROR, "ATF: atf_stats_timeout value between 10 and 50 seconds\n");
		return -1;
	}

	stats_timeout = (u8)timeout;
	ret = nl80211_atf_offload_stats_timeout(hapd->drv_priv,
						hapd->iface->current_hw_info->hw_idx,
						stats_timeout);
	if (ret) {
		wpa_printf(MSG_ERROR, "ATF: Failed to set ATF stats timeout\n");
		return ret;
	}

	algo->atf_stats_timeout = stats_timeout;

	wpa_printf(MSG_INFO, "ATF: ATF stats timeout is %d\n", algo->atf_stats_timeout);

	return 0;
}


static int
hostapd_ctrl_iface_atf_offload_g_atf_stats_timeout(struct hostapd_data *hapd,
						   char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	int len = 0, ret;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	ret = os_snprintf(buf, buflen, "ATF stats time out is %d\n",
			  iface->atf_algo->atf_stats_timeout);
	if (!os_snprintf_error(buflen, ret))
		len += ret;

	return len;
}


/*
 * atf_update_group_ac_airtime - Update per-AC airtime accumulators for a group.
 *
 * When peer_stats is NULL the accumulators are zeroed (reset mode), which
 * must be done once per group at the start of each stats update cycle.
 * Otherwise peer_stats contributions are added to the existing values.
 *
 * @group:       Group whose accumulators are updated.
 * @peer_stats:  Per-peer airtime consumption, or NULL to reset.
 */
static void
atf_update_group_ac_airtime(struct atf_group *group,
			    struct atf_airtime_consumption *peer_stats)
{
	int ac;

	for (ac = 0; ac < WMM_AC_NUM; ac++) {
		if (!peer_stats) {
			group->ac_actual_airtime[ac] = 0;
			group->ac_ul_airtime[ac] = 0;
		} else {
			group->ac_actual_airtime[ac] +=
				peer_stats->tx_consumption[ac].consumption;
			group->ac_ul_airtime[ac] +=
				peer_stats->rx_consumption[ac].consumption;
		}
	}
}


static int atf_offload_update_peer_airtime(struct hostapd_data *hapd)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_airtime_consumption *airtime_stats;
	struct atf_group *group;
	struct atf_peer_config *peer_config;
	struct atf_peer *implicit_peer;
	struct sta_info *sta, *tmp;
	u32 peer_airtime, peer_ul_airtime, radio_actual_airtime = 0, radio_ul_airtime = 0;
	int ac;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;
	airtime_stats = &algo->radio_airtime;

	if (dl_list_empty(&algo->groups)) {
		wpa_printf(MSG_ERROR, "ATF: No ssid/group configuration found\n");
		return -1;
	}

	for (ac = 0; ac < WMM_AC_NUM; ac++) {
		radio_actual_airtime += airtime_stats->tx_consumption[ac].consumption;
		radio_ul_airtime += airtime_stats->rx_consumption[ac].consumption;
	}

	dl_list_for_each(group, &algo->groups, struct atf_group, list) {
		group->actual_airtime = 0;
		group->ul_airtime = 0;
		group->actual_duration = 0;
		group->actual_ul_duration = 0;

		atf_update_group_ac_airtime(group, NULL);

		if (dl_list_empty(&group->explicit_peers))
			goto implicit_peers;

		dl_list_for_each_safe(sta, tmp, &group->explicit_peers,
				      struct sta_info, atf_candidate_list) {
			if (sta->atf_peer.peer_cfg_ref) {
				peer_config = sta->atf_peer.peer_cfg_ref;
				airtime_stats = &peer_config->peer_airtime;
				peer_airtime = 0;
				peer_ul_airtime = 0;

				for (ac = 0; ac < WMM_AC_NUM; ac++) {
					peer_airtime +=
						airtime_stats->tx_consumption[ac].consumption;
					peer_ul_airtime +=
						airtime_stats->rx_consumption[ac].consumption;
				}

				if (peer_airtime > 0 && radio_actual_airtime > 0) {
					peer_config->actual_duration =
						peer_airtime;
					peer_config->actual_airtime =
						(u32)((u64)peer_airtime * 100ULL / radio_actual_airtime);
				} else {
					peer_config->actual_duration = 0;
					peer_config->actual_airtime = 0;
				}

				if (peer_ul_airtime > 0 && radio_ul_airtime > 0) {
					peer_config->actual_ul_duration =
						peer_ul_airtime;
					peer_config->ul_airtime =
						(u32)((u64)peer_ul_airtime * 100ULL / radio_ul_airtime);
				} else {
					peer_config->actual_ul_duration = 0;
					peer_config->ul_airtime = 0;
				}

				group->actual_airtime += peer_config->actual_airtime;
				group->ul_airtime += peer_config->ul_airtime;
				group->actual_duration += peer_config->actual_duration;
				group->actual_ul_duration += peer_config->actual_ul_duration;

				atf_update_group_ac_airtime(group, airtime_stats);
			}
		}
implicit_peers:
		if (dl_list_empty(&group->implicit_peers))
			continue;

		dl_list_for_each_safe(sta, tmp, &group->implicit_peers,
				      struct sta_info, atf_candidate_list) {
			if (sta->atf_peer.calculated_airtime) {
				implicit_peer = &sta->atf_peer;
				airtime_stats = &implicit_peer->peer_airtime;
				peer_airtime = 0;
				peer_ul_airtime = 0;

				for (ac = 0; ac < WMM_AC_NUM; ac++) {
					peer_airtime +=
						airtime_stats->tx_consumption[ac].consumption;
					peer_ul_airtime +=
						airtime_stats->rx_consumption[ac].consumption;
				}

				if (peer_airtime > 0 && radio_actual_airtime > 0) {
					implicit_peer->actual_duration =
						peer_airtime;
					implicit_peer->actual_airtime =
						(u32)((u64)peer_airtime * 100ULL / radio_actual_airtime);
				} else {
					implicit_peer->actual_duration = 0;
					implicit_peer->actual_airtime = 0;
				}

				if (peer_ul_airtime > 0 && radio_ul_airtime > 0) {
					implicit_peer->actual_ul_duration =
						peer_ul_airtime;
					implicit_peer->ul_airtime =
						(u32)((u64)peer_ul_airtime * 100ULL / radio_ul_airtime);
				} else {
					implicit_peer->actual_ul_duration = 0;
					implicit_peer->ul_airtime = 0;
				}

				group->actual_airtime += implicit_peer->actual_airtime;
				group->ul_airtime += implicit_peer->ul_airtime;
				group->actual_duration += implicit_peer->actual_duration;
				group->actual_ul_duration += implicit_peer->actual_ul_duration;

				atf_update_group_ac_airtime(group, airtime_stats);
			}
		}
	}

	return 0;
}


static void atf_offload_print_ac_stats(struct hostapd_data *hapd)
{
	struct atf_algo *algo;
	struct atf_group *group;
	const char *ac_names[WMM_AC_NUM] = { "BE", "BK", "VI", "VO" };
	u32 radio_actual_airtime, radio_ul_airtime, actual_airtime;
	u32 ul_airtime;
	int ac, borrowed, unused;

	if (!hapd->iface || !hapd->iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return;
	}

	algo = hapd->iface->atf_algo;

	radio_actual_airtime = 0;
	radio_ul_airtime = 0;
	for (ac = 0; ac < WMM_AC_NUM; ac++) {
		radio_actual_airtime +=
			algo->radio_airtime.tx_consumption[ac].consumption;
		radio_ul_airtime +=
			algo->radio_airtime.rx_consumption[ac].consumption;
	}
	if (!radio_actual_airtime)
		radio_actual_airtime = 1;
	if (!radio_ul_airtime)
		radio_ul_airtime = 1;

	wpa_printf(MSG_INFO, "\n\nSHOW ATF AC TABLE\n");
	wpa_printf(MSG_INFO, "%-30s %6s %4s %16s %16s %10s %8s %14s %12s %16s",
		   "Group/SSID", "Group%", "AC", "Alloted%", "Actual%", "Borrowed",
		   "Unused", "Duration(us)", "UL%", "UL Duration(us)");

	dl_list_for_each(group, &algo->groups, struct atf_group, list) {
		wpa_printf(MSG_INFO, "%-30s %u.%u",
			   group->name,
			   group->user_cfg_airtime / 10,
			   group->user_cfg_airtime % 10);

		for (ac = 0; ac < WMM_AC_NUM; ac++) {
			if (!(group->calculated_ac_airtime[ac]))
				continue;

			actual_airtime = (group->ac_actual_airtime[ac] * 100) /
				 radio_actual_airtime;
			ul_airtime = (group->ac_ul_airtime[ac] * 100) /
				     radio_ul_airtime;

			borrowed = 0;
			unused = 0;
			if (actual_airtime > group->calculated_ac_airtime[ac] / 10)
				borrowed = actual_airtime - group->calculated_ac_airtime[ac] / 10;
			else
				unused = group->calculated_ac_airtime[ac] / 10 - actual_airtime;

			wpa_printf(MSG_INFO,
				   "%-30s %6s %-4s %13u.%u %13u.%u %10d %8d %14u %12u %16u",
				   "", "", ac_names[ac], group->calculated_ac_airtime[ac] / 10,
				   group->calculated_ac_airtime[ac] % 10, actual_airtime,
				   ((group->ac_actual_airtime[ac] * 100) % radio_actual_airtime) % 10,
				   borrowed, unused, group->ac_actual_airtime[ac], ul_airtime,
				   group->ac_ul_airtime[ac]);
		}
		wpa_printf(MSG_INFO, "\n");
	}
}


static void atf_offload_print_stats(struct hostapd_data *hapd)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_group *group;
	struct atf_peer_config *peer;
	struct sta_info *sta, *tmp;
	struct atf_peer *implicit_peer;
	int ret, borrowed, unused;
	u8 addr[ETH_ALEN];

	ret = atf_offload_update_peer_airtime(hapd);
	if (ret) {
		wpa_printf(MSG_ERROR, "ATF: failed to update ATF stats\n");
		return;
	}

	algo = iface->atf_algo;

	wpa_printf(MSG_INFO, "******************************* ATF STATS For SSID Groups **************************");
	wpa_printf(MSG_INFO, "GroupName       Configured  Actual  Borrowed  Unused  Duration(us)  ActualUL  UL(us)");

	dl_list_for_each(group, &algo->groups, struct atf_group, list) {
		borrowed = 0;
		unused = 0;

		if (group->actual_airtime > group->user_cfg_airtime / 10)
			borrowed = group->actual_airtime - group->user_cfg_airtime / 10;
		else
			unused = group->user_cfg_airtime / 10 - group->actual_airtime;

		wpa_printf(MSG_INFO, "%-18s %-10d %-7d %-9d %-7d %-13d %-9d %-7d",
			   group->name,
			   group->user_cfg_airtime / 10,
			   group->actual_airtime,
			   borrowed,
			   unused,
			   group->actual_duration,
			   group->ul_airtime,
			   group->actual_ul_duration);
	}

	wpa_printf(MSG_INFO, "******************************************************");
	wpa_printf(MSG_INFO, "**************** ATF STATS For PEERs *************************");
	wpa_printf(MSG_INFO, "PeerMAC             GroupName      Configured  Actual  Borrowed  Unused  Duration(us)  ActualUL  UL(us)");
	dl_list_for_each(group, &algo->groups, struct atf_group, list) {
		if (dl_list_empty(&group->explicit_peers))
			goto impilicit_peers;

		dl_list_for_each_safe(sta, tmp, &group->explicit_peers,
				      struct sta_info, atf_candidate_list) {
			if (sta->atf_peer.peer_cfg_ref) {
				peer = sta->atf_peer.peer_cfg_ref;
				borrowed = 0;
				unused = 0;

				if (peer->actual_airtime > sta->atf_peer.calculated_airtime / 10)
					borrowed = peer->actual_airtime - sta->atf_peer.calculated_airtime / 10;
				else
					unused = peer->calculated_for_airtime / 10 - peer->actual_airtime;

				wpa_printf(MSG_INFO, MACSTR "    %-17s %-10d %-7d %-9d %-7d %-13d %-9d %-7d",
					   MAC2STR(peer->addr),
					   peer->group->name,
					   sta->atf_peer.calculated_airtime / 10,
					   peer->actual_airtime,
					   borrowed,
					   unused,
					   peer->actual_duration,
					   peer->ul_airtime,
					   peer->actual_ul_duration);
			}
		}
impilicit_peers:
		if (dl_list_empty(&group->implicit_peers))
			continue;

		dl_list_for_each_safe(sta, tmp, &group->implicit_peers,
				      struct sta_info, atf_candidate_list) {
			if (sta->atf_peer.calculated_airtime) {
				implicit_peer = &sta->atf_peer;
				borrowed = 0;
				unused = 0;

				if (implicit_peer->actual_airtime > implicit_peer->calculated_airtime / 10)
					borrowed = implicit_peer->actual_airtime -
						   implicit_peer->calculated_airtime / 10;
				else
					unused = implicit_peer->calculated_airtime / 10 -
						 implicit_peer->actual_airtime;

				if (ap_sta_is_mld(sta->atf_peer.bss, sta))
					memcpy(addr, sta->mld_info.links[hapd->mld_link_id].peer_addr, 6);
				else
					memcpy(addr, sta->addr, 6);

				wpa_printf(MSG_INFO, MACSTR "    %-17s %-10d %-7d %-9d %-7d %-13d %-9d %-7d",
					   MAC2STR(addr),
					   implicit_peer->group->name,
					   sta->atf_peer.calculated_airtime / 10,
					   implicit_peer->actual_airtime,
					   borrowed,
					   unused,
					   implicit_peer->actual_duration,
					   implicit_peer->ul_airtime,
					   implicit_peer->actual_ul_duration);
			}
		}

	}
}


/*
 * atf_ac_str_to_id - convert AC name string to AC index
 * Returns 0-3 on success, -1 on invalid name.
 * AC index: 0=BE, 1=BK, 2=VI, 3=VO
 */
static int atf_ac_str_to_id(const char *ac_str)
{
	if (os_strcasecmp(ac_str, "BE") == 0)
		return WMM_AC_BE;
	if (os_strcasecmp(ac_str, "BK") == 0)
		return WMM_AC_BK;
	if (os_strcasecmp(ac_str, "VI") == 0)
		return WMM_AC_VI;
	if (os_strcasecmp(ac_str, "VO") == 0)
		return WMM_AC_VO;
	return -1;
}



static int
hostapd_ctrl_iface_atf_offload_addac(struct hostapd_data *hapd,
				     const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_group *group;
	char *input, *token, *context = NULL;
	char *name, *colon;
	char ac_name[WMM_AC_NUM];
	int ac_id, airtime, ret = 0, i, num_ac = 0;
	/* Staging array: index by AC id, -1 means not provided in this call */
	int new_ac[WMM_AC_NUM] = { -1, -1, -1, -1 };
	u32 total;

	if (!iface || !iface->conf || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!iface->conf->atf_offload) {
		wpa_printf(MSG_ERROR, "ATF: ATF is not enabled\n");
		return -1;
	}

	input = os_strdup(cmd);
	if (!input)
		return -1;

	name = str_token(input, " ", &context);
	if (!name || os_strlen(name) > WLAN_SSID_MAX_LEN) {
		wpa_printf(MSG_ERROR, "ATF: Invalid or missing SSID/group name\n");
		ret = -1;
		goto fail;
	}

	group = atf_find_group(algo, name);
	if (!group) {
		wpa_printf(MSG_ERROR,
			   "ATF: Group/SSID %s not found - configure airtime first\n",
			   name);
		ret = -1;
		goto fail;
	}

	if (!group->is_configured || !group->user_cfg_airtime) {
		wpa_printf(MSG_ERROR,
			   "ATF: Group %s has no airtime configured - run addssid/configatfgroup first\n",
			   group->name);
		ret = -1;
		goto fail;
	}

	/* Remaining tokens: AC:<val> pairs, 1 to 4 */
	while ((token = str_token(input, " ", &context)) != NULL) {
		colon = os_strchr(token, ':');
		if (!colon) {
			wpa_printf(MSG_ERROR,
				   "ATF: Malformed AC token %s, expected AC:<val>\n",
				   token);
			ret = -1;
			goto fail;
		}

		/* Split at : */
		*colon = '\0';
		os_strlcpy(ac_name, token, sizeof(ac_name));
		airtime = atoi(colon + 1);

		ac_id = atf_ac_str_to_id(ac_name);
		if (ac_id < 0) {
			wpa_printf(MSG_ERROR,
				   "ATF: Unknown AC name %s, expected BE/BK/VI/VO\n",
				   ac_name);
			ret = -1;
			goto fail;
		}

		if (airtime < 0 || airtime > 100) {
			wpa_printf(MSG_ERROR,
				   "ATF: AC %s value %d out of range (0-100)\n",
				   ac_name, airtime);
			ret = -1;
			goto fail;
		}

		new_ac[ac_id] = airtime;
		num_ac++;
	}

	if (num_ac == 0) {
		wpa_printf(MSG_ERROR,
			   "ATF: No AC tokens provided, expected AC:<val> pairs\n");
		ret = -1;
		goto fail;
	}

	/*
	 * Validate: sum already-configured ACs (not being overwritten) plus
	 * all new values must not exceed 100% (1000 units).
	 */
	total = 0;
	for (i = 0; i < WMM_AC_NUM; i++) {
		if (new_ac[i] >= 0)
			total += (u32)(new_ac[i] * 10);
		else if (ATF_AC_IS_BIT_SET(group->ac_bitmap, i))
			total += group->ac_airtime[i];
	}
	if (total > ATF_RADIO_DEFAULT_AIRTIME) {
		wpa_printf(MSG_ERROR,
			   "ATF: Total AC airtime for group %s would exceed 100%% (%u > %d)",
			   group->name, total, ATF_RADIO_DEFAULT_AIRTIME);
		ret = -1;
		goto fail;
	}

	/* Commit: all values validated, now write to group */
	for (i = 0; i < WMM_AC_NUM; i++) {
		if (new_ac[i] < 0)
			continue;
		group->ac_airtime[i] = (u32)(new_ac[i] * 10);
		group->ac_bitmap |= (u8)(1 << i);
		wpa_printf(MSG_INFO, "ATF: Group %s AC %d configured to %d%%",
			   group->name, i, new_ac[i]);
	}

fail:
	os_free(input);
	return ret;
}


static int
hostapd_ctrl_iface_atf_offload_delac(struct hostapd_data *hapd,
				     const char *cmd, char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_group *group;
	char *input, *name, *context = NULL;
	int ret = 0, len = 0;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!iface->conf->atf_offload) {
		wpa_printf(MSG_ERROR, "ATF: ATF is not enabled\n");
		return -1;
	}

	input = os_strdup(cmd);
	if (!input)
		return -1;

	name = str_token(input, " ", &context);
	if (!name || os_strlen(name) > WLAN_SSID_MAX_LEN) {
		wpa_printf(MSG_ERROR, "ATF: Invalid or missing SSID/group name\n");
		ret = -1;
		goto fail;
	}

	group = atf_find_group(algo, name);
	if (!group) {
		wpa_printf(MSG_ERROR,
			   "ATF: Group/SSID %s not found\n", name);
		ret = -1;
		goto fail;
	}

	if (!group->is_configured) {
		wpa_printf(MSG_ERROR,
			   "ATF: Group %s has no airtime configured\n",
			   group->name);
		ret = -1;
		goto fail;
	}

	/* Clear all AC config for this group */
	atf_reset_group_ac_config(group);
	wpa_printf(MSG_INFO, "ATF: Group %s all AC config cleared",
		   group->name);

	os_free(input);
	return len;

fail:
	os_free(input);
	return ret;
}


static int
hostapd_ctrl_iface_atf_offload_showatfacstats(struct hostapd_data *hapd,
					      char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	int ret, len = 0;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	if (!iface->conf->atf_offload) {
		wpa_printf(MSG_ERROR, "ATF: ATF is not enabled\n");
		return -1;
	}

	if (!algo->atf_stats_enabled) {
		wpa_printf(MSG_ERROR, "ATF: ATF stats is not enabled\n");
		return -1;
	}

	if (dl_list_empty(&algo->groups)) {
		wpa_printf(MSG_ERROR, "ATF: No groups configured\n");
		return -1;
	}

	if (!hapd->iface->current_hw_info) {
		wpa_printf(MSG_ERROR, "ATF: Failed to get configured radio\n");
		return -1;
	}

	ret = nl80211_atf_offload_showatfstats(hapd->drv_priv,
					       hapd->iface->current_hw_info->hw_idx,
					       hapd);
	if (ret) {
		wpa_printf(MSG_ERROR, "ATF: Failed to dump ATF stats\n");
		return ret;
	}

	ret = atf_offload_update_peer_airtime(hapd);
	if (ret) {
		wpa_printf(MSG_ERROR, "ATF: failed to update ATF stats\n");
		return ret;
	}

	atf_offload_print_ac_stats(hapd);

	ret = os_snprintf(buf, buflen, "Check hostapd logs for atf ac stats\n");
	if (!os_snprintf_error(buflen, ret))
		len = ret;

	return len;
}


int hostapd_ctrl_iface_atf_offload_showatfstats(struct hostapd_data *hapd,
                                               char *buf, size_t buflen)
{
       struct hostapd_iface *iface = hapd->iface;
       struct atf_algo *algo;
       int ret;

       if (!iface || !iface->atf_algo) {
               wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
               return -1;
       }

       algo = iface->atf_algo;

       if (!algo->atf_stats_enabled) {
               wpa_printf(MSG_ERROR, "ATF: ATF stats is not enabled\n");
               return -1;
       }

       ret = nl80211_atf_offload_showatfstats(hapd->drv_priv,
                                              hapd->iface->current_hw_info->hw_idx,
					      hapd);
       if (ret) {
               wpa_printf(MSG_ERROR, "ATF: Failed to dump ATF stats\n");
               return ret;
       }

       atf_offload_print_stats(hapd);
       return 0;
}



int
hostapd_ctrl_iface_config_atf_offload(struct hostapd_data *hapd,
		const char *cmd, char *buf, size_t buflen)
{
	if (os_strncmp(cmd, "commitatf ", 10) == 0)
		return hostapd_ctrl_iface_atf_offload_commitatf(hapd, cmd + 10);
	else if (os_strncmp(cmd, "get_commitatf", 13) == 0)
		return hostapd_ctrl_iface_atf_offload_get_commitatf(hapd, buf, buflen);
	else if (os_strncmp(cmd, "atfstrictsched ", 15) == 0)
		return hostapd_ctrl_iface_atf_offload_atfstrictsched(hapd, cmd + 15, buf, buflen);
	else if (os_strncmp(cmd, "gatfstrictsched", 15) == 0)
		return hostapd_ctrl_iface_atf_offload_gatfstrictsched(hapd, buf, buflen);
	else if (os_strncmp(cmd, "atfssidgroup ", 13) == 0)
		return hostapd_ctrl_iface_atf_offload_atfssidgroup(hapd, cmd + 13, buf, buflen);
	else if (os_strncmp(cmd, "g_atfssidgroup", 14) == 0)
		return hostapd_ctrl_iface_atf_offload_g_atfssidgroup(hapd, buf, buflen);
	else if (os_strncmp(cmd, "addatfgroup ", 12) == 0)
		return hostapd_ctrl_iface_atf_offload_addatfgroup(hapd, cmd + 12, buf, buflen);
	else if (os_strncmp(cmd, "configatfgroup ", 15) == 0)
		return hostapd_ctrl_iface_atf_offload_configatfgroup(hapd, cmd + 15, buf, buflen);
	else if (os_strncmp(cmd, "delatfgroup ", 12) == 0)
		return hostapd_ctrl_iface_atf_offload_delatfgroup(hapd, cmd + 12, buf, buflen);
	else if (os_strncmp(cmd, "showatfgroup", 12) == 0)
		return hostapd_ctrl_iface_atf_offload_showatfgroup(hapd, buf, buflen);
	else if (os_strncmp(cmd, "atfgroupsched ", 14) == 0)
		return hostapd_ctrl_iface_atf_offload_atfgroupsched(hapd, cmd + 14, buf, buflen);
	else if (os_strncmp(cmd, "g_atfgroupsched ", 16) == 0)
		return hostapd_ctrl_iface_atf_offload_g_atfgroupsched(hapd, cmd + 16, buf, buflen);
	else if (os_strncmp(cmd, "atfssidsched ", 13) == 0)
		return hostapd_ctrl_iface_atf_offload_atfssidsched(hapd, cmd + 13, buf, buflen);
	else if (os_strncmp(cmd, "g_atfssidsched", 14) == 0)
		return hostapd_ctrl_iface_atf_offload_g_atfssidsched(hapd, buf, buflen);
	else if (os_strncmp(cmd, "addssid ", 8) == 0)
		return hostapd_ctrl_iface_atf_offload_addssid(hapd, cmd + 8, buf, buflen);
	else if (os_strncmp(cmd, "delssid ", 8) == 0)
		return hostapd_ctrl_iface_atf_offload_delssid(hapd, cmd + 8, buf, buflen);
	else if (os_strncmp(cmd, "addsta ", 7) == 0)
		return hostapd_ctrl_iface_atf_offload_addsta(hapd, cmd + 7, buf, buflen);
	else if (os_strncmp(cmd, "delsta ", 7) == 0)
		return hostapd_ctrl_iface_atf_offload_delsta(hapd, cmd + 7, buf, buflen);
	else if (os_strncmp(cmd, "showatftable", 12) == 0)
		return hostapd_ctrl_iface_atf_offload_showatftable(hapd, buf, buflen);
	else if (os_strncmp(cmd, "showairtime", 11) == 0)
		return hostapd_ctrl_iface_atf_offload_showairtime(hapd, buf, buflen);
	else if (os_strncmp(cmd, "flushatftable", 13) == 0)
		return hostapd_ctrl_iface_atf_offload_flushatftable(hapd, buf, buflen);
	else if (os_strncmp(cmd, "enable_atf_stats ", 17) == 0)
		return hostapd_ctrl_iface_atf_offload_enable_atf_stats(hapd, cmd + 17, buf, buflen);
	else if (os_strncmp(cmd, "g_enable_atf_stats", 18) == 0)
		return hostapd_ctrl_iface_atf_offload_g_enable_atf_stats(hapd, buf, buflen);
	else if (os_strncmp(cmd, "atf_stats_timeout ", 18) == 0)
		return hostapd_ctrl_iface_atf_offload_atf_stats_timeout(hapd, cmd + 18,
									buf, buflen);
	else if (os_strncmp(cmd, "g_atf_stats_timeout", 19) == 0)
		return hostapd_ctrl_iface_atf_offload_g_atf_stats_timeout(hapd, buf, buflen);
	else if (os_strncmp(cmd, "showatfstats", 12) == 0)
		return hostapd_ctrl_iface_atf_offload_showatfstats(hapd, buf, buflen);
	else if (os_strncmp(cmd, "addac ", 6) == 0)
		return hostapd_ctrl_iface_atf_offload_addac(hapd, cmd + 6, buf, buflen);
	else if (os_strncmp(cmd, "delac ", 6) == 0)
		return hostapd_ctrl_iface_atf_offload_delac(hapd, cmd + 6, buf, buflen);
	else if (os_strncmp(cmd, "showatfacstats", 14) == 0)
		return hostapd_ctrl_iface_atf_offload_showatfacstats(hapd, buf, buflen);

	return -1;
}
