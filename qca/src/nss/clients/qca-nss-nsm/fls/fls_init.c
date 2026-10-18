/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "fls_flow.h"
#include "fls_chardev.h"
#include "fls_debug.h"

#ifndef FLS_LITE_ENABLE
#include "fls_conn.h"
#include <sfe_api.h>
#endif

#include <linux/module.h>

/* FLS lite is used with UDP classificaton */
#ifdef FLS_LITE_ENABLE
int udp_clf_enabled = 1;
#else
int udp_clf_enabled;
#endif

static int fls_init_udp_clf(void)
{
	fls_debug_init();

	if (!fls_flow_init()) {
		fls_debug_deinit();
		return -1;
	}
	return 0;
}

static int fls_init_default(void)
{
#ifndef FLS_LITE_ENABLE
	int err;

	err = fls_rfs_init();
	if (err) {
		return err;
	}

	err = fls_conn_tracker_init();
	if (err) {
		return err;
	}

	if (!fls_def_sensor_init(&fct.fsm)) {
		FLS_ERROR("Failed to register def sensor.\n");
		fls_rfs_shutdown();
		return -1;
	}

	sfe_fls_register(fls_conn_create, fls_conn_delete, fls_conn_stats_update);

	fls_debug_init();

	if (!fls_flow_init()) {
		fls_debug_deinit();
		sfe_fls_unregister();
		fls_rfs_shutdown();
		return -1;
	}
#endif
	return 0;
}

void __exit fls_exit(void)
{
	fls_flow_deinit();
	fls_debug_deinit();

	FLS_TRACE("udp_clf_enabled = %d\n", udp_clf_enabled);
	if (!udp_clf_enabled) {
#ifndef FLS_LITE_ENABLE
		FLS_TRACE("sfe_fls_unregister\n");
		sfe_fls_unregister();

		/*
		 * Perform existing connections force flush during module exit.
		 */
		fls_conn_flush();

		/*
		 * Shutdown rfs channel once all connections are flushed.
		 */
		fls_rfs_shutdown();

		/*
		 * Perform the cleanup for fls_conn
		 */
		fls_conn_tracker_exit();
#endif
	}
}

int __init fls_init(void)
{
	if (udp_clf_enabled) {
		return fls_init_udp_clf();
	}

	return fls_init_default();
}

module_init(fls_init)
module_exit(fls_exit)

MODULE_AUTHOR("Qualcomm Technologies, Inc.");
MODULE_DESCRIPTION("Flow Identification Module");
MODULE_LICENSE("Dual BSD/GPL");
