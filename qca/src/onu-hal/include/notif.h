/*
*
* Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: ISC
*
*/

#ifndef __notif_h__
#define __notif_h__

#include "libubus.h"

void notif_init(struct ubus_context* ctx);
void notif_send_dm_instance_added_for_transceiver_two(void);
void notif_send_dm_instance_removed_for_transceiver_two(void);
void notif_send_dm_object_changed_for_transceiver_one(void);
void notif_send_dm_object_changed_for_onu_activation(void);
void notif_send_omci_reset_mib(void);

#endif
