/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>
#include <linux/netlink.h>
#include <netlink/attr.h>
#include <netlink/genl/genl.h>
#include <netlink/msg.h>

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/wpa_debug.h"

#include "ap/hostapd.h"
#include "drivers/nl80211_copy.h"

#include "cmn.h"
#include "core.h"
#include "udbg_enh.h"

extern void (*hostapd_udbg_enh_wpa_printf_hook)(int level, const char *fmt,
						va_list ap);
extern void (*hostapd_udbg_enh_wpa_hexdump_hook)(int level,
						  const char *title,
						  const void *buf,
						  size_t len);

static struct udbg_hapd_handler *udbg_hapd_extn_handler;
static int udbg_hapd_log_hook_busy;

static void udbg_enh_core_service_timer_cb(void *eloop_ctx, void *timeout_ctx);

static void udbg_enh_permanent_stop(void);

/*
 * udbg_enh_reset_core_callbacks: Clears debug-client core callback hooks.
 *
 * @output: No return value; resets callbacks used by the core service layer.
 */
static void udbg_enh_reset_core_callbacks(void)
{
	udbg_client_core_schedule_service_fn = NULL;
	udbg_client_core_permanent_stop_fn = NULL;
}

/*
 * udbg_enh_cancel_core_service_timer: Cancels scheduled core service timer.
 * @handler: Handler whose timer state should be cleared.
 *
 * @output: No return value; unregisters timeout and resets timer flags.
 */
static void udbg_enh_cancel_core_service_timer(
			struct udbg_hapd_handler *handler)
{
	if (!handler || !handler->core_service_timer_scheduled)
		return;

	eloop_cancel_timeout(udbg_enh_core_service_timer_cb, handler, NULL);
	handler->core_service_timer_scheduled = false;
}

/*
 * udbg_enh_schedule_core_service_timer: Schedules delayed core service.
 *
 * @output: No return value; updates timer state when registration succeeds.
 */
static void udbg_enh_schedule_core_service_timer(void)
{
	struct udbg_hapd_handler *handler = udbg_hapd_extn_handler;
	unsigned int sec;
	unsigned int usec;
	uint32_t ms;

	if (!handler)
		return;

	if (handler->core_service_timer_scheduled)
		return;

	ms = handler->service_delay_ms;

	sec = ms / 1000;
	usec = (ms % 1000) * 1000;
	if (eloop_register_timeout(sec, usec, udbg_enh_core_service_timer_cb,
				   handler, NULL) < 0)
		return;

	handler->core_service_timer_scheduled = true;
}

/*
 * udbg_enh_teardown_connection_state: Tears down connection-related state.
 * @handler: Handler whose connection state should be closed/reset.
 * @reason: Optional reason string used for close diagnostic logs.
 *
 * @output: No return value; cancels timer and disconnects core socket transport.
 */
static void udbg_enh_teardown_connection_state(
	struct udbg_hapd_handler *handler, const char *reason)
{
	if (!handler)
		return;

	udbg_enh_cancel_core_service_timer(handler);
	udbg_client_core_disconnect();

	wpa_printf(MSG_INFO, "[udbg_enh] socket closed (%s)",
		   reason ? reason : "unspecified");
}

/*
 * udbg_enh_permanent_stop: Permanently stops debug-client forwarding.
 *
 * @output: No return value; tears down handler state, hooks, and callbacks.
 */
static void udbg_enh_permanent_stop(void)
{
	struct udbg_hapd_handler *handler = udbg_hapd_extn_handler;

	if (handler) {
		udbg_enh_teardown_connection_state(handler, "permanent stop");
	}

	hostapd_udbg_enh_wpa_printf_hook = NULL;
	hostapd_udbg_enh_wpa_hexdump_hook = NULL;
	udbg_hapd_extn_handler = NULL;
	udbg_enh_reset_core_callbacks();
	if (handler)
		os_free(handler);
}

/*
 * udbg_enh_core_service_timer_cb: Handles core-requested service wakeups.
 * @eloop_ctx: Handler pointer registered as timeout callback context.
 * @timeout_ctx: Unused timeout context from eloop API.
 *
 * @output: No return value; notifies core service delay expiry and drives steps.
 */
static void udbg_enh_core_service_timer_cb(void *eloop_ctx, void *timeout_ctx)
{
	struct udbg_hapd_handler *handler = eloop_ctx;

	if (!handler || handler != udbg_hapd_extn_handler)
		return;

	handler->core_service_timer_scheduled = false;

	udbg_client_core_service(true);
}

/*
 * udbg_enh_skip_broadcast_cmd_frame: Detects broadcast nl80211 frame commands.
 * @nlh: Netlink message header to inspect.
 *
 * @output: Returns true when frame should be skipped, otherwise false.
 */
static bool udbg_enh_skip_broadcast_cmd_frame(
	const struct nlmsghdr *nlh)
{
	const struct genlmsghdr *gnlh;
	struct nlattr *tb[NL80211_ATTR_MAX + 1] = { 0 };
	const u8 *frame;
	int frame_len;
	static const u8 broadcast_addr[6] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};

	if (!nlh || nlh->nlmsg_len < NLMSG_HDRLEN + sizeof(*gnlh))
		return true;

	gnlh = (const struct genlmsghdr *) ((const u8 *) nlh + NLMSG_HDRLEN);
	if (nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		      genlmsg_attrlen(gnlh, 0), NULL) < 0)
		return true;
	if (!tb[NL80211_ATTR_FRAME])
		return true;

	frame = nla_data(tb[NL80211_ATTR_FRAME]);
	frame_len = nla_len(tb[NL80211_ATTR_FRAME]);
	if (frame_len < (int) sizeof(broadcast_addr) + 4)
		return true;

	return os_memcmp(frame + 4, broadcast_addr, sizeof(broadcast_addr)) == 0;
}

/*
 * udbg_enh_filter_should_drop_nlmsg: Applies debug-client netlink filter policy.
 * @nlh: Netlink message header to evaluate.
 * @is_tx: Non-zero when message is transmit path, zero for receive path.
 *
 * @output: Returns true when message should be dropped, otherwise false.
 */
static bool udbg_enh_filter_should_drop_nlmsg(const struct nlmsghdr *nlh,
	int is_tx)
{
	const struct genlmsghdr *gnlh;

	if (!nlh || nlh->nlmsg_len < sizeof(*nlh))
		return true;

	/* Preserve generic netlink control-family discovery traffic. */
	if (nlh->nlmsg_type == NLMSG_MIN_TYPE)
		return false;

	if (nlh->nlmsg_type == NLMSG_ERROR || nlh->nlmsg_type == NLMSG_DONE)
		return true;

	if (nlh->nlmsg_type < NLMSG_MIN_TYPE ||
	    nlh->nlmsg_len < NLMSG_HDRLEN + sizeof(*gnlh))
		return false;

	gnlh = (const struct genlmsghdr *) ((const u8 *) nlh + NLMSG_HDRLEN);

	if (!is_tx && gnlh->cmd == NL80211_CMD_FRAME)
		return true;

	if ((gnlh->cmd == NL80211_CMD_FRAME ||
	     gnlh->cmd == NL80211_CMD_FRAME_TX_STATUS) &&
	    udbg_enh_skip_broadcast_cmd_frame(nlh))
		return true;

	if (gnlh->cmd == NL80211_CMD_TRIGGER_SCAN ||
	    gnlh->cmd == NL80211_CMD_GET_SCAN ||
	    gnlh->cmd == NL80211_CMD_NEW_SCAN_RESULTS ||
	    gnlh->cmd == NL80211_CMD_GET_WIPHY ||
	    gnlh->cmd == NL80211_CMD_SET_WIPHY ||
	    gnlh->cmd == NL80211_CMD_NEW_WIPHY ||
	    gnlh->cmd == NL80211_CMD_GET_SURVEY ||
	    gnlh->cmd == NL80211_CMD_NEW_SURVEY_RESULTS ||
	    gnlh->cmd == NL80211_CMD_SET_BEACON ||
	    gnlh->cmd == NL80211_CMD_SET_BSS ||
	    gnlh->cmd == NL80211_CMD_SET_MULTICAST_TO_UNICAST ||
		    gnlh->cmd == NL80211_CMD_OBSS_COLOR_COLLISION)
		return true;

	return false;
}

/*
 * udbg_enh_send_log: Filters and submits a
 * data payload with explicit debug message type.
 * @msg_type: Wire message type for this payload.
 * @payload: Payload bytes to send.
 * @len: Number of bytes in payload.
 *
 * @output: No return value; validates, queues, and advances transport state.
 */
static void udbg_enh_send_log(uint32_t msg_type, const void *payload,
	size_t len)
{
	struct udbg_hapd_handler *handler = udbg_hapd_extn_handler;

	if (!handler || !payload || !len) {
		errno = EINVAL;
		return;
	}

	udbg_client_core_send_log(msg_type, payload, len);

	if (!handler->defer_connect_until_post_daemonize)
		udbg_client_core_service(false);
}

/*
 * hostapd_udbg_enh_wpa_printf_extn: Mirrors wpa_printf output into debug stream.
 * @level: WPA log level from original printf call.
 * @fmt: Printf-style format string for the log message.
 * @ap: Variable argument list for fmt.
 *
 * @output: No return value; formats and sends text to debug client.
 */
void hostapd_udbg_enh_wpa_printf_extn(int level, const char *fmt,
					   va_list ap)
{
	struct udbg_hapd_handler *handler;
	va_list ap_copy;
	char stack_buf[1024];
	char *msg = stack_buf;
	size_t msg_size = sizeof(stack_buf);
	size_t msg_len;
	int n;

	handler = udbg_hapd_extn_handler;
	if (!handler || !fmt || udbg_hapd_log_hook_busy)
		return;
	if (level < wpa_debug_level)
		return;

	va_copy(ap_copy, ap);
	n = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap_copy);
	va_end(ap_copy);
	if (n < 0)
		return;

	if ((size_t) n >= sizeof(stack_buf)) {
		msg_size = (size_t) n + 1;
		msg = os_malloc(msg_size);
		if (!msg)
			return;
		va_copy(ap_copy, ap);
		vsnprintf(msg, msg_size, fmt, ap_copy);
		va_end(ap_copy);
	}

	msg_len = msg ? os_strlen(msg) : 0;
	if (msg_len) {
		udbg_hapd_log_hook_busy = 1;
		udbg_enh_send_log(UDBG_LOG, msg, msg_len);
		udbg_hapd_log_hook_busy = 0;
	}

	if (msg != stack_buf)
		os_free(msg);
}

/*
 * hostapd_udbg_enh_wpa_hexdump_extn: Converts hexdump logs to text and forwards.
 * @level: WPA log level from original hexdump call.
 * @title: Optional label/title for the hexdump.
 * @buf: Byte buffer to format as hexadecimal text.
 * @len: Number of bytes available in buf.
 *
 * @output: No return value; converts and sends encoded text to debug client.
 */
void hostapd_udbg_enh_wpa_hexdump_extn(int level, const char *title,
					    const void *buf, size_t len)
{
	static const char hex[] = "0123456789abcdef";
	struct udbg_hapd_handler *handler;
	const char *dump_title = title ? title : "";
	const u8 *data = buf;
	const char *suffix = "";
	size_t suffix_len = 0;
	size_t prefix_len;
	size_t total_len;
	size_t i;
	size_t msg_len;
	int n;
	char *msg;
	char *pos;

	handler = udbg_hapd_extn_handler;
	if (!handler || udbg_hapd_log_hook_busy)
		return;
	if (level < wpa_debug_level)
		return;

	n = snprintf(NULL, 0, "%s - hexdump(len=%lu):", dump_title,
		     (unsigned long) len);
	if (n < 0)
		return;
	prefix_len = (size_t) n;

	if (!data)
		suffix = " [NULL]";
	suffix_len = os_strlen(suffix);

	if (len > (SIZE_MAX - prefix_len - suffix_len - 1) / 3)
		return;

	total_len = prefix_len + suffix_len + (3 * len);
	msg = os_malloc(total_len + 1);
	if (!msg)
		return;

	n = os_snprintf(msg, total_len + 1, "%s - hexdump(len=%lu):",
			dump_title, (unsigned long) len);
	if (n < 0 || (size_t) n > total_len) {
		os_free(msg);
		return;
	}

	pos = msg + n;
	if (suffix_len) {
		os_memcpy(pos, suffix, suffix_len);
		pos += suffix_len;
	} else {
		for (i = 0; i < len; i++) {
			u8 v = data[i];
			*pos++ = ' ';
			*pos++ = hex[(v >> 4) & 0x0f];
			*pos++ = hex[v & 0x0f];
		}
	}
	*pos = '\0';

	msg_len = os_strlen(msg);
	if (msg_len) {
		udbg_hapd_log_hook_busy = 1;
		udbg_enh_send_log(UDBG_LOG, msg, msg_len);
		udbg_hapd_log_hook_busy = 0;
	}

	os_free(msg);
}

/*
 * hostapd_udbg_enh_nlmsg_dump_extn: Forwards selected netlink messages as debug logs.
 * @msg: Netlink message to inspect/forward.
 * @is_tx: Non-zero for TX direction, zero for RX direction.
 *
 * @output: No return value; sends netlink payload when filtering and state allow it.
 */
void hostapd_udbg_enh_nlmsg_dump_extn(struct nl_msg *msg, int is_tx)
{
	struct udbg_hapd_handler *handler = udbg_hapd_extn_handler;
	const struct nlmsghdr *nlh;

	if (!msg)
		return;
	if (!handler)
		return;

	nlh = nlmsg_hdr(msg);
	if (!nlh || nlh->nlmsg_len < sizeof(*nlh))
		return;
	if (udbg_enh_filter_should_drop_nlmsg(nlh, is_tx))
		return;

	udbg_enh_send_log(is_tx ? UDBG_NL_TX : UDBG_NL_RX,
			  nlh, nlh->nlmsg_len);
}

/*
 * hostapd_udbg_enh_post_daemonize_extn: Starts deferred debug-client connect flow.
 *
 * @output: No return value; clears defer flag and starts core-drive flow.
 */
void hostapd_udbg_enh_post_daemonize_extn(void)
{
	struct udbg_hapd_handler *handler = udbg_hapd_extn_handler;

	if (!handler ||
	    !handler->defer_connect_until_post_daemonize)
		return;

	handler->defer_connect_until_post_daemonize = false;
	udbg_client_core_service(false);
}

/*
 * hostapd_udbg_enh_init_extn: Initializes debug-client core/handler from config.
 * @interfaces: Hostapd interfaces object providing extension configuration.
 * @daemonize: Non-zero when startup is daemonized and connect should be deferred.
 *
 * @output: No return value; initializes when possible, otherwise logs and returns.
 */
void hostapd_udbg_enh_init_extn(
	struct hapd_interfaces *interfaces,
	int daemonize)
{
	struct udbg_hapd_handler *handler = NULL;
	const struct hostapd_config_extn *cfg;

	if (!interfaces)
		return;

	if (udbg_hapd_extn_handler)
		return;

	if (!interfaces->iface || !interfaces->count ||
		    !interfaces->iface[0] || !interfaces->iface[0]->conf)
		return;

	cfg = &interfaces->iface[0]->conf->conf_extn;

	if (!cfg->udbg_enh_enable)
		return;

	udbg_client_core_schedule_service_fn = udbg_enh_schedule_core_service_timer;
	udbg_client_core_permanent_stop_fn = udbg_enh_permanent_stop;

	if (udbg_client_core_init(
		    cfg->udbg_enh_server_ip,
		    cfg->udbg_enh_server_port,
		    cfg->udbg_enh_app_id,
		    cfg->udbg_enh_records,
		    cfg->udbg_enh_ring_max_bytes) < 0) {
		wpa_printf(MSG_ERROR,
			   "[udbg_enh] failed to initialize from hostapd.conf; debug client disabled");
		udbg_enh_reset_core_callbacks();
		return;
	}

	handler = os_zalloc(sizeof(*handler));
	if (!handler) {
		errno = ENOMEM;
		wpa_printf(MSG_INFO,
			   "[udbg_enh] failed to initialize debug client handler");
		udbg_client_core_deinit();
		udbg_enh_reset_core_callbacks();
		return;
	}

	/*
	 * Start in deferred mode to avoid connection/service work until caller
	 * explicitly starts the handler (or post-daemonize path starts it).
	 */
	handler->defer_connect_until_post_daemonize = true;
	handler->core_service_timer_scheduled = false;
	handler->service_delay_ms = cfg->udbg_enh_service_delay_ms;

	udbg_hapd_extn_handler = handler;
	hostapd_udbg_enh_wpa_printf_hook =
		hostapd_udbg_enh_wpa_printf_extn;
	hostapd_udbg_enh_wpa_hexdump_hook =
		hostapd_udbg_enh_wpa_hexdump_extn;
	if (!daemonize) {
		handler->defer_connect_until_post_daemonize = false;
		udbg_client_core_service(false);
	}
}

/*
 * hostapd_udbg_enh_deinit_extn: Releases debug-client hooks, transport, and handler.
 *
 * @output: No return value; clears globals and frees core resources when present.
 */
void hostapd_udbg_enh_deinit_extn(void)
{
	struct udbg_hapd_handler *handler = udbg_hapd_extn_handler;

	if (!udbg_hapd_extn_handler)
		return;

	hostapd_udbg_enh_wpa_printf_hook = NULL;
	hostapd_udbg_enh_wpa_hexdump_hook = NULL;

	/*
	 * Clear global forwarding pointer before tearing down transport/core
	 * to avoid stale pointer use from late debug hooks during shutdown.
	 */
	udbg_hapd_extn_handler = NULL;
	udbg_enh_teardown_connection_state(handler, "deinit");
	udbg_client_core_deinit();
	udbg_enh_reset_core_callbacks();
	os_free(handler);
}
