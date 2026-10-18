/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
*/
#include <linux/ip.h>
#include <linux/types.h>
#include <mmap_telemetry_public.h>

#include "sdc.h"
#include "sdc_macdb.h"
#include "sdc_telemetry.h"

static void sdc_telemetry_deq_interrupt_user(struct sdc_telemetry_ctx *ctx)
{
	struct sdc_telemetry_buf *buf = NULL;

	if (!ctx->write_allowed) {
		DEBUG_INFO("Write buffer not available.\n");
		return;
	}

	/*
	 * check if theres data in the queue to be sent to userspace.
	 * If not, just clear the in_use flag.
	 */
	if (ctx->head != ctx->tail) {
		buf = ctx->queue[ctx->tail];
		ctx->queue[ctx->tail] = NULL;
		ctx->tail = (ctx->tail + 1) % SDC_TELEMETRY_Q_MAX;
	}

	if (!buf) {
		ctx->mmap_empty = true;
		return;
	}

	ctx->mmap_empty = false;

	/*
	 * Copy the buffer and interrupt the user.
	 */
	DEBUG_INFO("%pM Sending Device info to userspace\n", buf->mac);
	memcpy(ctx->mmap_buf, buf, ctx->mmap_size);
	ctx->write_allowed = false;
	mmap_telemetry_notify(MMAP_TELEMETRY_UIO_DEV_CATEGORY_SXC, MMAP_TELEMETRY_MEM_TYPE_SDC, 0, ctx->mmap_size);

	kfree(buf);
}

static void sdc_telemetry_work_handler(struct work_struct *work)
{
	struct sdc_telemetry_ctx *ctx = container_of(work, struct sdc_telemetry_ctx, work);
	sdc_telemetry_deq_interrupt_user(ctx);
}

/*
 * Userspace calls write after its done reading the mmap region.
 */
static ssize_t sdc_telemetry_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	struct sdc_telemetry_ctx *ctx = &sdc_ctx.tele_ctx;
	ctx->mmap_empty = true;
	queue_work(system_wq, &ctx->work);
	return len;
}

/*
 * Poll function wakes up userspace when kernel writes the data.
 * This is done by calling wake_up_interruptible() on the waitq.
 */
static unsigned int sdc_telemetry_poll(struct file *file, poll_table *wait)
{
	struct sdc_telemetry_ctx *ctx = &sdc_ctx.tele_ctx;
	poll_wait(file, &ctx->waitq, wait);

	/*
	 * If theres data to be sent to userspace.
	 * return POLLING. This will wake up the userspace.
	 */
	if (!ctx->mmap_empty) {
		return POLLIN;
	}

	/*
	 * Nothing to send to userspace.
	 * Let the userspace sleep till the next set of data is ready.
	 */
	return 0;
}

static const struct file_operations sdc_telemetry_fops = {
	.owner = THIS_MODULE,
	.write = sdc_telemetry_write,
	.poll = sdc_telemetry_poll,
};

static struct miscdevice sdc_telemetry_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = SDC_TELEMETRY_DEV_NAME,
	.fops = &sdc_telemetry_fops,
	.mode = 0666,
};

bool sdc_telemetry_add_tlv(struct sdc_telemetry_buf *buf, struct sdc_telemetry_tlv *tlv, uint16_t max_len)
{
	uint16_t tlv_payload_sz;
	if (!buf || !tlv) {
		return false;
	}

	/*
	 * CHeck if we have space to add the buffer.
	 * Can handle the re-alloc here. But mostly not needed.
	 */
	if (buf->len + tlv->len > max_len) {
	       return false;
	}

	/*
	 * Copy TLV header first.
	 */
	uint8_t *data_ptr =(uint8_t *)buf + buf->len;
	memcpy(data_ptr, tlv, SDC_TELEMETRY_TLV_MDATA_SZ);

	/*
	 * Increment the total buffer len by the tlv header size.
	 */
	buf->len += SDC_TELEMETRY_TLV_MDATA_SZ;

	/*
	 * Write the Data from TLV into the telemetry buffer.
	 */
	data_ptr = (uint8_t *)buf + buf->len;
	tlv_payload_sz = tlv->len - SDC_TELEMETRY_TLV_MDATA_SZ;

	memcpy(data_ptr, tlv->value, tlv_payload_sz);
	buf->len += tlv_payload_sz;

	return true;
}

bool sdc_telemetry_enqueue(struct sdc_telemetry_ctx *ctx, struct sdc_telemetry_buf *buf)
{
	bool can_flush = false;

	spin_lock_bh(&ctx->lock);

	int next_head = (ctx->head + 1) % SDC_TELEMETRY_Q_MAX;
	/*
	 * QUEUE FULL case.
	 * TODO: Check if we can backpressure and maybe stop the packet filer all together.
	 */
	if (next_head == ctx->tail) {
		spin_unlock_bh(&ctx->lock);
		return false;
	}

	ctx->queue[ctx->head] = buf;
	ctx->head = next_head;

	/*
	 * Try to see if we can write the data to mmap.
	 * If there is no pending data to be read by the userspace we can go ahead
	 * and write to it and it will raise an interrupt.
	 *
	 * If its full, we just return withouth queuing the work unit.
	 * When we get an interrupt from the userspace, we go ahead and queue the work unit.
	 */
	if (ctx->mmap_empty) {
		can_flush = true;
	}

	spin_unlock_bh(&ctx->lock);

	if (can_flush) {
		queue_work(system_wq, &ctx->work);
	}

	return true;
}

/*
 * sdc_telemetry_notifier_call
 *
 * SDC notifier callback function
 */
static int sdc_telemetry_notifier_call(struct notifier_block *nb, unsigned long action, void *data)
{
	int ret;
	struct sdc_telemetry_msg_req *req = (struct sdc_telemetry_msg_req *)data;

	DEBUG_INFO("SDC Notifier function\n");
	switch (req->msg_type) {
	case SDC_TELEMETRY_MSG_ADDIF:
		DEBUG_INFO("sdc_telemetry_notifier_call: add intf_name %s\n", req->msg.add_del_if.ifname);
		ret = sdc_add_tracked_if_by_name(req->msg.add_del_if.ifname);
		if (ret)
			DEBUG_ERR("Failed to track if %s. Err: %d\n", req->msg.add_del_if.ifname, ret);

		break;

	case SDC_TELEMETRY_MSG_DELIF:
		DEBUG_INFO("sdc_telemetry_notifier_call: del intf_name %s\n", req->msg.add_del_if.ifname);
		sdc_del_tracked_if_by_name(req->msg.add_del_if.ifname);
		break;

	case SDC_TELEMETRY_MSG_CLEAR:
		DEBUG_INFO("Clear all tracked interfaces\n");
		sdc_clear_tracked_if();
		break;

	case SDC_TELEMETRY_MSG_RESET:
		DEBUG_INFO("Write region read by userspace, reset write bit.\n");
		sdc_ctx.tele_ctx.write_allowed = true;
		sdc_ctx.tele_ctx.mmap_empty = true;
		queue_work(system_wq, &sdc_ctx.tele_ctx.work);

		break;

	default:
		DEBUG_WARN("Not a valid command %d\n", req->msg_type);
		break;

	}

	return NOTIFY_OK;
}

/*
 * sdc_notifier
 *
 * Define a notifier block
 */
static struct notifier_block sdc_notifier = {
	.notifier_call = sdc_telemetry_notifier_call,
};

int sdc_telemetry_init(struct sdc_telemetry_ctx *ctx)
{
	int ret;

	ret = misc_register(&sdc_telemetry_miscdev);
	if (ret) {
		return ret;
	}

	mmap_telemetry_register_notifier(&sdc_notifier, MMAP_TELEMETRY_UIO_DEV_CATEGORY_SXC, MMAP_TELEMETRY_MEM_TYPE_SDC);
	ctx->mmap_buf = mmap_telemetry_alloc_mem(MMAP_TELEMETRY_UIO_DEV_CATEGORY_SXC,
						MMAP_TELEMETRY_MEM_TYPE_SDC,
						SDC_TELEMETRY_BUF_SZ , false);
	if (!ctx->mmap_buf) {
		misc_deregister(&sdc_telemetry_miscdev);
		return -ENOMEM;
	}

	ctx->write_allowed = true;
	ctx->mmap_size = SDC_TELEMETRY_BUF_SZ;
	ctx->mmap_empty = true;

	ctx->head = 0;
	ctx->tail = 0;

	spin_lock_init(&ctx->lock);

	/*
	 * POLL waitqueue.
	 */
	init_waitqueue_head(&ctx->waitq);
	INIT_WORK(&ctx->work, sdc_telemetry_work_handler);
	printk("Allocated mmap of size %u for SDC\n", ctx->mmap_size);
	return 0;
}

void sdc_telemetry_deinit(struct sdc_telemetry_ctx *ctx)
{
	int tail;
	int head;

	/*
	 * Drain the queue.
	 */
	spin_lock_bh(&ctx->lock);
	head = ctx->head;
	tail = ctx->tail;
	while (tail != head) {
		if (ctx->queue[tail]) {
			kfree(ctx->queue[tail]);
		}

		tail = (tail + 1) % SDC_TELEMETRY_Q_MAX;
	}
	spin_unlock_bh(&ctx->lock);

	/*
	 * Free MMAP memory.
	 */
	mmap_telemetry_free_mem(MMAP_TELEMETRY_UIO_DEV_CATEGORY_SXC, MMAP_TELEMETRY_MEM_TYPE_SDC);
	mmap_telemetry_unregister_notifier(&sdc_notifier, MMAP_TELEMETRY_UIO_DEV_CATEGORY_SXC, MMAP_TELEMETRY_MEM_TYPE_SDC);

	misc_deregister(&sdc_telemetry_miscdev);
}
