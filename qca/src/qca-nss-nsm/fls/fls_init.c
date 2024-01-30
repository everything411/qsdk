/*
 **************************************************************************
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "fls_chardev.h"
#include "fls_conn.h"
#include "fls_debug.h"
#include <sfe_api.h>
#include <linux/module.h>

void __exit fls_exit(void)
{
	sfe_fls_unregister();
	fls_chardev_shutdown();
	fls_debug_deinit();
}

int __init fls_init(void)
{
	int err;
	err = fls_chardev_init();
	if (err) {
		return err;
	}

	fls_conn_tracker_init();
	if (!fls_def_sensor_init(&fct.fsm)) {
		FLS_ERROR("Failed to register def sensor.\n");
		fls_chardev_shutdown();
		return -1;
	}

	fls_debug_init();
	sfe_fls_register(fls_conn_create, fls_conn_delete, fls_conn_stats_update);
	return 0;
}

module_init(fls_init)
module_exit(fls_exit)

MODULE_AUTHOR("Qualcomm Technologies, Inc.");
MODULE_DESCRIPTION("Flow Identification Module");
MODULE_LICENSE("Dual BSD/GPL");
