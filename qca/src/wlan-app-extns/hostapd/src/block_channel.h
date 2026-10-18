// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef BLOCK_CHANNEL_H
#define BLOCK_CHANNEL_H

#include <stddef.h>

struct hostapd_iface;
struct hostapd_config_extn;

int hostapd_config_parse_block_chanlist(struct hostapd_config_extn *ce,
					    const char *cmd);
int hostapd_set_block_chanlist(struct hostapd_iface *iface, const char *cmd);
int hostapd_clear_block_chanlist(struct hostapd_iface *iface, const char *cmd);
int hostapd_get_block_chanlist(struct hostapd_iface *iface,
			       char *reply, size_t reply_size);

#endif /* BLOCK_CHANNEL_H */
