/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef UDBG_ENH_H
#define UDBG_ENH_H

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

struct nl_msg;
struct hapd_interfaces;

struct udbg_hapd_handler {
	bool defer_connect_until_post_daemonize;
	bool core_service_timer_scheduled;
	uint32_t service_delay_ms;
};

/* Mirrors a formatted wpa_printf payload into debug client transport. */
void hostapd_udbg_enh_wpa_printf_extn(int level, const char *fmt,
					   va_list ap);
/* Mirrors a hexdump payload into debug client transport as text. */
void hostapd_udbg_enh_wpa_hexdump_extn(int level, const char *title,
					    const void *buf, size_t len);

/* Forwards netlink dump payload into the debug client pipeline. */
void hostapd_udbg_enh_nlmsg_dump_extn(struct nl_msg *msg, int is_tx);

/* UDBG lifecycle APIs used by hostapd main. */
void hostapd_udbg_enh_init_extn(
		struct hapd_interfaces *interfaces,
		int daemonize);
void hostapd_udbg_enh_post_daemonize_extn(void);
void hostapd_udbg_enh_deinit_extn(void);

#endif /* UDBG_ENH_H */
