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

#ifndef FLS_CHARDEV_H
#define FLS_CHARDEV_H

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/spinlock.h>

#define FLS_CHARDEV_MSG_MAX 256
#define FLS_CHARDEV_MSG_MASK (FLS_CHARDEV_MSG_MAX - 1)

struct fls_chardev {
	struct cdev cdev;
	struct class *cl;
	dev_t devid;
	wait_queue_head_t readq;
};

struct fls_chardev_msg_log {
	uint32_t write_index;
	struct fls_flow_tm flow_ring_buf_tm[FLS_CHARDEV_MSG_MAX];
	struct fls_flow_udp_clf flow_ring_buf_udp_clf[FLS_CHARDEV_MSG_MAX];
	spinlock_t lock;
};

bool fls_chardev_enqueue(void *flow);
void fls_chardev_shutdown(void);
int fls_chardev_init(void);

#endif
