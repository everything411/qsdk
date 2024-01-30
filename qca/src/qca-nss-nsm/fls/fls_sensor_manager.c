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

#include "fls_sensor_manager.h"

bool fls_sensor_manager_register(struct fls_sensor_manager *fsm, fls_sensor_cb cb, void *app_data)
{
	spin_lock(&(fsm->lock));
	if (fsm->sensor_count >= FLS_SENSOR_MANAGER_MAX_SENSORS - 1) {
		spin_unlock(&(fsm->lock));
		return false;
	}

	fsm->sensors[fsm->sensor_count] = cb;
	fsm->app_data[fsm->sensor_count] = app_data;
	fsm->sensor_count++;
	spin_unlock(&(fsm->lock));
	return true;
}

void fls_sensor_manager_call_all(struct fls_sensor_manager *fsm, struct fls_conn *conn, struct sk_buff *skb)
{
	uint32_t i;
	spin_lock(&(fsm->lock));
	for (i = 0; i < fsm->sensor_count; i++) {
		fsm->sensors[i](fsm->app_data[i], conn, skb);
	}
	spin_unlock(&(fsm->lock));
}

void fls_sensor_manager_init(struct fls_sensor_manager *fsm)
{
	spin_lock_init(&fsm->lock);
	fsm->sensor_count = 0;
}