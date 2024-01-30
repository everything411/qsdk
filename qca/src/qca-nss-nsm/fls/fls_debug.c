/*
 **************************************************************************
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
 **************************************************************************
 */

#include <linux/sysctl.h>
#include <linux/net.h>
#include "fls_debug.h"

#define FLS_DEBUG_LEVEL_DEFAULT FLS_DEBUG_LEVEL_WARN

static uint32_t fls_debug_level_current;
static uint32_t fls_debug_level_min = FLS_DEBUG_LEVEL_NONE;
static uint32_t fls_debug_level_max = FLS_DEBUG_LEVEL_MAX - 1;
static uint32_t fls_debug_sample_count_min = 1;
static uint32_t fls_debug_sample_count_max = FLS_DEF_SENSOR_MAX_SAMPLE_COUNT;
static uint32_t fls_debug_bool_min = 0;
static uint32_t fls_debug_bool_max = 1;
static struct ctl_table_header *fls_debug_header;

static struct ctl_table fls_debug_table[] = {
	{
		.procname	= "debug",
		.data		= &fls_debug_level_current,
		.maxlen		= sizeof(fls_debug_level_current),
		.extra1		= &fls_debug_level_min,
		.extra2		= &fls_debug_level_max,
		.mode		= 0644,
		.proc_handler	= &proc_douintvec_minmax,
	},
	{
		.procname	= "event_offset",
		.data		= &fls_def_sensor_delay,
		.maxlen		= sizeof(fls_def_sensor_delay),
		.mode		= 0644,
		.proc_handler	= &proc_douintvec,
	},
	{
		.procname	= "max_events",
		.data		= &fls_def_sensor_max_events,
		.maxlen		= sizeof(fls_def_sensor_delay),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec,
	},
	{
		.procname	= "sample_window",
		.data		= &fls_def_sensor_sample_length,
		.maxlen		= sizeof(fls_def_sensor_sample_length),
		.mode		= 0644,
		.proc_handler	= &proc_douintvec,
	},
	{
		.procname	= "sample_count",
		.data		= &fls_def_sensor_sample_count,
		.maxlen		= sizeof(fls_def_sensor_sample_count),
		.extra1		= &fls_debug_sample_count_min,
		.extra2		= &fls_debug_sample_count_max,
		.mode		= 0644,
		.proc_handler	= &proc_douintvec_minmax,
	},
	{
		.procname	= "stats_bytes",
		.data		= &fls_def_sensor_bytes,
		.maxlen		= sizeof(fls_def_sensor_bytes),
		.extra1		= &fls_debug_bool_min,
		.extra2		= &fls_debug_bool_max,
		.mode		= 0644,
		.proc_handler	= &proc_douintvec_minmax,
	},
	{
		.procname	= "stats_ipat",
		.data		= &fls_def_sensor_ipat,
		.maxlen		= sizeof(fls_def_sensor_ipat),
		.extra1		= &fls_debug_bool_min,
		.extra2		= &fls_debug_bool_max,
		.mode		= 0644,
		.proc_handler	= &proc_douintvec_minmax,
	},
	{ }
};

static struct ctl_table fls_debug_dir[] = {
	{
		.procname	= "fls",
		.mode		= 0555,
		.child		= fls_debug_table,
	},
	{ }
};

static struct ctl_table fls_debug_root_dir[] = {
	{
		.procname	= "net",
		.mode		= 0555,
		.child		= fls_debug_dir,
	},
	{ }
};

static int fls_conn_ipv4_sprint(uint32_t addr, char *str, size_t len)
{
	return snprintf(str, len, "%u.%u.%u.%u",
			addr & 0xFF,
			(addr >> 8) & 0xFF,
			(addr >> 16) & 0xFF,
			addr >> 24);
}

void fls_debug_print(uint32_t level, char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	if (level <= fls_debug_level_current) {
		vprintk(fmt, args);
	}
}

void fls_debug_print_event_info(struct fls_event *event)
{
	char ipaddr_str[16];

	if (fls_debug_level_current < FLS_DEBUG_LEVEL_INFO) {
		return;
	}

	printk("type: %d, dir: %d, ipv: %d, pro: %d\n", event->event_type, event->dir, event->ip_version, event->protocol);
	fls_conn_ipv4_sprint(event->orig_src_ip[0], ipaddr_str, 16);
	printk("orig_src = %s:%hu", ipaddr_str, ntohs(event->orig_src_port));
	fls_conn_ipv4_sprint(event->orig_dest_ip[0], ipaddr_str, 16);
	printk("orig_dst = %s:%hu\n", ipaddr_str, ntohs(event->orig_dest_port));

	fls_conn_ipv4_sprint(event->ret_src_ip[0], ipaddr_str, 16);
	printk("repl_src = %s:%hu", ipaddr_str, ntohs(event->ret_src_port));
	fls_conn_ipv4_sprint(event->ret_dest_ip[0], ipaddr_str, 16);
	printk("repl_dst = %s:%hu\n", ipaddr_str, ntohs(event->ret_dest_port));

}

void fls_debug_print_conn_info(struct fls_conn *conn)
{
	struct fls_conn *reply;
	char ipaddr_str[16];
	uint32_t i;

	if (fls_debug_level_current < FLS_DEBUG_LEVEL_INFO) {
		return;
	}

	printk("%p ipv: %u, pro: %u\n", conn, conn->ip_version, conn->protocol);
	if (conn->ip_version != 4) {
		printk("%p Cannot print ipv6 connections yet.\n", conn);
	}

	fls_conn_ipv4_sprint(conn->src_ip[0], ipaddr_str, 16);
	printk("%p orig_src = %s:%hu", conn, ipaddr_str, ntohs(conn->src_port));
	for (i = 1; i < 4; i++) {
		if (conn->src_ip[i]) {
			printk("%p orig_src[%u] = %x", conn, i, conn->src_ip[i]);
		}
	}

	fls_conn_ipv4_sprint(conn->dest_ip[0], ipaddr_str, 16);
	printk("%p orig_dst = %s:%hu\n", conn, ipaddr_str, ntohs(conn->dest_port));
	for (i = 1; i < 4; i++) {
		if (conn->dest_ip[i]) {
			printk("%p orig_dst[%u] = %x", conn, i, conn->dest_ip[i]);
		}
	}

	reply = conn->reverse;
	if (!reply) {
		return;
	}

	fls_conn_ipv4_sprint(reply->src_ip[0], ipaddr_str, 16);
	printk("%p repl_src = %s:%hu", reply, ipaddr_str, ntohs(reply->src_port));
	for (i = 1; i < 4; i++) {
		if (reply->src_ip[i]) {
			printk("%p repl_src[%u] = %x", reply, i, reply->src_ip[i]);
		}
	}

	fls_conn_ipv4_sprint(reply->dest_ip[0], ipaddr_str, 16);
	printk("%p repl_dst = %s:%hu\n", reply, ipaddr_str, ntohs(reply->dest_port));
	for (i = 1; i < 4; i++) {
		if (reply->src_ip[i]) {
			printk("%p repl_dst[%u] = %x", reply, i, reply->dest_ip[i]);
		}
	}
}

void fls_debug_deinit(void)
{
	if (fls_debug_header) {
		unregister_sysctl_table(fls_debug_header);
	}
}

void fls_debug_init(void)
{
	fls_debug_level_current = FLS_DEBUG_LEVEL_DEFAULT;
	fls_debug_header = register_sysctl_table(fls_debug_root_dir);
	if (!fls_debug_header) {
		printk("Failed to register fls sysctl table.\n");
	}
}