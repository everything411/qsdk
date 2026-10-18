/*
 * netfn_tcpst_tx.c
 *	Netfn TCP Speedtest Transmit file
 *
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/types.h>
#include <linux/string.h>
#include <net/sock.h>

#include "netfn_tcpst.h"
#include "netfn_tcpst_priv.h"

/*
 * netfn_tcpst_tx_timeout()
 *	Handles TCP test timeout and offset.
 */
static enum hrtimer_restart netfn_tcpst_tx_timeout(struct hrtimer *timer)
{
	struct netfn_tcpst_conn *conn = container_of(timer, struct netfn_tcpst_conn, hrtimer);
	struct netfn_tcpst_stats_ext *ext_stats = &conn->ext_stats;
	struct netfn_tcpst_stats *stats = &conn->stats;

	/*
	 * We take the bytes sample at 2 instances:
	 * 1. When the offset timer expired
	 * 2. When the duration timer expired
	 */
	netfn_tcpst_update_eth_stats(conn->eth_dev, stats);
	stats->test_bytes.sent = (ext_stats->test_tx_bytes - stats->test_bytes.sent);

	pr_debug("%px: Timer expired (offset(%d) duration (%d) state(0x%lx)\n", conn, conn->offset, conn->duration, conn->timer_state);

	/*
	 * Check if offset timer expired
	 */
	if (test_and_clear_bit(NETFN_TCPST_TIMER_FWD_BIT, &conn->timer_state)) {
		stats->test.eom_time = ktime_get_real_ns();
		set_bit(NETFN_TCPST_TIMER_COMP_BIT, &conn->timer_state);
		return HRTIMER_NORESTART;
	}

	/*
	 * Set the offset timer bit and forward the timer for duration time
	 */
	set_bit(NETFN_TCPST_TIMER_FWD_BIT, &conn->timer_state);
	hrtimer_forward_now(timer, ktime_set(conn->duration, 0));
	return HRTIMER_RESTART;
}

/*
 * netfn_tcpst_http_put_resp()
 *	Parse and verify the response
 */
static bool netfn_tcpst_http_put_resp(struct netfn_tcpst_conn *conn)
{
	struct msghdr msg = {0};
	char *buf = conn->buf;
	int len, valid = 0;
	struct kvec iov;

	/*
	 * Prepare receive message
	 */
	msg.msg_flags |= MSG_WAITALL;
	iov.iov_base = buf;
	iov.iov_len = NETFN_TCPST_HTTP_RESPONSE_LEN;

	/*
	 * Wait for the HTTP response
	 */
	len = kernel_recvmsg(READ_ONCE(conn->sock), &msg, &iov, 1, NETFN_TCPST_HTTP_RESPONSE_LEN, 0);
	if (len < 0) {
		pr_warn("%px: Couldn't receive the response on time\n", conn);
		return false;
	}

	pr_debug("%px: Response received(%s)\n", conn, buf);

	/*
	 * We only expect following responses:
	 * 1. "201 Created" : If target resource creates a new representation successfully
	 * 2. "200 OK" : If target resource updates the existing representation successfully
	 * 3. "204 No content" : If target resource is udpated and returning no content in body
	 *
	 * All other responses are considered as failure
	 */
	valid += !!(strnstr(buf, "201 Created", strlen(buf)));
	valid += !!(strnstr(buf, "200 OK", strlen(buf)));
	valid += !!(strnstr(buf, "204 No Content", strlen(buf)));
	if (!valid) {
		pr_warn("%px: Invalid response\n", conn);
		return false;
	}

	return true;
}

/*
 * netfn_tcpst_http_put()
 *	HTTP PUT support
 */
static bool netfn_tcpst_http_put(struct netfn_tcpst_conn *conn)
{
	struct netfn_tcpst_stats *stats = &conn->stats;
	struct netfn_tcpst_http *http = &conn->http;
	struct msghdr msg = {0};
	struct kvec iov = {0};
	int len;

	/*
	 * Prepare the HTTP PUT
	 */
	msg.msg_flags |= MSG_MORE;
	iov.iov_base = http->hdr;
	iov.iov_len = http->hdr_len;

	/*
	 * Send a PUT command to request the sending of a file and
	 * set ROMTime to the current time value
	 */
	stats->test.rom_time = ktime_get_real_ns();

	/*
	 * Send HTTP PUT request to server
	 */
	len = kernel_sendmsg(READ_ONCE(conn->sock), &msg, &iov, 1, http->hdr_len);
	if (len < 0) {
		pr_warn("%px: Error in kernel_sendmsg\n", conn);
		return false;
	}

	/*
	 * Record the bom_Time
	 */
	stats->test.bom_time = ktime_get_real_ns();
	return true;
}

/*
 * netfn_tcpst_tx_data()
 *	TCP transmit data
 */
void netfn_tcpst_tx_data(struct work_struct *work)
{
	struct netfn_tcpst_conn *conn = container_of(work, struct netfn_tcpst_conn, work);
	struct netfn_tcpst_stats_ext *ext_stats = &conn->ext_stats;
	struct netfn_tcpst_stats *stats = &conn->stats;
	struct netfn_tcpst_ctx *ctx = conn->ctx;
	struct netfn_tcpst *tcpst = &g_tcpst;
	bool resched, eom, timeout, abort;
	enum netfn_tcpst_state state;
	struct msghdr msg = {0};
	struct kvec iov = {0};
	uint64_t tx_bytes;
	int len;

	/*
	 * Prepare the TCP tx msg
	 */
	msg.msg_flags |= MSG_MORE;
	iov.iov_base = conn->buf;
	iov.iov_len = conn->buf_len;

	/*
	 * We send minimum of conn->test_bytes or conn->buf_len
	 */
	tx_bytes = (conn->test_bytes < conn->buf_len) ? conn->test_bytes : conn->buf_len;

	/*
	 * Send dummy buffer
	 */
	len = kernel_sendmsg(READ_ONCE(conn->sock), &msg, &iov, 1, tx_bytes);
	if (len < 0) {
		pr_warn("%px: Error in kernel_sendmsg error(%d) aborting on cpu(%d)\n", tcpst, len, smp_processor_id());
		netfn_tcpst_state_set(ctx, NETFN_TCPST_STATE_ABORTED, netfn_tcpst_state_get(ctx));
		atomic_inc(&ctx->comp_conn);
		return;
	}

	/*
	 * Increment the pkts and bytes counter
	 */
	ext_stats->test_tx_bytes += len;
	ext_stats->test_tx_pkts += !!len;
	conn->test_bytes -= len;

	/*
	 * Reschedule work if we haven't sent all data or no timeout received
	 */
	eom = !conn->test_bytes;
	timeout = test_bit(NETFN_TCPST_TIMER_COMP_BIT, &conn->timer_state);
	abort = (netfn_tcpst_state_get(ctx) == NETFN_TCPST_STATE_ABORTED);
	resched = (!timeout && !eom && !abort);
	if (likely(resched) && likely(schedule_work_on(smp_processor_id(), work))) {
		return;
	}

	pr_debug("%px: Not scheduling eom(%d) timeout(%d) abort(%d)\n", conn, eom, timeout, abort);
	state = netfn_tcpst_state_get(ctx);

	/*
	 * Mark state as completed only if all connections are done
	 */
	if (atomic_inc_return(&ctx->comp_conn) == ctx->num_conns)
		state = NETFN_TCPST_STATE_COMPLETED;

	/*
	 * Verify the received response for HTTP connection.
	 */
	if (netfn_tcpst_is_http(ctx) && eom && !netfn_tcpst_http_put_resp(conn)) {
		pr_debug("%px: Invalid response received\n", conn);
		netfn_tcpst_state_set(ctx, NETFN_TCPST_STATE_ABORTED, netfn_tcpst_state_get(ctx));
		return;
	}

	/*
	 * Check if duration timer has expired. For timebound test
	 * the stats are captured in timer callback.
	 */
	if (timeout) {
		pr_debug("%px: Timeout occured, marking test completed\n", conn);
		netfn_tcpst_state_set(ctx, state, NETFN_TCPST_STATE_INPROGRESS);
		return;
	}

	/*
	 * If EOM is received and its timebound test then we abort.
	 */
	if (eom && conn->duration) {
		netfn_tcpst_state_set(ctx, NETFN_TCPST_STATE_ABORTED, netfn_tcpst_state_get(ctx));
		pr_debug("%px: EOM received before timeout, marking test aborted\n", conn);
		return;
	}

	/*
	 * Abort the test if EOM is not received in file based test
	 */
	if (!eom) {
		netfn_tcpst_state_set(ctx, NETFN_TCPST_STATE_ABORTED, netfn_tcpst_state_get(ctx));
		pr_debug("%px: EOM is not received, marking test aborted\n", conn);
		return;
	}

	stats->test.eom_time = ktime_get_real_ns();
	stats->test_bytes.sent = ext_stats->test_tx_bytes;
	netfn_tcpst_update_eth_stats(conn->eth_dev, stats);
	netfn_tcpst_state_set(ctx, state, NETFN_TCPST_STATE_INPROGRESS);

	pr_debug("%px: Marking test completed\n", conn);

	return;
}

/*
 * netfn_tcpst_tx_work()
 *	TCP upload support
 */
void netfn_tcpst_tx_work(struct work_struct *work)
{
	struct netfn_tcpst_conn *conn = container_of(work, struct netfn_tcpst_conn, work);
	struct netfn_tcpst_stats *stats = &conn->stats;
	struct netfn_tcpst_ctx *ctx = conn->ctx;
	enum netfn_tcpst_err err;

	/*
	 * Capture the rom and bom time for raw mode.
	 * It will be overridden for HTTP case
	 */
	stats->test.rom_time = stats->test.bom_time = ktime_get_real_ns();

	/*
	 * If in HTTP test mode then perform HTTP PUT
	 */
	if (netfn_tcpst_is_http(ctx) && !netfn_tcpst_http_put(conn)) {
		struct netfn_tcpst_ctx *ctx = conn->ctx;

		pr_warn("%px: Error(%d) in connecting to HTTP server\n", conn, err);
		netfn_tcpst_state_set(ctx, NETFN_TCPST_STATE_ABORTED, netfn_tcpst_state_get(ctx));
		atomic_inc(&ctx->comp_conn);
		return;
	}

	/*
	 * Initialize test bytes
	 */
	conn->test_bytes = netfn_tcpst_is_http(ctx) ? conn->http.file_sz : conn->raw.max_bytes;

	/*
	 * Start the timer only if its time based test
	 */
	if (conn->duration) {
		conn->hrtimer.function = netfn_tcpst_tx_timeout;
		hrtimer_start(&conn->hrtimer, ktime_set(conn->offset, 0), HRTIMER_MODE_REL);
	} else {
		/*
		 * File based mode, update initial ethernet stats
		 */
		netfn_tcpst_update_eth_stats(conn->eth_dev, &conn->stats);
	}

	/*
	 * Override work function for HTTP data path
	 */
	INIT_WORK(work, netfn_tcpst_tx_data);

	/*
	 * Schedule the work again
	 */
	schedule_work_on(smp_processor_id(), work);
}
