/*
 **************************************************************************
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
 **************************************************************************
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/version.h>

#include "fls_flow.h"
#include "fls_chardev.h"
#include "fls_debug.h"

static struct fls_chardev chardev;
static struct fls_chardev_msg_log msg_log;
static struct fls_flow_tm temp_tm[FLS_CHARDEV_MSG_MAX];
static struct fls_flow_udp_clf temp_udp_clf[FLS_CHARDEV_MSG_MAX];
static const char* fls_chardev_name = " ";

static ssize_t fls_chardev_fread(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	unsigned long irqflags;
	uint32_t ret = 0;

	/*
	 * Copy full msg structure into buffer.
	 */
	if (!buffer) {
		FLS_ERROR("Could not read data due to missing buffer.\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&msg_log.lock, irqflags);

	if (!msg_log.write_index) {
		spin_unlock_irqrestore(&msg_log.lock, irqflags);
		FLS_INFO("msg log is empty. write_index:%d\n", msg_log.write_index);
		return ret;
	}

	if (!udp_clf_enabled) {
		memcpy(&temp_tm, &msg_log.flow_ring_buf_tm, sizeof(msg_log.flow_ring_buf_tm));
		memset(msg_log.flow_ring_buf_tm, 0, sizeof(msg_log.flow_ring_buf_tm));
	} else {
		memcpy(&temp_udp_clf, &msg_log.flow_ring_buf_udp_clf, sizeof(msg_log.flow_ring_buf_udp_clf));
		memset(msg_log.flow_ring_buf_udp_clf, 0, sizeof(msg_log.flow_ring_buf_udp_clf));
	}

	ret = msg_log.write_index;
	msg_log.write_index = 0;
	spin_unlock_irqrestore(&msg_log.lock, irqflags);

	if (!udp_clf_enabled) {
		if (copy_to_user(buffer, &temp_tm, sizeof(temp_tm))) {
			FLS_ERROR("Failed to write tm_msg to output buffer.\n");
			return -EIO;
		}
	} else {
		if (copy_to_user(buffer, &temp_udp_clf, sizeof(temp_udp_clf))) {
			FLS_ERROR("Failed to write udp_clf_msg to output buffer.\n");
			return -EIO;
		}
	}

	return ret;
}

static unsigned int fls_chardev_poll(struct file *file, struct poll_table_struct *wait)
{
	unsigned int ret = 0;
	unsigned long irqflags;

	poll_wait(file, &chardev.readq, wait);

	spin_lock_irqsave(&msg_log.lock, irqflags);
	if (msg_log.write_index != 0) {
		ret = POLLIN | POLLRDNORM;
	}
	spin_unlock_irqrestore(&msg_log.lock, irqflags);

	return ret;
}

static const struct file_operations fls_chardev_fops = {
	.owner = THIS_MODULE,
	.llseek = NULL,
	.read = fls_chardev_fread,
	.poll = fls_chardev_poll
};

bool fls_chardev_enqueue(void *flow)
{
	unsigned long irqflags;
	uint32_t write_index;
	struct fls_flow_tm *tm_flow;
	struct fls_flow_udp_clf *udp_clf_flow;
	bool ret = true;

	if (!udp_clf_enabled) {
		FLS_TRACE("FLS_FLOW: enqueue tm ");
		tm_flow = (struct fls_flow_tm *)flow;
	} else {
		FLS_TRACE("FLS_FLOW: enqueue udp_clf ");
		udp_clf_flow = (struct fls_flow_udp_clf *)flow;
	}

	spin_lock_irqsave(&msg_log.lock, irqflags);
	if (((msg_log.write_index + 1) & FLS_CHARDEV_MSG_MASK) == 0) {
		/*
		 * Reset the ring buffer, this case should only occur if buffer is full before tm/udp_clf is loaded
		 * tracking old data will result in invalid heavy hitters
		 */
		if (!udp_clf_enabled) {
			memset(msg_log.flow_ring_buf_tm, 0, sizeof(msg_log.flow_ring_buf_tm));
			tm_flow->flags |= FLS_FLOW_FLAG_RESET;
		} else {
			memset(msg_log.flow_ring_buf_udp_clf, 0, sizeof(msg_log.flow_ring_buf_udp_clf));
			udp_clf_flow->flags |= FLS_FLOW_FLAG_RESET;
		}

		msg_log.write_index = 0;
		ret = false;
	}

	write_index = msg_log.write_index;
	if (!udp_clf_enabled) {
		msg_log.flow_ring_buf_tm[write_index] = *tm_flow;
	} else {
		msg_log.flow_ring_buf_udp_clf[write_index] = *udp_clf_flow;
	}

	msg_log.write_index = (write_index + 1) & FLS_CHARDEV_MSG_MASK;
	spin_unlock_irqrestore(&msg_log.lock, irqflags);

	FLS_TRACE("Enqeued chardev msg at index [%u]", write_index);

	if (waitqueue_active(&chardev.readq)) {
		wake_up_interruptible(&chardev.readq);
	}

	return ret;
}

void fls_chardev_shutdown(void)
{
	cdev_del(&chardev.cdev);
	device_destroy(chardev.cl, chardev.devid);
	class_destroy(chardev.cl);
	unregister_chrdev_region(chardev.devid, 1);
}

int fls_chardev_init(void)
{
	int ret;

	spin_lock_init(&msg_log.lock);
	init_waitqueue_head(&chardev.readq);

	if (udp_clf_enabled) {
		fls_chardev_name = "fls_udp_clf";
	} else {
		fls_chardev_name = "fls_tm";
	}

	ret = alloc_chrdev_region(&(chardev.devid), 0, 1, fls_chardev_name);
	if (ret) {
		FLS_ERROR("Failed to allocate device id: %d\n", ret);
		return ret;
	}

	cdev_init(&chardev.cdev, &fls_chardev_fops);
	chardev.cdev.owner = THIS_MODULE;

	ret = cdev_add(&chardev.cdev, chardev.devid, 1);
	if (ret) {
		FLS_ERROR("Failed to add fls device: %d\n", ret);
		unregister_chrdev_region(chardev.devid, 1);
		return ret;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
	chardev.cl = class_create(THIS_MODULE, fls_chardev_name);
#else
	chardev.cl = class_create(fls_chardev_name);
#endif
	device_create(chardev.cl, NULL, chardev.devid, NULL, fls_chardev_name);

	if (!udp_clf_enabled) {
		memset(msg_log.flow_ring_buf_tm, 0, sizeof(msg_log.flow_ring_buf_tm));
	} else {
		memset(msg_log.flow_ring_buf_udp_clf, 0, sizeof(msg_log.flow_ring_buf_udp_clf));
	}

	return 0;
}
