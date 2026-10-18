// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef HOSTAPD_LOG_EXTN_H
#define HOSTAPD_LOG_EXTN_H

struct hostapd_data;

/* Socket path for direct datagrams to host_sshandler. */
#define HAPD_SSH_SOCK_PATH  "/var/run/host_sshandler.sock"

/* Ctrl_iface event prefix sent via wpa_msg. */
#define HOSTAPD_LOG_TRIGGER "HOSTAPD-LOG-TRIGGER "

/* Initialise/deinitialise per-BSS log extension state (trigger dedup list). */
void hostapd_log_extn_init(struct hostapd_data *hapd);
void hostapd_log_extn_deinit(struct hostapd_data *hapd);

/* Emit a deduplicated trigger log event for the given STA address. */
void hostapd_log_trigger_emit(struct hostapd_data *hapd, const u8 *addr,
			      const char *event_type);

/* Clear the dedup entry for addr, or all entries if addr is NULL. */
void hostapd_log_trigger_clear(struct hostapd_data *hapd, const u8 *addr);

#endif /* HOSTAPD_LOG_EXTN_H */
