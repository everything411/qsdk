// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "ap/hostapd.h"
#include "ap/ap_config.h"
#include "cmn.h"

int hostapd_config_parse_block_chanlist(struct hostapd_config_extn *ce,
					    const char *cmd)
{
	char *tmp, *token, *context = NULL, *end = NULL;
	long chan;
	int i;

	if (!ce || !cmd) {
		wpa_printf(MSG_ERROR, "Invalid input for block channel list config");
		return -1;
	}

	while (*cmd == ' ')
		cmd++;

	if (*cmd == '\0') {
		wpa_printf(MSG_ERROR,
			   "acs_block_chan_list: no channels specified");
		return -1;
	}

	tmp = os_strdup(cmd);
	if (!tmp)
		return -1;

	while ((token = str_token(tmp, " ", &context))) {
		chan = strtol(token, &end, 10);
		if (*token == '\0' || (end && *end != '\0') || chan < 1 || chan > 255) {
			wpa_printf(MSG_ERROR, "acs_block_chan_list: invalid channel: '%s'",
				   token);
			os_free(tmp);
			return -1;
		}

		for (i = 0; i < ce->block_chan_list.n_chan; i++) {
			if (ce->block_chan_list.chans[i] == (u8) chan)
				break;
		}
		if (i != ce->block_chan_list.n_chan)
			continue;

		if (ce->block_chan_list.n_chan >= EXTN_MAX_BLOCK_CHAN_LIST) {
			wpa_printf(MSG_ERROR,
				   "acs_block_chan_list: exceeded max size %d",
				   EXTN_MAX_BLOCK_CHAN_LIST);
			os_free(tmp);
			return -1;
		}

		ce->block_chan_list.chans[ce->block_chan_list.n_chan++] = (u8) chan;
	}

	os_free(tmp);
	wpa_printf(MSG_DEBUG, "acs_block_chan_list: configured %u blocked channels",
		   ce->block_chan_list.n_chan);

	return 0;
}

int hostapd_set_block_chanlist(struct hostapd_iface *iface, const char *cmd)
{
	char *tmp, *token, *context = NULL, *end = NULL;
	long chan;
	int i;
	struct hostapd_config_extn *ce;

	if (!iface || !iface->conf || !cmd) {
		wpa_printf(MSG_ERROR, "Invalid input for block channel list");
		return -1;
	}

	ce = &iface->conf->conf_extn;

	while (*cmd == ' ')
		cmd++;

	if (*cmd == '\0') {
		wpa_printf(MSG_ERROR,
			   "No channels specified. Use clear_block_chan_list to clear.");
		return -1;
	}

	tmp = os_strdup(cmd);
	if (!tmp)
		return -1;

	while ((token = str_token(tmp, " ", &context))) {
		chan = strtol(token, &end, 10);
		if (*token == '\0' || (end && *end != '\0') || chan < 1 || chan > 255) {
			wpa_printf(MSG_ERROR, "Invalid blocked channel: '%s'", token);
			os_free(tmp);
			return -1;
		}

		for (i = 0; i < ce->block_chan_list.n_chan; i++) {
			if (ce->block_chan_list.chans[i] == (u8) chan)
				break;
		}
		if (i != ce->block_chan_list.n_chan)
			continue;

		if (ce->block_chan_list.n_chan >= EXTN_MAX_BLOCK_CHAN_LIST) {
			wpa_printf(MSG_ERROR,
				   "Blocked channel list exceeded max size %d",
				   EXTN_MAX_BLOCK_CHAN_LIST);
			os_free(tmp);
			return -1;
		}

		ce->block_chan_list.chans[ce->block_chan_list.n_chan++] = (u8) chan;
	}

	os_free(tmp);
	wpa_printf(MSG_DEBUG, "Blocked channel list updated, count=%u",
		   ce->block_chan_list.n_chan);

	return 0;
}

int hostapd_get_block_chanlist(struct hostapd_iface *iface,
			       char *reply, size_t reply_size)
{
	int i, ret;
	char *pos = reply;
	char *end = reply + reply_size;
	struct hostapd_config_extn *ce;

	if (!iface || !iface->conf || !reply || !reply_size)
		return -1;

	ce = &iface->conf->conf_extn;

	if (ce->block_chan_list.n_chan == 0)
		return os_snprintf(reply, reply_size,
				   "Blocked channels: <empty>\n");

	ret = os_snprintf(pos, end - pos, "Blocked channels (%u):",
			  ce->block_chan_list.n_chan);
	if (os_snprintf_error(end - pos, ret))
		return -1;
	pos += ret;

	for (i = 0; i < ce->block_chan_list.n_chan; i++) {
		ret = os_snprintf(pos, end - pos, " %u",
				  ce->block_chan_list.chans[i]);
		if (os_snprintf_error(end - pos, ret))
			return -1;
		pos += ret;
	}

	ret = os_snprintf(pos, end - pos, "\n");
	if (os_snprintf_error(end - pos, ret))
		return -1;
	pos += ret;

	return (int) (pos - reply);
}

int hostapd_clear_block_chanlist(struct hostapd_iface *iface, const char *cmd)
{
	char *tmp, *token, *ctx = NULL, *end = NULL;
	long chan;
	u8 remove[256] = { 0 };
	int i;
	struct hostapd_config_extn *ce;

	if (!iface || !iface->conf || !cmd)
		return -1;

	ce = &iface->conf->conf_extn;

	while (*cmd == ' ')
		cmd++;

	/* empty => clear all */
	if (*cmd == '\0') {
		ce->block_chan_list.n_chan = 0;
		return 0;
	}

	tmp = os_strdup(cmd);
	if (!tmp)
		return -1;

	while ((token = str_token(tmp, " ", &ctx))) {
		chan = strtol(token, &end, 10);
		if (*token == '\0' || (end && *end != '\0') || chan < 1 || chan > 255) {
			os_free(tmp);
			return -1;
		}
		remove[(u8) chan] = 1;
	}
	os_free(tmp);

	i = 0;
	while (i < ce->block_chan_list.n_chan) {
		u8 c = ce->block_chan_list.chans[i];

		if (remove[c]) {
			ce->block_chan_list.chans[i] =
				ce->block_chan_list.chans[ce->block_chan_list.n_chan - 1];
			ce->block_chan_list.n_chan--;
			continue;
		}
		i++;
	}

	return 0;
}
