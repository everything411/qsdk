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

#ifndef __NSS_TCP_ST_LOG_H
#define __NSS_TCP_ST_LOG_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdint.h>
#include <limits.h>

#define  NSS_TCP_ST_ALIGN(x, b) (((x) + (b) - 1) & ~((b) - 1))

#define NSS_TCP_ST_COLOR_RST "\x1b[0m"
#define NSS_TCP_ST_COLOR_GRN "\x1b[32m"
#define NSS_TCP_ST_COLOR_RED "\x1b[31m"
#define NSS_TCP_ST_COLOR_MGT "\x1b[35m"

#define nss_tcp_st_log_error(fmt, arg...) printf(NSS_TCP_ST_COLOR_RED"[ERR]"NSS_TCP_ST_COLOR_RST fmt, ## arg)
#define nss_tcp_st_log_info(fmt, arg...) printf(fmt, ## arg)
#define nss_tcp_st_log_trace(fmt, arg...) printf(NSS_TCP_ST_COLOR_MGT"[TRC(<%s>)]"NSS_TCP_ST_COLOR_RST fmt, __func__, ## arg)
#define nss_tcp_st_log_options(fmt, arg...) printf(NSS_TCP_ST_COLOR_MGT"[OPT_%d]"NSS_TCP_ST_COLOR_RST fmt, ## arg)
#define nss_tcp_st_log_warn(fmt, arg...) printf(NSS_TCP_ST_COLOR_RED"[WARN]"NSS_TCP_ST_COLOR_RST fmt, ##arg)

#endif
