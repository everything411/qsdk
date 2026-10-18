/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */
#ifndef __NSS_TCP_ST_H
#define __NSS_TCP_ST_H

#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netfn_tcpst.h>
#include <netfn_tcpst_nl.h>
#include "nss-tcp-st-log.h"

struct nss_tcp_st_sk {
	struct nl_sock *sk;
	int family_id;
};

/**
 * nss_tcp_st_start()
 *      Starts the TCP Speedtest
 *
 * @cfg[in] TCP speedtest configuration
 *
 * @return
 * 0 if test is started, error in case of failure
 */
int nss_tcp_st_start(struct netfn_tcpst_cfg *cfg);

/**
 * nss_tcp_st_stop()
 *	Stops the TCP Speedtest
 *	Test will abort if stop is issued before test completion
 *	Status of test can be retrieved using nss_tcpst_get_stats()
 *
 * @cb[in] Callback called on completion
 * @void*[in] Application data
 *
 * @return
 * 0 if test is stopped, error in case of failure
 */
int nss_tcp_st_stop(netfn_tcpst_comp_t completion_cb, void *app_data);

/**
 * nss_tcp_st_get_stats()
 *	Returns the TCP stats
 *	User has to poll via this API for test completion
 *
 * @stats[in] Pointer to user allocated memory for statistics retrieval
 * @state[in] Pointer to user allocated memory for current Test state
 *
 * @return
 * 0 on successful retrieval of statistics, error in case of failure
 */
int nss_tcp_st_get_stats(struct netfn_tcpst_stats *stats, enum netfn_tcpst_state *state);

#endif /*__NSS_TCP_ST_H*/
