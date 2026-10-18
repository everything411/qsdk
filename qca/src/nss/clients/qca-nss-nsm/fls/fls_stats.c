/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include "fls_conn.h"
#include "fls_debug.h"
#include "fls_stats.h"

#ifndef FLS_LITE_ENABLE

/*
 * Debugfs root directory for FLS
 */
struct dentry *fls_debug_root_dir;

/*
 * Debugfs file for connection statistics
 */
static struct dentry *fls_gbl_stats_file;

/*
 * Debugfs file for per-connection statistics
 */
static struct dentry *fls_conn_stats_file;

/*
 * String representations for FLS conn counters
 */
static const char *fls_conn_counters_str[] = {
	"fls_conn_rfs_enqueue_xxl_count",
	"fls_conn_rfs_enqueue_xl_count",
	"fls_conn_rfs_event_type_def_count",
	"fls_conn_per_conn_delay_finished",
	"fls_conn_timer_init",
	"fls_conn_timer_delete"
};

/*
 * String representations for FLS conn exception counters
 */
static const char *fls_conn_exception_counters_str[] = {
	"fls_conn_exception_rfs_enqueue_xxl_fail",
	"fls_conn_exception_rfs_enqueue_xl_fail",
	"fls_conn_exception_rfs_enqueue_def_fail",
	"fls_conn_sensor_hwm_exceeded",
	"fls_conn_sensor_max_event_exceeded",
	"fls_conn_exception_cannot_create_event_unidir_flow",
	"fls_conn_exception_invalid_flags_window_timer_callback",
	"fls_conn_exception_default_sensor_disabled",
	"fls_conn_exception_reinspect_sfe_enable_fail"
};

/*
 * String representations for FLS global common counters
 */
static const char *fls_gbl_counters_str[] = {
	"active_connection_count",
	"create_request_count",
	"delete_request_count",
};

/*
 * String representations for FLS global common exception counters
 */
static const char *fls_gbl_exception_counters_str[] = {
	"fls_gbl_exception_mem_alloc_fail",
	"fls_gbl_exception_max_conn_limit",
	"fls_gbl_rfs_exception_channel_not_init",
	"fls_gbl_rfs_exception_buff_full",
	"fls_gbl_rfs_exception_no_event_pending",
	"fls_gbl_rfs_exception_event_queue_full",
	"fls_gbl_exception_reinspect_ecm_flush_fail"
};

/*
 * fls_gbl_stats_show()
 *	Read callback for debugfs connection stats file
 */
static int fls_gbl_stats_show(struct seq_file *s, void *unused)
{
	int i;

	seq_printf(s, "FLS Global Common Counters:\n");
	for (i = 0; i < FLS_GBL_COUNTERS_MAX; i++) {
		uint32_t value = (uint32_t)atomic_read(&fct.fls_gbl_counters[i]);
		if (value > 0) {
			seq_printf(s, "%s: %u\n", fls_gbl_counters_str[i], value);
		}
	}

	seq_printf(s, "\nFLS Global Common Exception Counters:\n");
	for (i = 0; i < FLS_GBL_EXCEPTION_COUNTERS_MAX; i++) {
		uint32_t value = (uint32_t)atomic_read(&fct.fls_gbl_exception_counters[i]);
		if (value > 0) {
			seq_printf(s, "%s: %u\n", fls_gbl_exception_counters_str[i], value);
		}
	}

	return 0;
}

/*
 * fls_gbl_stats_open()
 *	Open callback for debugfs connection stats file
 */
static int fls_gbl_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, fls_gbl_stats_show, NULL);
}

/*
 * fls_gbl_stats_release()
 *	Release callback for debugfs connection stats file
 */
static int fls_gbl_stats_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

/*
 * File operations for debugfs connection stats file
 */
static const struct file_operations fls_gbl_stats_fops = {
	.owner = THIS_MODULE,
	.open = fls_gbl_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = fls_gbl_stats_release,
};

/*
 * fls_conn_stats_show()
 *	Read callback for debugfs per-connection stats file.
 *	It iterates through all active connections and prints their statistics.
 */
static int fls_conn_stats_show(struct seq_file *s, void *unused)
{
	struct fls_conn *conn;
	int i;

	seq_printf(s, "FLS Connection Statistics:\n");

	spin_lock_bh(&fct.lock);

	conn = fct.all_connections_head;
	if (!conn) {
		seq_printf(s, "No active connections\n");
		spin_unlock_bh(&fct.lock);
		return 0;
	}

	while (conn) {
		if (conn->ip_version == 4) {
			seq_printf(s, "\nConnection @ %p: IP Version: %u, Protocol: %u, Traffic Class: %u, Source IP: %pI4, Source Port: %hu, Dest IP: %pI4, Dest Port: %hu\n", conn, conn->ip_version, conn->protocol, conn->traffic_class, &conn->src_ip[0], ntohs(conn->src_port), &conn->dest_ip[0], ntohs(conn->dest_port));
		} else if (conn->ip_version == 6) {
			seq_printf(s, "\nConnection @ %p: IP Version: %u, Protocol: %u, Traffic Class: %u, Source IP: %pI6, Source Port: %hu, Dest IP: %pI6, Dest Port: %hu\n", conn, conn->ip_version, conn->protocol, conn->traffic_class, &conn->src_ip[0], ntohs(conn->src_port), &conn->dest_ip[0], ntohs(conn->dest_port));
		}

		seq_printf(s, "  FLS Conn Counters:\n");
		for (i = 0; i < ARRAY_SIZE(fls_conn_counters_str); i++) {
			uint32_t value = (uint32_t)atomic_read(&conn->fls_conn_counters[i]);
			if (value > 0) {
				seq_printf(s, "    %s: %u\n", fls_conn_counters_str[i], value);
			}
		}

		seq_printf(s, "  FLS Conn Exception Counters:\n");
		for (i = 0; i < ARRAY_SIZE(fls_conn_exception_counters_str); i++) {
			uint32_t value = (uint32_t)atomic_read(&conn->fls_conn_exception_counters[i]);
			if (value > 0) {
				seq_printf(s, "    %s: %u\n", fls_conn_exception_counters_str[i], value);
			}
		}

		conn = conn->all_next;
	}

	spin_unlock_bh(&fct.lock);
	return 0;
}

/*
 * fls_conn_stats_open()
 *	Open callback for debugfs per-connection stats file
 */
static int fls_conn_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, fls_conn_stats_show, NULL);
}

/*
 * File operations for debugfs per-connection stats file
 */
static const struct file_operations fls_conn_stats_fops = {
	.owner = THIS_MODULE,
	.open = fls_conn_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * fls_stats_deinit()
 *	De-initializes debugfs entries for connections.
 */
void fls_stats_deinit(void)
{
	if (fls_gbl_stats_file) {
		debugfs_remove(fls_gbl_stats_file);
		fls_gbl_stats_file = NULL;
	}

	if (fls_conn_stats_file) {
		debugfs_remove(fls_conn_stats_file);
		fls_conn_stats_file = NULL;
	}
}

/*
 * fls_stats_init()
 *	Initializes debugfs entry for per-connection statistics.
 */
void fls_stats_init(void)
{
	if (!fls_debug_root_dir) {
		fls_debug_root_dir = debugfs_create_dir("fls", NULL);
		if (IS_ERR_OR_NULL(fls_debug_root_dir)) {
			FLS_ERROR("Failed to create debugfs directory 'fls'\n");
			fls_debug_root_dir = NULL;
			return;
		}
	}

	fls_gbl_stats_file = debugfs_create_file("gbl_stats", 0444, fls_debug_root_dir, NULL, &fls_gbl_stats_fops);
	if (IS_ERR_OR_NULL(fls_gbl_stats_file)) {
		FLS_ERROR("Failed to create debugfs file 'gbl_stats'\n");
		fls_gbl_stats_file = NULL;
		return;
	}

	FLS_INFO("Debugfs 'fls/gbl_stats' created\n");

	fls_conn_stats_file = debugfs_create_file("conn_stats", 0444, fls_debug_root_dir, &fct, &fls_conn_stats_fops);
	if (IS_ERR_OR_NULL(fls_conn_stats_file)) {
		FLS_ERROR("Failed to create debugfs file 'fls/conn_stats'\n");
		fls_conn_stats_file = NULL;
		return;
	}

	FLS_INFO("Debugfs 'fls/conn_stats' created\n");
}

#endif /* FLS_LITE_ENABLE */
