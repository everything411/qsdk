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

#ifndef FLS_SENSOR_MANAGER_H
#define FLS_SENSOR_MANAGER_H

#include <linux/skbuff.h>

#define FLS_SENSOR_MANAGER_MAX_SENSORS 8

struct fls_conn;

typedef uint8_t (*fls_sensor_cb)(void *app_data, struct fls_conn *conn, struct sk_buff *skb);

struct fls_sensor_manager {
	uint8_t sensor_count;
	fls_sensor_cb sensors[FLS_SENSOR_MANAGER_MAX_SENSORS];
	void *app_data[FLS_SENSOR_MANAGER_MAX_SENSORS];
	spinlock_t lock;
};

bool fls_sensor_manager_register(struct fls_sensor_manager *fsm, fls_sensor_cb cb, void *app_data);
void fls_sensor_manager_call_all(struct fls_sensor_manager *fsm, struct fls_conn *conn, struct sk_buff *skb);
void fls_sensor_manager_init(struct fls_sensor_manager *fsm);

#endif
