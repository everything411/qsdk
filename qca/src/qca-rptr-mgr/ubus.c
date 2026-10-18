/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ubus.h"

struct ubus_context *ctx;

void ubus_reconnect_timeout(void *eloop_data, void *user_ctx)
{
	if (ubus_reconnect(ctx, NULL)) {
		eloop_register_timeout(1, 0, ubus_reconnect_timeout, ctx, NULL);
		return;
	}

	ubus_add_uloop(ctx);
}

void rptr_mgr_ubus_connection_lost(struct ubus_context *ctx)
{
	uloop_fd_delete(&ctx->sock);
	eloop_register_timeout(1, 0, ubus_reconnect_timeout, ctx, NULL);
}

bool rptr_mgr_ubus_init(void)
{
	if (ctx)
		return true;

	eloop_add_uloop();
	ctx = ubus_connect(NULL);
	if (!ctx)
		return false;

	ctx->connection_lost = rptr_mgr_ubus_connection_lost;
	ubus_add_uloop(ctx);

	return true;
}

static void rptr_mgr_ubus_free(void)
{
	if (!ctx)
		return;

	uloop_fd_delete(&ctx->sock);
	ubus_free(ctx);
	ctx = NULL;
}
