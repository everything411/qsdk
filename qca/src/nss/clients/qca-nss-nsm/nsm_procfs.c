/*
 **************************************************************************
 * Copyright (c) 2022,2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/netdevice.h>
#include "exports/nsm_nl_fam.h"
#include "nsm_lat.h"
#include "nsm_sfe.h"

#define NSM_PROCFS_DATA_SZ 32

static struct ctl_table_header *nsm_procfs_latency_header;
static struct ctl_table_header *nsm_procfs_sfe_header;

static char nsm_procfs_data[NSM_PROCFS_DATA_SZ];

/*
 * nsm_procfs_init_handler()
 *	Procfs callback for latency statistics initialization.
 */
static int nsm_procfs_init_handler(struct ctl_table *table,
						int write, void __user *buffer,
						size_t *lenp, loff_t *ppos)
{
	char *name = nsm_procfs_data;
	int ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	if (nsm_lat_init(name)) {
		return 0;
	}
	return -1;
}

/*
 * nsm_procfs_deinit_handler()
 *	Procfs callback for latency statistics free.
 */
static int nsm_procfs_deinit_handler(struct ctl_table *table,
						int write, void __user *buffer,
						size_t *lenp, loff_t *ppos)
{
	char *name = nsm_procfs_data;
	int ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	if (nsm_lat_deinit(name)) {
		return 0;
	}
	return -1;
}

/*
 * nsm_procfs_enable_handler()
 *	Procfs callback for latency statistics enable.
 */
static int nsm_procfs_enable_handler(struct ctl_table *table,
						int write, void __user *buffer,
						size_t *lenp, loff_t *ppos)
{
	char *name = nsm_procfs_data;
	int ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	if (nsm_lat_enable(name)) {
		return 0;
	}
	return -1;
}

/*
 * nsm_procfs_disable_handler()
 *	Procfs callback for latency statistics disable.
 */
static int nsm_procfs_disable_handler(struct ctl_table *table,
						int write, void __user *buffer,
						size_t *lenp, loff_t *ppos)
{
	char *name = nsm_procfs_data;
	int ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	if (nsm_lat_disable(name)) {
		return 0;
	}
	return -1;
}

/*
 * nsm_procfs_debug_set_handler()
 *	Procfs callback to set debug service class for latency statistics.
 */
static int nsm_procfs_debug_set_handler(struct ctl_table *table,
						int write, void __user *buffer,
						size_t *lenp, loff_t *ppos)
{
	char *data = nsm_procfs_data;
	char *name;
	unsigned int sid;
	int ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	name = strsep(&data, " ");

	ret = kstrtouint(data, 0, &sid);

	if (ret) {
		return ret;
	}

	if (nsm_lat_set_debug(name, (uint8_t)sid)) {
		return 0;
	}
	return -1;
}

/*
 * nsm_procfs_debug_unset_handler()
 *	Procfs callback to stop collecting debug latency statistics.
 */
static int nsm_procfs_debug_unset_handler(struct ctl_table *table,
						int write, void __user *buffer,
						size_t *lenp, loff_t *ppos)
{
	char *name = nsm_procfs_data;
	int ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	if (nam_lat_unset_debug(name)) {
		return 0;
	}

	return -1;
}

/*
 * nsm_procfs_debug_print_handler()
 *	Procfs callback to print latency debug statistics.
 */
static int nsm_procfs_debug_print_handler(struct ctl_table *table,
						int write, void __user *buffer,
						size_t *lenp, loff_t *ppos)
{
	char *name = nsm_procfs_data;
	uint32_t max = 0, min = 0, avg = 0, last = 0;
	uint8_t sid;
	int ret = proc_dostring(table, write, buffer, lenp, ppos);

	if (ret || !write) {
		return ret;
	}

	if (!nsm_lat_debug_get(name, &sid, &max, &min, &avg, &last)) {
		return -1;
	}

	printk("sid[%hhu] max: %u, min: %u, avg: %u, last: %u", sid, max, min, avg, last);

	return 0;
}

/*
 * nsm_procfs_latency_print_handler()
 *	Procfs callback to print non-zero latency statistics.
 */
static int nsm_procfs_latency_print_handler(struct ctl_table *table,
						int write, void __user *buffer,
						size_t *lenp, loff_t *ppos)
{
	char *name = nsm_procfs_data;
	unsigned int sid;
	uint64_t hist[NETDEV_SAWF_DELAY_BUCKETS];
	uint64_t avg;
	int bucket;
	int ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	for (sid = 0; sid < NETDEV_SAWF_SID_MAX; sid++)
	{
		if (!nsm_lat_get(name, sid, hist, &avg)) {
			return -1;
		}

		if (!avg) {
			continue;
		}

		printk("%s [%u] avg: %llu\n", name, sid, avg);
		for (bucket = 0; bucket < NETDEV_SAWF_DELAY_BUCKETS; bucket++) {
			printk("%s [%u] hist[%i]: %llu\n", name, sid, bucket, hist[bucket]);
		}
	}
	return 0;
}

/*
 * nsm_procfs_stats_print_handler()
 *	Procfs callback to print service class statistics.
 */
static int nsm_procfs_stats_print_handler(struct ctl_table *table,
						int write, void __user *buffer,
						size_t *lenp, loff_t *ppos)
{
	struct nsm_sfe_stats *stats;
	unsigned int sid;
	int ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	ret = kstrtouint(nsm_procfs_data, 0, &sid);
	if (ret) {
		return ret;
	}

	stats = nsm_sfe_get_stats((uint8_t)sid);
	if (!stats) {
		return -1;
	}

	printk("[%u] pkts: %llu bytes: %llu\n", sid, stats->packets, stats->bytes);
	return 0;
}

/*
 * nsm_procfs_tput_handler()
 *	Procfs callback for latency statistics initialization.
 */
static int nsm_procfs_tput_print_handler(struct ctl_table *table,
						int write, void __user *buffer,
						size_t *lenp, loff_t *ppos)
{
	unsigned int sid, rem_pr, rem_br;
	uint64_t packet_rate, byte_rate;

	int ret = proc_dostring(table, write, buffer, lenp, ppos);
	if (ret || !write) {
		return ret;
	}

	ret = kstrtouint(nsm_procfs_data, 0, &sid);
	if (ret) {
		return ret;
	}

	if (nsm_sfe_get_throughput(sid, &packet_rate, &byte_rate)) {
		return -1;
	}

	rem_pr = do_div(packet_rate ,NSM_NL_PREC);
	rem_br = do_div(byte_rate,NSM_NL_PREC);
	printk("[%u] Pps: %llu.%u, Bps: %llu.%u\n",
		sid,
		packet_rate,
		rem_pr,
		byte_rate,
		rem_br);

	return 0;
}

static struct ctl_table nsm_procfs_latency_table[] = {
	{
		.procname	= "init",
		.data		= &nsm_procfs_data,
		.maxlen		= sizeof(char) * NSM_PROCFS_DATA_SZ,
		.mode		= 0644,
		.proc_handler	= &nsm_procfs_init_handler,
	},
	{
		.procname	= "deinit",
		.data		= &nsm_procfs_data,
		.maxlen		= sizeof(char) * NSM_PROCFS_DATA_SZ,
		.mode		= 0644,
		.proc_handler   = &nsm_procfs_deinit_handler,
	},
	{
		.procname	= "enable",
		.data		= &nsm_procfs_data,
		.maxlen		= sizeof(char) * NSM_PROCFS_DATA_SZ,
		.mode		= 0644,
		.proc_handler	= &nsm_procfs_enable_handler,
	},
	{
		.procname	= "disable",
		.data		= &nsm_procfs_data,
		.maxlen		= sizeof(char) * NSM_PROCFS_DATA_SZ,
		.mode		= 0644,
		.proc_handler	= &nsm_procfs_disable_handler,
	},
	{
		.procname	= "debug_set",
		.data		= &nsm_procfs_data,
		.maxlen		= sizeof(char) * NSM_PROCFS_DATA_SZ,
		.mode		= 0644,
		.proc_handler	= &nsm_procfs_debug_set_handler,
	},
	{
		.procname	= "debug_unset",
		.data		= &nsm_procfs_data,
		.maxlen		= sizeof(char) * NSM_PROCFS_DATA_SZ,
		.mode		= 0644,
		.proc_handler	= &nsm_procfs_debug_unset_handler,
	},
	{
		.procname	= "debug_print",
		.data		= &nsm_procfs_data,
		.maxlen		= sizeof(char) * NSM_PROCFS_DATA_SZ,
		.mode		= 0644,
		.proc_handler	= &nsm_procfs_debug_print_handler,
	},
	{
		.procname	= "latency_print",
		.data		= &nsm_procfs_data,
		.maxlen		= sizeof(char) * NSM_PROCFS_DATA_SZ,
		.mode		= 0644,
		.proc_handler	= &nsm_procfs_latency_print_handler,
	},
	{ }
};

static struct ctl_table nsm_procfs_sfe_table[] = {
	{
		.procname	= "stats",
		.data		= &nsm_procfs_data,
		.maxlen		= sizeof(char) * NSM_PROCFS_DATA_SZ,
		.mode		= 0644,
		.proc_handler	= &nsm_procfs_stats_print_handler,
	},
	{
		.procname	= "tput",
		.data		= &nsm_procfs_data,
		.maxlen		= sizeof(char) * NSM_PROCFS_DATA_SZ,
		.mode		= 0644,
		.proc_handler	= &nsm_procfs_tput_print_handler,
	},
	{ }
};

/*
 * nsm_procfs_deinit()
 *	Unregisters sysctl tables for NSM.
 */
void nsm_procfs_deinit(void)
{
	if (nsm_procfs_latency_header) {
		unregister_sysctl_table(nsm_procfs_latency_header);
	}
	if (nsm_procfs_sfe_header) {
		unregister_sysctl_table(nsm_procfs_sfe_header);
	}
}

/*
 * nsm_procfs_init()
 *	Registers sysctl tables for NSM.
 */
void nsm_procfs_init(void)
{
	nsm_procfs_latency_header = register_sysctl("net/nsm/latency", nsm_procfs_latency_table);
	if (!nsm_procfs_latency_header) {
		printk("Failed to register nsm latency sysctl table.\n");
	}
	nsm_procfs_sfe_header = register_sysctl("net/nsm/sfe", nsm_procfs_sfe_table);
	if (!nsm_procfs_sfe_header) {
		printk("Failed to register nsm sfe sysctl table.\n");
	}
}
