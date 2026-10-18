/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "fls_sensor_manager.h"

bool fls_sensor_manager_register(struct fls_sensor_manager *fsm, fls_sensor_cb cb, void *app_data)
{
	spin_lock_bh(&(fsm->lock));
	if (fsm->sensor_count >= FLS_SENSOR_MANAGER_MAX_SENSORS - 1) {
		spin_unlock_bh(&(fsm->lock));
		return false;
	}

	fsm->sensors[fsm->sensor_count] = cb;
	fsm->app_data[fsm->sensor_count] = app_data;
	fsm->sensor_count++;
	spin_unlock_bh(&(fsm->lock));
	return true;
}

void fls_sensor_manager_call_all(struct fls_sensor_manager *fsm, struct fls_conn *conn, struct sk_buff *skb)
{
	uint32_t i;
	spin_lock_bh(&(fsm->lock));
	for (i = 0; i < fsm->sensor_count; i++) {
		fsm->sensors[i](fsm->app_data[i], conn, skb);
	}
	spin_unlock_bh(&(fsm->lock));
}

void fls_sensor_manager_init(struct fls_sensor_manager *fsm)
{
	spin_lock_init(&fsm->lock);
	fsm->sensor_count = 0;
}
