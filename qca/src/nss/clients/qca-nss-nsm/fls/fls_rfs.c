/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * Default values for RFS module parameters
 */
#define FLS_RFS_EVENT_MAX_DEFAULT 128
#define FLS_RFS_EVENT_MASK(max) ((max) - 1)

#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>

#include "fls_debug.h"
#include "fls_rfs.h"
#include "fls_conn.h"

struct fls_event_log {
	uint32_t read_index;
	uint32_t write_index;
	struct fls_event *event_ring_buf;
	uint32_t mask;
	spinlock_t read_lock;
	spinlock_t write_lock;
};

static struct fls_rfs rfs;

static int fls_rfs_subbuf_factor = 1;
module_param(fls_rfs_subbuf_factor, int, 0644);
MODULE_PARM_DESC(fls_rfs_subbuf_factor, "Multiplier for RFS sub-buffer size");

static int fls_rfs_n_subbufs = 1;
module_param(fls_rfs_n_subbufs, int, 0644);
MODULE_PARM_DESC(fls_rfs_n_subbufs, "Number of RFS sub-buffers");

static u32 fls_rfs_event_max = FLS_RFS_EVENT_MAX_DEFAULT;
module_param(fls_rfs_event_max, uint, 0644);
MODULE_PARM_DESC(fls_rfs_event_max, "Maximum number of events in the ring buffer (must be power of 2 and >= 2)");

static u32 fls_rfs_write_delay_ms = 0;
module_param(fls_rfs_write_delay_ms, uint, 0644);
MODULE_PARM_DESC(fls_rfs_write_delay_ms, "Delay in milliseconds between write attempts when RFS channel is full");

static u32 fls_event_queue_delay_ms = 10;
module_param(fls_event_queue_delay_ms, uint, 0644);
MODULE_PARM_DESC(fls_event_queue_delay_ms, "Delay in milliseconds before processing queued events");

atomic_t fls_rfs_active = ATOMIC_INIT(0);
struct delayed_work fls_rfs_work;
struct workqueue_struct * fls_rfs_workqueue;
static struct fls_event_log event_log;
static char buf[sizeof(struct fls_rfs_telemetry_agent_header) + sizeof(struct fls_event)];

void fls_rfs_clean_events(void)
{
	unsigned long irqflags_write, irqflags_read;

	if (atomic_read(&fls_rfs_active) == 0) {
		FLS_WARN("Clean events failed. RFS inactive.\n");
		return;
	}

	spin_lock_irqsave(&event_log.read_lock, irqflags_read);

	if (event_log.read_index == event_log.write_index) {
		spin_unlock_irqrestore(&event_log.read_lock, irqflags_read);
		FLS_ERROR("Event log is empty. read_index:%d, write_index:%d\n", event_log.read_index, event_log.write_index);
		return;
	}

	spin_unlock_irqrestore(&event_log.read_lock, irqflags_read);

	spin_lock_irqsave(&event_log.write_lock, irqflags_write);

	memset(event_log.event_ring_buf, 0, (event_log.mask + 1) * sizeof(struct fls_event));
	printk("Flushed FLS rfs ring buffer, WI = %u, RI = %u", event_log.write_index, event_log.read_index);
	event_log.write_index = 0;

	spin_unlock_irqrestore(&event_log.write_lock, irqflags_write);

	/*
	 * It is best to reset write then read. If a race to acquire read_lock,
	 * between fls_rfs_clean_events() and fls_rfs_write() is won by fls_rfs_write(),
	 * a 0 filled buffer will be sent to userspace. This can easily be mitigated with
	 * filtering in userspace. If read were to be reset prior to write, it is possible
	 * the same event could be sent to userspace twice.
	 */
	spin_lock_irqsave(&event_log.read_lock, irqflags_read);

	event_log.read_index = 0;

	spin_unlock_irqrestore(&event_log.read_lock, irqflags_read);
}

void fls_rfs_write(struct work_struct *work) {
	unsigned long irqflags;
	unsigned budget = event_log.mask;

	if (atomic_read(&fls_rfs_active) == 0) {
		FLS_WARN("Write failed. RFS inactive.\n");
		return;
	}

	if (!rfs.rfschan) {
		FLS_ERROR("RFS channel not initialized, skipping write\n");
		atomic_inc(&fct.fls_gbl_exception_counters[FLS_GBL_EXCEPTION_CHANNEL_NOT_INIT]);
		return;
	}

	spin_lock_irqsave(&event_log.read_lock, irqflags);

	while (budget) {
		if (relay_buf_full(rfs.rbuf)) {
			FLS_TRACE("RFS Buffer is Full, did not write\n");
			atomic_inc(&fct.fls_gbl_exception_counters[FLS_GBL_RFS_BUFF_FULL]);
			goto queue_work;
		}

		if (event_log.read_index == event_log.write_index) {
			spin_unlock_irqrestore(&event_log.read_lock, irqflags);
			atomic_inc(&fct.fls_gbl_exception_counters[FLS_GBL_RFS_EXCEPTION_NO_EVENT_PENDING]);
			FLS_TRACE("Event log is empty. read_index:%d, write_index:%d\n", event_log.read_index, event_log.write_index);
			return;
		}

		memcpy(buf + sizeof(struct fls_rfs_telemetry_agent_header), &(event_log.event_ring_buf[event_log.read_index]), sizeof(struct fls_event));
		event_log.read_index = (event_log.read_index + 1) & event_log.mask;

		relay_write(rfs.rfschan , &buf, sizeof(buf));
		relay_flush(rfs.rfschan);
		FLS_TRACE("Wrote once\n");
		budget--;
	}

	spin_unlock_irqrestore(&event_log.read_lock, irqflags);
	return;

queue_work:
	/*
	 * In the cases where we are writing events faster than userpace can read
	 * we should continue to queue work, so to ensure that we will attempt
	 * to write again and aren't dependent on the enqueue() event
	 */
	if (event_log.read_index != event_log.write_index && atomic_read(&fls_rfs_active) == 1) {
		queue_delayed_work(fls_rfs_workqueue, &fls_rfs_work, msecs_to_jiffies(fls_rfs_write_delay_ms));
	}

	spin_unlock_irqrestore(&event_log.read_lock, irqflags);
}

bool fls_rfs_enqueue(struct fls_event *event)
{
	unsigned long irqflags;
	uint32_t write_index;

	if (atomic_read(&fls_rfs_active) == 0) {
		FLS_WARN("FLS RFS enqueue failed. RFS inactive.\n");
		return false;
	}

	FLS_INFO("FID: enqueue flow event.");
	fls_debug_print_event_info(event);

	spin_lock_irqsave(&event_log.write_lock, irqflags);
	if (((event_log.write_index + 1) & event_log.mask) == event_log.read_index) {
		spin_unlock_irqrestore(&event_log.write_lock, irqflags);
		atomic_inc(&fct.fls_gbl_exception_counters[FLS_GBL_RFS_EXCEPTION_EVENT_QUEUE_FULL]);
		return false;
	}

	write_index = event_log.write_index;
	event_log.event_ring_buf[write_index] = *event;
	event_log.write_index = (write_index + 1) & event_log.mask;
	spin_unlock_irqrestore(&event_log.write_lock, irqflags);

	FLS_INFO("Enqeued flow event at index [%u]", write_index);

	/*
	 * Only queue write task if there is not a write task already queued
	 */
	if (!delayed_work_pending(&fls_rfs_work)) {
		queue_delayed_work(fls_rfs_workqueue, &fls_rfs_work, msecs_to_jiffies(fls_event_queue_delay_ms));
		FLS_TRACE("Queued work to write to RFS\n");
	} else {
		FLS_TRACE("Did not queue RFS write work due to existing work in queue\n");
	}

	return true;
}

static void fls_rfs_tele_agent_header_fill(struct fls_rfs_telemetry_agent_header *tah)
{
	memset(tah, 0, sizeof(struct fls_rfs_telemetry_agent_header));
	tah->stats_type = FLS_RFS_TELEMETRY_DATA_TYPE;
	tah->payload_len = sizeof(struct fls_event);

	memcpy(buf, tah, sizeof(struct fls_rfs_telemetry_agent_header));
}

void fls_rfs_shutdown(void)
{
	atomic_set(&fls_rfs_active, 0);

	/*
	 * Stop scheduling and wait for in‑flight work to finish
	 */
	if (fls_rfs_workqueue) {
		cancel_delayed_work_sync(&fls_rfs_work);
		flush_workqueue(fls_rfs_workqueue);
	}

	/*
	 * Close the relay channel
	 */
	if (rfs.rfschan) {
		relay_close(rfs.rfschan);
		rfs.rfschan = NULL;
	}

	if (event_log.event_ring_buf) {
		vfree(event_log.event_ring_buf);
		event_log.event_ring_buf = NULL;
	}

	/*
	 * Tear down debugfs
	 */
	if (rfs.de) {
		debugfs_remove_recursive(rfs.de);
		rfs.de = NULL;
		fls_debug_root_dir = NULL;
	}

	/*
	 * Destroy workqueue
	 */
	if (fls_rfs_workqueue) {
		destroy_workqueue(fls_rfs_workqueue);
		fls_rfs_workqueue = NULL;
	}
}

static int fls_rfs_remove_buf_file_handler(struct dentry *dentry)
{
	debugfs_remove(dentry);
	return 0;
}

static struct dentry *fls_rfs_create_buf_file_handler(const char *filename,
		struct dentry *parent,
		umode_t mode,
		struct rchan_buf *buf,
		int *is_global)
{
	struct dentry *buf_file;

	buf_file = debugfs_create_file(filename, mode, parent, buf,
			&relay_file_operations);
	if (IS_ERR(buf_file))
		return NULL;

	rfs.rbuf = buf;

	*is_global = 1;
	return buf_file;
}

static struct rchan_callbacks fls_rfs_telemetry_agent_cb = {
	.create_buf_file = fls_rfs_create_buf_file_handler,
	.remove_buf_file = fls_rfs_remove_buf_file_handler,
};

int fls_rfs_init(void)
{
	struct fls_rfs_telemetry_agent_header tah;
	int subbuf_size;

	/*
	 * Validate event_max is a power of 2 and >= 2
	 */
	if (fls_rfs_event_max < 2 || (fls_rfs_event_max & (fls_rfs_event_max - 1))) {
		FLS_WARN("Invalid event_max %u, must be power of 2 and >= 2. Using default %d\n",
			fls_rfs_event_max, FLS_RFS_EVENT_MAX_DEFAULT);
		fls_rfs_event_max = FLS_RFS_EVENT_MAX_DEFAULT;
	}

	spin_lock_init(&event_log.read_lock);
	spin_lock_init(&event_log.write_lock);

	fls_debug_root_dir = debugfs_create_dir(FLS_RFS_NAME, NULL);
	rfs.de = fls_debug_root_dir;
	if (IS_ERR_OR_NULL(rfs.de)) {
		rfs.de = NULL;
		fls_debug_root_dir = NULL;
		return -EPERM;
	}

	/*
	 * Initialize event ring buffer using the module parameter
	 */
	event_log.event_ring_buf = vzalloc(fls_rfs_event_max * sizeof(struct fls_event));
	if (!event_log.event_ring_buf)
		return -ENOMEM;

	event_log.mask = FLS_RFS_EVENT_MASK(fls_rfs_event_max);

	/*
	 * Calculate the sub-buffer size for FLS, based on the subbuf_factor (multiple of the message size)
	 */
	subbuf_size = fls_rfs_subbuf_factor * (sizeof(struct fls_rfs_telemetry_agent_header) + sizeof(struct fls_event));
	rfs.rfschan = relay_open("fls_ifli",
		rfs.de,
		subbuf_size,
		fls_rfs_n_subbufs, &fls_rfs_telemetry_agent_cb, NULL);
	if (!rfs.rfschan) {
		debugfs_remove_recursive(rfs.de);
		rfs.de = NULL;
		fls_debug_root_dir = NULL;
		vfree(event_log.event_ring_buf);
		event_log.event_ring_buf = NULL;
		return -EPERM;
	}

	fls_rfs_workqueue = create_singlethread_workqueue("fls_rfs_workqueue");
	if(!fls_rfs_workqueue) {
		FLS_WARN("Failed to initialize FLS RFS workqueue\n");
		vfree(event_log.event_ring_buf);
		event_log.event_ring_buf = NULL;
		relay_close(rfs.rfschan);
		rfs.rfschan = NULL;
		debugfs_remove_recursive(rfs.de);
		rfs.de = NULL;
		fls_debug_root_dir = NULL;
		return -ENOMEM;
	}

	fls_rfs_tele_agent_header_fill(&tah);

	INIT_DELAYED_WORK(&fls_rfs_work, fls_rfs_write);
	atomic_set(&fls_rfs_active, 1);

	return 0;
}
