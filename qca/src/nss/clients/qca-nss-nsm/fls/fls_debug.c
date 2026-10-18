/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/sysctl.h>
#include <linux/net.h>
#include <linux/proc_fs.h>
#include "fls_debug.h"
#include "fls_flow.h"
#include "fls_stats.h"
#ifndef FLS_LITE_ENABLE
#include <ecm_ae_classifier_public.h>
#endif

#define FLS_DEBUG_LEVEL_DEFAULT FLS_DEBUG_LEVEL_ERROR

#ifndef FLS_LITE_ENABLE
static int fls_conn_ipv4_sprint(uint32_t addr, char *str, size_t len);
#endif

static uint32_t fls_debug_level_current;
static uint32_t fls_debug_level_min = FLS_DEBUG_LEVEL_NONE;
static uint32_t fls_debug_level_max = FLS_DEBUG_LEVEL_MAX - 1;
static struct ctl_table_header *fls_debug_header;
static struct proc_dir_entry *pentry;

static struct ctl_table fls_debug_table_udp_clf[] = {
	{
		.procname	= "debug",
		.data		= &fls_debug_level_current,
		.maxlen		= sizeof(fls_debug_level_current),
		.extra1		= &fls_debug_level_min,
		.extra2		= &fls_debug_level_max,
		.mode		= 0644,
		.proc_handler	= &proc_douintvec_minmax,
	},
	{ }
};

#ifndef FLS_LITE_ENABLE
static uint32_t fls_debug_sample_count_min = 1;
static uint32_t fls_debug_sample_count_max = FLS_DEF_SENSOR_MAX_SAMPLE_COUNT;
static uint32_t fls_debug_bool_min = 0;
static uint32_t fls_debug_bool_max = 1;

/*
 * fls_debug_sample_timer_freq_handler()
 *	Handler to calculate the timer frequency of fls_def_sensor_sample_timer
 */
static int fls_debug_sample_timer_freq_handler(struct ctl_table *table, int write, void __user *buffer, size_t *lenp, loff_t *ppos) {
	int ret;
	uint32_t tmp_xl, tmp_xxl, tmp;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (write) {
		/*
		 * Find greatest common factor using Euclidean method
		 */
		tmp_xl = fls_def_sensor_xl_window;
		tmp_xxl = fls_def_sensor_xxl_window;
		while(tmp_xxl != 0) {
			tmp = tmp_xxl;
			tmp_xxl = tmp_xl % tmp_xxl;
			tmp_xl = tmp;
		}
		fls_def_sensor_sample_freq = tmp_xl;

		FLS_INFO("XL/XXL sample Frequency: %u\n", fls_def_sensor_sample_freq);
    	}

	return ret;
}

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
		.procname	= "window_sz",
		.data		= fls_def_sensor_window_sz,
		.maxlen		= sizeof(fls_def_sensor_window_sz),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec,
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
		.procname	= "pkts_hwm",
		.data		= &fls_def_sensor_pkts_hwm,
		.maxlen		= sizeof(fls_def_sensor_pkts_hwm),
		.mode		= 0644,
		.proc_handler	= &proc_douintvec,
	},
	{
		.procname	= "bytes_hwm",
		.data		= &fls_def_sensor_bytes_hwm,
		.maxlen		= sizeof(fls_def_sensor_bytes_hwm),
		.mode		= 0644,
		.proc_handler	= &proc_douintvec,
	},
	{
		.procname	= "stats_bytes_en",
		.data		= &fls_def_sensor_bytes,
		.maxlen		= sizeof(fls_def_sensor_bytes),
		.extra1		= &fls_debug_bool_min,
		.extra2		= &fls_debug_bool_max,
		.mode		= 0644,
		.proc_handler	= &proc_douintvec_minmax,
	},
	{
		.procname	= "stats_ipat_en",
		.data		= &fls_def_sensor_ipat,
		.maxlen		= sizeof(fls_def_sensor_ipat),
		.extra1		= &fls_debug_bool_min,
		.extra2		= &fls_debug_bool_max,
		.mode		= 0644,
		.proc_handler	= &proc_douintvec_minmax,
	},
	{
		.procname	= "stop_forever",
		.data		= &fls_def_sensor_stop_forever,
		.maxlen		= sizeof(fls_def_sensor_stop_forever),
		.extra1		= &fls_debug_bool_min,
		.extra2		= &fls_debug_bool_max,
		.mode		= 0644,
		.proc_handler	= &proc_douintvec_minmax,
	},
	{
		.procname	= "stats_burst_en",
		.data		= &fls_def_sensor_burst,
		.maxlen		= sizeof(fls_def_sensor_burst),
		.extra1		= &fls_debug_bool_min,
		.extra2		= &fls_debug_bool_max,
		.mode		= 0644,
		.proc_handler	= &proc_douintvec_minmax,
	},
	{
		.procname	= "burst_thresh",
		.data		= fls_def_sensor_burst_threshold,
		.maxlen		= sizeof(fls_def_sensor_burst_threshold),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec,
	},
	{
		.procname	= "burst_short_intvl",
		.data		= fls_def_sensor_burst_short_intvl,
		.maxlen		= sizeof(fls_def_sensor_burst_short_intvl),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec,
	},
	{
		.procname	= "burst_long_intvl",
		.data		= fls_def_sensor_burst_long_intvl,
		.maxlen		= sizeof(fls_def_sensor_burst_long_intvl),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec
	},
	{
		.procname	= "xxl_sz_threshold",
		.data		= &fls_def_sensor_xxl_sz_threshold,
		.maxlen		= sizeof(fls_def_sensor_xxl_sz_threshold),
		.mode		= 0644,
		.proc_handler	= &proc_douintvec,
	},
	{
		.procname	= "xxl_short",
		.data		= &fls_def_sensor_xxl_short,
		.maxlen		= sizeof(fls_def_sensor_xxl_short),
		.mode		= 0644,
		.proc_handler	= &proc_douintvec,
	},
	{
		.procname	= "xxl_long",
		.data		= &fls_def_sensor_xxl_long,
		.maxlen		= sizeof(fls_def_sensor_xxl_long),
		.mode		= 0644,
		.proc_handler	= &proc_douintvec,
	},
	{
		.procname	= "xxl_window",
		.data		= &fls_def_sensor_xxl_window,
		.maxlen		= sizeof(fls_def_sensor_xxl_window),
		.mode		= 0644,
		.proc_handler	= fls_debug_sample_timer_freq_handler,
	},
	{
		.procname	= "conn_timeout",
		.data		= &fls_conn_timeout,
		.maxlen		= sizeof(fls_conn_timeout),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec
	},
	{
		.procname	= "xl_window",
		.data		= &fls_def_sensor_xl_window,
		.maxlen		= sizeof(fls_def_sensor_xl_window),
		.mode		= 0644,
		.proc_handler	= fls_debug_sample_timer_freq_handler,
	},
	{ }
};

/*
 * fls_debug_ecm_flush_5tuple()
 *     Ask ECM to defunct the connection matching this 5-tuple, forcing a
 *     PPE-accelerated flow back to the SFE software path.
 */
static bool fls_debug_ecm_flush_5tuple(struct fls_cmdinfo *packetinfo)
{
	struct in6_addr src_ip6;
	struct in6_addr dst_ip6;
	char src_str[16];
	char dst_str[16];

	if (packetinfo->version == 4) {
		fls_conn_ipv4_sprint(packetinfo->src_ip[0], src_str, sizeof(src_str));
		fls_conn_ipv4_sprint(packetinfo->dst_ip[0], dst_str, sizeof(dst_str));
		FLS_WARN("ECM flush v4 request: proto=%u src=%s:%hu dst=%s:%hu (raw src_port=0x%04x dst_port=0x%04x)\n",
				packetinfo->protocol, src_str, ntohs(packetinfo->src_port),
				dst_str, ntohs(packetinfo->dst_port),
				packetinfo->src_port, packetinfo->dst_port);

		return ecm_ae_classifier_decelerate_v4_connection(packetinfo->src_ip[0],
								packetinfo->src_port,
								packetinfo->dst_ip[0],
								packetinfo->dst_port,
								packetinfo->protocol);
	}

	memcpy(&src_ip6, packetinfo->src_ip, sizeof(src_ip6));
	memcpy(&dst_ip6, packetinfo->dst_ip, sizeof(dst_ip6));

	FLS_WARN("ECM flush v6 request: proto=%u src=%pI6c:%hu dst=%pI6c:%hu (raw src_port=0x%04x dst_port=0x%04x)\n",
			packetinfo->protocol, &src_ip6, ntohs(packetinfo->src_port),
			&dst_ip6, ntohs(packetinfo->dst_port),
			packetinfo->src_port, packetinfo->dst_port);

	return ecm_ae_classifier_decelerate_v6_connection(src_ip6, packetinfo->src_port,
							dst_ip6, packetinfo->dst_port,
							packetinfo->protocol);
}

static ssize_t fls_pfsops_write(struct file *file, const char __user *buffer, size_t length, loff_t *ppos)
{
	int count;
	struct fls_cmdinfo packetinfo;
	struct fls_conn *conn;
	char src_str[16];
	char dst_str[16];
	bool found;

	count = min(length, sizeof(struct fls_cmdinfo));
	if (copy_from_user((char*)&packetinfo, buffer, count)) {
		FLS_ERROR("copy from user failed.\n");
		return -EFAULT;
	}

	if (packetinfo.version == 4) {
		fls_conn_ipv4_sprint(packetinfo.src_ip[0], src_str, sizeof(src_str));
		fls_conn_ipv4_sprint(packetinfo.dst_ip[0], dst_str, sizeof(dst_str));
		FLS_TRACE("FLS: Received cmd=%u ipv=%u proto=%u src=%s:%hu dst=%s:%hu\n",
				packetinfo.cmd, packetinfo.version, packetinfo.protocol,
				src_str, ntohs(packetinfo.src_port),
				dst_str, ntohs(packetinfo.dst_port));
	} else {
		struct in6_addr src_ip6;
		struct in6_addr dst_ip6;

		memcpy(&src_ip6, packetinfo.src_ip, sizeof(src_ip6));
		memcpy(&dst_ip6, packetinfo.dst_ip, sizeof(dst_ip6));
		FLS_TRACE("FLS: Received cmd=%u ipv=%u proto=%u src=%pI6c:%hu dst=%pI6c:%hu\n",
				packetinfo.cmd, packetinfo.version, packetinfo.protocol,
				&src_ip6, ntohs(packetinfo.src_port),
				&dst_ip6, ntohs(packetinfo.dst_port));
	}

	switch (packetinfo.cmd) {
	case FLS_PFS_RESULT:
		FLS_TRACE("\nFLS: Receive stop command.\n");
		conn = fls_conn_lookup(packetinfo.version, packetinfo.protocol,
					packetinfo.src_ip,
					packetinfo.src_port,
					packetinfo.dst_ip,
					packetinfo.dst_port);
		if(conn) {
			spin_lock_bh(&fct.lock);
			conn->stats.isd.sendevent = false;
			conn->traffic_class = packetinfo.data.classid;

			if(conn->reverse) {
				conn->reverse->stats.isd.sendevent = false;
				conn->reverse->traffic_class = packetinfo.data.classid;
			}
			if (fls_def_sensor_max_events != -1 && fls_def_sensor_stop_forever)  {
				FLS_TRACE("Lookup succeed! Stop XXL collection (FOREVER).");
				conn->flags &= ~SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;
				if (conn->reverse) {
					conn->reverse->flags &= ~SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;
				}
				spin_unlock_bh(&fct.lock);
				break;
			}
			FLS_TRACE("Lookup succeed! Stop XXL collection (For this epoch).");
			fls_debug_print_conn_info(conn);
			spin_unlock_bh(&fct.lock);
		} else {
			FLS_TRACE("Lookup failed!\n");
		}

		break;

	case FLS_PFS_EVENT:
		FLS_ERROR("FLSP + procfs is not supported %d.\n", packetinfo.cmd);
		break;

	case FLS_PFS_FLUSH:
		FLS_ERROR("FLSP + procfs is not supported %d.\n", packetinfo.cmd);
		break;

	case FLS_PFS_CLEAN_EVENTS:
		fls_rfs_clean_events();
		break;

	case FLS_PFS_REINSPECT:
		FLS_TRACE("\nFLS: Receive reinspect command.\n");
		conn = fls_conn_lookup(packetinfo.version, packetinfo.protocol,
					packetinfo.src_ip,
					packetinfo.src_port,
					packetinfo.dst_ip,
					packetinfo.dst_port);
		if (conn) {
			FLS_WARN("Lookup succeeded! Resume stats collection.\n");

			spin_lock_bh(&fct.lock);
			fls_debug_print_conn_info(conn);
			spin_unlock_bh(&fct.lock);

			if (packetinfo.version == 4) {
				found = sfe_ipv4_fls_enable(packetinfo.protocol, packetinfo.src_ip[0], packetinfo.src_port,
							packetinfo.dst_ip[0], packetinfo.dst_port);
			} else {
				found = sfe_ipv6_fls_enable(packetinfo.protocol, packetinfo.src_ip, packetinfo.src_port,
							packetinfo.dst_ip, packetinfo.dst_port);
			}

			if (found) {
				/*
				 * Rearm must happen outside fct.lock: it calls hrtimer_cancel(),
				 * which can block waiting for a running timer callback to finish.
				 */
				fls_def_sensor_conn_rearm(conn);
			} else {
				FLS_WARN("SFE fls_enable did not find a matching connection-match for reinspect 5-tuple.\n");
				atomic_inc(&conn->fls_conn_exception_counters[FLS_CONN_EXCEPTION_REINSPECT_SFE_ENABLE_FAIL]);
				if (conn->reverse) {
					atomic_inc(&conn->reverse->fls_conn_exception_counters[FLS_CONN_EXCEPTION_REINSPECT_SFE_ENABLE_FAIL]);
				}
			}
			break;
		}

		FLS_TRACE("Lookup failed! Flow may be in PPE, flushing 5-tuple via ECM.\n");
		if (!fls_debug_ecm_flush_5tuple(&packetinfo)) {
			FLS_WARN("ECM flush for reinspect 5-tuple did not find a matching connection.\n");
			atomic_inc(&fct.fls_gbl_exception_counters[FLS_GBL_EXCEPTION_REINSPECT_ECM_FLUSH_FAIL]);
		}
		break;

	default:
		FLS_ERROR("Unrecognized command %d.\n", packetinfo.cmd);
	}

	return count;
}

static const struct proc_ops fls_pfsops = {
	.proc_write = fls_pfsops_write,
	.proc_read = seq_read,
};

static int fls_conn_ipv4_sprint(uint32_t addr, char *str, size_t len)
{
	return snprintf(str, len, "%u.%u.%u.%u",
			addr & 0xFF,
			(addr >> 8) & 0xFF,
			(addr >> 16) & 0xFF,
			addr >> 24);
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

	if (!conn) {
		printk("fls_debug_print_conn_info: NULL connection pointer\n");
		return;
	}

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
	printk("%p: traffic_class=%u\n", conn, conn->traffic_class);

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
	printk("%p: traffic_class=%u\n", reply, reply->traffic_class);
}
#endif

void fls_debug_print(uint32_t level, char *fmt, ...) {
	va_list args;

	if (level <= fls_debug_level_current) {
		va_start(args, fmt);
		vprintk(fmt, args);
		va_end(args);
	}
}

void fls_debug_deinit(void)
{
	if (!udp_clf_enabled) {
		if (pentry)
			proc_remove(pentry);
	}

	if (fls_debug_header) {
		unregister_sysctl_table(fls_debug_header);
	}

#ifndef FLS_LITE_ENABLE
	fls_stats_deinit();
#endif
}

void fls_debug_init(void)
{
	fls_debug_level_current = FLS_DEBUG_LEVEL_DEFAULT;

	if (udp_clf_enabled) {
		fls_debug_header = register_sysctl("net/fls-lite", fls_debug_table_udp_clf);
		if (!fls_debug_header) {
			FLS_ERROR("Failed to register fls sysctl table.\n");
		}

		return;
	}

#ifndef FLS_LITE_ENABLE
	pentry = proc_create("fls_cmd", 0644, NULL, &fls_pfsops);
	if (!pentry) {
		FLS_ERROR("Failed to register fls procfs cmd file\n");
	}

	fls_debug_header = register_sysctl("net/fls", fls_debug_table);
	if (!fls_debug_header) {
		FLS_ERROR("Failed to register fls sysctl table.\n");
	}

	fls_stats_init();
#endif
}
