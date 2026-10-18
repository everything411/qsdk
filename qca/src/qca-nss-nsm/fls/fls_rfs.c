/*
 **************************************************************************
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#define FLS_RFS_EVENT_MAX 128
#define FLS_RFS_EVENT_MASK (FLS_RFS_EVENT_MAX - 1)

#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/version.h>
#include <linux/delay.h>

#include "fls_debug.h"
#include "fls_rfs.h"
#include "fls_conn.h"

struct fls_event_log {
	uint32_t read_index;
	uint32_t write_index;
	struct fls_event event_ring_buf[FLS_RFS_EVENT_MAX];
	spinlock_t read_lock;
	spinlock_t write_lock;
};

static struct fls_rfs rfs;

struct delayed_work fls_rfs_work;
struct workqueue_struct * fls_rfs_workqueue;
static struct fls_event_log event_log;
static char buf[sizeof(struct fls_rfs_telemetry_agent_header) + sizeof(struct fls_event)];

void fls_rfs_clean_events(void)
{
	unsigned long irqflags_write, irqflags_read;

	spin_lock_irqsave(&event_log.read_lock, irqflags_read);

	if (event_log.read_index == event_log.write_index) {
		spin_unlock_irqrestore(&event_log.read_lock, irqflags_read);
		FLS_ERROR("Event log is empty. read_index:%d, write_index:%d\n", event_log.read_index, event_log.write_index);
		return;
	}

	spin_unlock_irqrestore(&event_log.read_lock, irqflags_read);

	spin_lock_irqsave(&event_log.write_lock, irqflags_write);

	memset(event_log.event_ring_buf, 0, sizeof(event_log.event_ring_buf));
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

	spin_lock_irqsave(&event_log.read_lock, irqflags);

	if (relay_buf_full(rfs.rbuf)) {
		FLS_TRACE("RFS Buffer is Full, did not write\n");
		goto queue_work;
	}

	if (event_log.read_index == event_log.write_index) {
		spin_unlock_irqrestore(&event_log.read_lock, irqflags);
		FLS_ERROR("Event log is empty. read_index:%d, write_index:%d\n", event_log.read_index, event_log.write_index);
		return;
	}

	memcpy(buf + sizeof(struct fls_rfs_telemetry_agent_header), &(event_log.event_ring_buf[event_log.read_index]), sizeof(struct fls_event));
	event_log.read_index = (event_log.read_index + 1) & FLS_RFS_EVENT_MASK;

	relay_write(rfs.rfschan , &buf, sizeof(buf));
	relay_flush(rfs.rfschan);
	FLS_TRACE("Wrote once\n");

queue_work:
	/*
	 * In the cases where we are writing events faster than userpace can read
	 * we should continue to queue work, so to ensure that we will attempt
	 * to write again and aren't dependent on the enqueue() event
	 */
	if (event_log.read_index != event_log.write_index) {
		queue_delayed_work(fls_rfs_workqueue, &fls_rfs_work, FLS_RFS_WRITE_DELAY);
	}

	spin_unlock_irqrestore(&event_log.read_lock, irqflags);
}

bool fls_rfs_enqueue(struct fls_event *event)
{
	unsigned long irqflags;
	uint32_t write_index;

	FLS_INFO("FID: enqueue flow event.");
	fls_debug_print_event_info(event);

	spin_lock_irqsave(&event_log.write_lock, irqflags);
	if (((event_log.write_index + 1) & FLS_RFS_EVENT_MASK) == event_log.read_index) {
		spin_unlock_irqrestore(&event_log.write_lock, irqflags);
		return false;
	}

	write_index = event_log.write_index;
	event_log.event_ring_buf[write_index] = *event;
	event_log.write_index = (write_index + 1) & FLS_RFS_EVENT_MASK;
	spin_unlock_irqrestore(&event_log.write_lock, irqflags);

	FLS_INFO("Enqeued flow event at index [%u]", write_index);

	/*
	 * Only queue write task if there is not a write task already queued
	 */
	if (!delayed_work_pending(&fls_rfs_work)) {
		queue_delayed_work(fls_rfs_workqueue, &fls_rfs_work, FLS_RFS_WRITE_DELAY);
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
	if (rfs.rfschan) {
		relay_close(rfs.rfschan);
	}

	debugfs_remove_recursive(rfs.de);
	rfs.de = NULL;

	cancel_delayed_work_sync(&fls_rfs_work);
	destroy_workqueue(fls_rfs_workqueue);
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

	spin_lock_init(&event_log.read_lock);
	spin_lock_init(&event_log.write_lock);

	rfs.de = debugfs_create_dir(FLS_RFS_NAME, NULL);
	if (rfs.de == NULL)
		return -EPERM;

	rfs.rfschan = relay_open("fls_ifli",
		rfs.de,
		(sizeof(struct fls_rfs_telemetry_agent_header) + sizeof(struct fls_event)),
		 1, &fls_rfs_telemetry_agent_cb, NULL);
	if (!rfs.rfschan) {
		debugfs_remove_recursive(rfs.de);
		rfs.de = NULL;
		return -EPERM;
	}

	fls_rfs_workqueue = create_singlethread_workqueue("fls_rfs_workqueue");
	if(!fls_rfs_workqueue) {
		FLS_WARN("Failed to initialize FLS RFS workqueue\n");
		return false;
	}

	fls_rfs_tele_agent_header_fill(&tah);

	INIT_DELAYED_WORK(&fls_rfs_work, fls_rfs_write);

	return 0;
}
