/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __NETFN_HLOS_H
#define __NETFN_HLOS_H

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


#define  NETFN_ALIGN(x, b) (((x) + (b) - 1) & ~((b) - 1))

#define NETFN_COLOR_RST "\x1b[0m"
#define NETFN_COLOR_GRN "\x1b[32m"
#define NETFN_COLOR_RED "\x1b[31m"
#define NETFN_COLOR_MGT "\x1b[35m"

#define netfn_log_error(fmt, arg...) printf(NETFN_COLOR_RED"[ERR]"NETFN_COLOR_RST fmt, ## arg)
#define netfn_log_info(fmt, arg...) printf(NETFN_COLOR_GRN"[INF]"NETFN_COLOR_RST fmt, ## arg)
#define netfn_log_trace(fmt, arg...) printf(NETFN_COLOR_MGT"[TRC(<%s>)]"NETFN_COLOR_RST fmt, __func__, ## arg)
#define netfn_log_options(fmt, arg...) printf(NETFN_COLOR_MGT"[OPT_%d]"NETFN_COLOR_RST fmt, ## arg)
#define netfn_log_warn(fmt, arg...) printf(NETFN_COLOR_RED"[WARN]"NETFN_COLOR_RST fmt, ##arg)

#define netfn_log_arg_options(num, param) netfn_log_options("'%s'\n", num, (param)->name)
#define netfn_log_arg_error(param) netfn_log_error("invalid args for '%s'\n", (param)->name)
#define netfn_log_data_error(param) netfn_log_error("invalid data for '%s'\n", (param)->name)

#endif
