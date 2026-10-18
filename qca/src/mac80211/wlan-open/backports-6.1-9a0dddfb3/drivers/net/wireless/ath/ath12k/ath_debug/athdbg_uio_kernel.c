// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/uio_driver.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/xarray.h>
#include "../core.h"
#include "athdbg_uio.h"
#include "../debug.h"

static DEFINE_XARRAY(athdbg_ctxs);
static struct athdbg_uio_ctx *default_uio_ctx;

static inline struct athdbg_uio_ctx *athdbg_get_ctx(struct ath12k_base *ab)
{
	return xa_load(&athdbg_ctxs, (unsigned long)ab);
}

static inline int athdbg_put_ctx(struct ath12k_base *ab, struct athdbg_uio_ctx *ctx)
{
	return xa_err(xa_store(&athdbg_ctxs, (unsigned long)ab, ctx, GFP_KERNEL));
}

static inline void athdbg_del_ctx(struct ath12k_base *ab)
{
	xa_erase(&athdbg_ctxs, (unsigned long)ab);
}

static int athdbg_uio_irqcontrol(struct uio_info *info, s32 irq_on)
{
	return 0;
}

static inline void __athdbg_ring_write(struct athdbg_uio_ctx *ctx,
				       u8 level, u64 mask,
				       const char *msg)
{
	struct ath12k_debug_log_entry *base;
	u32 *pidx;
	size_t size_bytes;
	u32 entries;
	u32 idx;

	base = ctx->dbg_buf;
	pidx = &ctx->dbg_idx;
	size_bytes = ctx->dbg_size_bytes;

	if (!base || !size_bytes)
		return;

	entries = size_bytes / sizeof(*base);

	if (!entries)
		return;

	rcu_read_lock();

	idx = (*pidx)++ % entries;

	base[idx].timestamp = ktime_to_ns(ktime_get());
	base[idx].debug_mask = mask;
	base[idx].log_level  = level;
	strscpy(base[idx].message, msg, sizeof(base[idx].message));

	/* Wait for all the writes to be finished */
	smp_wmb();

	rcu_read_unlock();
}

int athdbg_uio_register(struct ath12k_base *ab)
{
	struct athdbg_uio_ctx *ctx;
	int ret = 0;
	char pd_name[64];

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	ctx->ab = ab;

	snprintf(pd_name, sizeof(pd_name), "ath12k_uio_%s", dev_name(ab->dev));
	ctx->pdev = platform_device_register_simple(pd_name, -1, NULL, 0);
	if (IS_ERR(ctx->pdev)) {
		ret = PTR_ERR(ctx->pdev);
		ctx->pdev = NULL;
		goto err_free_ctx;
	}
	ctx->uio_dev = &ctx->pdev->dev;

	ctx->dbg_size_bytes  = ATH12K_UIO_DBG_SIZE_BYTES;
	ctx->crit.size_bytes = ATH12K_UIO_CRIT_SIZE_BYTES;

	ctx->dbg_buf  = kzalloc(ctx->dbg_size_bytes, GFP_KERNEL);
	if (!ctx->dbg_buf) {
		ret = -ENOMEM;
		goto err_unreg_pdev;
	}

	ctx->crit.buf = kzalloc(ctx->crit.size_bytes, GFP_KERNEL);
	if (!ctx->crit.buf) {
		ret = -ENOMEM;
		goto err_free_dbg_buf;
	}

	/* Initialize critical ring metadata */
	ctx->crit.ring = (struct ath12k_crit_ring *)ctx->crit.buf;
	ctx->crit.ring->version = 0x00010000;
	ctx->crit.ring->count = (ctx->crit.size_bytes - sizeof(struct ath12k_crit_ring))
				 / sizeof(struct ath12k_crit_record);
	ctx->crit.ring->write_idx = 0;
	ctx->crit.ring->read_idx = 0;
	ctx->crit.ring->dropped = 0;
	ctx->crit.ring->flags = 1;/* Overwrite mode enabled by default */

	/* Records start after metadata */
	ctx->crit.records = (struct ath12k_crit_record *)((u8 *)ctx->crit.buf +
			     sizeof(struct ath12k_crit_ring));

	ctx->uio_info = devm_kzalloc(ab->dev, sizeof(*ctx->uio_info), GFP_KERNEL);
	if (!ctx->uio_info) {
		ret = -ENOMEM;
		goto err_free_rings;
	}

	ctx->uio_info->name    = "ath12k_log";
	ctx->uio_info->version = "0.2";
	ctx->uio_info->mem[0].memtype = UIO_MEM_LOGICAL;
	ctx->uio_info->mem[0].addr = (unsigned long)ctx->dbg_buf;
	ctx->uio_info->mem[0].size = ctx->dbg_size_bytes;
	ctx->uio_info->mem[0].internal_addr = ctx->dbg_buf;

	ctx->uio_info->mem[1].memtype = UIO_MEM_LOGICAL;
	ctx->uio_info->mem[1].addr = (unsigned long)ctx->crit.buf;
	ctx->uio_info->mem[1].size = ctx->crit.size_bytes;
	ctx->uio_info->mem[1].internal_addr = ctx->crit.buf;

	ctx->uio_info->irq        = UIO_IRQ_CUSTOM;
	ctx->uio_info->irqcontrol = athdbg_uio_irqcontrol;

	#ifdef CONFIG_UIO
	ret = uio_register_device(ctx->uio_dev, ctx->uio_info);
	#endif

	if (ret)
		goto err_free_rings;
	ret = athdbg_put_ctx(ab, ctx);
	if (ret)
		goto err_free_rings;

	if (!default_uio_ctx)
		default_uio_ctx = ctx;

	return 0;

err_free_rings:
	#ifdef CONFIG_UIO
	if (ctx->uio_info)
		uio_unregister_device(ctx->uio_info);
	kfree(ctx->crit.buf);
err_free_dbg_buf:
	kfree(ctx->dbg_buf);
err_unreg_pdev:
	if (ctx->pdev)
		platform_device_unregister(ctx->pdev);
err_free_ctx:
	kfree(ctx);
	return ret;
}
EXPORT_SYMBOL(athdbg_uio_register);

void athdbg_uio_unregister(struct ath12k_base *ab)
{
	struct athdbg_uio_ctx *ctx = athdbg_get_ctx(ab);

	if (!ctx)
		return;

	athdbg_del_ctx(ab);

	if (default_uio_ctx == ctx)
		default_uio_ctx = NULL;

	#ifdef CONFIG_UIO
	if (ctx->uio_info)
		uio_unregister_device(ctx->uio_info);
	kfree(ctx->dbg_buf);
	kfree(ctx->crit.buf);
	if (ctx->pdev)
		platform_device_unregister(ctx->pdev);

	kfree(ctx);
}
EXPORT_SYMBOL(athdbg_uio_unregister);

void athdbg_uio_log_info(struct ath12k_base *ab, const char *fmt, va_list args)
{
	struct athdbg_uio_ctx *ctx;
	char msg[256];

	if (!fmt)
		return;

	vsnprintf(msg, sizeof(msg), fmt, args);

	if (likely(ab))
		ctx = athdbg_get_ctx(ab);
	else
		ctx = default_uio_ctx;

	if (!ctx)
		return;

	__athdbg_ring_write(ctx, ATH12K_LOG_LEVEL_INFO,
			    ATH12K_DBG_ANY, msg);
}
EXPORT_SYMBOL(athdbg_uio_log_info);

void athdbg_uio_log_warn(struct ath12k_base *ab, const char *fmt, va_list args)
{
	struct athdbg_uio_ctx *ctx;
	char msg[256];

	if (!fmt)
		return;

	vsnprintf(msg, sizeof(msg), fmt, args);

	if (likely(ab))
		ctx = athdbg_get_ctx(ab);
	else
		ctx = default_uio_ctx;

	if (!ctx)
		return;

	__athdbg_ring_write(ctx, ATH12K_LOG_LEVEL_WARN,
			    ATH12K_DBG_ANY, msg);
}
EXPORT_SYMBOL(athdbg_uio_log_warn);

void athdbg_uio_log_err(struct ath12k_base *ab, const char *fmt, va_list args)
{
	struct athdbg_uio_ctx *ctx;
	char msg[256];

	if (!fmt)
		return;

	vsnprintf(msg, sizeof(msg), fmt, args);

	if (likely(ab))
		ctx = athdbg_get_ctx(ab);
	else
		ctx = default_uio_ctx;

	if (!ctx)
		return;

	__athdbg_ring_write(ctx, ATH12K_LOG_LEVEL_ERR,
			    ATH12K_DBG_ANY, msg);
}
EXPORT_SYMBOL(athdbg_uio_log_err);

void athdbg_uio_log_debug(struct ath12k_base *ab, u64 mask, const char *fmt, va_list args)
{
	struct athdbg_uio_ctx *ctx;
	char msg[256];

	if (!fmt)
		return;

	vsnprintf(msg, sizeof(msg), fmt, args);

	if (likely(ab))
		ctx = athdbg_get_ctx(ab);
	else
		ctx = default_uio_ctx;

	if (!ctx)
		return;

	__athdbg_ring_write(ctx, ATH12K_LOG_LEVEL_DEBUG, mask, msg);
}
EXPORT_SYMBOL(athdbg_uio_log_debug);

void athdbg_uio_critical_failure_trigger(struct ath12k_base *ab, uint32_t crit_enum)
{
#ifdef CPTCFG_ATHDEBUG_UIO_LOGGING
	struct athdbg_uio_ctx *ctx;
	struct ath12k_crit_ring *ring;
	struct ath12k_crit_record *record;
	u32 write_idx;
	bool overwrite;

	if (likely(ab))
		ctx = athdbg_get_ctx(ab);
	else
		ctx = default_uio_ctx;

	if (!ctx || !ctx->crit.ring)
		return;

	ring = ctx->crit.ring;
	overwrite = ring->flags & 0x1;

	rcu_read_lock();

	/* Check if buffer is full */
	if (!overwrite && ((ring->write_idx + 1) % ring->count == ring->read_idx)) {
		ring->dropped++;
		rcu_read_unlock();
		return;
	}

	write_idx = ring->write_idx;
	record = &ctx->crit.records[write_idx];

	/* Fill record */
	record->radio_id = ab ? ab->device_id : 0xFFFF;  /* 0xFFFF = unknown */
	record->crit_enum = crit_enum;
	record->ts_nsec = ktime_to_ns(ktime_get());

	/* Memory barrier before updating index */
	smp_wmb();

	/* Update write index */
	ring->write_idx = (write_idx + 1) % ring->count;

	rcu_read_unlock();

	if (ctx->uio_info)
		uio_event_notify(ctx->uio_info);
#endif
}
EXPORT_SYMBOL(athdbg_uio_critical_failure_trigger);
