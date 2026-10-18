/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include <linux/ip.h>
#include <linux/types.h>
#include <mmap_telemetry_public.h>
#include "sac.h"
#include "sac_telemetry.h"

/*
 * To identify if mmap data is read by userspace
 */
static bool telemetry_write_en;

/*
 * Number of outstanding DNS responses in telemetry queue
 */
static uint16_t count;

/*
 * sac_telemetry_send_data()
 *	Send data to user space.
 */
bool sac_telemetry_send_data(struct sac_ctx *ctx, uint8_t *data, uint32_t data_len)
{
	if (!telemetry_write_en) {
		/*
		 * TODO: add retry logic.
		 */
		atomic64_inc(&ctx->stats.dns_write_dis);
		DEBUG_ERROR("Data write disabled, outstanding count: %d\n", count);
		return false;
	}

	if (!ctx->mmap_buf ||
	    (ctx->mmap_offset + data_len) > ctx->mmap_size) {
		DEBUG_ERROR("Invalid mmap state: offset=%u len=%u size=%u\n",
			    ctx->mmap_offset, data_len, ctx->mmap_size);
		return false;
	}

	/*
	 * Copy data in telemetry buffer and notify.
	 */
	memcpy((ctx->mmap_buf + ctx->mmap_offset), data, data_len);
	mmap_telemetry_notify(MMAP_TELEMETRY_UIO_DEV_CATEGORY_SXC,
			      MMAP_TELEMETRY_MEM_TYPE_SAC,
			      ctx->mmap_offset, data_len);

	/*
	 * Prevent sending DNS response if we have reached the limit.
	 */
	if (++count > SAC_TELEMETRY_DNS_RESPONSE_COUNT_MAX) {
		telemetry_write_en = false;
		atomic64_inc(&ctx->stats.dns_tx_limit_reached);
	}

	DEBUG_TRACE("Telemetry frame sent: len=%u outstanding=%u\n", data_len, count);
	return true;
}

/*
 * sac_telemetry_recv_cb()
 *	SAC notifier callback function to receive data from user space.
 */
static int sac_telemetry_recv_cb(struct notifier_block *nb, unsigned long action, void *data)
{
	struct sac_telemetry_request *req = (struct sac_telemetry_request *)data;
	struct sac_ctx *ctx = &sac_ctx_g;

	DEBUG_TRACE("SAC Notifier message type: %d\n", req->msg_type);

	switch (req->msg_type) {
	case SAC_TELEMETRY_MSG_TYPE_CONFIG:
		sac_handle_iface_config(&(req->iface));
		break;

	case SAC_TELEMETRY_MSG_TYPE_ENABLE:
		sac_enable();
		break;

	case SAC_TELEMETRY_MSG_TYPE_DISABLE:
		sac_disable();
		break;

	case SAC_TELEMETRY_MSG_TYPE_RESET:
		telemetry_write_en = true;
		if (count > 0)
			count--;
		atomic64_inc(&ctx->stats.dns_rx_ack);
		break;

	default:
		DEBUG_TRACE("SAC Notifier unsupported message type: %d\n", req->msg_type);
		break;
	}

	return NOTIFY_OK;
}

/*
 * Define a notifier block
 */
static struct notifier_block sac_notifier = {
	.notifier_call = sac_telemetry_recv_cb,
};

/*
 * sac_telemetry_init()
 *	Initialize telemetry for SAC driver.
 */
int sac_telemetry_init(struct sac_ctx *ctx)
{
	/*
	 * Register SAC notifier and allocate memory.
	 */
	mmap_telemetry_register_notifier(&sac_notifier,
					 MMAP_TELEMETRY_UIO_DEV_CATEGORY_SXC,
					 MMAP_TELEMETRY_MEM_TYPE_SAC);
	ctx->mmap_buf = mmap_telemetry_alloc_mem(MMAP_TELEMETRY_UIO_DEV_CATEGORY_SXC,
							MMAP_TELEMETRY_MEM_TYPE_SAC,
							SAC_TELEMETRY_BUF_SZ, false);
	if (!ctx->mmap_buf) {
		DEBUG_ERROR("Unable to allocate memory for mmap telemetry\n");
		return -ENOMEM;
	}

	ctx->mmap_size = SAC_TELEMETRY_BUF_SZ;
	ctx->mmap_offset = 0;
	telemetry_write_en = true;
	count = 0;

	DEBUG_INFO("Allocated mmap of size %u for SAC, mmap_buf: %px\n", ctx->mmap_size, ctx->mmap_buf);
	return 0;
}

/*
 * sac_telemetry_deinit()
 *	Deinitialize telemetry for SAC driver.
 */
void sac_telemetry_deinit(struct sac_ctx *ctx)
{
	/*
	 * Unregister SAC notifier and free memory.
	 */
	mmap_telemetry_unregister_notifier(&sac_notifier,
					   MMAP_TELEMETRY_UIO_DEV_CATEGORY_SXC,
					   MMAP_TELEMETRY_MEM_TYPE_SAC);
	mmap_telemetry_free_mem(MMAP_TELEMETRY_UIO_DEV_CATEGORY_SXC, MMAP_TELEMETRY_MEM_TYPE_SAC);
	telemetry_write_en = false;
	ctx->mmap_buf = NULL;
	ctx->mmap_offset = 0;

	DEBUG_INFO("Notifier unregistered and memory freed\n");
}
