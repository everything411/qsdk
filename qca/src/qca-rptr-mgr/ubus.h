/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __RPTR_MGR_UBUS_H
#define __RPTR_MGR_UBUS_H

#include <libubox/avl.h>
#include <libubus.h>
#include <libubox/uloop.h>
#include <eloop.h>

void ubus_reconnect_timeout(void *eloop_data, void *user_ctx);
void rptr_mgr_ubus_connection_lost(struct ubus_context *ctx);
bool rptr_mgr_ubus_init(void);
#endif
