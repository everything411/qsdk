// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#include "athdbg_uio.h"
#include "athdbg_core.h"
#include <linux/notifier.h>
#ifdef CPTCFG_ATHDEBUG_UIO_LOGGING
#include <qca-debug-uio/debug_uio_public.h>
#endif

extern struct ath_debug_base *athdbg_base;

static bool ath12k_wait_for_dump_upload;

static int athdbg_uio_notify_from_ss_cb(struct notifier_block *nb,
					 unsigned long action, void *data);

static struct notifier_block athdbg_ss_handler_nb = {
	.notifier_call = athdbg_uio_notify_from_ss_cb,
	.priority      = 0,
};

static int athdbg_uio_notify_from_ss_cb(struct notifier_block *nb,
					 unsigned long action, void *data)
{
	u8 *raw = (u8 *)data;

	pr_info("Host sshandler: userspace interrupt received, payload = %02x %02x %02x %02x\n",
		raw[0], raw[1], raw[2], raw[3]);

	/* Perform any driver-specific handling here. */
	return NOTIFY_OK;
}

/* App name sent by the debug agent via IOCTL_APP_STATUS. */
#define ATH12K_DA_APP_NAME     "debug_agent"
#define ATH12K_DA_APP_NAME_MAX 32

static int athdbg_da_app_status_cb(struct notifier_block *nb,
				   unsigned long action, void *data)
{
	const char *app_name = (const char *)data;

	if (!app_name ||
	    strncmp(app_name, ATH12K_DA_APP_NAME, ATH12K_DA_APP_NAME_MAX))
		return NOTIFY_DONE;

	ath12k_wait_for_dump_upload = (action == 1);

	pr_info("ath12k: DA application is available  %s\n",
		action == 1 ? "up" : "down");

	return NOTIFY_OK;
}

static struct notifier_block athdbg_da_app_status_nb = {
	.notifier_call = athdbg_da_app_status_cb,
	.priority      = 0,
};

/*
 * athdbg_uio_reset_rings() - Reset HOST data and interrupt ring indices
 * to empty state (front=0, rear=0).
 *
 * Called from athdbg_uio_register() before registering the notifier to
 * guarantee a clean ring state on every driver load or reload. Without
 * this reset, stale rear/front indices left by a previous run can make
 * the ring appear full, causing the first debug_uio_write_data() call
 * to return -ENOSPC and drop the critical event.
 */
static void athdbg_uio_reset_rings(void)
{
	void *data_rb;
	void *intr_rb;
	atomic_t *front, *rear;

	/* Reset data ring (map 1) for HOST */
	data_rb = debug_uio_get_mem(DEBUG_UIO_DEV_HOST,
				    DEBUG_UIO_MAP_TYPE_DATA);
	if (data_rb) {
		front = (atomic_t *)data_rb;
		rear  = (atomic_t *)((u8 *)data_rb + sizeof(atomic_t));
		atomic_set(front, 0);
		atomic_set(rear,  0);
		pr_info("athdbg_uio: host data ring reset (front=0 rear=0)\n");
	} else {
		pr_warn("athdbg_uio: host data ring not available for reset\n");
	}

	/* Reset interrupt ring (map 0) for HOST */
	intr_rb = debug_uio_get_mem(DEBUG_UIO_DEV_HOST,
				    DEBUG_UIO_MAP_TYPE_INTERRUPT);
	if (intr_rb) {
		front = (atomic_t *)intr_rb;
		rear  = (atomic_t *)((u8 *)intr_rb + sizeof(atomic_t));
		atomic_set(front, 0);
		atomic_set(rear,  0);
		pr_info("athdbg_uio: host intr ring reset (front=0 rear=0)\n");
	} else {
		pr_warn("athdbg_uio: host intr ring not available for reset\n");
	}
}

int athdbg_uio_register(void)
{
	int ret = 0;

	/* Reset HOST rings before registering notifier to ensure clean
	 * state on driver reload — prevents -ENOSPC on first write
	 */
	athdbg_uio_reset_rings();

	ret = debug_uio_register_notifier(&athdbg_ss_handler_nb,
					  DEBUG_UIO_DEV_HOST,
					  DEBUG_UIO_MAP_TYPE_INTERRUPT);
	if (ret) {
		pr_err("Host sshandler: notifier registration failed: %d\n", ret);
		return ret;
	}

	ret = debug_uio_register_app_status_notifier(&athdbg_da_app_status_nb);
	if (ret)
		pr_err("athdbg_uio: DA app status notifier registration failed: %d\n",
		       ret);

	return ret;
}

int athdbg_uio_unregister(void)
{
	debug_uio_unregister_notifier(&athdbg_ss_handler_nb,
				      DEBUG_UIO_DEV_HOST,
				      DEBUG_UIO_MAP_TYPE_INTERRUPT);

	debug_uio_unregister_app_status_notifier(&athdbg_da_app_status_nb);
	ath12k_wait_for_dump_upload = false;

	return 0;
}

/* Map 1 layout: [front(int32)][rear(int32)][4 x 1020-byte payload blocks] */
#define ATHDBG_UIO_DATA_RING_HEADER_BYTES 8
#define ATHDBG_UIO_MAX_BUFFER_SIZE        1020

void athdbg_uio_buff_write(struct ath12k_crit_record *payload)
{
	void *data_rb;
	atomic_t *rear_atomic;
	u32 rear, offset;
	int ret;

	/*
	 * Use debug_uio data + interrupt rings:
	 *  - snapshot rear BEFORE write to compute offset
	 *  - write payload via debug_uio_write_data() into data ring (map 1)
	 *  - send interrupt metadata via debug_uio_notify() with offset/size
	 */

	data_rb = debug_uio_get_mem(DEBUG_UIO_DEV_HOST,
				    DEBUG_UIO_MAP_TYPE_DATA);
	if (!data_rb) {
		pr_err("athdbg_uio: host data ring not available\n");
		return;
	}

	/*
	 * Data ring (map 1) memory layout:
	 *
	 *  Offset 0               : front (atomic_t, 4 bytes) - read index,
	 *                           advanced by userspace
	 *  Offset sizeof(atomic_t): rear  (atomic_t, 4 bytes) - write index,
	 *                           advanced by kernel
	 *  Offset 8 (HEADER_BYTES): payload[0] (1020 bytes)
	 *  Offset 8 + 1020        : payload[1] (1020 bytes)
	 *  Offset 8 + n*1020      : payload[n] (1020 bytes)
	 *
	 *  payload[n] offset = ATHDBG_UIO_DATA_RING_HEADER_BYTES
	 *                      + n * ATHDBG_UIO_MAX_BUFFER_SIZE
	 */
	rear_atomic = (atomic_t *)((u8 *)data_rb + sizeof(atomic_t));
	rear = (u32)atomic_read(rear_atomic);

	/* Offset into map 1 where this payload will land */
	offset = ATHDBG_UIO_DATA_RING_HEADER_BYTES +
		 rear * ATHDBG_UIO_MAX_BUFFER_SIZE;

	spin_lock_bh(&athdbg_base->uio_lock);

	/* Set timestamp after acquiring lock so it reflects actual write time */
	payload->ts_nsec = ktime_to_ns(ktime_get());

	/* Write critical record into HOST data ring */
	ret = debug_uio_write_data(ATH12K_DEV, payload,
				   sizeof(*payload));
	if (ret == -ENOSPC) {
		pr_err("athdbg_uio: HOST data ring full, dropping critical event\n");
		athdbg_base->uio_trace.dropped++;
		spin_unlock_bh(&athdbg_base->uio_lock);
		return;
	}

	/* Notify userspace with the offset and size, via interrupt ring */
	ret = debug_uio_notify(ATH12K_DEV, offset,
			       (u32)sizeof(*payload));
	if (ret)
		pr_err("athdbg_uio: debug_uio_notify failed: %d\n", ret);

	spin_unlock_bh(&athdbg_base->uio_lock);
}

bool athdbg_uio_check_dump_upload(void)
{
	return ath12k_wait_for_dump_upload;
}
EXPORT_SYMBOL(athdbg_uio_check_dump_upload);

void athdbg_uio_critical_failure_trigger(struct ath12k_base *ab, uint32_t crit_enum)
{
	struct ath12k_crit_record payload;

	payload.radio_id = ab ? ab->device_id : 0xFFFF;  /* 0xFFFF = unknown */
	payload.crit_enum = crit_enum;

	athdbg_uio_buff_write(&payload);
}
EXPORT_SYMBOL(athdbg_uio_critical_failure_trigger);
